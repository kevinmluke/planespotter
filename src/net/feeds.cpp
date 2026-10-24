#include "feeds.h"

#include <ArduinoJson.h>

#include <algorithm>
#include <cmath>

#include "../core/geo.h"
#include "../core/text.h"
#include "../core/timing.h"
#include "config.h"
#include "http.h"

namespace feeds {
namespace {

// Per-service back-off after failures, rate limiting or server errors.
timing::Backoff gAdsbBackoff[2] = {{ADSB_POLL_MS, 10 * 60 * 1000UL}, {ADSB_POLL_MS, 10 * 60 * 1000UL}};
timing::Backoff gRouteBackoff(30000, 30 * 60 * 1000UL);
timing::Backoff gHexdbBackoff(30000, 30 * 60 * 1000UL);
timing::Backoff gTleBackoff(15 * 60 * 1000UL, 12 * 3600 * 1000UL);

// Records the outcome of a request; true if it may be retried later (don't cache a negative answer).
bool track(timing::Backoff& b, const http::Result& r) {
  if (r.retryable()) {
    b.failure(millis(), r.retryAfterS);
    return true;
  }
  b.success();
  return false;
}

void copyTrim(char* dst, size_t n, const char* src) {
  if (!src) {
    dst[0] = 0;
    return;
  }
  while (*src == ' ') src++;
  size_t len = strnlen(src, n - 1);
  while (len > 0 && src[len - 1] == ' ') len--;
  memcpy(dst, src, len);
  dst[len] = 0;
}

void parseAircraft(JsonObjectConst o, ac::Aircraft& a) {
  copyTrim(a.hex, sizeof(a.hex), o["hex"] | "");
  copyTrim(a.callsign, sizeof(a.callsign), o["flight"] | "");
  copyTrim(a.reg, sizeof(a.reg), o["r"] | "");
  copyTrim(a.type, sizeof(a.type), o["t"] | "");
  copyTrim(a.category, sizeof(a.category), o["category"] | "");
  a.lat = o["lat"] | NAN;
  a.lon = o["lon"] | NAN;
  if (o["alt_baro"].is<const char*>()) {  // "ground"
    a.onGround = true;
    a.altFt = 0;
  } else if (o["alt_baro"].is<int>() || o["alt_baro"].is<float>()) {
    a.altFt = o["alt_baro"].as<int32_t>();
  } else if (o["alt_geom"].is<int>()) {
    a.altFt = o["alt_geom"].as<int32_t>();
  }
  a.gsKt = o["gs"] | -1.0f;
  a.trackDeg = o["track"] | (o["true_heading"] | -1.0f);
  a.vrateFpm = o["baro_rate"] | (o["geom_rate"] | 0);
  a.squawk = static_cast<uint16_t>(atoi(o["squawk"] | "0"));
  const char* em = o["emergency"] | "none";
  a.emergencyFlag = strcmp(em, "none") != 0 && strcmp(em, "") != 0;
  a.military = ((o["dbFlags"] | 0) & 1) != 0;
  a.seenPosS = o["seen_pos"] | (o["seen"] | 0.0f);
}

JsonDocument& aircraftFilter() {
  static JsonDocument f;
  if (f.isNull()) {
    for (const char* list : {"ac", "aircraft"}) {
      JsonObject e = f[list].add<JsonObject>();
      for (const char* k : {"hex", "flight", "r", "t", "category", "lat", "lon", "alt_baro", "alt_geom", "gs", "track",
                            "true_heading", "baro_rate", "geom_rate", "squawk", "emergency", "dbFlags", "seen_pos",
                            "seen"}) {
        e[k] = true;
      }
    }
  }
  return f;
}

// ---- Route cache (LRU by fetch time)
Route gCache[ROUTE_CACHE_SIZE];

Route* findCached(const char* cs) {
  for (auto& r : gCache)
    if (r.callsign[0] && strcmp(r.callsign, cs) == 0) return &r;
  return nullptr;
}

Route* cacheSlot(const char* cs) {
  Route* r = findCached(cs);
  if (r) return r;
  const uint32_t now = millis();
  Route* oldest = &gCache[0];
  for (auto& c : gCache) {
    if (!c.callsign[0]) return &c;
    if (now - c.fetchedMs > now - oldest->fetchedMs) oldest = &c;  // compare ages: wrap-safe
  }
  return oldest;
}

void airportFrom(JsonObjectConst ap, char* city, size_t cityN, char* code, size_t codeN, double& lat, double& lon) {
  text::toFontSafe(ap["municipality"] | (ap["name"] | ""), city, cityN);
  const char* iata = ap["iata_code"] | "";
  copyTrim(code, codeN, iata[0] ? iata : (ap["icao_code"] | ""));
  lat = ap["latitude"] | NAN;
  lon = ap["longitude"] | NAN;
}

// Rejects routes the aircraft is clearly not flying (stale or reused callsigns).
bool plausible(const Route& r, double lat, double lon) {
  if (std::isnan(r.oLat) || std::isnan(r.dLat) || std::isnan(lat)) return true;  // can't tell
  const double total = geo::distanceKm(r.oLat, r.oLon, r.dLat, r.dLon);
  const double toO = geo::distanceKm(lat, lon, r.oLat, r.oLon);
  const double toD = geo::distanceKm(lat, lon, r.dLat, r.dLon);
  if (toO < 150 || toD < 150) return true;
  const double xt = geo::crossTrackKm(r.oLat, r.oLon, r.dLat, r.dLon, lat, lon);
  // Allow airways/vectors to deviate more on long routes; never accept a detour far beyond the trip.
  return xt < std::max(250.0, total * 0.15) && toO + toD < total * 1.6 + 300;
}

}  // namespace

AircraftResult fetchAircraft(double lat, double lon, float radiusKm, ac::Aircraft* out, int max) {
  AircraftResult res;
  const int nm = std::min(250, static_cast<int>(std::ceil(radiusKm / 1.852)) + 1);
  char url[160];
  JsonDocument doc;
  static http::Result r;  // keeps the last status while every source is backing off
  const char* fmts[] = {ADSB_URL_FMT, ADSB_FALLBACK_URL_FMT};
  for (int i = 0; i < 2; i++) {
    if (!gAdsbBackoff[i].ready(millis())) continue;
    snprintf(url, sizeof(url), fmts[i], lat, lon, nm);
    doc.clear();
    r = http::getJson(url, doc, aircraftFilter());
    if (r.ok()) {
      gAdsbBackoff[i].success();
      break;
    }
    gAdsbBackoff[i].failure(millis(), r.retryAfterS);
  }
  res.status = r.status;
  if (!r.ok()) return res;

  JsonArrayConst list = doc["ac"].is<JsonArrayConst>() ? doc["ac"].as<JsonArrayConst>() : doc["aircraft"].as<JsonArrayConst>();
  static ac::Aircraft all[160];
  int n = 0;
  for (JsonObjectConst o : list) {
    if (n >= 160) break;
    ac::Aircraft a;
    parseAircraft(o, a);
    if (std::isnan(a.lat) || std::isnan(a.lon) || !a.hex[0]) continue;
    a.distKm = static_cast<float>(geo::distanceKm(lat, lon, a.lat, a.lon));
    a.bearingDeg = static_cast<float>(geo::bearingDeg(lat, lon, a.lat, a.lon));
    if (a.distKm > radiusKm) continue;
    all[n++] = a;
  }
  std::sort(all, all + n, [](const ac::Aircraft& x, const ac::Aircraft& y) { return x.distKm < y.distKm; });
  res.totalInRange = n;
  res.count = std::min(n, max);
  std::copy(all, all + res.count, out);
  res.ok = true;
  return res;
}

bool cachedRoute(const char* callsign, Route& out) {
  Route* r = findCached(callsign);
  if (!r) return false;
  out = *r;
  return true;
}

bool lookupRoute(const char* callsign, double lat, double lon, Route& out) {
  if (!callsign || !callsign[0]) return false;
  Route* c = findCached(callsign);
  if (c && (c->known || millis() - c->fetchedMs < ROUTE_RETRY_MS)) {
    c->plausible = c->known && plausible(*c, lat, lon);
    out = *c;
    return true;
  }
  Route r;
  copyTrim(r.callsign, sizeof(r.callsign), callsign);

  char url[128];
  snprintf(url, sizeof(url), ROUTE_URL_FMT, r.callsign);
  JsonDocument filter;
  JsonObject fr = filter["response"]["flightroute"].to<JsonObject>();
  fr["callsign_iata"] = true;
  fr["airline"]["name"] = true;
  for (const char* end : {"origin", "destination"}) {
    for (const char* k : {"municipality", "name", "iata_code", "icao_code", "latitude", "longitude"}) fr[end][k] = true;
  }
  if (!gRouteBackoff.ready(millis())) return false;
  JsonDocument doc;
  http::Result hr = http::getJson(url, doc, filter, 8000);
  if (track(gRouteBackoff, hr)) return false;  // network trouble: don't cache, try again later
  JsonObjectConst route = doc["response"]["flightroute"];
  if (hr.ok() && !route.isNull()) {
    r.known = true;
    copyTrim(r.flightIata, sizeof(r.flightIata), route["callsign_iata"] | "");
    text::toFontSafe(route["airline"]["name"] | "", r.airline, sizeof(r.airline));
    airportFrom(route["origin"], r.originCity, sizeof(r.originCity), r.originCode, sizeof(r.originCode), r.oLat, r.oLon);
    airportFrom(route["destination"], r.destCity, sizeof(r.destCity), r.destCode, sizeof(r.destCode), r.dLat, r.dLon);
  } else if ((hr.status == 404 || hr.ok()) && gHexdbBackoff.ready(millis())) {
    // Fallback: hexdb gives ICAO airport codes only ("LFPG-KJFK").
    snprintf(url, sizeof(url), ROUTE_FALLBACK_URL_FMT, r.callsign);
    JsonDocument hd;
    JsonDocument hf;
    hf["route"] = true;
    const http::Result xr = http::getJson(url, hd, hf, 8000);
    track(gHexdbBackoff, xr);
    if (xr.ok()) {
      const char* rt = hd["route"] | "";
      const char* dash = strchr(rt, '-');
      if (dash && dash - rt == 4 && strlen(dash + 1) >= 4) {
        r.known = true;
        copyTrim(r.originCode, sizeof(r.originCode), String(rt).substring(0, 4).c_str());
        copyTrim(r.destCode, sizeof(r.destCode), String(dash + 1).substring(0, 4).c_str());
      }
    }
  } else if (!hr.ok() && hr.status != 404) {
    return false;  // unexpected answer: don't cache, try again later
  }
  r.fetchedMs = millis();
  r.plausible = r.known && plausible(r, lat, lon);
  *cacheSlot(r.callsign) = r;
  out = r;
  return true;
}

float routeProgress(const Route& r, double lat, double lon) {
  if (std::isnan(r.oLat) || std::isnan(r.dLat)) return -1;
  const double total = geo::distanceKm(r.oLat, r.oLon, r.dLat, r.dLon);
  if (total < 50) return -1;
  const double along = geo::alongTrackKm(r.oLat, r.oLon, r.dLat, r.dLon, lat, lon);
  return static_cast<float>(std::min(1.0, std::max(0.0, along / total)));
}

bool fetchTle(char* line1, char* line2) {
  if (!gTleBackoff.ready(millis())) return false;
  String body;
  const http::Result r = http::getText(TLE_URL, body, 512);
  track(gTleBackoff, r);
  if (!r.ok()) return false;
  // Expect: name line, line 1, line 2
  int l1 = body.indexOf("\n1 ");
  int l2 = body.indexOf("\n2 ");
  if (l1 < 0 || l2 < 0) return false;
  String a = body.substring(l1 + 1, l1 + 70);
  String b = body.substring(l2 + 1, l2 + 70);
  a.trim();
  b.trim();
  if (a.length() != 69 || b.length() != 69) return false;
  strcpy(line1, a.c_str());
  strcpy(line2, b.c_str());
  return true;
}

}  // namespace feeds

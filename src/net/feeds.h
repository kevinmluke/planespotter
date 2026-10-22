// Data feeds: nearby aircraft (ADS-B aggregators), flight routes, ISS orbital elements.
#pragma once

#include <Arduino.h>

#include "../core/aircraft.h"

namespace feeds {

constexpr int kMaxAircraft = 64;

struct AircraftResult {
  bool ok = false;
  int status = 0;        // HTTP status of the last attempt
  int count = 0;
  int totalInRange = 0;  // before truncation to kMaxAircraft
};

// Fetches aircraft within `radiusKm` of home, sorted by distance. Tries the
// primary aggregator, then the fallback.
AircraftResult fetchAircraft(double lat, double lon, float radiusKm, ac::Aircraft* out, int max);

struct Route {
  char callsign[9] = "";
  bool known = false;
  bool plausible = false;
  char flightIata[10] = "";
  char airline[40] = "";
  char originCity[32] = "", originCode[5] = "";
  char destCity[32] = "", destCode[5] = "";
  double oLat = NAN, oLon = NAN, dLat = NAN, dLon = NAN;
  uint32_t fetchedMs = 0;
};

// Looks up a callsign (adsbdb, then hexdb). Results, including "unknown", are cached.
// `lat/lon` is the aircraft's current position, used to reject stale routes.
bool lookupRoute(const char* callsign, double lat, double lon, Route& out);
// Cache-only lookup (no network). Returns false if not cached.
bool cachedRoute(const char* callsign, Route& out);

// Fraction of the route flown (0..1) from the aircraft position, or -1 if unknown.
float routeProgress(const Route& r, double lat, double lon);

// ISS orbital elements from CelesTrak. Lines are 69 characters + NUL.
bool fetchTle(char* line1, char* line2);

}  // namespace feeds

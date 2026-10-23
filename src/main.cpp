// PlaneSpotter - ESP32-S3 + 5.79" e-paper desk display.
//
// Two tasks:
//   - network task (core 0): aircraft polling, routes, ISS elements, pass
//     prediction, OTA. Writes into `gShared` under `gLock`.
//   - Arduino loop (core 1): button, web server, Wi-Fi provisioning, choosing
//     what to show and drawing it.
#include <Arduino.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_random.h>

#include <algorithm>
#include <cmath>
#include <new>

#include "app/app.h"
#include "app/settings.h"
#include "config.h"
#include "core/aircraft.h"
#include "core/geo.h"
#include "core/iss.h"
#include "core/text.h"
#include "generated/aircraft_db.h"
#include "hw/display.h"
#include "net/feeds.h"
#include "net/http.h"
#include "net/ota.h"
#include "net/portal.h"
#include "core/santa.h"
#include "ui/screens.h"

using i18n::S;
using i18n::tr;

namespace {

// ===========================================================================
// Shared state

struct Shared {
  // Inputs for the network task
  bool hasLocation = false;
  double lat = 0, lon = 0;
  float fetchRadiusKm = 80;
  uint32_t configGen = 0;  // bumped whenever location changes
  char wantRoute[9] = "";
  double wantLat = 0, wantLon = 0;
  bool otaRequested = false;
  bool autoUpdate = true;

  // Aircraft
  ac::Aircraft list[feeds::kMaxAircraft];
  int count = 0;
  bool everOk = false;
  uint32_t lastOkMs = 0;
  time_t lastOkUnix = 0;
  int lastStatus = 0;
  uint32_t lastAttemptMs = 0;

  // Current route
  feeds::Route route;

  // ISS
  bool tleValid = false;
  char tle1[70] = "", tle2[70] = "";
  uint32_t tleGen = 0;
  bool hasPass = false;
  iss::Pass pass;

  // OTA
  bool otaRunning = false;
  ota::Progress otaProgress;
};

Shared gShared;
SemaphoreHandle_t gLock;

struct Lock {
  Lock() { xSemaphoreTake(gLock, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(gLock); }
};

Settings gSettings;

bool timeValid() { return time(nullptr) > 1700000000; }

// ===========================================================================
// Network task

void loadTleCache() {
  Preferences p;
  p.begin("cache", true);
  String a = p.getString("tle1", ""), b = p.getString("tle2", "");
  p.end();
  iss::Tracker t;
  if (a.length() == 69 && b.length() == 69 && t.setTle(a.c_str(), b.c_str())) {
    Lock l;
    strcpy(gShared.tle1, a.c_str());
    strcpy(gShared.tle2, b.c_str());
    gShared.tleValid = true;
    gShared.tleGen++;
  }
}

void otaProgressCb(const ota::Progress& p) {
  Lock l;
  gShared.otaProgress = p;
}

void netTask(void*) {
  uint32_t lastTleTry = 0, lastPassMs = 0, lastPassGen = 0, lastTleGenForPass = 0;
  uint32_t nextOtaMs = OTA_CHECK_FIRST_MS;
  iss::Tracker tracker;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(200));
    if (WiFi.status() != WL_CONNECTED) continue;
    const uint32_t now = millis();

    bool hasLoc;
    double lat, lon;
    float radius;
    uint32_t gen;
    char want[9];
    double wLat, wLon;
    {
      Lock l;
      hasLoc = gShared.hasLocation;
      lat = gShared.lat;
      lon = gShared.lon;
      radius = gShared.fetchRadiusKm;
      gen = gShared.configGen;
      strcpy(want, gShared.wantRoute);
      wLat = gShared.wantLat;
      wLon = gShared.wantLon;
    }

    // ---- Aircraft
    if (hasLoc && now - gShared.lastAttemptMs >= ADSB_POLL_MS) {
      static ac::Aircraft buf[feeds::kMaxAircraft];
      gShared.lastAttemptMs = now;
      const feeds::AircraftResult r = feeds::fetchAircraft(lat, lon, radius, buf, feeds::kMaxAircraft);
      Lock l;
      gShared.lastStatus = r.status;
      if (r.ok && gen == gShared.configGen) {
        memcpy(gShared.list, buf, sizeof(ac::Aircraft) * r.count);
        gShared.count = r.count;
        gShared.everOk = true;
        gShared.lastOkMs = millis();
        gShared.lastOkUnix = time(nullptr);
      }
      if (r.ok) ota::reportHealthy();
    }

    // ---- Route for the aircraft on screen (with a back-off if the lookup service is unreachable)
    static char lastRouteTry[9] = "";
    static uint32_t lastRouteTryMs = 0;
    if (want[0]) {
      feeds::Route r;
      bool have;
      {
        Lock l;
        have = strcmp(gShared.route.callsign, want) == 0;
      }
      const bool backoff = strcmp(lastRouteTry, want) == 0 && now - lastRouteTryMs < 30000;
      if (!have && !backoff) {
        strcpy(lastRouteTry, want);
        lastRouteTryMs = now;
        if (feeds::lookupRoute(want, wLat, wLon, r)) {
          Lock l;
          gShared.route = r;
        }
      }
    }

    // ---- ISS orbital elements
    bool tleValid;
    char tle1[70], tle2[70];
    uint32_t tleGen;
    {
      Lock l;
      tleValid = gShared.tleValid;
      strcpy(tle1, gShared.tle1);
      strcpy(tle2, gShared.tle2);
      tleGen = gShared.tleGen;
    }
    if (timeValid()) {
      iss::Tracker t;
      const bool have = tleValid && t.setTle(tle1, tle2);
      const double ageDays = have ? t.tleAgeDays(time(nullptr)) : 1e9;
      if ((!have || ageDays * 86400 > TLE_REFRESH_S) && (lastTleTry == 0 || now - lastTleTry > 15 * 60 * 1000UL)) {
        lastTleTry = now;
        char a[70], b[70];
        if (feeds::fetchTle(a, b) && t.setTle(a, b)) {
          Preferences p;
          p.begin("cache", false);
          p.putString("tle1", a);
          p.putString("tle2", b);
          p.end();
          Lock l;
          strcpy(gShared.tle1, a);
          strcpy(gShared.tle2, b);
          gShared.tleValid = true;
          tleGen = ++gShared.tleGen;
          strcpy(tle1, a);
          strcpy(tle2, b);
          tleValid = true;
        }
      }
    }

    // ---- Next visible ISS pass (recomputed every 10 min, or when inputs change)
    if (hasLoc && timeValid() && tleValid &&
        (lastPassMs == 0 || now - lastPassMs > 10 * 60 * 1000UL || gen != lastPassGen || tleGen != lastTleGenForPass)) {
      lastPassMs = now;
      lastPassGen = gen;
      lastTleGenForPass = tleGen;
      tracker.setTle(tle1, tle2);
      tracker.setObserver(lat, lon, 0);
      static iss::Pass p;
      const bool found = tracker.nextVisiblePass(time(nullptr), ISS_PASS_SEARCH_H, p);
      Lock l;
      gShared.hasPass = found;
      if (found) gShared.pass = p;
    }

    // ---- OTA
    bool otaReq, autoUpd;
    {
      Lock l;
      otaReq = gShared.otaRequested;
      autoUpd = gShared.autoUpdate;
    }
    const bool otaConfigured = strncmp(GITHUB_REPO, "YOUR-", 5) != 0;
    if (otaConfigured && (otaReq || (autoUpd && now >= nextOtaMs && !ota::pendingVerify()))) {
      nextOtaMs = now + OTA_CHECK_EVERY_MS;
      {
        Lock l;
        gShared.otaRequested = false;
        gShared.otaRunning = true;
      }
      ota::checkAndInstall(OTA_MANIFEST_URL, otaProgressCb);  // reboots on success
      Lock l;
      gShared.otaRunning = false;
      gShared.otaProgress = ota::progress();
    }
  }
}

// ===========================================================================
// Wi-Fi and the setup/settings portal
//
// The portal is the display's own Wi-Fi network serving the setup page. It opens
// for first-time setup and whenever the button is held for 3 seconds, so the phone
// always talks to the display directly (works on any home or office network).

bool gStationUp = false;
uint32_t gConnectedSinceMs = 0;
uint32_t gDisconnectedSinceMs = 0;
bool gTimeConfigured = false;

bool gPortalOpen = false;
bool gSettingsMode = false;      // opened by a long press (vs first-time setup)
uint32_t gPortalCloseAtMs = 0;   // scheduled close (after a successful setup, or a request)
char gApSsid[33], gApPass[17];

// Join attempt requested from the page
ui::JoinResult gJoin = ui::JoinResult::None;
app::WifiRequest gJoinReq;
uint32_t gJoinStartMs = 0;
volatile uint8_t gLastDisconnectReason = 0;

bool gResetWarning = false;
bool gForceRedraw = false;
bool gForceFull = false;  // e.g. after a theme change: a partial refresh would leave ghosting

void configureTime() {
  configTzTime(gSettings.tz, NTP_SERVER_1, NTP_SERVER_2);
  gTimeConfigured = true;
}

void beginStation(const char* ssid, const char* pass, const char* eapUser, const char* eapIdentity) {
  WiFi.setHostname(MDNS_HOST);
  if (eapUser && eapUser[0]) {
    // WPA2-Enterprise, PEAP/MSCHAPv2 (the common office setup). The server certificate
    // is not pinned: most networks don't publish theirs.
    WiFi.begin(ssid, WPA2_AUTH_PEAP, eapIdentity && eapIdentity[0] ? eapIdentity : eapUser, eapUser, pass);
  } else {
    WiFi.begin(ssid, pass);
  }
}

void connectSaved() {
  if (!gSettings.ssid[0]) return;
  WiFi.setAutoReconnect(true);
  beginStation(gSettings.ssid, gSettings.password, gSettings.eapUser, gSettings.eapIdentity);
}

void openPortal(bool settingsMode) {
  if (gPortalOpen) return;
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(gApSsid, sizeof(gApSsid), AP_SSID_PREFIX "%02X%02X", mac[4], mac[5]);
  static const char* words[] = {"skies", "radar", "orbit", "cloud", "wings", "comet"};
  snprintf(gApPass, sizeof(gApPass), "%s-%04u", words[esp_random() % 6], static_cast<unsigned>(esp_random() % 10000));
  // If the saved network is unreachable, stop background retries: their channel hopping
  // would keep knocking the phone off the display's network.
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false);
  }
  portal::startAccessPoint(gApSsid, gApPass);
  gPortalOpen = true;
  gSettingsMode = settingsMode;
  gPortalCloseAtMs = 0;
  gJoin = ui::JoinResult::None;
  gForceRedraw = true;
}

void closePortal() {
  if (!gPortalOpen) return;
  portal::stopAccessPoint();
  gPortalOpen = false;
  gPortalCloseAtMs = 0;
  gJoin = ui::JoinResult::None;
  if (WiFi.status() != WL_CONNECTED) connectSaved();
  WiFi.setAutoReconnect(true);
  gForceRedraw = true;
}

bool setupComplete() { return gSettings.ssid[0] && gSettings.hasLocation; }

// After joining: can we reach the internet, or is there a sign-in page in the way?
ui::JoinResult checkInternet() {
  NetworkClient client;
  HTTPClient h;
  h.setTimeout(6000);
  h.setConnectTimeout(6000);
  if (!h.begin(client, "http://connectivitycheck.gstatic.com/generate_204")) return ui::JoinResult::NoInternet;
  const int code = h.GET();
  h.end();
  if (code == 204) return ui::JoinResult::Ok;
  if (code > 0) return ui::JoinResult::Captive;  // something answered in Google's place
  return ui::JoinResult::NoInternet;
}

ui::JoinResult failureFromReason(uint8_t reason) {
  switch (reason) {
    case WIFI_REASON_NO_AP_FOUND: return ui::JoinResult::NotFound;
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_802_1X_AUTH_FAILED:
    case WIFI_REASON_MIC_FAILURE: return ui::JoinResult::WrongPassword;
    default: return ui::JoinResult::Failed;
  }
}

void startJoin(const app::WifiRequest& req) {
  gJoinReq = req;
  gJoin = ui::JoinResult::Connecting;
  gJoinStartMs = millis();
  gLastDisconnectReason = 0;
  WiFi.disconnect(false);
  delay(50);
  beginStation(req.ssid, req.pass, req.user, nullptr);
  gForceRedraw = true;
}

void joinLoop() {
  if (gJoin != ui::JoinResult::Connecting) return;
  const uint32_t now = millis();
  if (WiFi.status() == WL_CONNECTED) {
    const ui::JoinResult r = checkInternet();
    if (r == ui::JoinResult::Ok) {
      snprintf(gSettings.ssid, sizeof(gSettings.ssid), "%s", gJoinReq.ssid);
      snprintf(gSettings.password, sizeof(gSettings.password), "%s", gJoinReq.pass);
      snprintf(gSettings.eapUser, sizeof(gSettings.eapUser), "%s", gJoinReq.user);
      gSettings.eapIdentity[0] = 0;
      settings::save(gSettings);
      configureTime();
      gPortalCloseAtMs = now + 45000;  // let the phone show "All set" before the network disappears
    } else {
      // Reachable network, but no usable internet: go back to the previous one.
      WiFi.disconnect(false);
      connectSaved();
    }
    gJoin = r;
    gForceRedraw = true;
    return;
  }
  const uint8_t reason = gLastDisconnectReason;
  const bool definite = reason == WIFI_REASON_NO_AP_FOUND || reason == WIFI_REASON_AUTH_FAIL ||
                        reason == WIFI_REASON_802_1X_AUTH_FAILED;
  if ((definite && now - gJoinStartMs > 6000) || now - gJoinStartMs > 25000) {
    gJoin = failureFromReason(reason);
    WiFi.disconnect(false);
    connectSaved();
    gForceRedraw = true;
  }
}

void netLoop() {
  const uint32_t now = millis();
  joinLoop();
  const bool up = WiFi.status() == WL_CONNECTED;
  if (up && !gStationUp) {
    gStationUp = true;
    gConnectedSinceMs = now;
    if (!gTimeConfigured) configureTime();
  } else if (!up && gStationUp) {
    gStationUp = false;
    gDisconnectedSinceMs = now;
  }
  // Nudge reconnection if the automatic retries seem stuck (not while the portal is open).
  if (!up && !gPortalOpen && gSettings.ssid[0] && now - gDisconnectedSinceMs > 60000) {
    gDisconnectedSinceMs = now;
    WiFi.disconnect(false);
    connectSaved();
  }
  if (gPortalOpen) {
    if (gPortalCloseAtMs && now > gPortalCloseAtMs && setupComplete()) closePortal();
    // Settings left open and untouched: close after 10 minutes.
    else if (gSettingsMode && gJoin != ui::JoinResult::Connecting && portal::idleMs() > 10 * 60 * 1000UL) closePortal();
  }
}

// ===========================================================================
// Button: short press = next mode (or close settings), 3 s = settings, 10 s = factory reset

void pushSharedConfig() {
  Lock l;
  gShared.hasLocation = gSettings.hasLocation;
  gShared.lat = gSettings.lat;
  gShared.lon = gSettings.lon;
  gShared.fetchRadiusKm = std::max<float>(gSettings.rangeKm, gSettings.rareRangeKm);
  gShared.autoUpdate = gSettings.autoUpdate;
  gShared.configGen++;
  gShared.count = 0;
  gShared.route = feeds::Route();
  gShared.wantRoute[0] = 0;
}

void buttonLoop() {
  static bool down = false;
  static uint32_t downAt = 0;
  const bool pressed = digitalRead(PIN_BUTTON) == LOW;
  const uint32_t now = millis();
  if (pressed && !down) {
    down = true;
    downAt = now;
  } else if (pressed && down) {
    const uint32_t held = now - downAt;
    if (held > 6000 && !gResetWarning) {
      gResetWarning = true;
      gForceRedraw = true;
    }
    if (held > 10000) {
      settings::eraseAll();
      WiFi.disconnect(true, true);
      gResetWarning = false;
      ui::MessageScreen m;
      m.c.lang = gSettings.lang;
      m.c.dark = gSettings.darkTheme;
      snprintf(m.title, sizeof(m.title), "%s", tr(gSettings.lang, S::ResetDone));
      display::show(display::Kind::Message, 1, [](ui::Canvas& c, const void* v) { ui::drawMessage(c, *static_cast<const ui::MessageScreen*>(v)); }, &m, true);
      delay(1500);
      ESP.restart();
    }
  } else if (!pressed && down) {
    down = false;
    const uint32_t held = now - downAt;
    gResetWarning = false;
    if (held < 40) return;  // bounce
    if (held < 1000) {
      if (gPortalOpen && setupComplete()) {
        closePortal();
      } else if (!gPortalOpen) {
        gSettings.mode = static_cast<ui::Mode>((static_cast<int>(gSettings.mode) + 1) % 3);
        settings::save(gSettings);
      }
    } else if (held >= 3000 && held < 6000) {
      openPortal(setupComplete());
    }
    gForceRedraw = true;
  }
}

// ===========================================================================
// Screen selection and view-model building

int tzOffsetSeconds() {
  const time_t now = time(nullptr);
  struct tm lt, gt;
  localtime_r(&now, &lt);
  gmtime_r(&now, &gt);
  int dayDiff = lt.tm_yday - gt.tm_yday;
  if (dayDiff > 1) dayDiff = -1;
  if (dayDiff < -1) dayDiff = 1;
  return dayDiff * 86400 + (lt.tm_hour - gt.tm_hour) * 3600 + (lt.tm_min - gt.tm_min) * 60;
}

void formatClock(char* out, size_t n, time_t t) {
  if (!timeValid()) {
    snprintf(out, n, "--:--");
    return;
  }
  struct tm lt;
  localtime_r(&t, &lt);
  strftime(out, n, "%H:%M", &lt);
}

ui::Common makeCommon(ui::Freshness f, int delaySec, int count) {
  ui::Common c = ui::Common();  // value-initialised: padding zeroed (keeps frame hashes stable)
  c.dark = gSettings.darkTheme;
  c.lang = gSettings.lang;
  c.units = gSettings.units;
  c.status.freshness = f;
  c.status.delaySec = f == ui::Freshness::Delayed ? delaySec / 10 * 10 : 0;  // 10 s steps: fewer refreshes
  formatClock(c.status.clock, sizeof(c.status.clock), time(nullptr));
  c.status.aircraftCount = count;
  c.status.mode = gSettings.mode;
  c.status.wifiOk = WiFi.status() == WL_CONNECTED;
  return c;
}

template <typename T>
void drawThunk(ui::Canvas& c, const void* v);
template <>
void drawThunk<ui::AircraftScreen>(ui::Canvas& c, const void* v) { ui::drawAircraft(c, *static_cast<const ui::AircraftScreen*>(v)); }
template <>
void drawThunk<ui::EmptySkyScreen>(ui::Canvas& c, const void* v) { ui::drawEmptySky(c, *static_cast<const ui::EmptySkyScreen*>(v)); }
template <>
void drawThunk<ui::IssMapScreen>(ui::Canvas& c, const void* v) { ui::drawIssMap(c, *static_cast<const ui::IssMapScreen*>(v)); }
template <>
void drawThunk<ui::IssAlertScreen>(ui::Canvas& c, const void* v) { ui::drawIssAlert(c, *static_cast<const ui::IssAlertScreen*>(v)); }
template <>
void drawThunk<ui::SetupScreen>(ui::Canvas& c, const void* v) { ui::drawSetup(c, *static_cast<const ui::SetupScreen*>(v)); }
template <>
void drawThunk<ui::OfflineScreen>(ui::Canvas& c, const void* v) { ui::drawOffline(c, *static_cast<const ui::OfflineScreen*>(v)); }
template <>
void drawThunk<ui::MessageScreen>(ui::Canvas& c, const void* v) { ui::drawMessage(c, *static_cast<const ui::MessageScreen*>(v)); }

// Draws `vm` if it changed and the e-paper may refresh now.
template <typename T>
void present(display::Kind kind, const T& vm, uint32_t minIntervalMs = MIN_REDRAW_MS) {
  const uint32_t h = display::hash(&vm, sizeof(vm));
  const bool kindChanged = display::lastKind() != kind;
  if (!kindChanged && !gForceRedraw && millis() - display::lastRefreshMs() < minIntervalMs) return;
  display::show(kind, h, drawThunk<T>, &vm, gForceFull);
  gForceRedraw = false;
  gForceFull = false;
}

// Static view models (large; keep them off the stack). Zeroed before each build so
// padding bytes don't defeat the change detection hash.
ui::AircraftScreen gAs;
ui::EmptySkyScreen gEs;
ui::IssMapScreen gIm;
ui::IssAlertScreen gIa;
ui::SetupScreen gSs;
ui::OfflineScreen gOs;
ui::MessageScreen gMs;

template <typename T>
void reset(T& vm) {
  memset(static_cast<void*>(&vm), 0, sizeof(vm));
  new (&vm) T();  // default member values; padding stays zero for the hash
}

ac::Selector gSelector;
uint32_t gBootMessageUntil = 0;
char gBootMessage[96] = "";

float q(float v, float step) { return std::round(v / step) * step; }

void fillAircraftView(ui::AircraftView& v, const ac::Aircraft& a, const ac::Selection& sel, const feeds::Route* route) {
  const db::TypeInfo* t = db::findType(a.type);
  v.family = t ? t->family : db::familyForCategory(a.category);
  snprintf(v.callsign, sizeof(v.callsign), "%s", a.callsign);
  snprintf(v.reg, sizeof(v.reg), "%s", a.reg);
  snprintf(v.typeCode, sizeof(v.typeCode), "%s", a.type);
  snprintf(v.model, sizeof(v.model), "%s", t ? t->name : (a.type[0] ? a.type : db::kFamilies[v.family].name));
  const db::Airline* al = db::findAirline(a.callsign);
  if (route && route->known && route->airline[0]) snprintf(v.airline, sizeof(v.airline), "%s", route->airline);
  else if (al) snprintf(v.airline, sizeof(v.airline), "%s", al->name);
  snprintf(v.flight, sizeof(v.flight), "%s", route && route->flightIata[0] ? route->flightIata : a.callsign);
  if (strcmp(a.hex, santa::kHex) == 0) {
    v.hasRoute = true;
    snprintf(v.originCity, sizeof(v.originCity), "North Pole");
    snprintf(v.originCode, sizeof(v.originCode), "NPL");
    snprintf(v.destCity, sizeof(v.destCity), "Every home");
    snprintf(v.destCode, sizeof(v.destCode), "XMS");
    v.routeProgress = 0.5f;
  } else if (route && route->known && route->plausible) {
    v.hasRoute = true;
    snprintf(v.originCity, sizeof(v.originCity), "%s", route->originCity);
    snprintf(v.originCode, sizeof(v.originCode), "%s", route->originCode);
    snprintf(v.destCity, sizeof(v.destCity), "%s", route->destCity);
    snprintf(v.destCode, sizeof(v.destCode), "%s", route->destCode);
    v.routeProgress = q(feeds::routeProgress(*route, a.lat, a.lon), 0.01f);
  }
  // Quantised so that the e-paper only refreshes for visible changes
  v.altFt = a.altFt == ac::kAltUnknown ? a.altFt : static_cast<int32_t>(std::lround(a.altFt / 100.0) * 100);
  v.onGround = a.onGround;
  v.vrateFpm = static_cast<int>(std::lround(a.vrateFpm / 100.0) * 100);
  v.gsKt = a.gsKt < 0 ? -1 : q(a.gsKt, 5);
  v.trackDeg = a.trackDeg < 0 ? -1 : q(a.trackDeg, 5);
  v.distKm = q(a.distKm, a.distKm < 10 ? 0.2f : 1.0f);
  v.bearingDeg = q(a.bearingDeg, 5);
  v.takeover = sel.takeover;
  v.reason = sel.rarity.reason;
  v.squawk = a.squawk;
  v.emergencyFlag = a.emergencyFlag;
}

int fillBlips(ui::Blip* blips, const ac::Aircraft* list, const ac::Rarity* rar, int n, int selected, float rangeKm) {
  int k = 0;
  for (int i = 0; i < n && k < ui::AircraftScreen::kMaxBlips; i++) {
    if (list[i].distKm > rangeKm || list[i].onGround) continue;
    ui::Blip b = ui::Blip();
    b.distKm = q(list[i].distKm, 0.5f);
    b.bearingDeg = q(list[i].bearingDeg, 3);
    b.trackDeg = list[i].trackDeg < 0 ? -1 : q(list[i].trackDeg, 15);
    b.selected = i == selected;
    b.rare = rar[i].score >= gSettings.rareThreshold();
    blips[k++] = b;
  }
  return k;
}

bool buildIssMap(int64_t now, const char* tle1, const char* tle2, bool hasPass, const iss::Pass& pass) {
  iss::Tracker t;
  if (!t.setTle(tle1, tle2)) return false;
  reset(gIm);
  const double age = t.tleAgeDays(now);
  gIm.c = makeCommon(age <= TLE_MAX_AGE_DAYS ? ui::Freshness::Live : ui::Freshness::Offline, 0, -1);
  // Quantise time to a minute so the map refreshes at most once per minute
  const int64_t tq = now - now % 60;
  if (!t.position(tq, gIm.pos)) return false;
  gIm.homeLat = gSettings.lat;
  gIm.homeLon = gSettings.lon;
  gIm.distanceKm = q(geo::distanceKm(gSettings.lat, gSettings.lon, gIm.pos.latDeg, gIm.pos.lonDeg), 10);
  constexpr int kStep = 70;
  gIm.trackCount = t.groundTrack(tq - 45 * 60, tq + 95 * 60, kStep, gIm.trackLat, gIm.trackLon, ui::IssMapScreen::kTrackPts);
  gIm.trackNowIndex = 45 * 60 / kStep;
  double sl, so;
  sgp4::subsolarPoint(sgp4::unixToJd(static_cast<double>(tq)), sl, so);
  gIm.sunLat = sl;
  gIm.sunLon = so;
  gIm.hasPass = hasPass;
  if (hasPass) gIm.nextPass = pass;
  gIm.now = tq;
  gIm.tzOffsetS = tzOffsetSeconds();
  gIm.tleAgeDays = static_cast<int>(age);
  return true;
}

void showOffline(ui::OfflineReason reason, const char* detail, time_t lastOk) {
  reset(gOs);
  gOs.c = makeCommon(ui::Freshness::Offline, 0, -1);
  gOs.reason = reason;
  if (detail) snprintf(gOs.detail, sizeof(gOs.detail), "%s", detail);
  if (lastOk && timeValid()) {
    char hm[8];
    formatClock(hm, sizeof(hm), lastOk);
    snprintf(gOs.lastUpdate, sizeof(gOs.lastUpdate), gSettings.lang == i18n::Lang::FR ? "à %s" : "at %s", hm);
  }
  present(display::Kind::Offline, gOs, 30000);
}

void showMessage(const char* title, const char* l1, const char* l2, int progress, uint32_t minInterval = 2000) {
  reset(gMs);
  gMs.c = makeCommon(ui::Freshness::Live, 0, -1);
  snprintf(gMs.title, sizeof(gMs.title), "%s", title);
  snprintf(gMs.line1, sizeof(gMs.line1), "%s", l1 ? l1 : "");
  snprintf(gMs.line2, sizeof(gMs.line2), "%s", l2 ? l2 : "");
  gMs.progress = progress;
  present(display::Kind::Message, gMs, minInterval);
}

void render() {
  const uint32_t nowMs = millis();
  const i18n::Lang L = gSettings.lang;

  // ---- Setup / settings (the display's own Wi-Fi network is open)
  if (gResetWarning) {
    showMessage(tr(L, S::HoldToReset), "", "", -1, 0);
    return;
  }
  if (gPortalOpen) {
    reset(gSs);
    gSs.c = makeCommon(ui::Freshness::Live, 0, -1);
    gSs.settingsMode = gSettingsMode;
    snprintf(gSs.apSsid, sizeof(gSs.apSsid), "%s", gApSsid);
    snprintf(gSs.apPassword, sizeof(gSs.apPassword), "%s", gApPass);
    snprintf(gSs.qrPayload, sizeof(gSs.qrPayload), "WIFI:T:WPA;S:%s;P:%s;;", gApSsid, gApPass);
    gSs.join = gJoin;
    snprintf(gSs.joinSsid, sizeof(gSs.joinSsid), "%s", gJoinReq.ssid);
    present(display::Kind::Setup, gSs, 1000);
    return;
  }
  const bool wifiUp = WiFi.status() == WL_CONNECTED;

  // ---- OTA progress, post-update message
  static Shared snap;  // large: keep it off the loop task's stack
  {
    Lock l;
    snap.count = gShared.count;
    memcpy(snap.list, gShared.list, sizeof(ac::Aircraft) * gShared.count);
    snap.everOk = gShared.everOk;
    snap.lastOkMs = gShared.lastOkMs;
    snap.lastOkUnix = gShared.lastOkUnix;
    snap.lastStatus = gShared.lastStatus;
    snap.route = gShared.route;
    snap.tleValid = gShared.tleValid;
    strcpy(snap.tle1, gShared.tle1);
    strcpy(snap.tle2, gShared.tle2);
    snap.hasPass = gShared.hasPass;
    snap.pass = gShared.pass;
    snap.otaRunning = gShared.otaRunning;
    snap.otaProgress = gShared.otaProgress;
  }
  if (snap.otaRunning && snap.otaProgress.phase >= ota::Phase::Downloading &&
      snap.otaProgress.phase <= ota::Phase::Rebooting) {
    char title[48];
    snprintf(title, sizeof(title), tr(L, S::Updating), snap.otaProgress.version);
    showMessage(title, tr(L, S::DoNotUnplug), "", snap.otaProgress.percent);
    return;
  }
  if (gBootMessageUntil > nowMs) {
    showMessage(gBootMessage, "", "", -1);
    return;
  }

  // ---- Data freshness
  const uint32_t ageS = snap.everOk ? (nowMs - snap.lastOkMs) / 1000 : UINT32_MAX;
  ui::Freshness fresh = ageS <= FRESH_LIVE_S ? ui::Freshness::Live
                        : ageS <= FRESH_OFFLINE_S ? ui::Freshness::Delayed
                                                  : ui::Freshness::Offline;
  const bool aircraftUsable = fresh != ui::Freshness::Offline;

  // ---- Christmas easter egg: around local midnight on December 24, Santa's sleigh flies
  // over the home and joins the aircraft list (it then takes over as a rare aircraft).
  if (aircraftUsable && gSettings.hasLocation && timeValid() && snap.count < feeds::kMaxAircraft) {
    ac::Aircraft sleigh;
    if (santa::sleigh(time(nullptr), gSettings.lat, gSettings.lon, gSettings.rareRangeKm, sleigh)) {
      snap.list[snap.count++] = sleigh;
    }
  }

  // ---- Aircraft selection (rarity, emergencies, hysteresis)
  static ac::Rarity rar[feeds::kMaxAircraft];
  for (int i = 0; i < snap.count; i++) {
    const db::TypeInfo* t = db::findType(snap.list[i].type);
    const bool milType = t && db::kFamilies[t->family].military;
    rar[i] = ac::scoreRarity(snap.list[i], db::typeRarity(t), milType);
  }
  ac::SelectorConfig cfg;
  cfg.rangeKm = gSettings.rangeKm;
  cfg.rareRangeKm = gSettings.rareRangeKm;
  cfg.rareThreshold = gSettings.rareThreshold();
  gSelector.setConfig(cfg);
  ac::Selection sel = aircraftUsable ? gSelector.select(snap.list, rar, snap.count, nowMs) : ac::Selection();
  int inRange = 0;
  for (int i = 0; i < snap.count; i++)
    if (snap.list[i].distKm <= gSettings.rangeKm && !snap.list[i].onGround) inRange++;

  // Ask the network task for the route of whatever is on screen (Santa needs no lookup)
  if (sel.index >= 0 && strcmp(snap.list[sel.index].hex, santa::kHex) != 0) {
    const ac::Aircraft& a = snap.list[sel.index];
    Lock l;
    snprintf(gShared.wantRoute, sizeof(gShared.wantRoute), "%s", a.callsign);
    gShared.wantLat = a.lat;
    gShared.wantLon = a.lon;
  }

  const int64_t now = time(nullptr);
  const bool emergency = sel.index >= 0 && sel.takeover && sel.rarity.reason == ac::RareReason::Emergency;

  // ---- ISS pass alert window
  bool issAlert = false;
  if (gSettings.issAlerts && timeValid() && snap.hasPass && snap.pass.visible && !emergency) {
    issAlert = now >= snap.pass.visStart.t - ISS_ALERT_LEAD_S && now <= snap.pass.visEnd.t;
  }
  if (issAlert) {
    reset(gIa);
    gIa.c = makeCommon(ui::Freshness::Live, 0, -1);
    gIa.pass = snap.pass;
    gIa.now = now - now % 60;  // countdown has minute resolution
    gIa.tzOffsetS = tzOffsetSeconds();
    gIa.inProgress = now >= snap.pass.visStart.t;
    if (gIa.inProgress) {
      iss::Tracker t;
      t.setTle(snap.tle1, snap.tle2);
      t.setObserver(gSettings.lat, gSettings.lon, gSettings.altM);
      t.skyPosition(now, gIa.current);
      gIa.now = 0;        // not shown while in progress; keep it out of the change hash
      gIa.current.t = 0;
      gIa.current.azDeg = q(gIa.current.azDeg, 3);
      gIa.current.elDeg = q(gIa.current.elDeg, 2);
    }
    present(display::Kind::IssAlert, gIa, gIa.inProgress ? 5000 : MIN_REDRAW_MS);
    return;
  }

  // ---- Aircraft (takeover in any mode; normal view in Auto/Aircraft)
  const bool wantAircraft = gSettings.mode != ui::Mode::Iss || (sel.index >= 0 && sel.takeover);
  if (wantAircraft && aircraftUsable && sel.index >= 0) {
    const ac::Aircraft& a = snap.list[sel.index];
    reset(gAs);
    gAs.c = makeCommon(fresh, static_cast<int>(ageS), inRange);
    const bool routeMatches = strcmp(snap.route.callsign, a.callsign) == 0;
    fillAircraftView(gAs.ac, a, sel, routeMatches ? &snap.route : nullptr);
    gAs.rangeKm = gSettings.rangeKm;
    gAs.blipCount = fillBlips(gAs.blips, snap.list, rar, snap.count, sel.index, gSettings.rangeKm);
    present(sel.takeover ? display::Kind::Takeover : display::Kind::Aircraft, gAs);
    return;
  }

  // ---- Nothing to show from the aircraft feed
  if (gSettings.mode != ui::Mode::Iss && !aircraftUsable) {
    if (!snap.everOk && nowMs - gConnectedSinceMs < 45000 && wifiUp) {
      char line[64];
      snprintf(line, sizeof(line), tr(L, S::Connecting), gSettings.ssid);
      showMessage("PlaneSpotter", line, FW_VERSION, -1);
      return;
    }
    if (!wifiUp) showOffline(ui::OfflineReason::Wifi, gSettings.ssid, snap.lastOkUnix);
    else if (!timeValid()) showOffline(ui::OfflineReason::Time, nullptr, snap.lastOkUnix);
    else showOffline(ui::OfflineReason::Api, http::describe(snap.lastStatus), snap.lastOkUnix);
    return;
  }
  if (gSettings.mode == ui::Mode::Aircraft) {
    reset(gEs);
    gEs.c = makeCommon(fresh, static_cast<int>(ageS), 0);
    gEs.rangeKm = gSettings.rangeKm;
    gEs.hasPass = snap.hasPass;
    if (snap.hasPass) gEs.nextPass = snap.pass;
    gEs.now = now - now % 60;
    gEs.tzOffsetS = tzOffsetSeconds();
    present(display::Kind::EmptySky, gEs, 60000);
    return;
  }

  // ---- ISS map (Auto with quiet skies, or ISS mode)
  if (!timeValid()) {
    showOffline(ui::OfflineReason::Time, nullptr, snap.lastOkUnix);
    return;
  }
  if (!snap.tleValid || !buildIssMap(now, snap.tle1, snap.tle2, snap.hasPass, snap.pass)) {
    showOffline(wifiUp ? ui::OfflineReason::Api : ui::OfflineReason::Wifi, wifiUp ? "CelesTrak" : gSettings.ssid, 0);
    return;
  }
  if (gIm.tleAgeDays > TLE_MAX_AGE_DAYS) {
    showOffline(wifiUp ? ui::OfflineReason::Api : ui::OfflineReason::Wifi, "CelesTrak", 0);
    return;
  }
  gIm.c.status.mode = gSettings.mode;
  present(display::Kind::IssMap, gIm, 55000);
}

}  // namespace

// ===========================================================================
// app:: hooks for the web server

namespace app {

const Settings& settings() { return gSettings; }
bool settingsMode() { return gSettingsMode; }

void submit(const Settings& s, const WifiRequest* wifi) {
  const bool tzChanged = strcmp(s.tz, gSettings.tz) != 0;
  if (s.darkTheme != gSettings.darkTheme) gForceFull = true;
  // Keep the Wi-Fi credentials we have: new ones are only saved once they work.
  Settings next = s;
  snprintf(next.ssid, sizeof(next.ssid), "%s", gSettings.ssid);
  snprintf(next.password, sizeof(next.password), "%s", gSettings.password);
  snprintf(next.eapUser, sizeof(next.eapUser), "%s", gSettings.eapUser);
  snprintf(next.eapIdentity, sizeof(next.eapIdentity), "%s", gSettings.eapIdentity);
  gSettings = next;
  settings::save(gSettings);
  if (tzChanged && gTimeConfigured) configureTime();
  pushSharedConfig();
  gSelector.reset();
  if (wifi) startJoin(*wifi);
  gForceRedraw = true;
}

ui::JoinResult joinResult() { return gJoin; }
const char* joinSsid() { return gJoinReq.ssid; }
const char* connectedSsid() { return WiFi.status() == WL_CONNECTED ? gSettings.ssid : ""; }

void requestOtaCheck() {
  Lock l;
  gShared.otaRequested = true;
}

void closeSettings() {
  if (setupComplete()) gPortalCloseAtMs = millis() + 1000;  // after the response has gone out
}

}  // namespace app

// ===========================================================================

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  gLock = xSemaphoreCreateMutex();
  settings::load(gSettings);
  ota::begin();
  display::begin();

  bool rolledBack = false;
  char ver[16];
  if (ota::takeBootMessage(ver, sizeof(ver), rolledBack)) {
    snprintf(gBootMessage, sizeof(gBootMessage), tr(gSettings.lang, rolledBack ? S::UpdateRolledBack : S::UpdatedTo), ver);
    gBootMessageUntil = millis() + 10000;
  }

  loadTleCache();
  pushSharedConfig();
  WiFi.onEvent([](arduino_event_id_t, arduino_event_info_t info) {
    gLastDisconnectReason = info.wifi_sta_disconnected.reason;
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.mode(WIFI_STA);
  gDisconnectedSinceMs = millis();
  connectSaved();
  if (!setupComplete()) openPortal(false);
  iss::setYieldHook([] { vTaskDelay(1); });  // long pass searches must not starve the watchdog
  xTaskCreatePinnedToCore(netTask, "net", 16384, nullptr, 1, nullptr, 0);
}

void loop() {
  portal::loop();
  buttonLoop();
  netLoop();
  ota::tick();
  render();
  delay(20);
}

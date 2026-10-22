#include "portal.h"

#include <ArduinoJson.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "../app/app.h"
#include "../web/pages.h"
#include "config.h"

namespace db {
extern const uint8_t kCitiesGz[];
extern const size_t kCitiesGzLen;
}  // namespace db

namespace portal {
namespace {

WebServer gServer(80);
DNSServer gDns;
bool gApActive = false;
bool gServerStarted = false;
uint32_t gLastActivityMs = 0;

void touch() { gLastActivityMs = millis(); }

void sendJson(int code, const JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  gServer.sendHeader("Cache-Control", "no-store");
  gServer.send(code, "application/json", out);
}

void redirectToPage() {
  gServer.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
  gServer.send(302, "text/plain", "");
}

const char* joinName(ui::JoinResult r) {
  switch (r) {
    case ui::JoinResult::Connecting: return "connecting";
    case ui::JoinResult::Ok: return "ok";
    case ui::JoinResult::WrongPassword: return "wrong_password";
    case ui::JoinResult::NotFound: return "not_found";
    case ui::JoinResult::Captive: return "captive";
    case ui::JoinResult::NoInternet: return "no_internet";
    case ui::JoinResult::Failed: return "failed";
    default: return "idle";
  }
}

bool isEnterprise(wifi_auth_mode_t m) {
  return m == WIFI_AUTH_WPA2_ENTERPRISE || m == WIFI_AUTH_WPA3_ENTERPRISE || m == WIFI_AUTH_WPA2_WPA3_ENTERPRISE ||
         m == WIFI_AUTH_WPA_ENTERPRISE || m == WIFI_AUTH_WPA3_ENT_192;
}

// ---- Handlers

void handleRoot() {
  touch();
  gServer.sendHeader("Cache-Control", "no-store");
  gServer.send(200, "text/html", kSetupPage);
}

void handleCities() {
  touch();
  // Pre-compressed at build time; the browser inflates it.
  gServer.sendHeader("Content-Encoding", "gzip");
  gServer.sendHeader("Cache-Control", "max-age=86400");
  gServer.send_P(200, "text/tab-separated-values; charset=utf-8", reinterpret_cast<const char*>(db::kCitiesGz),
                 db::kCitiesGzLen);
}

void handleScan() {
  touch();
  const int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING || n < 0) {
    if (n != WIFI_SCAN_RUNNING) WiFi.scanNetworks(true);
    gServer.send(200, "application/json", "{\"scanning\":true}");
    return;
  }
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < n && arr.size() < 25; i++) {
    const String ssid = WiFi.SSID(i);
    if (!ssid.length()) continue;
    bool dup = false;
    for (JsonObject o : arr) dup |= ssid == (o["ssid"] | "");
    if (dup) continue;  // results are sorted by signal, so the first one is the strongest
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = ssid;
    o["rssi"] = WiFi.RSSI(i);
    o["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    o["ent"] = isEnterprise(WiFi.encryptionType(i));
  }
  WiFi.scanDelete();
  WiFi.scanNetworks(true);  // keep the list fresh for the next poll
  sendJson(200, doc);
}

void handleState() {
  touch();
  const Settings& s = app::settings();
  JsonDocument doc;
  doc["mode"] = app::settingsMode() ? "settings" : "setup";
  doc["version"] = FW_VERSION;
  doc["mac"] = WiFi.macAddress();
  doc["connectedSsid"] = app::connectedSsid();
  doc["hasLocation"] = s.hasLocation;
  doc["tz"] = s.tz;
  JsonObject j = doc["join"].to<JsonObject>();
  j["state"] = joinName(app::joinResult());
  j["ssid"] = app::joinSsid();
  JsonObject o = doc["settings"].to<JsonObject>();
  o["lang"] = static_cast<int>(s.lang);
  o["theme"] = s.darkTheme ? 1 : 0;
  o["units"] = static_cast<int>(s.units);
  o["mode"] = static_cast<int>(s.mode);
  o["range"] = s.rangeKm;
  o["rareRange"] = s.rareRangeKm;
  o["rareLevel"] = s.rareLevel;
  o["issAlerts"] = s.issAlerts;
  o["autoUpdate"] = s.autoUpdate;
  o["place"] = s.placeName;
  o["lat"] = s.lat;
  o["lon"] = s.lon;
  sendJson(200, doc);
}

void handleSave() {
  touch();
  JsonDocument in;
  if (deserializeJson(in, gServer.arg("plain"))) return gServer.send(400, "text/plain", "bad json");
  Settings s = app::settings();
  s.lang = (in["lang"] | 0) == 1 ? i18n::Lang::FR : i18n::Lang::EN;
  s.darkTheme = (in["theme"] | 0) == 1;
  s.units = static_cast<text::Units>(constrain(in["units"] | 0, 0, 2));
  s.mode = static_cast<ui::Mode>(constrain(in["mode"] | 0, 0, 2));
  s.rangeKm = constrain(in["range"] | 30, 2, 150);
  s.rareRangeKm = constrain(in["rareRange"] | 80, s.rangeKm, 250);
  s.rareLevel = constrain(in["rareLevel"] | 2, 0, 3);
  s.issAlerts = in["issAlerts"] | true;
  s.autoUpdate = in["autoUpdate"] | true;

  JsonObjectConst loc = in["location"];
  if (!loc.isNull()) {
    const double lat = loc["lat"] | NAN, lon = loc["lon"] | NAN;
    if (lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) {
      s.lat = lat;
      s.lon = lon;
      s.hasLocation = true;
      snprintf(s.placeName, sizeof(s.placeName), "%s", loc["place"] | "");
      const char* tz = loc["tz"] | "";
      if (*tz && strlen(tz) < sizeof(s.tz)) snprintf(s.tz, sizeof(s.tz), "%s", tz);
    }
  }

  JsonObjectConst w = in["wifi"];
  app::WifiRequest req;
  const bool hasWifi = !w.isNull() && strlen(w["ssid"] | "") > 0;
  if (hasWifi) {
    snprintf(req.ssid, sizeof(req.ssid), "%s", w["ssid"] | "");
    snprintf(req.pass, sizeof(req.pass), "%s", w["pass"] | "");
    snprintf(req.user, sizeof(req.user), "%s", w["user"] | "");
  }
  gServer.send(200, "application/json", "{\"ok\":true}");
  app::submit(s, hasWifi ? &req : nullptr);
}

void handleUpdate() {
  touch();
  app::requestOtaCheck();
  gServer.send(200, "application/json", "{\"ok\":true}");
}

void handleClose() {
  gServer.send(200, "application/json", "{\"ok\":true}");
  app::closeSettings();
}

void setupServer() {
  if (gServerStarted) return;
  gServer.on("/", HTTP_GET, handleRoot);
  gServer.on("/cities.tsv", HTTP_GET, handleCities);
  gServer.on("/scan", HTTP_GET, handleScan);
  gServer.on("/api/state", HTTP_GET, handleState);
  gServer.on("/api/save", HTTP_POST, handleSave);
  gServer.on("/api/update", HTTP_POST, handleUpdate);
  gServer.on("/api/close", HTTP_POST, handleClose);
  // Captive-portal probes from Apple, Android and Windows land here: send them to the page,
  // which makes phones open it automatically after joining the display's network.
  gServer.onNotFound(redirectToPage);
  gServer.begin();
  gServerStarted = true;
}

}  // namespace

void startAccessPoint(const char* ssid, const char* password) {
  WiFi.mode(WIFI_AP_STA);  // keep the home connection (if any) while settings are open
  WiFi.softAP(ssid, password);
  delay(100);
  gDns.setErrorReplyCode(DNSReplyCode::NoError);
  gDns.start(53, "*", WiFi.softAPIP());
  gApActive = true;
  touch();
  WiFi.scanNetworks(true);
  setupServer();
  log_i("Setup network %s up at %s", ssid, WiFi.softAPIP().toString().c_str());
}

void stopAccessPoint() {
  if (!gApActive) return;
  gDns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  gApActive = false;
}

bool accessPointActive() { return gApActive; }

uint32_t idleMs() { return millis() - gLastActivityMs; }

void loop() {
  if (!gApActive) return;
  gDns.processNextRequest();
  gServer.handleClient();
}

}  // namespace portal

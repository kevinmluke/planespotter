#include "settings.h"

#include <Preferences.h>

namespace {

constexpr const char* kNs = "ps";

}  // namespace

int Settings::rareThreshold() const {
  switch (rareLevel) {
    case 0: return 101;  // never
    case 1: return 80;
    case 3: return 40;
    default: return 55;
  }
}

namespace settings {

void load(Settings& s) {
  Preferences p;
  p.begin(kNs, true);
  p.getString("ssid", s.ssid, sizeof(s.ssid));
  p.getString("pass", s.password, sizeof(s.password));
  s.hasLocation = p.getBool("hasLoc", false);
  s.lat = p.getDouble("lat", 0);
  s.lon = p.getDouble("lon", 0);
  s.altM = p.getFloat("alt", 0);
  p.getString("place", s.placeName, sizeof(s.placeName));
  if (p.isKey("tz")) p.getString("tz", s.tz, sizeof(s.tz));
  s.lang = static_cast<i18n::Lang>(p.getUChar("lang", 0));
  s.units = static_cast<text::Units>(p.getUChar("units", 0));
  s.mode = static_cast<ui::Mode>(p.getUChar("mode", 0));
  s.darkTheme = p.getBool("dark", false);
  s.rangeKm = p.getUShort("range", 30);
  s.rareRangeKm = p.getUShort("rareRange", 80);
  s.rareLevel = p.getUChar("rare", 2);
  s.issAlerts = p.getBool("issAlerts", true);
  s.autoUpdate = p.getBool("autoUpd", true);
  p.end();

  // Sanity limits (a corrupted value must not break the API query)
  if (s.rangeKm < 2 || s.rangeKm > 150) s.rangeKm = 30;
  if (s.rareRangeKm < s.rangeKm || s.rareRangeKm > 250) s.rareRangeKm = s.rangeKm > 80 ? s.rangeKm : 80;
  if (static_cast<int>(s.lang) > 1) s.lang = i18n::Lang::EN;
  if (static_cast<int>(s.units) > 2) s.units = text::Units::Aviation;
  if (static_cast<int>(s.mode) > 2) s.mode = ui::Mode::Auto;
}

void save(const Settings& s) {
  Preferences p;
  p.begin(kNs, false);
  p.putString("ssid", s.ssid);
  p.putString("pass", s.password);
  p.remove("eapUser");  // left over from builds that supported work (WPA2-Enterprise) logins
  p.remove("eapId");
  p.putBool("hasLoc", s.hasLocation);
  p.putDouble("lat", s.lat);
  p.putDouble("lon", s.lon);
  p.putFloat("alt", s.altM);
  p.putString("place", s.placeName);
  p.putString("tz", s.tz);
  p.putUChar("lang", static_cast<uint8_t>(s.lang));
  p.putUChar("units", static_cast<uint8_t>(s.units));
  p.putUChar("mode", static_cast<uint8_t>(s.mode));
  p.putBool("dark", s.darkTheme);
  p.putUShort("range", s.rangeKm);
  p.putUShort("rareRange", s.rareRangeKm);
  p.putUChar("rare", s.rareLevel);
  p.putBool("issAlerts", s.issAlerts);
  p.putBool("autoUpd", s.autoUpdate);
  p.end();
}

void eraseAll() {
  Preferences p;
  p.begin(kNs, false);
  p.clear();
  p.end();
  Preferences c;  // cached data (TLE, OTA bookkeeping)
  c.begin("cache", false);
  c.clear();
  c.end();
}

}  // namespace settings

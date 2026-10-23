// Host unit tests for the pure-C++ core (geo, text, i18n, rarity/selection, data tables).
#include <cmath>
#include <cstdio>
#include <cstring>

#include "../../src/core/aircraft.h"
#include "../../src/core/geo.h"
#include "../../src/core/i18n.h"
#include "../../src/core/santa.h"
#include "../../src/core/text.h"
#include "../../src/generated/aircraft_db.h"

static int failures = 0, checks = 0;
#define CHECK(cond)                                                     \
  do {                                                                  \
    checks++;                                                           \
    if (!(cond)) {                                                      \
      failures++;                                                       \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);       \
    }                                                                   \
  } while (0)
#define CHECK_STR(a, b)                                                                  \
  do {                                                                                   \
    checks++;                                                                            \
    if (std::strcmp((a), (b)) != 0) {                                                    \
      failures++;                                                                        \
      std::printf("FAIL %s:%d  \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b));       \
    }                                                                                    \
  } while (0)

static void testGeo() {
  // Paris CDG -> New York JFK ~ 5834 km, initial bearing ~ 292 deg
  const double d = geo::distanceKm(49.0097, 2.5479, 40.6413, -73.7781);
  CHECK(std::fabs(d - 5834) < 15);
  const double b = geo::bearingDeg(49.0097, 2.5479, 40.6413, -73.7781);
  CHECK(std::fabs(b - 292.3) < 1.0);
  CHECK(geo::compassIndex16(0) == 0);
  CHECK(geo::compassIndex16(359) == 0);
  CHECK(geo::compassIndex16(247) == 11);  // WSW
  CHECK(geo::compassIndex8(135) == 3);    // SE
  // A point on the route has ~0 cross-track; along-track is between 0 and the total.
  const double xt = geo::crossTrackKm(49.0097, 2.5479, 40.6413, -73.7781, 51.5, -20.0);
  CHECK(xt < 120);
  const double at = geo::alongTrackKm(49.0097, 2.5479, 40.6413, -73.7781, 51.5, -20.0);
  CHECK(at > 1000 && at < 2500);
}

static void testText() {
  char out[64];
  text::toFontSafe("Kraków", out, sizeof(out));
  CHECK_STR(out, "Kraków");  // ó is Latin-1: kept
  text::toFontSafe("Łódź", out, sizeof(out));
  CHECK_STR(out, "Lódz");
  text::toFontSafe("İstanbul Sabiha Gökçen", out, sizeof(out));
  CHECK_STR(out, "Istanbul Sabiha Gökçen");
  text::toFontSafe("東京", out, sizeof(out));
  CHECK_STR(out, "??");
  text::toFontSafe("Saint\xE2\x80\x93\xC3\x89tienne", out, sizeof(out));
  CHECK_STR(out, "Saint-Étienne");
  text::toFontSafe("abcdefghij", out, 5);
  CHECK_STR(out, "abcd");
  text::toUpperLatin1("Montréal", out, sizeof(out));
  CHECK_STR(out, "MONTRÉAL");

  text::formatThousands(36000, false, out, sizeof(out));
  CHECK_STR(out, "36,000");
  text::formatThousands(36000, true, out, sizeof(out));
  CHECK_STR(out, "36\xC2\xA0" "000");
  text::formatAltitude(36010, text::Units::Aviation, false, out, sizeof(out));
  CHECK_STR(out, "36,000 ft");
  text::formatAltitude(36000, text::Units::Metric, true, out, sizeof(out));
  CHECK_STR(out, "10\xC2\xA0" "970 m");
  text::formatSpeed(450, text::Units::Metric, out, sizeof(out));
  CHECK_STR(out, "833 km/h");
  text::formatDistance(5.55f, text::Units::Metric, true, out, sizeof(out));
  CHECK_STR(out, "5,6 km");
  text::formatDistance(30, text::Units::Aviation, false, out, sizeof(out));
  CHECK_STR(out, "16 NM");
}

static void testI18n() {
  using i18n::Lang;
  using i18n::S;
  CHECK_STR(i18n::tr(Lang::EN, S::Live), "LIVE");
  CHECK_STR(i18n::tr(Lang::FR, S::Offline), "HORS LIGNE");
  CHECK_STR(i18n::tr(Lang::FR, S::ResetDone), "Réglages effacés. Redémarrage.");
  CHECK_STR(i18n::compass16(Lang::FR, 11), "OSO");
  char buf[64];
  std::snprintf(buf, sizeof(buf), i18n::tr(Lang::FR, S::LookDir), i18n::directionPrefix(Lang::FR, 2),
                i18n::direction8(Lang::FR, 2));
  CHECK_STR(buf, "Regardez à l'est");
  std::snprintf(buf, sizeof(buf), i18n::tr(Lang::EN, S::LookDir), i18n::directionPrefix(Lang::EN, 7),
                i18n::direction8(Lang::EN, 7));
  CHECK_STR(buf, "Look in the north-west");
}

static ac::Aircraft mk(const char* hex, float dist, int32_t alt = 20000, uint16_t squawk = 1000) {
  ac::Aircraft a;
  std::strcpy(a.hex, hex);
  a.distKm = dist;
  a.altFt = alt;
  a.squawk = squawk;
  return a;
}

static void testRarityAndSelection() {
  ac::Aircraft a = mk("aaaaaa", 10);
  CHECK(ac::scoreRarity(a, 0, false).score == 0);
  a.squawk = 7700;
  CHECK(ac::scoreRarity(a, 0, false).reason == ac::RareReason::Emergency);
  a.squawk = 1000;
  a.military = true;
  const ac::Rarity r = ac::scoreRarity(a, 0, false);
  CHECK(r.score == 65 && r.reason == ac::RareReason::Military);
  CHECK(ac::scoreRarity(a, 100, true).score == 100);  // B-2 keeps its own score

  ac::SelectorConfig cfg;
  cfg.rangeKm = 30;
  cfg.rareRangeKm = 80;
  cfg.rareThreshold = 55;
  ac::Selector sel(cfg);

  // Closest wins
  ac::Aircraft list[4] = {mk("000001", 12), mk("000002", 8), mk("000003", 40), mk("000004", 3, 0)};
  list[3].onGround = true;
  ac::Rarity rar[4];
  ac::Selection s = sel.select(list, rar, 4, 0);
  CHECK(s.index == 1 && !s.takeover);

  // Hysteresis: a slightly closer aircraft doesn't steal the screen...
  list[0].distKm = 7;
  s = sel.select(list, rar, 4, 30000);
  CHECK(s.index == 1);
  // ...but a much closer one does, once the hold time has passed
  list[0].distKm = 4;
  s = sel.select(list, rar, 4, 31000);
  CHECK(s.index == 0);

  // A rare aircraft further away (but inside the rare range) takes over
  rar[2] = {90, ac::RareReason::RareType};
  s = sel.select(list, rar, 4, 32000);
  CHECK(s.index == 2 && s.takeover);
  // An emergency outranks it
  list[1].squawk = 7700;
  rar[1] = ac::scoreRarity(list[1], 0, false);
  s = sel.select(list, rar, 4, 33000);
  CHECK(s.index == 1 && s.takeover && s.rarity.reason == ac::RareReason::Emergency);
  // Threshold 101 disables takeovers
  cfg.rareThreshold = 101;
  sel.setConfig(cfg);
  list[1].squawk = 1000;
  rar[1] = {};
  s = sel.select(list, rar, 4, 34000);
  CHECK(!s.takeover);
  // Nothing in range
  ac::Aircraft far[1] = {mk("00000a", 50)};
  ac::Rarity none[1];
  s = sel.select(far, none, 1, 90000);
  CHECK(s.index == -1);
}

static void testDb() {
  CHECK(db::kFamilyCount == 64);  // 63 aircraft families + the Christmas sleigh
  const db::TypeInfo* t = db::findType("A388");
  CHECK(t && std::strcmp(db::kFamilies[t->family].key, "a380") == 0);
  CHECK(db::typeRarity(t) == 60);
  t = db::findType("A337");
  CHECK(t && db::typeRarity(t) >= 80);
  CHECK(db::findType("ZZZZ") == nullptr);
  CHECK(db::findType("B38M") != nullptr);
  const db::Airline* al = db::findAirline("AFR1234");
  CHECK(al && std::strcmp(al->name, "Air France") == 0);
  al = db::findAirline("ASCOT21");
  CHECK(al && std::strstr(al->name, "Royal Air Force"));
  CHECK(db::findAirline("AFRX12") == nullptr);  // prefix must be followed by a digit
  CHECK(std::strcmp(db::kFamilies[db::familyForCategory("A7")].key, "helicopter") == 0);
  // Every type points at a valid family; tables sorted for binary search
  for (int i = 0; i < db::kTypeCount; i++) {
    CHECK(db::kTypes[i].family < db::kFamilyCount);
    if (i) CHECK(std::strcmp(db::kTypes[i - 1].code, db::kTypes[i].code) < 0);
  }
}

static void testSanta() {
  const int64_t dec24 = 1798070400;  // 2026-12-24 00:00 UTC
  CHECK(santa::yearOf(dec24) == 2026);
  CHECK(santa::yearOf(dec24 - 86400 * 400) == 2025);
  // San Anselmo (122.56 W): overhead at solar midnight, 08:10 UTC on the 25th
  const double lat = 37.97, lon = -122.56;
  const int64_t mid = santa::midnightOverhead(dec24, lon);
  CHECK(mid == dec24 + 86400 + 29414);
  ac::Aircraft a;
  CHECK(!santa::sleigh(mid - 11 * 60, lat, lon, 80, a));        // not yet
  CHECK(santa::sleigh(mid - 9 * 60, lat, lon, 80, a));          // coming in from the east
  CHECK(a.lon > lon && a.distKm > 50 && a.distKm < 80 && a.trackDeg == 270);
  CHECK(std::strcmp(a.type, santa::kType) == 0 && std::strcmp(a.callsign, "SANTA1") == 0);
  CHECK(santa::sleigh(mid, lat, lon, 80, a));                   // overhead, low
  CHECK(a.distKm < 1.0f && a.altFt <= 1500);
  CHECK(santa::sleigh(mid + 9 * 60, lat, lon, 80, a));          // leaving to the west
  CHECK(a.lon < lon && a.distKm > 50);
  CHECK(!santa::sleigh(mid + 11 * 60, lat, lon, 80, a));
  CHECK(!santa::sleigh(mid - 86400 * 30, lat, lon, 80, a));     // only on Christmas Eve
  // Santa is the rarest aircraft there is: he takes over the screen
  const db::TypeInfo* t = db::findType(santa::kType);
  CHECK(t && db::typeRarity(t) == 100);
  CHECK(std::strcmp(db::findAirline("SANTA1")->name, "North Pole Express") == 0);
  santa::sleigh(mid - 5 * 60, lat, lon, 80, a);
  ac::Aircraft list[2] = {mk("abc123", 3), a};
  ac::Rarity rar[2] = {ac::scoreRarity(list[0], 0, false), ac::scoreRarity(list[1], db::typeRarity(t), false)};
  ac::SelectorConfig cfg;
  ac::Selector sel(cfg);
  ac::Selection r = sel.select(list, rar, 2, 0);
  CHECK(r.index == 1 && r.takeover && r.rarity.reason == ac::RareReason::RareType);
}

int main() {
  testGeo();
  testText();
  testI18n();
  testRarityAndSelection();
  testDb();
  testSanta();
  std::printf("unit tests: %d checks, %s\n", checks, failures ? "FAILURES" : "OK");
  return failures ? 1 : 0;
}

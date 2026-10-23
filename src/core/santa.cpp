#include "santa.h"

#include <cmath>
#include <cstring>

#include "geo.h"

namespace santa {
namespace {

int64_t daysFromCivil(int y, unsigned m, unsigned d) {  // Howard Hinnant's algorithm
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
}

}  // namespace

int yearOf(int64_t t) {
  int64_t days = (t >= 0 ? t : t - 86399) / 86400 + 719468;
  const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(days - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  const unsigned m = mp + (mp < 10 ? 3 : -9);
  return static_cast<int>(yoe + era * 400 + (m <= 2));
}

int64_t midnightOverhead(int64_t now, double lonDeg) {
  const int64_t dec25 = daysFromCivil(yearOf(now), 12, 25) * 86400;
  return dec25 - static_cast<int64_t>(std::llround(lonDeg / 15.0 * 3600.0));
}

bool sleigh(int64_t now, double homeLat, double homeLon, float rangeKm, ac::Aircraft& out) {
  const int64_t t0 = midnightOverhead(now, homeLon);
  const int64_t dt = now - t0;  // < 0: still to the east
  if (dt < -kVisitHalfS || dt > kVisitHalfS) return false;

  // Straight east-to-west pass over the home, edge of the range to edge of the range.
  const double span = std::fmax(2.0, rangeKm * 0.95);
  const double eastKm = -span * static_cast<double>(dt) / kVisitHalfS;
  const double kmPerDegLon = 111.32 * std::cos(homeLat * geo::kDeg);
  ac::Aircraft a;
  std::strcpy(a.hex, kHex);
  std::strcpy(a.callsign, kCallsign);
  std::strcpy(a.reg, "NP-001");
  std::strcpy(a.type, kType);
  a.lat = homeLat + 0.0045;  // a little to the north, so he is never exactly on the home marker
  a.lon = homeLon + (kmPerDegLon > 1 ? eastKm / kmPerDegLon : 0);
  const double frac = std::fabs(static_cast<double>(dt)) / kVisitHalfS;  // 0 overhead, 1 at the edges
  a.altFt = static_cast<int32_t>(std::lround((1200 + 9000 * frac * frac) / 100.0) * 100);
  a.vrateFpm = static_cast<int16_t>(dt < 0 ? -900 * frac : 900 * frac);
  a.gsKt = static_cast<float>(2 * span / (2.0 * kVisitHalfS / 3600.0) / 1.852);
  a.trackDeg = 270;
  a.squawk = 1225;  // 12/25
  a.seenPosS = 0;
  a.distKm = static_cast<float>(geo::distanceKm(homeLat, homeLon, a.lat, a.lon));
  a.bearingDeg = static_cast<float>(geo::bearingDeg(homeLat, homeLon, a.lat, a.lon));
  out = a;
  return true;
}

}  // namespace santa

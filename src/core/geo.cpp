#include "geo.h"

#include <cmath>

namespace geo {

double normalizeDeg(double deg) {
  deg = std::fmod(deg, 360.0);
  if (deg < 0) deg += 360.0;
  return deg;
}

double distanceKm(double lat1, double lon1, double lat2, double lon2) {
  const double p1 = lat1 * kDeg, p2 = lat2 * kDeg;
  const double dp = (lat2 - lat1) * kDeg, dl = (lon2 - lon1) * kDeg;
  const double a = std::sin(dp / 2) * std::sin(dp / 2) +
                   std::cos(p1) * std::cos(p2) * std::sin(dl / 2) * std::sin(dl / 2);
  return 2 * kEarthRadiusKm * std::atan2(std::sqrt(a), std::sqrt(1 - a));
}

double bearingDeg(double lat1, double lon1, double lat2, double lon2) {
  const double p1 = lat1 * kDeg, p2 = lat2 * kDeg, dl = (lon2 - lon1) * kDeg;
  const double y = std::sin(dl) * std::cos(p2);
  const double x = std::cos(p1) * std::sin(p2) - std::sin(p1) * std::cos(p2) * std::cos(dl);
  return normalizeDeg(std::atan2(y, x) / kDeg);
}

double crossTrackKm(double latA, double lonA, double latB, double lonB, double latP, double lonP) {
  const double d13 = distanceKm(latA, lonA, latP, lonP) / kEarthRadiusKm;
  const double t13 = bearingDeg(latA, lonA, latP, lonP) * kDeg;
  const double t12 = bearingDeg(latA, lonA, latB, lonB) * kDeg;
  return std::fabs(std::asin(std::sin(d13) * std::sin(t13 - t12)) * kEarthRadiusKm);
}

double alongTrackKm(double latA, double lonA, double latB, double lonB, double latP, double lonP) {
  const double d13 = distanceKm(latA, lonA, latP, lonP) / kEarthRadiusKm;
  const double t13 = bearingDeg(latA, lonA, latP, lonP) * kDeg;
  const double t12 = bearingDeg(latA, lonA, latB, lonB) * kDeg;
  const double dxt = std::asin(std::sin(d13) * std::sin(t13 - t12));
  const double c = std::cos(dxt);
  if (std::fabs(c) < 1e-12) return 0;
  double r = std::acos(std::fmin(1.0, std::fmax(-1.0, std::cos(d13) / c))) * kEarthRadiusKm;
  return std::cos(t13 - t12) < 0 ? -r : r;
}

int compassIndex16(double b) { return static_cast<int>(std::floor(normalizeDeg(b) / 22.5 + 0.5)) % 16; }
int compassIndex8(double b) { return static_cast<int>(std::floor(normalizeDeg(b) / 45.0 + 0.5)) % 8; }

}  // namespace geo

// Great-circle helpers. Pure C++ (no Arduino), unit-tested on the host.
#pragma once

namespace geo {

constexpr double kEarthRadiusKm = 6371.0088;
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;

double distanceKm(double lat1, double lon1, double lat2, double lon2);
// Initial bearing from point 1 to point 2, degrees clockwise from true north [0, 360).
double bearingDeg(double lat1, double lon1, double lat2, double lon2);
// Distance of point P from the great circle A->B (absolute value, km).
double crossTrackKm(double latA, double lonA, double latB, double lonB, double latP, double lonP);
// Distance travelled along A->B up to the foot of the perpendicular from P (km, can be < 0).
double alongTrackKm(double latA, double lonA, double latB, double lonB, double latP, double lonP);

// 16-point compass sector index (0 = N, 1 = NNE, ... 15 = NNW).
int compassIndex16(double bearingDeg);
// 8-point compass sector index (0 = N, 1 = NE, ... 7 = NW).
int compassIndex8(double bearingDeg);

double normalizeDeg(double deg);  // -> [0, 360)

}  // namespace geo

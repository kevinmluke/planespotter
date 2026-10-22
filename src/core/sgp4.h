// Compact SGP4 propagator (near-earth branch only, which covers the ISS).
// Follows Vallado et al. "Revisiting Spacetrack Report #3" (2006), WGS-72 constants.
// Pure C++ (no Arduino); validated on the host against the python-sgp4 reference.
#pragma once

#include <cstdint>

namespace sgp4 {

struct Vec3 {
  double x, y, z;
};

class Satellite {
 public:
  // Parses the two TLE lines. Returns false on malformed input, checksum error,
  // or a deep-space orbit (period >= 225 min), which this propagator does not handle.
  bool init(const char* line1, const char* line2);

  // Position (km) and velocity (km/s) in the TEME frame, `minutes` after the TLE epoch.
  bool propagate(double minutes, Vec3& r, Vec3& v) const;
  // Same, at an absolute Julian date (UTC).
  bool propagateJd(double jd, Vec3& r, Vec3& v) const;

  double epochJd() const { return jdEpoch_; }
  bool valid() const { return valid_; }

 private:
  bool valid_ = false;
  double jdEpoch_ = 0;
  // Mean elements
  double bstar_, inclo_, nodeo_, ecco_, argpo_, mo_, noKozai_, no_;
  // Initialised coefficients
  int isimp_;
  double ao_, con41_, x1mth2_, x7thm1_, cc1_, cc4_, cc5_, d2_, d3_, d4_, delmo_, eta_,
      argpdot_, omgcof_, sinmao_, t2cof_, t3cof_, t4cof_, t5cof_, xlcof_, aycof_, xmcof_,
      nodecf_, mdot_, nodedot_;
};

// ---------- Time & frames ----------
double unixToJd(double unixSeconds);
double jdToUnix(double jd);
// Greenwich mean sidereal time (radians) at a UT1 Julian date (UTC is close enough here).
double gmst(double jd);
// TEME -> Earth-fixed (rotation by GMST; polar motion ignored, < 20 m error).
Vec3 temeToEcef(const Vec3& teme, double jd);

// ---------- Observer geometry ----------
struct Geodetic {
  double latDeg, lonDeg, altKm;
};
Vec3 geodeticToEcef(const Geodetic& g);
Geodetic ecefToGeodetic(const Vec3& ecef);

struct LookAngle {
  double azDeg;    // degrees clockwise from true north
  double elDeg;    // degrees above the horizon
  double rangeKm;
};
LookAngle lookAngle(const Geodetic& observer, const Vec3& targetEcef);

// ---------- Sun ----------
// Unit vector * distance (km) towards the Sun, in an Earth-centred inertial frame
// (mean equator of date; within the accuracy needed for twilight/shadow tests).
Vec3 sunEci(double jd);
// Sub-solar point (declination and longitude where the Sun is at zenith), degrees.
void subsolarPoint(double jd, double& latDeg, double& lonDeg);
// Sun elevation above the observer's horizon, degrees.
double sunElevationDeg(const Geodetic& observer, double jd);
// True when a satellite at `satEci` (TEME, km) is lit by the Sun (cylindrical shadow model).
bool isSunlit(const Vec3& satEci, double jd);

}  // namespace sgp4

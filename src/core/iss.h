// ISS tracking: live position, ground track and visible-pass prediction.
// Pure C++ (no Arduino), unit-tested on the host.
#pragma once

#include <cstdint>

#include "sgp4.h"

namespace iss {

struct Position {
  double latDeg, lonDeg, altKm;
  double speedKmh;
  bool sunlit;
};

struct SkyPoint {
  int64_t t;     // unix seconds
  float azDeg;
  float elDeg;
};

struct Pass {
  int64_t rise = 0, culmination = 0, set = 0;  // above the geometric horizon
  float maxElDeg = 0;
  float riseAzDeg = 0, maxAzDeg = 0, setAzDeg = 0;
  // Visible part: ISS sunlit, observer in darkness (Sun below -6 deg), ISS >= 10 deg high.
  bool visible = false;
  SkyPoint visStart{}, visMax{}, visEnd{};
  // Sampled sky track of the visible part, for drawing on the sky chart.
  static constexpr int kTrackMax = 48;
  SkyPoint track[kTrackMax];
  int trackLen = 0;
};

constexpr float kMinVisibleElDeg = 10.0f;

// Optional callback invoked every few dozen propagations during long searches
// (lets an RTOS task yield). nullptr disables it.
void setYieldHook(void (*hook)());
constexpr float kMaxSunElDeg = -6.0f;  // civil twilight

class Tracker {
 public:
  bool setTle(const char* line1, const char* line2);
  bool hasTle() const { return sat_.valid(); }
  // Age of the orbital elements at unix time `now`, in days.
  double tleAgeDays(int64_t now) const;

  void setObserver(double latDeg, double lonDeg, double altM);

  bool position(int64_t t, Position& out) const;
  bool skyPosition(int64_t t, SkyPoint& out) const;

  // Ground track points from t0 to t1 every `stepS` seconds. Returns count written.
  int groundTrack(int64_t t0, int64_t t1, int stepS, float* lats, float* lons, int maxPoints) const;

  // Next passes above the horizon starting at `from`, searching `hours` ahead.
  // Returns number of passes written (passes still in progress at `from` are included).
  int findPasses(int64_t from, int hours, Pass* out, int maxPasses) const;
  // Convenience: the next pass with a visible part that has not ended yet at `from`.
  bool nextVisiblePass(int64_t from, int hours, Pass& out) const;

  double elevationAt(int64_t t) const;

 private:
  void fillVisibility(Pass& p) const;

  sgp4::Satellite sat_;
  sgp4::Geodetic obs_{0, 0, 0};
};

}  // namespace iss

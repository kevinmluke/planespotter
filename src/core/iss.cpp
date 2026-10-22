#include "iss.h"

#include <cmath>

namespace iss {

using sgp4::Vec3;

namespace {
void (*gYield)() = nullptr;
int gYieldCounter = 0;
inline void maybeYield() {
  if (gYield && ++gYieldCounter % 48 == 0) gYield();
}
}  // namespace

void setYieldHook(void (*hook)()) { gYield = hook; }

bool Tracker::setTle(const char* l1, const char* l2) { return sat_.init(l1, l2); }

double Tracker::tleAgeDays(int64_t now) const {
  return sgp4::unixToJd(static_cast<double>(now)) - sat_.epochJd();
}

void Tracker::setObserver(double lat, double lon, double altM) { obs_ = {lat, lon, altM / 1000.0}; }

bool Tracker::position(int64_t t, Position& out) const {
  const double jd = sgp4::unixToJd(static_cast<double>(t));
  Vec3 r, v;
  if (!sat_.propagateJd(jd, r, v)) return false;
  const sgp4::Geodetic g = sgp4::ecefToGeodetic(sgp4::temeToEcef(r, jd));
  out.latDeg = g.latDeg;
  out.lonDeg = g.lonDeg;
  out.altKm = g.altKm;
  out.speedKmh = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z) * 3600.0;
  out.sunlit = sgp4::isSunlit(r, jd);
  return true;
}

bool Tracker::skyPosition(int64_t t, SkyPoint& out) const {
  maybeYield();
  const double jd = sgp4::unixToJd(static_cast<double>(t));
  Vec3 r, v;
  if (!sat_.propagateJd(jd, r, v)) return false;
  const sgp4::LookAngle la = sgp4::lookAngle(obs_, sgp4::temeToEcef(r, jd));
  out = {t, static_cast<float>(la.azDeg), static_cast<float>(la.elDeg)};
  return true;
}

int Tracker::groundTrack(int64_t t0, int64_t t1, int stepS, float* lats, float* lons, int maxPoints) const {
  int n = 0;
  for (int64_t t = t0; t <= t1 && n < maxPoints; t += stepS) {
    Position p;
    if (!position(t, p)) break;
    lats[n] = static_cast<float>(p.latDeg);
    lons[n] = static_cast<float>(p.lonDeg);
    n++;
  }
  return n;
}

double Tracker::elevationAt(int64_t t) const {
  SkyPoint s;
  return skyPosition(t, s) ? s.elDeg : -90.0;
}

// Binary search for the horizon crossing between a (below/above) and b.
static int64_t refineCrossing(const Tracker& tr, int64_t a, int64_t b, bool rising,
                              double (Tracker::*elev)(int64_t) const) {
  while (b - a > 1) {
    const int64_t m = a + (b - a) / 2;
    const bool above = (tr.*elev)(m) >= 0.0;
    if (above == rising) b = m;
    else a = m;
  }
  return b;
}

void Tracker::fillVisibility(Pass& p) const {
  p.visible = false;
  p.trackLen = 0;
  const int64_t span = p.set - p.rise;
  if (span <= 0) return;
  // Sample every 5 s; keep the samples that meet all three visibility conditions.
  const int64_t step = 5;
  bool inVis = false;
  for (int64_t t = p.rise; t <= p.set; t += step) {
    const double jd = sgp4::unixToJd(static_cast<double>(t));
    Vec3 r, v;
    if (!sat_.propagateJd(jd, r, v)) break;
    const sgp4::LookAngle la = sgp4::lookAngle(obs_, sgp4::temeToEcef(r, jd));
    const bool ok = la.elDeg >= kMinVisibleElDeg && sgp4::isSunlit(r, jd) &&
                    sgp4::sunElevationDeg(obs_, jd) <= kMaxSunElDeg;
    const SkyPoint sp{t, static_cast<float>(la.azDeg), static_cast<float>(la.elDeg)};
    if (ok) {
      if (!inVis && !p.visible) {
        p.visStart = sp;
        p.visMax = sp;
      }
      if (!p.visible || inVis) {  // only the first continuous visible segment
        p.visible = true;
        inVis = true;
        p.visEnd = sp;
        if (sp.elDeg > p.visMax.elDeg) p.visMax = sp;
      }
    } else if (inVis) {
      inVis = false;
    }
  }
  if (!p.visible) return;
  // Down-sample the visible segment into the fixed-size track.
  const int64_t visSpan = p.visEnd.t - p.visStart.t;
  const int n = visSpan <= 0 ? 1 : Pass::kTrackMax;
  for (int i = 0; i < n; i++) {
    const int64_t t = p.visStart.t + (n == 1 ? 0 : visSpan * i / (n - 1));
    SkyPoint sp;
    if (skyPosition(t, sp)) p.track[p.trackLen++] = sp;
  }
}

int Tracker::findPasses(int64_t from, int hours, Pass* out, int maxPasses) const {
  if (!sat_.valid() || maxPasses <= 0) return 0;
  const int64_t end = from + static_cast<int64_t>(hours) * 3600;
  const int64_t step = 60;
  int count = 0;
  int64_t t = from;
  double el = elevationAt(t);

  // If a pass is already in progress, walk back to find its rise.
  if (el >= 0) {
    int64_t back = t;
    while (elevationAt(back - step) >= 0 && from - back < 3600) back -= step;
    t = refineCrossing(*this, back - step, back, true, &Tracker::elevationAt);
    el = 0;
  }

  while (t < end && count < maxPasses) {
    // Find the next rise.
    if (el < 0) {
      int64_t a = t;
      double ea = el;
      while (a < end) {
        const int64_t b = a + step;
        const double eb = elevationAt(b);
        if (ea < 0 && eb >= 0) {
          t = refineCrossing(*this, a, b, true, &Tracker::elevationAt);
          break;
        }
        a = b;
        ea = eb;
      }
      if (a >= end) break;
    }
    Pass p;
    p.rise = t;
    SkyPoint sp;
    skyPosition(t, sp);
    p.riseAzDeg = sp.azDeg;
    // Walk until the set, tracking culmination.
    int64_t a = t;
    p.maxElDeg = -90;
    while (true) {
      const int64_t b = a + step;
      SkyPoint sb;
      if (!skyPosition(b, sb)) break;
      if (sb.elDeg > p.maxElDeg) {
        p.maxElDeg = sb.elDeg;
        p.culmination = b;
        p.maxAzDeg = sb.azDeg;
      }
      if (sb.elDeg < 0) {
        p.set = refineCrossing(*this, a, b, false, &Tracker::elevationAt);
        break;
      }
      a = b;
      if (b - p.rise > 2 * 3600) break;  // safety
    }
    if (p.set == 0) break;
    // Refine culmination to the second around the coarse maximum.
    for (int64_t c = p.culmination - step; c <= p.culmination + step; c += 2) {
      SkyPoint sc;
      if (skyPosition(c, sc) && sc.elDeg > p.maxElDeg) {
        p.maxElDeg = sc.elDeg;
        p.maxAzDeg = sc.azDeg;
        p.culmination = c;
      }
    }
    skyPosition(p.set, sp);
    p.setAzDeg = sp.azDeg;
    fillVisibility(p);
    out[count++] = p;
    t = p.set + step;
    el = elevationAt(t);
  }
  return count;
}

bool Tracker::nextVisiblePass(int64_t from, int hours, Pass& out) const {
  static Pass passes[16];  // large struct; keep it off the (small) task stack
  const int n = findPasses(from, hours, passes, 16);
  for (int i = 0; i < n; i++) {
    if (passes[i].visible && passes[i].visEnd.t >= from) {
      out = passes[i];
      return true;
    }
  }
  return false;
}

}  // namespace iss

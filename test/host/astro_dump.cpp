// Host helper: prints SGP4 states and predicted passes so test_astro.py can
// compare them with python-sgp4 / Skyfield.
// Usage: astro_dump <line1> <line2> <lat> <lon> <unix_start>
#include <cstdio>
#include <cstdlib>

#include "../../src/core/iss.h"
#include "../../src/core/sgp4.h"

int main(int argc, char** argv) {
  if (argc < 6) return 2;
  sgp4::Satellite sat;
  if (!sat.init(argv[1], argv[2])) {
    std::printf("INIT_FAIL\n");
    return 1;
  }
  std::printf("EPOCH %.9f\n", sat.epochJd());
  for (int m = 0; m <= 2880; m += 360) {
    sgp4::Vec3 r, v;
    sat.propagate(m, r, v);
    std::printf("STATE %d %.6f %.6f %.6f %.9f %.9f %.9f\n", m, r.x, r.y, r.z, v.x, v.y, v.z);
  }
  iss::Tracker tr;
  tr.setTle(argv[1], argv[2]);
  tr.setObserver(std::atof(argv[3]), std::atof(argv[4]), 50);
  const long long start = std::atoll(argv[5]);
  iss::Position p;
  tr.position(start, p);
  std::printf("SUBPOINT %.4f %.4f %.3f %.1f %d\n", p.latDeg, p.lonDeg, p.altKm, p.speedKmh, p.sunlit);
  static iss::Pass passes[20];
  const int n = tr.findPasses(start, 48, passes, 20);
  for (int i = 0; i < n; i++) {
    const iss::Pass& q = passes[i];
    std::printf("PASS %lld %lld %lld %.2f %.1f %.1f %d %lld %lld %.1f\n", (long long)q.rise,
                (long long)q.culmination, (long long)q.set, q.maxElDeg, q.riseAzDeg, q.setAzDeg,
                q.visible, (long long)q.visStart.t, (long long)q.visEnd.t, q.visMax.elDeg);
  }
  return 0;
}

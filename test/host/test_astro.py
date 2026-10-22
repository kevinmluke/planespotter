#!/usr/bin/env python3
"""Cross-checks the firmware's SGP4 / pass predictor against python-sgp4 and Skyfield.

Run via `make -C test/host` (needs: pip install sgp4 skyfield).
"""
import subprocess
import sys
import time

from sgp4.api import Satrec
from skyfield.api import EarthSatellite, load, wgs84

TLE = (
    "1 25544U 98067A   26294.04791053  .00009575  00000+0  18188-3 0  9996",
    "2 25544  51.6327  37.2623 0006759 289.5321  70.4938 15.48967427591263",
)
# Observers: Paris and Montreal
OBSERVERS = [(48.8566, 2.3522), (45.5019, -73.5674)]

failures = 0


def check(cond, msg):
    global failures
    if not cond:
        failures += 1
        print("FAIL:", msg)


def run_dump(lat, lon, start):
    out = subprocess.run(
        ["./build/astro_dump", TLE[0], TLE[1], str(lat), str(lon), str(start)],
        capture_output=True, text=True, check=True).stdout
    return [line.split() for line in out.splitlines()]


sat = Satrec.twoline2rv(*TLE)
ts = load.timescale()
es = EarthSatellite(*TLE, "ISS", ts)
start = int(es.epoch.utc_datetime().timestamp()) + 3600

for lat, lon in OBSERVERS:
    rows = run_dump(lat, lon, start)
    # 1) Raw SGP4 state vectors vs the reference implementation
    for row in rows:
        if row[0] == "STATE":
            m = float(row[1])
            e, r, v = sat.sgp4_tsince(m)
            check(e == 0, f"reference error at {m}")
            dr = sum((float(row[2 + i]) - r[i]) ** 2 for i in range(3)) ** 0.5
            dv = sum((float(row[5 + i]) - v[i]) ** 2 for i in range(3)) ** 0.5
            check(dr < 0.05 and dv < 5e-5, f"state t={m} dr={dr:.4f} km dv={dv:.2e}")
        if row[0] == "SUBPOINT":
            t = ts.from_datetime(__import__("datetime").datetime.fromtimestamp(start, __import__("datetime").timezone.utc))
            sp = wgs84.subpoint_of(es.at(t))
            check(abs(sp.latitude.degrees - float(row[1])) < 0.05, f"sub lat {row[1]} vs {sp.latitude.degrees}")
            dlon = (sp.longitude.degrees - float(row[2]) + 540) % 360 - 180
            check(abs(dlon) < 0.05, f"sub lon {row[2]} vs {sp.longitude.degrees}")

    # 2) Rise / culmination / set vs Skyfield's event finder
    obs = wgs84.latlon(lat, lon, 50)
    t0 = ts.from_datetime(__import__("datetime").datetime.fromtimestamp(start, __import__("datetime").timezone.utc))
    t1 = ts.tt_jd(t0.tt + 2)
    times, events = es.find_events(obs, t0, t1, altitude_degrees=0.0)
    ref = []
    cur = {}
    for ti, ev in zip(times, events):
        ts_unix = ti.utc_datetime().timestamp()
        if ev == 0:
            cur = {"rise": ts_unix}
        elif ev == 1 and "rise" in cur:
            cur["culm"] = ts_unix
            cur["maxel"] = (es - obs).at(ti).altaz()[0].degrees
        elif ev == 2 and "rise" in cur:
            cur["set"] = ts_unix
            ref.append(cur)
            cur = {}
    mine = [r for r in rows if r[0] == "PASS"]
    check(len(mine) >= len(ref) and len(ref) > 0, f"pass count mine={len(mine)} ref={len(ref)}")
    for rp in ref:
        match = [m for m in mine if abs(float(m[1]) - rp["rise"]) < 30]
        check(bool(match), f"no pass matching rise {time.ctime(rp['rise'])}")
        if match:
            m = match[0]
            check(abs(float(m[3]) - rp["set"]) < 30, f"set {m[3]} vs {rp['set']}")
            check(abs(float(m[4]) - rp["maxel"]) < 0.5, f"maxel {m[4]} vs {rp['maxel']:.2f}")
    visible = sum(1 for m in mine if m[7] == "1")
    print(f"observer {lat},{lon}: {len(mine)} passes in 48 h ({visible} visible), reference {len(ref)}")

print("astro tests:", "OK" if failures == 0 else f"{failures} FAILURES")
sys.exit(1 if failures else 0)

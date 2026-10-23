// Christmas easter egg: on Christmas Eve, Santa's sleigh flies over every home at
// local midnight and shows up in the flight tracker as a rare aircraft.
// Pure C++ (no Arduino), unit-tested on the host.
#pragma once

#include <cstdint>

#include "aircraft.h"

namespace santa {

constexpr const char* kType = "SLEI";      // type designator in data/aircraft_types.csv
constexpr const char* kCallsign = "SANTA1";
constexpr const char* kHex = "5a4e7a";     // not a real ICAO address range in use
constexpr int64_t kVisitHalfS = 10 * 60;   // overhead within +/- 10 minutes of midnight

// Local (solar) midnight at the end of December 24 for a home at `lonDeg`, in the year of `now`.
int64_t midnightOverhead(int64_t now, double lonDeg);

// Fills `out` with Santa's sleigh if it is near the home at `now`: it comes in from the
// east, passes overhead at midnight and leaves to the west, crossing `rangeKm` either side.
// Returns false outside the visit.
bool sleigh(int64_t now, double homeLat, double homeLon, float rangeKm, ac::Aircraft& out);

// Gregorian year of a unix time (UTC).
int yearOf(int64_t t);

}  // namespace santa

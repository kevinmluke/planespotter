// Aircraft model, rarity scoring and "what goes on screen" selection.
// Pure C++ (no Arduino), unit-tested on the host.
#pragma once

#include <climits>
#include <cstdint>

namespace ac {

constexpr int32_t kAltUnknown = INT32_MIN;

struct Aircraft {
  char hex[7] = {0};
  char callsign[9] = {0};
  char reg[12] = {0};
  char type[5] = {0};   // ICAO type designator, e.g. "A20N"
  char category[3] = {0};  // ADS-B emitter category, e.g. "A3"
  double lat = 0, lon = 0;
  int32_t altFt = kAltUnknown;  // barometric altitude
  bool onGround = false;
  float gsKt = -1;       // ground speed, < 0 if unknown
  float trackDeg = -1;   // true track, < 0 if unknown
  int16_t vrateFpm = 0;
  uint16_t squawk = 0;   // decimal digits as read, e.g. 7700
  bool military = false;   // database flag from the ADS-B aggregator
  bool emergencyFlag = false;  // aircraft-declared emergency (not "none")
  float seenPosS = 0;     // seconds since last position
  // Derived
  float distKm = 0;
  float bearingDeg = 0;   // from home to the aircraft
};

enum class RareReason : uint8_t { None, Emergency, RareType, Military };

struct Rarity {
  int score = 0;            // 0..100
  RareReason reason = RareReason::None;
};

// Score an aircraft from its type entry (typeRarity from the generated table),
// military flag, squawk and declared emergency.
Rarity scoreRarity(const Aircraft& a, int typeRarity, bool typeIsMilitary);

// Emergency squawks: 7500 hijack, 7600 radio failure, 7700 general emergency.
bool isEmergencySquawk(uint16_t squawk);

struct SelectorConfig {
  float rangeKm = 30;         // "closest aircraft" radius
  float rareRangeKm = 80;     // rare aircraft can take over from further away
  int rareThreshold = 55;     // minimum rarity score to take over (101 = never)
  int32_t minAltFt = -2000;   // ignore anything below (and anything on the ground)
  uint32_t minHoldMs = 20000; // keep a selection at least this long
  float switchRatio = 0.75f;  // a challenger must be this much closer to replace the current one
};

struct Selection {
  int index = -1;           // index into the aircraft array, -1 if nothing to show
  bool takeover = false;    // rare/emergency takeover
  Rarity rarity;
};

// Stateful picker with hysteresis so the screen does not flap between two
// aircraft at similar distances.
class Selector {
 public:
  explicit Selector(const SelectorConfig& cfg = SelectorConfig()) : cfg_(cfg) {}
  void setConfig(const SelectorConfig& cfg) { cfg_ = cfg; }

  // `rarities[i]` must be the precomputed scoreRarity() of `list[i]`.
  Selection select(const Aircraft* list, const Rarity* rarities, int count, uint32_t nowMs);
  void reset() { currentHex_[0] = 0; }

 private:
  SelectorConfig cfg_;
  char currentHex_[7] = {0};
  bool currentTakeover_ = false;
  uint32_t selectedAtMs_ = 0;
};

}  // namespace ac

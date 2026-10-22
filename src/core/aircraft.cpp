#include "aircraft.h"

#include <cstring>

namespace ac {

bool isEmergencySquawk(uint16_t s) { return s == 7500 || s == 7600 || s == 7700; }

Rarity scoreRarity(const Aircraft& a, int typeRarity, bool typeIsMilitary) {
  Rarity r;
  if (isEmergencySquawk(a.squawk) || a.emergencyFlag) {
    r.score = 100;
    r.reason = RareReason::Emergency;
    return r;
  }
  r.score = typeRarity;
  if (typeRarity > 0) r.reason = RareReason::RareType;
  if (a.military || typeIsMilitary) {
    // Military gets a floor of 65; a rare military type (B-2, V-22...) keeps its higher score.
    if (r.score < 65) {
      r.score = 65;
      r.reason = RareReason::Military;
    } else if (r.reason == RareReason::None) {
      r.reason = RareReason::Military;
    }
  }
  return r;
}

namespace {

bool eligible(const Aircraft& a, const SelectorConfig& c) {
  if (a.onGround) return false;
  if (a.altFt != kAltUnknown && a.altFt < c.minAltFt) return false;
  return a.seenPosS < 60;
}

bool qualifiesForTakeover(const Aircraft& a, const Rarity& r, const SelectorConfig& c) {
  if (r.score < c.rareThreshold) return false;
  // Emergencies may be on the ground (e.g. after landing) but must still be nearby.
  if (r.reason != RareReason::Emergency && !eligible(a, c)) return false;
  return a.distKm <= c.rareRangeKm;
}

}  // namespace

Selection Selector::select(const Aircraft* list, const Rarity* rar, int count, uint32_t nowMs) {
  Selection out;
  int currentIdx = -1;
  for (int i = 0; i < count; i++) {
    if (currentHex_[0] && std::strcmp(list[i].hex, currentHex_) == 0) currentIdx = i;
  }

  // 1) Best takeover candidate: highest score, then closest.
  int rareIdx = -1;
  for (int i = 0; i < count; i++) {
    if (!qualifiesForTakeover(list[i], rar[i], cfg_)) continue;
    if (rareIdx < 0 || rar[i].score > rar[rareIdx].score ||
        (rar[i].score == rar[rareIdx].score && list[i].distKm < list[rareIdx].distKm)) {
      rareIdx = i;
    }
  }

  // 2) Closest eligible aircraft inside the normal range.
  int closeIdx = -1;
  for (int i = 0; i < count; i++) {
    if (!eligible(list[i], cfg_) || list[i].distKm > cfg_.rangeKm) continue;
    if (closeIdx < 0 || list[i].distKm < list[closeIdx].distKm) closeIdx = i;
  }

  int pick;
  bool takeover;
  if (rareIdx >= 0) {
    // Keep showing the current takeover if it still qualifies and scores as high.
    if (currentTakeover_ && currentIdx >= 0 && qualifiesForTakeover(list[currentIdx], rar[currentIdx], cfg_) &&
        rar[currentIdx].score >= rar[rareIdx].score) {
      pick = currentIdx;
    } else {
      pick = rareIdx;
    }
    takeover = true;
  } else {
    pick = closeIdx;
    takeover = false;
    const bool currentStillOk = currentIdx >= 0 && !currentTakeover_ && eligible(list[currentIdx], cfg_) &&
                                list[currentIdx].distKm <= cfg_.rangeKm;
    if (currentStillOk && closeIdx >= 0 && closeIdx != currentIdx) {
      const bool held = nowMs - selectedAtMs_ < cfg_.minHoldMs;
      const bool muchCloser = list[closeIdx].distKm < list[currentIdx].distKm * cfg_.switchRatio;
      if (held || !muchCloser) pick = currentIdx;
    }
  }

  if (pick < 0) {
    currentHex_[0] = 0;
    currentTakeover_ = false;
    return out;
  }
  if (std::strcmp(list[pick].hex, currentHex_) != 0) {
    std::strncpy(currentHex_, list[pick].hex, sizeof(currentHex_) - 1);
    currentHex_[sizeof(currentHex_) - 1] = 0;
    selectedAtMs_ = nowMs;
  }
  currentTakeover_ = takeover;
  out.index = pick;
  out.takeover = takeover;
  out.rarity = rar[pick];
  return out;
}

}  // namespace ac

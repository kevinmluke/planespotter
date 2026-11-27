#include "timing.h"

namespace timing {

uint32_t jitter(uint32_t ms, uint32_t rnd, uint32_t pct) {
  const uint64_t spread = static_cast<uint64_t>(ms) * pct / 100;
  return static_cast<uint32_t>(ms - spread + rnd % (2 * spread + 1));
}

uint32_t parseRetryAfterS(const char* value) {
  if (!value) return 0;
  while (*value == ' ') value++;
  if (*value < '0' || *value > '9') return 0;
  uint32_t s = 0;
  for (; *value >= '0' && *value <= '9'; value++) {
    s = s * 10 + static_cast<uint32_t>(*value - '0');
    if (s > 86400) return 86400;
  }
  while (*value == ' ') value++;
  return *value ? 0 : s;
}

void Backoff::failure(uint32_t nowMs, uint32_t retryAfterS) {
  failures_++;
  const int shift = failures_ - 1 < 16 ? failures_ - 1 : 16;
  uint64_t wait = static_cast<uint64_t>(base_) << shift;
  if (wait > max_) wait = max_;
  if (static_cast<uint64_t>(retryAfterS) * 1000 > wait) wait = static_cast<uint64_t>(retryAfterS) * 1000;
  sinceMs_ = nowMs;
  waitMs_ = static_cast<uint32_t>(wait);
}

bool DailyBudget::take(int64_t nowUnix, uint32_t nowMs) {
  // Clock set (after 2023-11-14): UTC days. Otherwise uptime days, kept apart by the sign.
  const int64_t day = nowUnix > 1700000000 ? nowUnix / 86400 : -1 - static_cast<int64_t>(nowMs / 86400000UL);
  if (day != day_) {
    day_ = day;
    used_ = 0;
  }
  if (used_ >= max_) return false;
  used_++;
  return true;
}

bool tleFetchDue(bool haveTle, int64_t lastOkUnix, int64_t nowUnix, int64_t refreshS, bool tried, uint32_t lastTryMs,
                 uint32_t nowMs, uint32_t retryMs) {
  const bool stale = !haveTle || lastOkUnix <= 0 || nowUnix < lastOkUnix || nowUnix - lastOkUnix >= refreshS;
  return stale && (!tried || elapsed(nowMs, lastTryMs, retryMs));
}

}  // namespace timing

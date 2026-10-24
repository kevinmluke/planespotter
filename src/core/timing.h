// Timing helpers that survive the millis() wrap (every ~49.7 days), plus polite
// back-off and jitter for the free web APIs.
// Pure C++ (no Arduino), unit-tested on the host.
#pragma once

#include <cstdint>

namespace timing {

// True once `intervalMs` has passed since `sinceMs`. Correct across the millis() wrap
// as long as the interval is below ~24.8 days.
inline bool elapsed(uint32_t nowMs, uint32_t sinceMs, uint32_t intervalMs) { return nowMs - sinceMs >= intervalMs; }

// `ms` spread by +/- `pct` percent, picked by `rnd` (e.g. esp_random()), so a fleet of
// displays doesn't hit a service in lockstep.
uint32_t jitter(uint32_t ms, uint32_t rnd, uint32_t pct = 20);

// Seconds from a Retry-After header, or 0 if absent or not a plain number of seconds
// (the HTTP-date form is ignored). Capped at one day.
uint32_t parseRetryAfterS(const char* value);

// Exponential back-off for one remote service: after n failures in a row wait
// base * 2^(n-1), capped at `maxMs`, or longer if the server said so (Retry-After).
class Backoff {
 public:
  Backoff(uint32_t baseMs, uint32_t maxMs) : base_(baseMs), max_(maxMs) {}
  bool ready(uint32_t nowMs) const { return failures_ == 0 || elapsed(nowMs, sinceMs_, waitMs_); }
  void success() { failures_ = 0; }
  void failure(uint32_t nowMs, uint32_t retryAfterS = 0);
  int failures() const { return failures_; }
  uint32_t waitMs() const { return failures_ ? waitMs_ : 0; }

 private:
  uint32_t base_, max_;
  uint32_t sinceMs_ = 0, waitMs_ = 0;
  int failures_ = 0;
};

// Whether to download fresh ISS orbital elements: none cached, the last successful
// download (unix time, 0 = unknown) is `refreshS` old, or the clock went backwards.
// After a failed attempt (`lastTryMs`, if `tried`), wait `retryMs` before the next one.
bool tleFetchDue(bool haveTle, int64_t lastOkUnix, int64_t nowUnix, int64_t refreshS, bool tried, uint32_t lastTryMs,
                 uint32_t nowMs, uint32_t retryMs);

}  // namespace timing

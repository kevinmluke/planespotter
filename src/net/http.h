// HTTPS helpers. Every request verifies the server against the Mozilla root
// bundle that ships with the ESP32 core. Any successful request also counts as the
// health signal for freshly installed firmware (see ota::reportHealthy()).
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace http {

struct Result {
  int status = 0;            // HTTP status, or a negative HTTPClient error
  uint32_t retryAfterS = 0;  // from a Retry-After header (seconds), 0 if none
  bool ok() const { return status >= 200 && status < 300; }
  // Network trouble, rate limited or a server error: back off before asking again.
  bool retryable() const { return status < 0 || status == 429 || status >= 500; }
};

// GET + streamed JSON parse with an ArduinoJson filter (keeps RAM small for big responses).
Result getJson(const char* url, JsonDocument& doc, const JsonDocument& filter, uint32_t timeoutMs = 12000);
// GET into a String (small text payloads only).
Result getText(const char* url, String& out, size_t maxLen = 4096, uint32_t timeoutMs = 12000);

const char* describe(int status);  // short human-readable error

}  // namespace http

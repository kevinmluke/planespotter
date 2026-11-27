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

// Error codes beyond HTTPClient's own (all negative, so retryable()).
constexpr int kErrJson = -200;      // -200 - DeserializationError::Code (-201..-205)
constexpr int kErrTooBig = -212;    // reply larger than the download buffer
constexpr int kErrTruncated = -213; // fewer bytes than Content-Length announced
constexpr int kErrSlow = -214;      // reply did not arrive within the timeout

// JSON documents allocated here live in PSRAM, leaving internal RAM for TLS and Wi-Fi.
ArduinoJson::Allocator* psram();

// GET over HTTP/1.1 (chunked or not), download the whole reply into PSRAM, check it is
// complete, then parse it with an ArduinoJson filter.
Result getJson(const char* url, JsonDocument& doc, const JsonDocument& filter, uint32_t timeoutMs = 12000);
// GET into a String (small text payloads only).
Result getText(const char* url, String& out, size_t maxLen = 4096, uint32_t timeoutMs = 12000);

const char* describe(int status);  // short human-readable error

}  // namespace http

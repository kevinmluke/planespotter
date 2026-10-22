#include "http.h"

#include <HTTPClient.h>
#include <NetworkClientSecure.h>

#include "config.h"

namespace http {
namespace {

const char* kUserAgent = "PlaneSpotter/" FW_VERSION " (ESP32 e-paper desk display)";

bool begin(HTTPClient& h, NetworkClientSecure& c, const char* url, uint32_t timeoutMs) {
  c.useBuiltinCACertBundle();
  c.setTimeout(timeoutMs / 1000 + 1);
  h.setReuse(false);
  h.setTimeout(timeoutMs);
  h.setConnectTimeout(8000);
  h.useHTTP10(true);  // no chunked encoding: lets ArduinoJson read the raw stream
  h.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!h.begin(c, url)) return false;
  h.setUserAgent(kUserAgent);
  h.addHeader("Accept", "application/json, text/plain");
  return true;
}

}  // namespace

Result getJson(const char* url, JsonDocument& doc, const JsonDocument& filter, uint32_t timeoutMs) {
  Result r;
  NetworkClientSecure client;
  HTTPClient h;
  if (!begin(h, client, url, timeoutMs)) {
    r.status = -100;
    return r;
  }
  r.status = h.GET();
  if (r.ok()) {
    const DeserializationError err =
        deserializeJson(doc, h.getStream(), DeserializationOption::Filter(filter), DeserializationOption::NestingLimit(12));
    if (err) {
      log_w("JSON error %s for %s", err.c_str(), url);
      r.status = -200;
    }
  } else {
    log_w("HTTP %d for %s", r.status, url);
  }
  h.end();
  return r;
}

Result getText(const char* url, String& out, size_t maxLen, uint32_t timeoutMs) {
  Result r;
  NetworkClientSecure client;
  HTTPClient h;
  if (!begin(h, client, url, timeoutMs)) {
    r.status = -100;
    return r;
  }
  r.status = h.GET();
  if (r.ok()) {
    out = "";
    NetworkClient* s = h.getStreamPtr();
    const uint32_t start = millis();
    while ((h.connected() || s->available()) && out.length() < maxLen && millis() - start < timeoutMs) {
      while (s->available() && out.length() < maxLen) out += static_cast<char>(s->read());
      delay(2);
    }
  }
  h.end();
  return r;
}

const char* describe(int status) {
  switch (status) {
    case -100: return "bad URL";
    case -200: return "bad data";
    case HTTPC_ERROR_CONNECTION_REFUSED: return "connection refused";
    case HTTPC_ERROR_READ_TIMEOUT: return "timeout";
    case HTTPC_ERROR_CONNECTION_LOST: return "connection lost";
    case 429: return "rate limited";
    default: break;
  }
  if (status < 0) return "network error";
  static char buf[16];
  snprintf(buf, sizeof(buf), "HTTP %d", status);
  return buf;
}

}  // namespace http

#include "http.h"

#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <esp_heap_caps.h>

#include "../core/timing.h"
#include "config.h"
#include "ota.h"

namespace http {
namespace {

const char* kUserAgent = "PlaneSpotter/" FW_VERSION " (ESP32 e-paper desk display)";
constexpr size_t kMaxBody = 1536 * 1024;  // 250 NM around a busy hub is ~1 MB

void* psAlloc(size_t n) {
  void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : malloc(n);  // no PSRAM: fall back to internal RAM
}
void* psRealloc(void* p, size_t n) {
  void* q = heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return q ? q : realloc(p, n);
}

struct PsramAllocator : ArduinoJson::Allocator {
  void* allocate(size_t n) override { return psAlloc(n); }
  void deallocate(void* p) override { free(p); }
  void* reallocate(void* p, size_t n) override { return psRealloc(p, n); }
};

// Reply body collected in PSRAM, capped at kMaxBody.
class Body {
 public:
  ~Body() { free(buf_); }
  bool append(const uint8_t* d, size_t n) {
    if (len_ + n > kMaxBody) return false;
    if (len_ + n > cap_) {
      size_t cap = cap_ ? cap_ : 32 * 1024;
      while (cap < len_ + n) cap *= 2;
      if (cap > kMaxBody) cap = kMaxBody;
      uint8_t* b = static_cast<uint8_t*>(psRealloc(buf_, cap));
      if (!b) return false;
      buf_ = b;
      cap_ = cap;
    }
    memcpy(buf_ + len_, d, n);
    len_ += n;
    return true;
  }
  const uint8_t* data() const { return buf_; }
  size_t size() const { return len_; }

 private:
  uint8_t* buf_ = nullptr;
  size_t len_ = 0, cap_ = 0;
};

// Reads the reply body (plain or chunked) with a hard deadline: a server that stalls
// without closing the connection can't hang the network task.
class Reader {
 public:
  Reader(NetworkClient* s, uint32_t timeoutMs) : s_(s), start_(millis()), timeout_(timeoutMs) {}

  // `want` bytes into `b`, or everything until the server closes if want < 0.
  int copy(Body& b, long want) {
    uint8_t tmp[1024];
    while (want != 0) {
      if (expired()) return kErrSlow;
      const int av = s_->available();
      if (av <= 0) {
        if (!s_->connected()) return want > 0 ? kErrTruncated : 0;
        delay(2);
        continue;
      }
      size_t n = static_cast<size_t>(av) < sizeof(tmp) ? static_cast<size_t>(av) : sizeof(tmp);
      if (want > 0 && n > static_cast<size_t>(want)) n = static_cast<size_t>(want);
      const int got = s_->read(tmp, n);
      if (got <= 0) continue;
      if (!b.append(tmp, got)) return kErrTooBig;
      if (want > 0) want -= got;
    }
    return 0;
  }

  // One CRLF-terminated line (without the CRLF), at most `max` characters.
  int line(char* out, size_t max) {
    size_t n = 0;
    while (true) {
      if (expired()) return kErrSlow;
      if (s_->available() <= 0) {
        if (!s_->connected()) return kErrTruncated;
        delay(2);
        continue;
      }
      const int c = s_->read();
      if (c < 0) continue;
      if (c == '\n') break;
      if (c != '\r' && n + 1 < max) out[n++] = static_cast<char>(c);
    }
    out[n] = 0;
    return 0;
  }

  int chunked(Body& b) {
    char hdr[24];
    while (true) {
      if (int e = line(hdr, sizeof(hdr))) return e;
      char* end = nullptr;
      const long len = strtol(hdr, &end, 16);
      if (end == hdr || len < 0) return kErrJson - 3;  // not a chunk header
      if (len == 0) return 0;                          // last chunk (trailers ignored)
      if (int e = copy(b, len)) return e;
      if (int e = line(hdr, sizeof(hdr))) return e;    // CRLF after the chunk data
    }
  }

 private:
  bool expired() const { return millis() - start_ > timeout_; }
  NetworkClient* s_;
  uint32_t start_, timeout_;
};

// Downloads the reply body; returns 0 or a negative error code.
int download(HTTPClient& h, Body& b, uint32_t timeoutMs) {
  NetworkClient* s = h.getStreamPtr();
  if (!s) return HTTPC_ERROR_CONNECTION_LOST;
  Reader rd(s, timeoutMs);
  if (h.header("Transfer-Encoding").equalsIgnoreCase("chunked")) return rd.chunked(b);
  const int expected = h.getSize();  // -1 when the server just closes at the end
  return rd.copy(b, expected > 0 ? expected : -1);
}

bool begin(HTTPClient& h, NetworkClientSecure& c, const char* url, uint32_t timeoutMs) {
  c.useBuiltinCACertBundle();
  c.setTimeout(timeoutMs);  // milliseconds in arduino-esp32 3.x
  h.setReuse(false);
  h.setTimeout(timeoutMs);
  h.setConnectTimeout(8000);
  h.useHTTP10(false);  // some APIs now refuse HTTP/1.0 (426); download() handles chunked replies
  h.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!h.begin(c, url)) return false;
  h.setUserAgent(kUserAgent);
  h.addHeader("Accept", "application/json, text/plain");
  static const char* kCollect[] = {"Retry-After", "Transfer-Encoding"};
  h.collectHeaders(kCollect, 2);
  return true;
}

void finish(HTTPClient& h, Result& r) {
  if (r.ok()) ota::reportHealthy();
  if (r.status > 0) r.retryAfterS = timing::parseRetryAfterS(h.header("Retry-After").c_str());
  h.end();
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
    Body body;
    const int e = download(h, body, timeoutMs);
    if (e) {
      log_w("download error %d (%u bytes) for %s", e, static_cast<unsigned>(body.size()), url);
      r.status = e;
    } else {
      // const input: strings are copied into the document (PSRAM when it uses psram())
      const DeserializationError err = deserializeJson(doc, body.data(), body.size(), DeserializationOption::Filter(filter),
                                                       DeserializationOption::NestingLimit(12));
      if (err) {
        log_w("JSON error %s (%u bytes) for %s", err.c_str(), static_cast<unsigned>(body.size()), url);
        r.status = kErrJson - static_cast<int>(err.code());
      }
    }
  } else {
    log_w("HTTP %d for %s", r.status, url);
  }
  finish(h, r);
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
    Body body;
    const int e = download(h, body, timeoutMs);
    if (e) {
      r.status = e;
    } else {
      const size_t n = body.size() < maxLen ? body.size() : maxLen;
      out = "";
      out.reserve(n);
      for (size_t i = 0; i < n; i++) out += static_cast<char>(body.data()[i]);
    }
  }
  finish(h, r);
  return r;
}

ArduinoJson::Allocator* psram() {
  static PsramAllocator a;
  return &a;
}

const char* describe(int status) {
  switch (status) {
    case -100: return "bad URL";
    case kErrJson - 1: return "bad data: empty reply";       // EmptyInput
    case kErrJson - 2: return "bad data: cut off";           // IncompleteInput
    case kErrJson - 3: return "bad data: not JSON";          // InvalidInput
    case kErrJson - 4: return "bad data: out of memory";     // NoMemory
    case kErrJson - 5: return "bad data: too deep";          // TooDeep
    case kErrTooBig: return "reply too large";
    case kErrTruncated: return "download cut off";
    case kErrSlow: return "download too slow";
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

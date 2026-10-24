#include "ota.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Preferences.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <mbedtls/base64.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>

#include "config.h"
#include "http.h"
#include "ota_pubkey.h"

// Tell the Arduino core not to validate a freshly updated image on its own:
// we only mark it valid once it has proven it works (see reportHealthy()).
extern "C" bool verifyRollbackLater() { return true; }

namespace ota {
namespace {

// A version that failed its health check is skipped for this long, then tried again
// (the failure may have been the network rather than the firmware).
constexpr int64_t kBadVersionRetryS = 7 * 86400;

Progress gProgress;
bool gPending = false;
bool gValidated = false;
uint32_t gBootMs = 0;

void setPhase(Phase p, void (*cb)(const Progress&), int percent = -1) {
  gProgress.phase = p;
  if (percent >= 0) gProgress.percent = percent;
  if (cb) cb(gProgress);
}

bool fail(const char* why, void (*cb)(const Progress&)) {
  log_e("OTA failed: %s", why);
  snprintf(gProgress.error, sizeof(gProgress.error), "%s", why);
  setPhase(Phase::Failed, cb);
  return false;
}

void toHex(const uint8_t* in, size_t n, char* out) {
  static const char* h = "0123456789abcdef";
  for (size_t i = 0; i < n; i++) {
    out[2 * i] = h[in[i] >> 4];
    out[2 * i + 1] = h[in[i] & 15];
  }
  out[2 * n] = 0;
}

bool verifySignature(const String& message, const char* sigB64) {
  uint8_t sig[128];
  size_t sigLen = 0;
  if (mbedtls_base64_decode(sig, sizeof(sig), &sigLen, reinterpret_cast<const uint8_t*>(sigB64), strlen(sigB64)) != 0)
    return false;
  uint8_t hash[32];
  mbedtls_sha256(reinterpret_cast<const uint8_t*>(message.c_str()), message.length(), hash, 0);
  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);
  static const char kPem[] = OTA_PUBKEY_PEM;
  bool ok = mbedtls_pk_parse_public_key(&pk, reinterpret_cast<const uint8_t*>(kPem), sizeof(kPem)) == 0 &&
            mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, sizeof(hash), sig, sigLen) == 0;
  mbedtls_pk_free(&pk);
  return ok;
}

}  // namespace

int compareVersions(const char* a, const char* b) {
  int va[3] = {0, 0, 0}, vb[3] = {0, 0, 0};
  sscanf(a, "%d.%d.%d", &va[0], &va[1], &va[2]);
  sscanf(b, "%d.%d.%d", &vb[0], &vb[1], &vb[2]);
  for (int i = 0; i < 3; i++)
    if (va[i] != vb[i]) return va[i] < vb[i] ? -1 : 1;
  return 0;
}

void begin() {
  gBootMs = millis();
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  if (esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY) {
    gPending = true;
    log_i("Running a new image in trial mode; waiting for a health signal");
  }
}

bool pendingVerify() { return gPending && !gValidated; }

void reportHealthy() {
  if (!gPending || gValidated) return;
  if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
    gValidated = true;
    log_i("Firmware " FW_VERSION " marked valid");
  }
}

void tick() {
  if (pendingVerify() && millis() - gBootMs > OTA_HEALTH_DEADLINE_MS) {
    log_e("New firmware did not become healthy in time: rolling back");
    esp_ota_mark_app_invalid_rollback_and_reboot();  // does not return on success
  }
}

bool takeBootMessage(char* version, size_t n, bool& rolledBack) {
  Preferences p;
  p.begin("cache", false);
  String pending = p.getString("otaTarget", "");
  if (!pending.length()) {
    p.end();
    return false;
  }
  p.remove("otaTarget");
  rolledBack = pending != FW_VERSION;
  // Skip a version that failed its health check for a while; a newer release clears this.
  // The clock isn't set yet at boot: checkAndInstall() records when the skip started.
  if (rolledBack) {
    p.putString("otaBad", pending);
    p.remove("otaBadAt");
  }
  p.end();
  snprintf(version, n, "%s", FW_VERSION);
  return true;
}

const Progress& progress() { return gProgress; }

bool checkAndInstall(const char* manifestUrl, void (*cb)(const Progress&)) {
  gProgress = Progress();
  setPhase(Phase::Checking, nullptr, 0);

  JsonDocument filter;
  for (const char* k : {"version", "url", "size", "sha256", "signature"}) filter[k] = true;
  JsonDocument m;
  http::Result r = http::getJson(manifestUrl, m, filter, 15000);
  if (!r.ok()) return fail(http::describe(r.status), nullptr);

  const char* version = m["version"] | "";
  const char* url = m["url"] | "";
  const uint32_t size = m["size"] | 0;
  const char* sha = m["sha256"] | "";
  const char* sig = m["signature"] | "";
  if (!*version || !*url || !size || strlen(sha) != 64 || !*sig) return fail("incomplete manifest", nullptr);

  String message = String("planespotter-fw\n") + version + "\n" + size + "\n" + sha + "\n" + url + "\n";
  if (!verifySignature(message, sig)) return fail("bad signature", nullptr);
  reportHealthy();  // a verified manifest proves networking, TLS and crypto work
  if (pendingVerify()) return false;  // never stack updates on an unproven image
  if (compareVersions(version, FW_VERSION) <= 0) {
    setPhase(Phase::UpToDate, nullptr);
    return false;
  }
  {
    Preferences p;
    p.begin("cache", false);
    const String bad = p.getString("otaBad", "");
    int64_t badAt = p.getLong64("otaBadAt", 0);
    const int64_t now = time(nullptr);
    const bool clockSet = now > 1700000000;
    if (bad == version && !badAt && clockSet) p.putLong64("otaBadAt", badAt = now);
    p.end();
    if (bad == version && (!clockSet || !badAt || now < badAt || now - badAt < kBadVersionRetryS))
      return fail("version previously rolled back", nullptr);
  }
  snprintf(gProgress.version, sizeof(gProgress.version), "%s", version);
  log_i("Installing %s (%u bytes)", version, size);

  // ---- Download into the inactive slot while hashing
  setPhase(Phase::Downloading, cb, 0);
  NetworkClientSecure client;
  client.useBuiltinCACertBundle();
  HTTPClient h;
  h.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  h.setTimeout(20000);
  if (!h.begin(client, url)) return fail("bad firmware URL", cb);
  const int code = h.GET();
  if (code != 200) {
    h.end();
    return fail(http::describe(code), cb);
  }
  const int len = h.getSize();
  if (len > 0 && static_cast<uint32_t>(len) != size) {
    h.end();
    return fail("size mismatch", cb);
  }
  if (!Update.begin(size, U_FLASH)) {
    h.end();
    return fail("no room for update", cb);
  }
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  NetworkClient* stream = h.getStreamPtr();
  static uint8_t buf[4096];
  uint32_t got = 0;
  uint32_t lastData = millis();
  int lastReported = -1;
  while (got < size) {
    const size_t avail = stream->available();
    if (!avail) {
      if (!h.connected() || millis() - lastData > 20000) break;
      delay(5);
      continue;
    }
    const size_t n = stream->readBytes(buf, std::min(avail, std::min(sizeof(buf), static_cast<size_t>(size - got))));
    if (Update.write(buf, n) != n) break;
    mbedtls_sha256_update(&ctx, buf, n);
    got += n;
    lastData = millis();
    const int pct = static_cast<int>(100ULL * got / size);
    if (pct / 10 != lastReported / 10) {
      lastReported = pct;
      setPhase(Phase::Downloading, cb, pct);
    }
  }
  h.end();
  uint8_t digest[32];
  mbedtls_sha256_finish(&ctx, digest);
  mbedtls_sha256_free(&ctx);
  if (got != size) {
    Update.abort();
    return fail("download interrupted", cb);
  }

  setPhase(Phase::Verifying, cb, 100);
  char hex[65];
  toHex(digest, 32, hex);
  if (strcasecmp(hex, sha) != 0) {
    Update.abort();
    return fail("hash mismatch", cb);
  }
  if (!Update.end()) {  // also runs the ESP-IDF image check and switches the boot slot
    return fail(Update.errorString(), cb);
  }
  Preferences p;
  p.begin("cache", false);
  p.putString("otaTarget", version);
  p.end();
  setPhase(Phase::Rebooting, cb, 100);
  delay(500);
  ESP.restart();
  return true;
}

}  // namespace ota

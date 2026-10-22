// Signed over-the-air updates with automatic rollback.
//
// Flow:  manifest.json (version, url, size, sha256, ECDSA-P256 signature)
//   -> signature checked against the public key compiled into this firmware
//   -> version must be newer than the running one (no downgrades / replays)
//   -> image streamed into the inactive OTA slot while hashing
//   -> hash must match the signed one, otherwise the slot is discarded
//   -> reboot into the new image in "pending verify" state
//   -> the new image must reach the flight API within OTA_HEALTH_DEADLINE_MS,
//      otherwise (or if it crashes/reboots first) the bootloader rolls back.
#pragma once

#include <Arduino.h>

namespace ota {

enum class Phase : uint8_t { Idle, Checking, Downloading, Verifying, Rebooting, Failed, UpToDate };

struct Progress {
  Phase phase = Phase::Idle;
  int percent = 0;
  char version[16] = "";
  char error[48] = "";
};

// Call once at boot. Detects a pending-verify image and the outcome of a previous update.
void begin();

// True while the running image still has to prove itself.
bool pendingVerify();
// Health signal: call when the firmware did something useful (e.g. fetched aircraft).
void reportHealthy();
// Call regularly: rolls back if the health deadline passed.
void tick();

// Message to show once after boot: "Updated to X" or "Update failed, back on X". Empty if none.
// `rolledBack` tells which one it was.
bool takeBootMessage(char* version, size_t n, bool& rolledBack);

// Checks the manifest and installs if newer. Blocks for the duration of the download.
// `onProgress` is called with updated progress (may redraw the screen).
bool checkAndInstall(const char* manifestUrl, void (*onProgress)(const Progress&));

const Progress& progress();

// Semantic version comparison: <0, 0, >0
int compareVersions(const char* a, const char* b);

}  // namespace ota

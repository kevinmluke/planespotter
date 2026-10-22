// View models: everything a screen needs, already resolved to plain values.
// The renderer only reads these, so screens can be rendered on the host for previews.
#pragma once

#include <cstdint>

#include "../core/aircraft.h"
#include "../core/i18n.h"
#include "../core/iss.h"
#include "../core/text.h"

namespace ui {

using i18n::Lang;
using text::Units;

enum class Freshness : uint8_t { Live, Delayed, Offline };
enum class Mode : uint8_t { Auto = 0, Aircraft = 1, Iss = 2 };

struct Status {
  Freshness freshness = Freshness::Live;
  int delaySec = 0;
  char clock[8] = "--:--";   // local time HH:MM
  int aircraftCount = -1;    // -1: don't show
  Mode mode = Mode::Auto;
  bool wifiOk = true;
};

struct Common {
  bool dark = false;  // dark theme: white on black
  Lang lang = Lang::EN;
  Units units = Units::Aviation;
  Status status;
};

struct AircraftView {
  char flight[12] = "";        // "AF1234" (IATA) or callsign
  char callsign[9] = "";
  char airline[40] = "";
  char model[48] = "";
  char reg[12] = "";
  char typeCode[5] = "";
  bool hasRoute = false;
  char originCity[32] = "", originCode[5] = "";
  char destCity[32] = "", destCode[5] = "";
  float routeProgress = -1;    // 0..1 along the route, < 0 unknown
  int32_t altFt = ac::kAltUnknown;
  bool onGround = false;
  int vrateFpm = 0;
  float gsKt = -1;
  float trackDeg = -1;
  float distKm = 0;
  float bearingDeg = 0;        // from home
  int family = 0;
  bool takeover = false;
  ac::RareReason reason = ac::RareReason::None;
  uint16_t squawk = 0;
  bool emergencyFlag = false;
};

struct Blip {
  float distKm, bearingDeg, trackDeg;
  bool selected;
  bool rare;
};

struct AircraftScreen {
  Common c;
  AircraftView ac;
  static constexpr int kMaxBlips = 48;
  Blip blips[kMaxBlips];
  int blipCount = 0;
  float rangeKm = 30;
};

struct EmptySkyScreen {
  Common c;
  float rangeKm = 30;
  Blip blips[AircraftScreen::kMaxBlips];
  int blipCount = 0;
  // Optional ISS teaser
  bool hasPass = false;
  iss::Pass nextPass;
  int64_t now = 0;
  int tzOffsetS = 0;
};

struct IssMapScreen {
  Common c;
  iss::Position pos{};
  float homeLat = 0, homeLon = 0;
  float distanceKm = 0;  // great-circle distance from home to the sub-satellite point
  static constexpr int kTrackPts = 120;
  float trackLat[kTrackPts], trackLon[kTrackPts];
  int trackCount = 0;
  int trackNowIndex = 0;  // points before this are in the past
  float sunLat = 0, sunLon = 0;
  bool hasPass = false;
  iss::Pass nextPass;
  int64_t now = 0;
  int tzOffsetS = 0;     // local time offset for formatting pass times
  int tleAgeDays = 0;
  bool overLand = false;
};

struct IssAlertScreen {
  Common c;
  iss::Pass pass;
  int64_t now = 0;
  int tzOffsetS = 0;
  bool inProgress = false;   // the visible part has started
  iss::SkyPoint current{};   // live sky position while in progress
};

enum class JoinResult : uint8_t { None, Connecting, Ok, WrongPassword, NotFound, Captive, NoInternet, Failed };

// First-time setup and later settings: both run over the display's own Wi-Fi network.
struct SetupScreen {
  Common c;
  bool settingsMode = false;   // false: first-time setup (shown in both languages)
  char apSsid[33] = "";
  char apPassword[17] = "";
  char qrPayload[160] = "";    // WIFI:T:WPA;S:...;P:...;;
  JoinResult join = JoinResult::None;
  char joinSsid[33] = "";
};

enum class OfflineReason : uint8_t { Wifi, Api, Time, NoLocation };

struct OfflineScreen {
  Common c;
  OfflineReason reason = OfflineReason::Wifi;
  char lastUpdate[32] = "";  // already formatted ("18:42", "never")
  char detail[48] = "";      // e.g. network name or HTTP status
};

struct MessageScreen {
  Common c;
  char title[48] = "";
  char line1[96] = "";
  char line2[96] = "";
  int progress = -1;  // 0..100 draws a progress bar
};

}  // namespace ui

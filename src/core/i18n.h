// English / French UI strings. Pure C++ (no Arduino).
#pragma once

#include <cstdint>

namespace i18n {

enum class Lang : uint8_t { EN = 0, FR = 1 };

enum class S : uint16_t {
  // Status bar
  Live, Delayed, Offline, AircraftCount,
  // Aircraft screen
  Altitude, Speed, Heading, Distance, RouteUnknown, OnGround, Reg,
  QuietSkies, NoAircraftWithin,
  // Takeover banners
  RareAircraft, Military, Emergency, Sq7500, Sq7600, Sq7700, DeclaredEmergency,
  // ISS
  Iss, IssAltitude, IssSpeed, InSunlight, InShadow, NextVisiblePass, NoVisiblePass,
  Today, Tomorrow, MaxElevation, OrbitAge, ComputedOnDevice, Over, Duration,
  // ISS alert
  IssVisibleIn, IssNow, LookDir, AppearsIn, RisesTo, DisappearsIn, VisibleFor, HighNow,
  WaitingForDark,
  // Setup / settings (served over the display's own Wi-Fi)
  SetupTitle, SettingsTitle, ScanWithCamera, SetupThen, SettingsThen, NoCamera, CloseHint,
  Connecting, ConnectedTo, FailPassword, FailNotFound, FailCaptive, FailNoInternet, FailGeneric, HoldForWifi,
  // Offline
  OfflineTitle, ReasonWifi, ReasonApi, ReasonTime, ReasonNoLocation, LastUpdate, Never,
  Retrying, NotLive,
  // Updates
  Updating, DoNotUnplug, UpdatedTo, UpdateRolledBack,
  // Modes & button
  ModeAuto, ModeAircraft, ModeIss, HoldToReset, ResetDone,
  Count_
};

const char* tr(Lang lang, S id);

// Compass point names. `index16` 0 = N ... 15 = NNW.
const char* compass16(Lang lang, int index16);
// Sentence form of an 8-point direction, e.g. "north-west" / "nord-ouest".
const char* direction8(Lang lang, int index8);
// Preposition used before direction8: "in the " / "au " / "à l'".
const char* directionPrefix(Lang lang, int index8);

}  // namespace i18n

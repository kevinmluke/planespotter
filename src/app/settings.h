// Persistent user settings (NVS).
#pragma once

#include <Arduino.h>

#include "../core/i18n.h"
#include "../core/text.h"
#include "../ui/model.h"

struct Settings {
  // Wi-Fi
  char ssid[33] = "";
  char password[65] = "";
  // Location
  bool hasLocation = false;
  double lat = 0, lon = 0;
  float altM = 0;
  char placeName[48] = "";
  char tz[64] = "UTC0";  // POSIX TZ string
  // Display
  i18n::Lang lang = i18n::Lang::EN;
  text::Units units = text::Units::Imperial;
  bool clock24 = false;       // false: 12-hour clock (9:41 PM)
  ui::Mode mode = ui::Mode::Auto;
  bool darkTheme = false;     // white on black
  uint16_t rangeKm = 30;      // closest-aircraft radius
  uint16_t rareRangeKm = 80;  // rare takeover radius
  uint8_t rareLevel = 2;      // 0 off, 1 only very rare, 2 normal, 3 everything unusual
  bool issAlerts = true;
  // Updates
  bool autoUpdate = true;

  int rareThreshold() const;
};

namespace settings {

void load(Settings& s);
void save(const Settings& s);
void eraseAll();

}  // namespace settings

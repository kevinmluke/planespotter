// Hooks the setup page uses to talk to the application (main loop thread only).
#pragma once

#include <Arduino.h>

#include "../ui/model.h"
#include "settings.h"

namespace app {

struct WifiRequest {
  char ssid[33] = "";
  char pass[65] = "";
  char user[64] = "";  // non-empty: WPA2-Enterprise (PEAP)
};

const Settings& settings();
// True when the page was opened from a long press (settings), false for first-time setup.
bool settingsMode();
// Applies display options and location right away. With `wifi`, also starts joining that
// network; the outcome is reported through joinResult().
void submit(const Settings& s, const WifiRequest* wifi);
ui::JoinResult joinResult();
const char* joinSsid();
const char* connectedSsid();  // "" when not connected
void requestOtaCheck();
void closeSettings();

}  // namespace app

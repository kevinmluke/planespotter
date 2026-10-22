// The display's own Wi-Fi network (with captive DNS) and the setup/settings page.
// Used for the first-time setup and whenever the button is held for 3 seconds,
// so it works on any home or office network: the phone talks to the display directly.
#pragma once

#include <Arduino.h>

namespace portal {

void startAccessPoint(const char* ssid, const char* password);
void stopAccessPoint();
bool accessPointActive();
// Milliseconds since the page last talked to the display (for the settings timeout).
uint32_t idleMs();

void loop();

}  // namespace portal

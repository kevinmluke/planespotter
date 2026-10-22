// Screen renderers. Draw into any Adafruit_GFX target (the e-paper buffer on
// the device, a GFXcanvas1 on the host for previews). 792 x 272, 1-bit.
#pragma once

#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>

#include "model.h"

namespace ui {

constexpr int kW = 792;
constexpr int kH = 272;
constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xFFFF;

struct Canvas {
  Adafruit_GFX& g;
  U8G2_FOR_ADAFRUIT_GFX& u8;
};

void drawAircraft(Canvas& cv, const AircraftScreen& s);
void drawEmptySky(Canvas& cv, const EmptySkyScreen& s);
void drawIssMap(Canvas& cv, const IssMapScreen& s);
void drawIssAlert(Canvas& cv, const IssAlertScreen& s);
void drawSetup(Canvas& cv, const SetupScreen& s);
void drawOffline(Canvas& cv, const OfflineScreen& s);
void drawMessage(Canvas& cv, const MessageScreen& s);

}  // namespace ui

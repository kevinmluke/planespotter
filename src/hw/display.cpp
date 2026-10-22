#include "display.h"

#include <GxEPD2_BW.h>
#include <SPI.h>

#include "config.h"

namespace display {
namespace {

GxEPD2_BW<GxEPD2_579_GDEY0579T93, GxEPD2_579_GDEY0579T93::HEIGHT> gEpd(
    GxEPD2_579_GDEY0579T93(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
U8G2_FOR_ADAFRUIT_GFX gU8;
SPIClass gSpi(FSPI);

Kind gLastKind = Kind::None;
uint32_t gLastHash = 0;
uint32_t gLastRefreshMs = 0;
uint32_t gLastFullMs = 0;
int gPartials = 0;

}  // namespace

void begin() {
  gSpi.begin(PIN_EPD_SCK, -1, PIN_EPD_MOSI, PIN_EPD_CS);
  gEpd.epd2.selectSPI(gSpi, SPISettings(10000000, MSBFIRST, SPI_MODE0));
  gEpd.init(0, true, 2, false);  // no serial diag, initial full refresh, reset pulse 2 ms
  gEpd.setRotation(0);
  gEpd.setFullWindow();
  gU8.begin(gEpd);
}

uint32_t hash(const void* data, size_t len, uint32_t h) {
  const uint8_t* p = static_cast<const uint8_t*>(data);
  for (size_t i = 0; i < len; i++) {
    h ^= p[i];
    h *= 16777619u;
  }
  return h;
}

bool show(Kind kind, uint32_t contentHash, void (*draw)(ui::Canvas&, const void*), const void* model, bool forceFull) {
  if (kind == gLastKind && contentHash == gLastHash && !forceFull) return false;
  const uint32_t now = millis();
  const bool full = forceFull || kind != gLastKind || gPartials >= FULL_REFRESH_EVERY ||
                    now - gLastFullMs > FULL_REFRESH_MAX_MS || gLastKind == Kind::None;

  ui::Canvas cv{gEpd, gU8};
  gEpd.setFullWindow();
  draw(cv, model);
  gEpd.display(!full);  // writes the buffer, then a full or partial refresh
  gEpd.powerOff();      // keep panel RAM (needed for the next partial) but cut the high voltage

  if (full) {
    gPartials = 0;
    gLastFullMs = now;
  } else {
    gPartials++;
  }
  gLastKind = kind;
  gLastHash = contentHash;
  gLastRefreshMs = now;
  return true;
}

uint32_t lastRefreshMs() { return gLastRefreshMs; }
Kind lastKind() { return gLastKind; }

}  // namespace display

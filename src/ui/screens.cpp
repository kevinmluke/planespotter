#include "screens.h"

#include <qrcode.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "../core/geo.h"
#include "../generated/aircraft_db.h"

namespace ui {

using i18n::S;
using i18n::tr;

namespace {

// ---------------------------------------------------------------------------
// Fonts (U8g2, Latin-1 coverage for French)

const uint8_t* const F_LABEL = u8g2_font_helvB10_tf;   // small caps labels
const uint8_t* const F_TINY = u8g2_font_helvR10_tf;
const uint8_t* const F_SMALL = u8g2_font_fur11_tf;
const uint8_t* const F_BODY = u8g2_font_fur14_tf;
const uint8_t* const F_BODY_B = u8g2_font_fub14_tf;
const uint8_t* const F_MED = u8g2_font_fur17_tf;
const uint8_t* const F_MED_B = u8g2_font_fub17_tf;
const uint8_t* const F_VALUE = u8g2_font_fub20_tf;
const uint8_t* const F_BIG = u8g2_font_fub25_tf;
const uint8_t* const F_HUGE = u8g2_font_fub30_tf;
const uint8_t* const F_GIANT = u8g2_font_fub42_tf;

constexpr double kDeg = 3.14159265358979323846 / 180.0;

// Theme: light = black ink on white paper; dark = the reverse. Set at the start of every frame.
uint16_t gInk = kBlack;
uint16_t gPaper = kWhite;
bool gDark = false;

void beginFrame(Canvas& cv, const Common& c) {
  gDark = c.dark;
  gInk = gDark ? kWhite : kBlack;
  gPaper = gDark ? kBlack : kWhite;
  cv.g.fillScreen(gPaper);
}

enum class Align { Left, Center, Right };

int textWidth(Canvas& cv, const uint8_t* font, const char* s) {
  cv.u8.setFont(font);
  return cv.u8.getUTF8Width(s);
}

int drawText(Canvas& cv, const uint8_t* font, int x, int y, const char* s, Align a = Align::Left,
             uint16_t color = gInk) {
  cv.u8.setFont(font);
  cv.u8.setFontMode(1);
  cv.u8.setForegroundColor(color);
  cv.u8.setBackgroundColor(color == gInk ? gPaper : gInk);
  const int w = cv.u8.getUTF8Width(s);
  if (a == Align::Center) x -= w / 2;
  else if (a == Align::Right) x -= w;
  cv.u8.drawUTF8(x, y, s);
  return w;
}

// Truncates `s` (UTF-8) with "..." so it fits in maxW using `font`.
void ellipsize(Canvas& cv, const uint8_t* font, const char* s, int maxW, char* out, size_t n) {
  std::snprintf(out, n, "%s", s);
  if (textWidth(cv, font, out) <= maxW) return;
  size_t len = std::strlen(out);
  while (len > 0) {
    // step back one UTF-8 code point
    do {
      len--;
    } while (len > 0 && (static_cast<unsigned char>(out[len]) & 0xC0) == 0x80);
    out[len] = 0;
    char tmp[128];
    std::snprintf(tmp, sizeof(tmp), "%s...", out);
    if (textWidth(cv, font, tmp) <= maxW) {
      std::snprintf(out, n, "%s", tmp);
      return;
    }
  }
}

// Draws with the largest font from `fonts` that fits, ellipsizing with the last one.
int drawFit(Canvas& cv, const uint8_t* const* fonts, int nFonts, int x, int y, int maxW, const char* s,
            Align a = Align::Left, uint16_t color = gInk) {
  for (int i = 0; i < nFonts; i++) {
    if (textWidth(cv, fonts[i], s) <= maxW) return drawText(cv, fonts[i], x, y, s, a, color);
  }
  char buf[128];
  ellipsize(cv, fonts[nFonts - 1], s, maxW, buf, sizeof(buf));
  return drawText(cv, fonts[nFonts - 1], x, y, buf, a, color);
}

// ---------------------------------------------------------------------------
// Geometry helpers

struct P {
  float x, y;
};

void fillPoly(Canvas& cv, const P* pts, int n, uint16_t color) {
  float minY = pts[0].y, maxY = pts[0].y;
  for (int i = 1; i < n; i++) {
    minY = std::fmin(minY, pts[i].y);
    maxY = std::fmax(maxY, pts[i].y);
  }
  float xs[32];
  for (int y = static_cast<int>(std::ceil(minY)); y <= static_cast<int>(std::floor(maxY)); y++) {
    int k = 0;
    const float sy = y + 0.0f;
    for (int i = 0, j = n - 1; i < n && k < 32; j = i++) {
      const P& a = pts[i];
      const P& b = pts[j];
      if ((a.y <= sy && b.y > sy) || (b.y <= sy && a.y > sy)) {
        xs[k++] = a.x + (sy - a.y) * (b.x - a.x) / (b.y - a.y);
      }
    }
    for (int i = 1; i < k; i++) {  // insertion sort
      float v = xs[i];
      int j = i - 1;
      while (j >= 0 && xs[j] > v) {
        xs[j + 1] = xs[j];
        j--;
      }
      xs[j + 1] = v;
    }
    for (int i = 0; i + 1 < k; i += 2) {
      const int x0 = static_cast<int>(std::lround(xs[i]));
      const int x1 = static_cast<int>(std::lround(xs[i + 1]));
      if (x1 >= x0) cv.g.drawFastHLine(x0, y, x1 - x0 + 1, color);
    }
  }
}

void rotate(P* pts, int n, float cx, float cy, float deg) {
  const float c = std::cos(deg * kDeg), s = std::sin(deg * kDeg);
  for (int i = 0; i < n; i++) {
    const float x = pts[i].x, y = pts[i].y;
    pts[i] = {cx + x * c - y * s, cy + x * s + y * c};
  }
}

void thickLine(Canvas& cv, float x0, float y0, float x1, float y1, float w, uint16_t color) {
  const float dx = x1 - x0, dy = y1 - y0;
  const float len = std::sqrt(dx * dx + dy * dy);
  if (len < 0.01f) {
    cv.g.fillCircle(std::lround(x0), std::lround(y0), std::lround(w / 2), color);
    return;
  }
  const float nx = -dy / len * w / 2, ny = dx / len * w / 2;
  const P q[4] = {{x0 + nx, y0 + ny}, {x1 + nx, y1 + ny}, {x1 - nx, y1 - ny}, {x0 - nx, y0 - ny}};
  fillPoly(cv, q, 4, color);
}

void dashedLine(Canvas& cv, int x0, int y0, int x1, int y1, int on, int off, uint16_t color) {
  const float dx = x1 - x0, dy = y1 - y0;
  const float len = std::sqrt(dx * dx + dy * dy);
  if (len < 1) return;
  for (float t = 0; t < len; t += on + off) {
    const float t1 = std::fmin(len, t + on);
    cv.g.drawLine(std::lround(x0 + dx * t / len), std::lround(y0 + dy * t / len), std::lround(x0 + dx * t1 / len),
                  std::lround(y0 + dy * t1 / len), color);
  }
}

void dottedCircle(Canvas& cv, int cx, int cy, int r, int stepDeg) {
  for (int a = 0; a < 360; a += stepDeg) {
    cv.g.drawPixel(cx + std::lround(r * std::cos(a * kDeg)), cy + std::lround(r * std::sin(a * kDeg)), gInk);
  }
}

// Top-down airplane symbol, nose pointing to `headingDeg` (0 = up/north).
void planeIcon(Canvas& cv, float cx, float cy, float headingDeg, float size, uint16_t color) {
  const float s = size / 12.0f;
  P body[] = {{0, -12 * s}, {1.6f * s, -9 * s}, {1.6f * s, 8 * s}, {0, 11 * s}, {-1.6f * s, 8 * s}, {-1.6f * s, -9 * s}};
  P wings[] = {{0, -4 * s}, {11 * s, 2 * s}, {11 * s, 4 * s}, {0, 1 * s}, {-11 * s, 4 * s}, {-11 * s, 2 * s}};
  P tail[] = {{0, 6 * s}, {5 * s, 10 * s}, {5 * s, 11.5f * s}, {0, 9.5f * s}, {-5 * s, 11.5f * s}, {-5 * s, 10 * s}};
  rotate(body, 6, cx, cy, headingDeg);
  rotate(wings, 6, cx, cy, headingDeg);
  rotate(tail, 6, cx, cy, headingDeg);
  fillPoly(cv, body, 6, color);
  fillPoly(cv, wings, 6, color);
  fillPoly(cv, tail, 6, color);
}

// Arrow pointing to `deg` (0 = up), centred on (cx, cy).
void arrow(Canvas& cv, float cx, float cy, float deg, float len, uint16_t color) {
  P a[] = {{0, -len / 2}, {len * 0.32f, -len * 0.05f}, {len * 0.1f, -len * 0.05f}, {len * 0.1f, len / 2},
           {-len * 0.1f, len / 2}, {-len * 0.1f, -len * 0.05f}, {-len * 0.32f, -len * 0.05f}};
  rotate(a, 7, cx, cy, deg);
  fillPoly(cv, a, 7, color);
}

void triangleUpDown(Canvas& cv, int x, int y, bool up, int size) {
  if (up) cv.g.fillTriangle(x, y - size, x - size, y + size / 2, x + size, y + size / 2, gInk);
  else cv.g.fillTriangle(x, y + size, x - size, y - size / 2, x + size, y - size / 2, gInk);
}

void drawQr(Canvas& cv, int x, int y, int maxSize, const char* payload) {
  for (uint8_t version = 2; version <= 10; version++) {
    QRCode qr;
    uint8_t buf[400];
    if (qrcode_getBufferSize(version) > sizeof(buf)) break;
    if (qrcode_initText(&qr, buf, version, ECC_MEDIUM, payload) != 0) continue;
    const int quiet = 2;
    const int scale = maxSize / (qr.size + 2 * quiet);
    const int size = (qr.size + 2 * quiet) * scale;
    cv.g.fillRect(x, y, size, size, kWhite);
    for (uint8_t my = 0; my < qr.size; my++) {
      for (uint8_t mx = 0; mx < qr.size; mx++) {
        if (qrcode_getModule(&qr, mx, my)) {
          cv.g.fillRect(x + (mx + quiet) * scale, y + (my + quiet) * scale, scale, scale, kBlack);
        }
      }
    }
    return;
  }
}

// ---------------------------------------------------------------------------
// Shared pieces

void formatLocalTime(int64_t unixT, int tzOffsetS, char* out, size_t n) {
  const int64_t local = unixT + tzOffsetS;
  int64_t daySec = local % 86400;
  if (daySec < 0) daySec += 86400;
  std::snprintf(out, n, "%02d:%02d", static_cast<int>(daySec / 3600), static_cast<int>((daySec / 60) % 60));
}

int64_t localDay(int64_t unixT, int tzOffsetS) {
  const int64_t l = unixT + tzOffsetS;
  return l >= 0 ? l / 86400 : (l - 86399) / 86400;
}

const char* weekday(Lang lang, int64_t day) {
  static const char* en[] = {"Thu", "Fri", "Sat", "Sun", "Mon", "Tue", "Wed"};  // 1970-01-01 was a Thursday
  static const char* fr[] = {"Jeu.", "Ven.", "Sam.", "Dim.", "Lun.", "Mar.", "Mer."};
  const int i = static_cast<int>(((day % 7) + 7) % 7);
  return lang == Lang::FR ? fr[i] : en[i];
}

// "Today 21:14", "Tomorrow 05:32", "Wed 22:01"
void formatWhen(Lang lang, int64_t t, int64_t now, int tz, char* out, size_t n) {
  char hm[8];
  formatLocalTime(t, tz, hm, sizeof(hm));
  const int64_t dd = localDay(t, tz) - localDay(now, tz);
  const char* day = dd == 0 ? tr(lang, S::Today) : dd == 1 ? tr(lang, S::Tomorrow) : weekday(lang, localDay(t, tz));
  std::snprintf(out, n, "%s %s", day, hm);
}

void wifiIcon(Canvas& cv, int x, int y, bool ok, uint16_t color) {
  // three arcs + dot, (x, y) = bottom centre
  for (int r = 4; r <= 12; r += 4) {
    for (int a = 225; a <= 315; a += 6) {
      cv.g.drawPixel(x + std::lround(r * std::cos(a * kDeg)), y + std::lround(r * std::sin(a * kDeg)), color);
      cv.g.drawPixel(x + std::lround((r + 1) * std::cos(a * kDeg)), y + std::lround((r + 1) * std::sin(a * kDeg)), color);
    }
  }
  cv.g.fillCircle(x, y - 1, 1, color);
  if (!ok) {
    thickLine(cv, x - 9, y - 12, x + 9, y + 1, 2, color);
  }
}

// Status strip: freshness badge + clock on the left, count/mode/wifi on the right.
void statusBar(Canvas& cv, const Common& c, int x0, int x1, int y, bool inverted) {
  const uint16_t fg = inverted ? gPaper : gInk;
  const uint16_t bg = inverted ? gInk : gPaper;
  const Status& st = c.status;
  char buf[48];
  int x = x0;
  const int bh = 17;
  const int top = y - 13;
  switch (st.freshness) {
    case Freshness::Live: {
      const char* t = tr(c.lang, S::Live);
      const int w = textWidth(cv, F_LABEL, t) + 22;
      cv.g.fillRoundRect(x, top, w, bh, 4, fg);
      cv.g.fillCircle(x + 8, top + bh / 2, 3, bg);
      drawText(cv, F_LABEL, x + 15, y, t, Align::Left, bg);
      x += w + 8;
      break;
    }
    case Freshness::Delayed: {
      std::snprintf(buf, sizeof(buf), tr(c.lang, S::Delayed), st.delaySec);
      const int w = textWidth(cv, F_LABEL, buf) + 16;
      cv.g.drawRoundRect(x, top - 1, w, bh + 2, 4, fg);
      cv.g.drawRoundRect(x + 1, top, w - 2, bh, 3, fg);
      drawText(cv, F_LABEL, x + 8, y, buf, Align::Left, fg);
      x += w + 8;
      break;
    }
    case Freshness::Offline: {
      const char* t = tr(c.lang, S::Offline);
      const int w = textWidth(cv, F_LABEL, t) + 12;
      cv.g.fillRect(x, top, w, bh, fg);
      drawText(cv, F_LABEL, x + 6, y, t, Align::Left, bg);
      x += w + 8;
      break;
    }
  }
  drawText(cv, F_LABEL, x, y, st.clock, Align::Left, fg);

  int rx = x1;
  wifiIcon(cv, rx - 10, y + 1, st.wifiOk, fg);
  rx -= 26;
  const char* mode = st.mode == Mode::Auto ? tr(c.lang, S::ModeAuto)
                     : st.mode == Mode::Aircraft ? tr(c.lang, S::ModeAircraft)
                                                 : tr(c.lang, S::ModeIss);
  const int mw = textWidth(cv, F_LABEL, mode) + 10;
  cv.g.drawRoundRect(rx - mw, top, mw, bh, 4, fg);
  drawText(cv, F_LABEL, rx - mw + 5, y, mode, Align::Left, fg);
  rx -= mw + 10;
  if (st.aircraftCount >= 0) {
    std::snprintf(buf, sizeof(buf), tr(c.lang, S::AircraftCount), st.aircraftCount);
    drawText(cv, F_LABEL, rx, y, buf, Align::Right, fg);
  }
}

// Radar scope centred on home, north up.
void radar(Canvas& cv, int cx, int cy, int r, const Blip* blips, int n, float rangeKm, const Common& c) {
  cv.g.drawCircle(cx, cy, r, gInk);
  cv.g.drawCircle(cx, cy, r - 1, gInk);
  dottedCircle(cv, cx, cy, r * 2 / 3, 4);
  dottedCircle(cv, cx, cy, r / 3, 6);
  for (int a = 0; a < 360; a += 30) {
    const int len = (a % 90 == 0) ? 8 : 4;
    cv.g.drawLine(cx + std::lround((r - len) * std::sin(a * kDeg)), cy - std::lround((r - len) * std::cos(a * kDeg)),
                  cx + std::lround(r * std::sin(a * kDeg)), cy - std::lround(r * std::cos(a * kDeg)), gInk);
  }
  // North label on a white knock-out
  cv.g.fillRect(cx - 7, cy - r - 9, 15, 15, gPaper);
  drawText(cv, F_LABEL, cx, cy - r + 4, "N", Align::Center);
  // Home
  cv.g.drawLine(cx - 5, cy, cx + 5, cy, gInk);
  cv.g.drawLine(cx, cy - 5, cx, cy + 5, gInk);
  cv.g.drawCircle(cx, cy, 2, gInk);

  const Blip* sel = nullptr;
  for (int i = 0; i < n; i++) {
    const Blip& b = blips[i];
    if (b.distKm > rangeKm) continue;
    if (b.selected) {
      sel = &b;
      continue;
    }
    const float rr = b.distKm / rangeKm * (r - 6);
    const float x = cx + rr * std::sin(b.bearingDeg * kDeg), y = cy - rr * std::cos(b.bearingDeg * kDeg);
    if (b.trackDeg >= 0) planeIcon(cv, x, y, b.trackDeg, 9, gInk);
    else cv.g.fillCircle(std::lround(x), std::lround(y), 2, gInk);
    if (b.rare) cv.g.drawCircle(std::lround(x), std::lround(y), 8, gInk);
  }
  if (sel) {
    const float rr = sel->distKm / rangeKm * (r - 6);
    const float x = cx + rr * std::sin(sel->bearingDeg * kDeg), y = cy - rr * std::cos(sel->bearingDeg * kDeg);
    dashedLine(cv, cx, cy, std::lround(x), std::lround(y), 3, 3, gInk);
    cv.g.fillCircle(std::lround(x), std::lround(y), 13, gPaper);
    cv.g.drawCircle(std::lround(x), std::lround(y), 13, gInk);
    cv.g.drawCircle(std::lround(x), std::lround(y), 14, gInk);
    planeIcon(cv, x, y, sel->trackDeg >= 0 ? sel->trackDeg : 0, 17, gInk);
  }
  char buf[24];
  text::formatDistance(rangeKm, c.units, c.lang == Lang::FR, buf, sizeof(buf));
  drawText(cv, F_SMALL, cx + r, cy + r + 2, buf, Align::Right);
}

// Altitude / speed / heading / distance cells.
void statCells(Canvas& cv, const AircraftView& a, const Common& c, int x0, int width, int yTop, int yBottom) {
  const bool fr = c.lang == Lang::FR;
  const int cw = width / 4;
  const int yLabel = yTop + 16, yValue = yTop + 46, ySub = yTop + 66;
  (void)yBottom;
  const uint8_t* const valueFonts[] = {F_VALUE, F_MED_B, F_BODY_B};
  char v[32], sub[40];
  for (int i = 1; i < 4; i++) {
    for (int y = yTop + 6; y < yBottom - 4; y += 3) cv.g.drawPixel(x0 + i * cw - 4, y, gInk);
  }

  // Altitude
  drawText(cv, F_LABEL, x0 + 4, yLabel, tr(c.lang, S::Altitude));
  if (a.onGround) {
    drawFit(cv, valueFonts, 3, x0 + 4, yValue, cw - 12, tr(c.lang, S::OnGround));
  } else if (a.altFt != ac::kAltUnknown) {
    text::formatAltitude(a.altFt, c.units, fr, v, sizeof(v));
    drawFit(cv, valueFonts, 3, x0 + 4, yValue, cw - 12, v);
    if (std::abs(a.vrateFpm) >= 250) {
      char rate[24];
      if (c.units == Units::Metric) std::snprintf(rate, sizeof(rate), "%d m/s", static_cast<int>(std::lround(std::abs(a.vrateFpm) * 0.00508)));
      else {
        char num[16];
        text::formatThousands(std::lround(std::abs(a.vrateFpm) / 50.0) * 50, fr, num, sizeof(num));
        std::snprintf(rate, sizeof(rate), "%s ft/min", num);
      }
      triangleUpDown(cv, x0 + 10, ySub - 5, a.vrateFpm > 0, 5);
      drawText(cv, F_SMALL, x0 + 20, ySub, rate);
    }
  } else {
    drawText(cv, F_VALUE, x0 + 4, yValue, "-");
  }

  // Speed (+ secondary unit underneath)
  int x = x0 + cw;
  drawText(cv, F_LABEL, x + 4, yLabel, tr(c.lang, S::Speed));
  if (a.gsKt >= 0) {
    text::formatSpeed(a.gsKt, c.units, v, sizeof(v));
    drawFit(cv, valueFonts, 3, x + 4, yValue, cw - 12, v);
    text::formatSpeed(a.gsKt, c.units == Units::Metric ? Units::Aviation : Units::Metric, sub, sizeof(sub));
    drawText(cv, F_SMALL, x + 4, ySub, sub);
  } else {
    drawText(cv, F_VALUE, x + 4, yValue, "-");
  }

  // Heading with a little arrow
  x = x0 + 2 * cw;
  drawText(cv, F_LABEL, x + 4, yLabel, tr(c.lang, S::Heading));
  if (a.trackDeg >= 0) {
    std::snprintf(v, sizeof(v), "%d°", static_cast<int>(std::lround(a.trackDeg)) % 360);
    const int w = drawText(cv, F_VALUE, x + 4, yValue, v);
    arrow(cv, x + w + 22, yValue - 9, a.trackDeg, 22, gInk);
    drawText(cv, F_SMALL, x + 4, ySub, i18n::compass16(c.lang, geo::compassIndex16(a.trackDeg)));
  } else {
    drawText(cv, F_VALUE, x + 4, yValue, "-");
  }

  // Distance from home + direction
  x = x0 + 3 * cw;
  drawText(cv, F_LABEL, x + 4, yLabel, tr(c.lang, S::Distance));
  text::formatDistance(a.distKm, c.units, fr, v, sizeof(v));
  drawFit(cv, valueFonts, 3, x + 4, yValue, cw - 10, v);
  std::snprintf(sub, sizeof(sub), "%s  %d°", i18n::compass16(c.lang, geo::compassIndex16(a.bearingDeg)),
                static_cast<int>(std::lround(a.bearingDeg)) % 360);
  drawText(cv, F_SMALL, x + 4, ySub, sub);
}

// Origin -> destination with a progress marker.
void routeLine(Canvas& cv, const AircraftView& a, const Common& c, int x0, int x1, int yText, int yLine) {
  if (!a.hasRoute) {
    dashedLine(cv, x0, yLine, x1, yLine, 4, 4, gInk);
    const char* t = tr(c.lang, S::RouteUnknown);
    const int w = textWidth(cv, F_BODY, t);
    cv.g.fillRect((x0 + x1) / 2 - w / 2 - 8, yLine - 12, w + 16, 24, gPaper);
    drawText(cv, F_BODY, (x0 + x1) / 2, yLine + 6, t, Align::Center);
    return;
  }
  const uint8_t* const cityFonts[] = {F_MED_B, F_BODY_B, F_SMALL};
  const int half = (x1 - x0) / 2 - 8;
  char o[64], d[64];
  std::snprintf(o, sizeof(o), "%s", a.originCity[0] ? a.originCity : a.originCode);
  std::snprintf(d, sizeof(d), "%s", a.destCity[0] ? a.destCity : a.destCode);
  drawFit(cv, cityFonts, 3, x0, yText, half, o);
  drawFit(cv, cityFonts, 3, x1, yText, half, d, Align::Right);
  // Line with endpoints and the aircraft marker
  cv.g.fillCircle(x0 + 4, yLine, 4, gInk);
  cv.g.fillCircle(x1 - 4, yLine, 5, gInk);
  cv.g.fillCircle(x1 - 4, yLine, 2, gPaper);
  const int lx0 = x0 + 10, lx1 = x1 - 12;
  if (a.routeProgress >= 0) {
    const int px = lx0 + std::lround((lx1 - lx0) * std::fmin(1.0f, std::fmax(0.0f, a.routeProgress)));
    thickLine(cv, lx0, yLine, px, yLine, 3, gInk);
    dashedLine(cv, px, yLine, lx1, yLine, 3, 3, gInk);
    cv.g.fillRect(px - 11, yLine - 11, 22, 22, gPaper);
    planeIcon(cv, px, yLine, 90, 14, gInk);
  } else {
    thickLine(cv, lx0, yLine, lx1, yLine, 2, gInk);
    cv.g.fillTriangle(lx1 + 2, yLine, lx1 - 8, yLine - 5, lx1 - 8, yLine + 5, gInk);
  }
  drawText(cv, F_SMALL, x0, yLine + 20, a.originCode);
  drawText(cv, F_SMALL, x1, yLine + 20, a.destCode, Align::Right);
}

// One-row variant: "Paris  CDG  ---✈---  JFK  New York".
void routeCompact(Canvas& cv, const AircraftView& a, const Common& c, int x0, int x1, int y) {
  if (!a.hasRoute) {
    drawText(cv, F_BODY, x0, y, tr(c.lang, S::RouteUnknown));
    return;
  }
  const uint8_t* const fonts[] = {F_MED_B, F_BODY_B, F_SMALL};
  const int third = (x1 - x0) / 3;
  char o[64], d[64];
  std::snprintf(o, sizeof(o), "%s  %s", a.originCity[0] ? a.originCity : "", a.originCode);
  std::snprintf(d, sizeof(d), "%s  %s", a.destCode, a.destCity[0] ? a.destCity : "");
  int fi = 0;
  while (fi < 2 && (textWidth(cv, fonts[fi], o) > third + 20 || textWidth(cv, fonts[fi], d) > third + 20)) fi++;
  const int wo = drawFit(cv, fonts + fi, 3 - fi, x0, y, third + 20, o);
  const int wd = drawFit(cv, fonts + fi, 3 - fi, x1, y, third + 20, d, Align::Right);
  const int lx0 = x0 + wo + 16, lx1 = x1 - wd - 16, ly = y - 7;
  if (lx1 - lx0 < 40) return;
  const float p = a.routeProgress >= 0 ? std::fmin(1.0f, std::fmax(0.0f, a.routeProgress)) : 0.5f;
  const int px = lx0 + std::lround((lx1 - lx0) * p);
  if (a.routeProgress >= 0) {
    thickLine(cv, lx0, ly, px, ly, 3, gInk);
    dashedLine(cv, px, ly, lx1, ly, 3, 3, gInk);
  } else {
    thickLine(cv, lx0, ly, lx1, ly, 2, gInk);
  }
  cv.g.fillTriangle(lx1 + 2, ly, lx1 - 7, ly - 5, lx1 - 7, ly + 5, gInk);
  cv.g.fillRect(px - 10, ly - 10, 20, 20, gPaper);
  planeIcon(cv, px, ly, 90, 13, gInk);
}

void silhouette(Canvas& cv, int family, bool large, int x, int y) {
  if (family < 0 || family >= db::kFamilyCount) family = 0;
  if (large) cv.g.drawBitmap(x, y, db::kSilLarge[family], db::kSilLargeW, db::kSilLargeH, gInk);
  else cv.g.drawBitmap(x, y, db::kSilSmall[family], db::kSilSmallW, db::kSilSmallH, gInk);
}

void identityBlock(Canvas& cv, const AircraftView& a, int x, int maxW, int y0, bool large) {
  const uint8_t* const flightFonts[] = {F_BIG, F_VALUE};
  const uint8_t* const airlineFonts[] = {large ? F_MED : F_BODY, F_SMALL};
  const uint8_t* const modelFonts[] = {large ? F_MED_B : F_BODY_B, F_SMALL};
  const char* flight = a.flight[0] ? a.flight : (a.callsign[0] ? a.callsign : a.reg);
  int y = y0;
  drawFit(cv, flightFonts, 2, x, y, maxW, flight[0] ? flight : "-");
  y += large ? 28 : 24;
  if (a.airline[0]) {
    drawFit(cv, airlineFonts, 2, x, y, maxW, a.airline);
    y += large ? 26 : 22;
  }
  drawFit(cv, modelFonts, 2, x, y, maxW, a.model[0] ? a.model : a.typeCode);
  y += large ? 22 : 20;
  char buf[40];
  if (a.reg[0] && a.typeCode[0]) std::snprintf(buf, sizeof(buf), "%s  ·  %s", a.reg, a.typeCode);
  else std::snprintf(buf, sizeof(buf), "%s", a.reg[0] ? a.reg : a.typeCode);
  drawText(cv, F_SMALL, x, y, buf);
}

}  // namespace

// ===========================================================================

void drawAircraft(Canvas& cv, const AircraftScreen& s) {
  const AircraftView& a = s.ac;
  beginFrame(cv, s.c);

  if (!a.takeover) {
    statusBar(cv, s.c, 6, kW - 6, 16, false);
    cv.g.drawFastHLine(0, 22, kW, gInk);
    cv.g.drawFastVLine(526, 22, kH - 22, gInk);
    silhouette(cv, a.family, false, 8, 30);
    identityBlock(cv, a, 252, 268, 54, false);
    routeLine(cv, a, s.c, 10, 514, 140, 158);
    cv.g.drawFastHLine(0, 186, 526, gInk);
    statCells(cv, a, s.c, 4, 522, 186, kH);
    radar(cv, 659, 150, 112, s.blips, s.blipCount, s.rangeKm, s.c);
    return;
  }

  // ---- Takeover layout: inverted banner, large silhouette, no radar
  cv.g.fillRect(0, 0, kW, 36, gInk);
  const char* kind = a.reason == ac::RareReason::Emergency ? tr(s.c.lang, S::Emergency)
                     : a.reason == ac::RareReason::Military ? tr(s.c.lang, S::Military)
                                                            : tr(s.c.lang, S::RareAircraft);
  if (a.reason == ac::RareReason::Emergency) {
    const S detail = a.squawk == 7500 ? S::Sq7500 : a.squawk == 7600 ? S::Sq7600
                     : a.squawk == 7700 ? S::Sq7700 : S::DeclaredEmergency;
    const int kw = drawText(cv, F_MED_B, 38, 26, kind, Align::Left, gPaper);
    const uint8_t* const detailFonts[] = {F_BODY, F_SMALL};
    drawFit(cv, detailFonts, 2, 38 + kw + 14, 25, 470 - kw - 14, tr(s.c.lang, detail), Align::Left, gPaper);
    // Warning triangle
    cv.g.fillTriangle(16, 6, 4, 29, 28, 29, gPaper);
    cv.g.fillRect(15, 13, 3, 9, gInk);
    cv.g.fillRect(15, 24, 3, 3, gInk);
  } else {
    drawText(cv, F_MED_B, 38, 26, kind, Align::Left, gPaper);
    // Star
    P star[10];
    for (int i = 0; i < 10; i++) {
      const float r = (i % 2 == 0) ? 12.f : 5.f;
      star[i] = {16 + r * static_cast<float>(std::sin(i * 36 * kDeg)), 18 - r * static_cast<float>(std::cos(i * 36 * kDeg))};
    }
    fillPoly(cv, star, 10, gPaper);
  }
  Common cw = s.c;
  cw.status.aircraftCount = -1;
  statusBar(cv, cw, 520, kW - 8, 24, true);

  silhouette(cv, a.family, true, 6, 42);
  identityBlock(cv, a, 436, 350, 74, true);
  routeCompact(cv, a, s.c, 436, kW - 10, 186);
  cv.g.drawFastHLine(0, 196, kW, gInk);
  statCells(cv, a, s.c, 6, kW - 10, 194, kH);
}

void drawEmptySky(Canvas& cv, const EmptySkyScreen& s) {
  beginFrame(cv, s.c);
  statusBar(cv, s.c, 6, kW - 6, 16, false);
  cv.g.drawFastHLine(0, 22, kW, gInk);
  cv.g.drawFastVLine(526, 22, kH - 22, gInk);
  drawText(cv, F_HUGE, 24, 96, tr(s.c.lang, S::QuietSkies));
  char dist[24], line[96];
  text::formatDistance(s.rangeKm, s.c.units, s.c.lang == Lang::FR, dist, sizeof(dist));
  std::snprintf(line, sizeof(line), tr(s.c.lang, S::NoAircraftWithin), dist);
  drawText(cv, F_MED, 24, 130, line);
  if (s.hasPass) {
    cv.g.drawFastHLine(24, 168, 480, gInk);
    drawText(cv, F_LABEL, 24, 192, tr(s.c.lang, S::NextVisiblePass));
    char when[48];
    formatWhen(s.c.lang, s.nextPass.visStart.t, s.now, s.tzOffsetS, when, sizeof(when));
    std::snprintf(line, sizeof(line), "ISS  ·  %s  ·  max %d°", when, static_cast<int>(std::lround(s.nextPass.visMax.elDeg)));
    drawText(cv, F_MED_B, 24, 222, line);
  }
  radar(cv, 659, 150, 112, s.blips, s.blipCount, s.rangeKm, s.c);
}

// ---------------------------------------------------------------------------

void drawIssMap(Canvas& cv, const IssMapScreen& s) {
  beginFrame(cv, s.c);
  const int W = db::kMapW, H = db::kMapH;
  const int rowBytes = W / 8;

  // Day/night: precompute per-row and per-column terms of cos(solar zenith).
  const float sd = std::sin(s.sunLat * kDeg), cd = std::cos(s.sunLat * kDeg);
  static float colCos[db::kMapW];
  for (int x = 0; x < W; x++) {
    const float lon = (x + 0.5f) / W * 360.f - 180.f;
    colCos[x] = std::cos((lon - s.sunLon) * kDeg);
  }
  auto land = [&](int x, int y) -> bool {
    if (x < 0) x += W;
    if (x >= W) x -= W;
    if (y < 0 || y >= H) return false;
    return (db::kWorldLand[y * rowBytes + x / 8] >> (7 - (x & 7))) & 1;
  };
  for (int y = 0; y < H; y++) {
    const float lat = 90.f - (y + 0.5f) / H * 180.f;
    const float a = std::sin(lat * kDeg) * sd, b = std::cos(lat * kDeg) * cd;
    for (int x = 0; x < W; x++) {
      const bool night = a + b * colCos[x] < -0.0145f;  // sun 50' below the horizon
      const bool l = land(x, y);
      // `ink` = pixel drawn in the theme's ink colour. Both themes keep night darker than day.
      bool ink;
      if (gDark) {
        // Dark: black sea, white land by day, grey (50%) land by night, white coastline.
        if (l) {
          const bool coast = !land(x - 1, y) || !land(x + 1, y) || !land(x, y - 1) || !land(x, y + 1);
          ink = coast || !night || ((x + y) & 1) == 0;
        } else {
          ink = false;
        }
      } else {
        // Light: white sea (dotted at night), grey land by day, black land by night.
        if (l) {
          const bool coast = !land(x - 1, y) || !land(x + 1, y) || !land(x, y - 1) || !land(x, y + 1);
          ink = night || coast || ((x + y) & 1) == 0;
        } else {
          ink = night && (x & 1) == 0 && (y & 1) == 0;
        }
      }
      if (ink) cv.g.drawPixel(x, y, gInk);
    }
  }
  // Equator, dotted
  for (int x = 0; x < W; x += 4) cv.g.drawPixel(x, H / 2, land(x, H / 2) ? gPaper : gInk);

  auto px = [&](float lon) { return (lon + 180.f) / 360.f * W; };
  auto py = [&](float lat) { return (90.f - lat) / 180.f * H; };

  // Ground track: paper-coloured casing then an ink line, so it reads over day and night.
  for (int pass = 0; pass < 2; pass++) {
    for (int i = 1; i < s.trackCount; i++) {
      const float x0 = px(s.trackLon[i - 1]), x1 = px(s.trackLon[i]);
      if (std::fabs(x1 - x0) > W / 2) continue;  // antimeridian wrap
      const float y0 = py(s.trackLat[i - 1]), y1 = py(s.trackLat[i]);
      const bool future = i > s.trackNowIndex;
      if (pass == 0) thickLine(cv, x0, y0, x1, y1, future ? 5 : 3, gPaper);
      else if (future) thickLine(cv, x0, y0, x1, y1, 2, gInk);
      else if (i % 2 == 0) cv.g.drawLine(std::lround(x0), std::lround(y0), std::lround(x1), std::lround(y1), gInk);
    }
  }
  // Home
  const int hx = std::lround(px(s.homeLon)), hy = std::lround(py(s.homeLat));
  cv.g.fillCircle(hx, hy, 6, gPaper);
  cv.g.fillTriangle(hx, hy - 5, hx - 5, hy, hx + 5, hy, gInk);
  cv.g.fillRect(hx - 3, hy, 7, 5, gInk);
  // ISS marker: ring + solar-panel glyph
  const int ix = std::lround(px(static_cast<float>(s.pos.lonDeg))), iy = std::lround(py(static_cast<float>(s.pos.latDeg)));
  cv.g.fillCircle(ix, iy, 13, gPaper);
  cv.g.drawCircle(ix, iy, 13, gInk);
  cv.g.drawCircle(ix, iy, 12, gInk);
  cv.g.fillRect(ix - 9, iy - 4, 6, 8, gInk);
  cv.g.fillRect(ix + 4, iy - 4, 6, 8, gInk);
  cv.g.drawFastHLine(ix - 3, iy, 7, gInk);
  cv.g.fillRect(ix - 1, iy - 2, 3, 5, gInk);

  // ---- Right panel
  const int x0 = W + 10, x1 = kW - 8;
  cv.g.drawFastVLine(W, 0, H, gInk);
  Common cc = s.c;
  cc.status.aircraftCount = -1;
  statusBar(cv, cc, x0, x1, 16, false);
  cv.g.drawFastHLine(W, 22, kW - W, gInk);
  drawText(cv, F_HUGE, x0, 60, "ISS");
  drawText(cv, F_SMALL, x0 + 74, 46, s.pos.sunlit ? tr(s.c.lang, S::InSunlight) : tr(s.c.lang, S::InShadow));
  char buf[64], num[24];
  std::snprintf(buf, sizeof(buf), "%.1f°%c  %.1f°%c", std::fabs(s.pos.latDeg), s.pos.latDeg >= 0 ? 'N' : 'S',
                std::fabs(s.pos.lonDeg), s.pos.lonDeg >= 0 ? 'E' : (s.c.lang == Lang::FR ? 'O' : 'W'));
  drawText(cv, F_SMALL, x0 + 74, 61, buf);

  const bool fr = s.c.lang == Lang::FR;
  drawText(cv, F_LABEL, x0, 86, tr(s.c.lang, S::IssAltitude));
  drawText(cv, F_LABEL, x0 + 100, 86, tr(s.c.lang, S::IssSpeed));
  if (s.c.units == Units::Metric || s.c.units == Units::Aviation) {
    text::formatThousands(std::lround(s.pos.altKm), fr, num, sizeof(num));
    std::snprintf(buf, sizeof(buf), "%s km", num);
    drawText(cv, F_MED_B, x0, 110, buf);
    text::formatThousands(std::lround(s.pos.speedKmh / 10.0) * 10, fr, num, sizeof(num));
    std::snprintf(buf, sizeof(buf), "%s km/h", num);
  } else {
    text::formatThousands(std::lround(s.pos.altKm / 1.609344), fr, num, sizeof(num));
    std::snprintf(buf, sizeof(buf), "%s mi", num);
    drawText(cv, F_MED_B, x0, 110, buf);
    text::formatThousands(std::lround(s.pos.speedKmh / 1.609344 / 10.0) * 10, fr, num, sizeof(num));
    std::snprintf(buf, sizeof(buf), "%s mph", num);
  }
  const uint8_t* const speedFonts[] = {F_MED_B, F_BODY_B};
  drawFit(cv, speedFonts, 2, x0 + 100, 110, kW - 6 - (x0 + 100), buf);
  char dist[24];
  text::formatDistance(s.distanceKm, s.c.units == Units::Aviation ? Units::Metric : s.c.units, fr, dist, sizeof(dist));
  std::snprintf(buf, sizeof(buf), fr ? "à %s de chez vous" : "%s from home", dist);
  drawText(cv, F_SMALL, x0, 130, buf);

  cv.g.drawFastHLine(W, 144, kW - W, gInk);
  drawText(cv, F_LABEL, x0, 166, tr(s.c.lang, S::NextVisiblePass));
  if (s.hasPass) {
    char when[48];
    formatWhen(s.c.lang, s.nextPass.visStart.t, s.now, s.tzOffsetS, when, sizeof(when));
    drawText(cv, F_VALUE, x0, 196, when);
    const int az = geo::compassIndex16(s.nextPass.visStart.azDeg);
    const int mins = static_cast<int>(std::lround((s.nextPass.visEnd.t - s.nextPass.visStart.t) / 60.0));
    std::snprintf(buf, sizeof(buf), fr ? "max %d°  ·  %d min  ·  depuis %s" : "max %d°  ·  %d min  ·  from %s",
                  static_cast<int>(std::lround(s.nextPass.visMax.elDeg)), mins < 1 ? 1 : mins,
                  i18n::compass16(s.c.lang, az));
    drawText(cv, F_SMALL, x0, 218, buf);
  } else {
    drawText(cv, F_BODY, x0, 194, tr(s.c.lang, S::NoVisiblePass));
  }
  cv.g.drawFastHLine(W, 236, kW - W, gInk);
  drawText(cv, F_SMALL, x0, 254, tr(s.c.lang, S::ComputedOnDevice));
  std::snprintf(buf, sizeof(buf), tr(s.c.lang, S::OrbitAge), s.tleAgeDays);
  drawText(cv, F_SMALL, x0, 268, buf);
}

// ---------------------------------------------------------------------------

namespace {

// Sky chart: horizon circle, zenith in the centre, north up, east right.
P skyXY(int cx, int cy, int r, float az, float el) {
  const float rr = r * (90.f - std::fmax(0.f, el)) / 90.f;
  return {cx + rr * static_cast<float>(std::sin(az * kDeg)), cy - rr * static_cast<float>(std::cos(az * kDeg))};
}

void skyChart(Canvas& cv, int cx, int cy, int r, const iss::Pass& p, const iss::SkyPoint* now, Lang lang) {
  cv.g.drawCircle(cx, cy, r, gInk);
  cv.g.drawCircle(cx, cy, r - 1, gInk);
  dottedCircle(cv, cx, cy, r * 2 / 3, 5);  // 30 deg
  dottedCircle(cv, cx, cy, r / 3, 8);      // 60 deg
  cv.g.drawPixel(cx, cy, gInk);
  const char* labels[4] = {"N", "E", "S", lang == Lang::FR ? "O" : "W"};
  for (int i = 0; i < 4; i++) {
    const float a = i * 90 * kDeg;
    const int lx = cx + std::lround((r + 12) * std::sin(a)), ly = cy - std::lround((r + 12) * std::cos(a));
    drawText(cv, F_LABEL, lx, ly + 5, labels[i], Align::Center);
  }
  drawText(cv, F_SMALL, cx + 3, cy - r / 3 + 4, "60°");
  drawText(cv, F_SMALL, cx + 3, cy - r * 2 / 3 + 4, "30°");

  for (int i = 1; i < p.trackLen; i++) {
    const P a = skyXY(cx, cy, r, p.track[i - 1].azDeg, p.track[i - 1].elDeg);
    const P b = skyXY(cx, cy, r, p.track[i].azDeg, p.track[i].elDeg);
    thickLine(cv, a.x, a.y, b.x, b.y, 4, gInk);
  }
  if (p.trackLen >= 2) {
    // Arrow head at the end, start ring, max dot
    const P e0 = skyXY(cx, cy, r, p.track[p.trackLen - 2].azDeg, p.track[p.trackLen - 2].elDeg);
    const P e1 = skyXY(cx, cy, r, p.track[p.trackLen - 1].azDeg, p.track[p.trackLen - 1].elDeg);
    const float ang = std::atan2(e1.y - e0.y, e1.x - e0.x);
    P head[3] = {{e1.x + 9 * std::cos(ang), e1.y + 9 * std::sin(ang)},
                 {e1.x + 9 * std::cos(ang + 2.5f), e1.y + 9 * std::sin(ang + 2.5f)},
                 {e1.x + 9 * std::cos(ang - 2.5f), e1.y + 9 * std::sin(ang - 2.5f)}};
    fillPoly(cv, head, 3, gInk);
    const P s0 = skyXY(cx, cy, r, p.visStart.azDeg, p.visStart.elDeg);
    cv.g.fillCircle(std::lround(s0.x), std::lround(s0.y), 7, gPaper);
    cv.g.drawCircle(std::lround(s0.x), std::lround(s0.y), 7, gInk);
    cv.g.drawCircle(std::lround(s0.x), std::lround(s0.y), 6, gInk);
  }
  if (now) {
    const P q = skyXY(cx, cy, r, now->azDeg, now->elDeg);
    cv.g.fillCircle(std::lround(q.x), std::lround(q.y), 11, gPaper);
    cv.g.fillCircle(std::lround(q.x), std::lround(q.y), 8, gInk);
  }
}

void dirSentence(Lang lang, S id, float az, char* out, size_t n, int deg = -1) {
  const int i8 = geo::compassIndex8(az);
  if (deg >= 0) std::snprintf(out, n, tr(lang, id), i18n::directionPrefix(lang, i8), i18n::direction8(lang, i8), deg);
  else std::snprintf(out, n, tr(lang, id), i18n::directionPrefix(lang, i8), i18n::direction8(lang, i8));
}

}  // namespace

void drawIssAlert(Canvas& cv, const IssAlertScreen& s) {
  beginFrame(cv, s.c);
  const iss::Pass& p = s.pass;
  const Lang L = s.c.lang;
  cv.g.fillRect(0, 0, kW, 36, gInk);
  drawText(cv, F_MED_B, 14, 26, s.inProgress ? tr(L, S::IssNow) : tr(L, S::IssVisibleIn), Align::Left, gPaper);
  Common cc = s.c;
  cc.status.aircraftCount = -1;
  statusBar(cv, cc, 470, kW - 8, 24, true);

  char line[96], hm[8];
  if (!s.inProgress) {
    const int64_t secs = p.visStart.t - s.now;
    const int mins = static_cast<int>((secs + 59) / 60);
    if (secs < 60) std::snprintf(line, sizeof(line), "< 1 min");
    else std::snprintf(line, sizeof(line), "%d min", mins);
    const int w = drawText(cv, F_GIANT, 14, 96, line);
    formatLocalTime(p.visStart.t, s.tzOffsetS, hm, sizeof(hm));
    std::snprintf(line, sizeof(line), L == Lang::FR ? "à %s" : "at %s", hm);
    drawText(cv, F_MED, 24 + w, 96, line);

    dirSentence(L, S::AppearsIn, p.visStart.azDeg, line, sizeof(line), static_cast<int>(std::lround(p.visStart.elDeg)));
    drawText(cv, F_MED_B, 16, 140, line);
    int y = 170;
    if (p.visMax.t - p.visStart.t > 30 && p.visMax.elDeg - p.visStart.elDeg > 2) {
      const int i8 = geo::compassIndex8(p.visMax.azDeg);
      std::snprintf(line, sizeof(line), tr(L, S::RisesTo), static_cast<int>(std::lround(p.visMax.elDeg)),
                    i18n::directionPrefix(L, i8), i18n::direction8(L, i8));
      drawText(cv, F_MED, 16, y, line);
      y += 30;
    }
    dirSentence(L, S::DisappearsIn, p.visEnd.azDeg, line, sizeof(line));
    drawText(cv, F_MED, 16, y, line);
    y += 30;
    const int dur = static_cast<int>(std::lround((p.visEnd.t - p.visStart.t) / 60.0));
    std::snprintf(line, sizeof(line), tr(L, S::VisibleFor), dur < 1 ? 1 : dur);
    drawText(cv, F_MED, 16, y, line);
  } else {
    dirSentence(L, S::LookDir, s.current.azDeg, line, sizeof(line));
    const uint8_t* const lookFonts[] = {F_HUGE, F_BIG, F_VALUE};
    drawFit(cv, lookFonts, 3, 16, 96, 480, line);
    std::snprintf(line, sizeof(line), tr(L, S::HighNow), static_cast<int>(std::lround(s.current.elDeg)));
    drawText(cv, F_VALUE, 16, 136, line);
    dirSentence(L, S::DisappearsIn, p.visEnd.azDeg, line, sizeof(line));
    char t2[120];
    formatLocalTime(p.visEnd.t, s.tzOffsetS, hm, sizeof(hm));
    std::snprintf(t2, sizeof(t2), "%s  ·  %s", line, hm);
    drawText(cv, F_MED, 16, 176, t2);
  }
  skyChart(cv, 652, 156, 100, p, s.inProgress ? &s.current : nullptr, L);
}

// ---------------------------------------------------------------------------

namespace {

// Setup status line ("Connecting to X...", "Wrong password for X"...), or false if none.
bool joinMessage(Lang lang, JoinResult r, const char* ssid, char* out, size_t n) {
  S id;
  switch (r) {
    case JoinResult::Connecting: id = S::Connecting; break;
    case JoinResult::Ok: id = S::ConnectedTo; break;
    case JoinResult::WrongPassword: id = S::FailPassword; break;
    case JoinResult::NotFound: id = S::FailNotFound; break;
    case JoinResult::Captive: id = S::FailCaptive; break;
    case JoinResult::NoInternet: id = S::FailNoInternet; break;
    case JoinResult::Failed: id = S::FailGeneric; break;
    default: return false;
  }
  std::snprintf(out, n, tr(lang, id), ssid);
  return true;
}

}  // namespace

void drawSetup(Canvas& cv, const SetupScreen& s) {
  beginFrame(cv, s.c);
  drawQr(cv, 12, 12, 248, s.qrPayload);
  const int x = 282, w = kW - x - 10;
  // First-time setup: the language isn't chosen yet, so say everything in both.
  const bool both = !s.settingsMode;
  const Lang L = both ? Lang::EN : s.c.lang;
  const uint8_t* const msgFonts[] = {F_MED_B, F_BODY_B, F_SMALL};

  if (s.settingsMode) {
    drawText(cv, F_BIG, x, 44, tr(L, S::SettingsTitle));
  } else {
    drawText(cv, F_BIG, x, 44, "PlaneSpotter");
    drawText(cv, F_LABEL, x + 2, 66, "SETUP  ·  CONFIGURATION");
  }

  int y = 106;
  char line[128];
  if (joinMessage(L, s.join, s.joinSsid, line, sizeof(line))) {
    // Result banner: inverted for problems, outlined while connecting or on success.
    const bool problem = s.join != JoinResult::Connecting && s.join != JoinResult::Ok;
    const int bh = both ? 56 : 34;
    if (problem) cv.g.fillRoundRect(x - 6, y - 26, w + 6, bh, 6, gInk);
    else cv.g.drawRoundRect(x - 6, y - 26, w + 6, bh, 6, gInk);
    const uint16_t c = problem ? gPaper : gInk;
    drawFit(cv, msgFonts, 3, x + 4, y, w - 14, line, Align::Left, c);
    if (both && joinMessage(Lang::FR, s.join, s.joinSsid, line, sizeof(line))) {
      const uint8_t* const frFonts[] = {F_BODY, F_SMALL};
      drawFit(cv, frFonts, 2, x + 4, y + 22, w - 14, line, Align::Left, c);
    }
    y += bh + 8;
    if (s.join == JoinResult::Connecting || s.join == JoinResult::Ok) return;  // keep the screen calm
    if (both) {
      // Not enough room for the full instructions in two languages under the banner.
      drawText(cv, F_BODY_B, x, y + 22, tr(Lang::EN, S::ScanWithCamera));
      drawText(cv, F_BODY, x, y + 46, tr(Lang::FR, S::ScanWithCamera));
      std::snprintf(line, sizeof(line), tr(L, S::NoCamera), s.apSsid, s.apPassword);
      const uint8_t* const footFonts[] = {F_SMALL, F_TINY};
      drawFit(cv, footFonts, 2, x, 262, w, line);
      return;
    }
  }

  drawText(cv, F_MED_B, x, y + 18, tr(L, S::ScanWithCamera));
  drawText(cv, F_BODY, x, y + 42, tr(L, s.settingsMode ? S::SettingsThen : S::SetupThen));
  if (both) {
    drawText(cv, F_MED_B, x, y + 80, tr(Lang::FR, S::ScanWithCamera));
    drawText(cv, F_BODY, x, y + 104, tr(Lang::FR, S::SetupThen));
  } else {
    drawText(cv, F_BODY, x, y + 80, tr(L, S::CloseHint));
  }
  std::snprintf(line, sizeof(line), tr(L, S::NoCamera), s.apSsid, s.apPassword);
  const uint8_t* const footFonts[] = {F_SMALL, F_TINY};
  drawFit(cv, footFonts, 2, x, 262, w, line);
}

void drawOffline(Canvas& cv, const OfflineScreen& s) {
  beginFrame(cv, s.c);
  // Hatched frame: recognisable from across the room, impossible to confuse with live data.
  for (int i = -kH; i < kW; i += 14) {
    for (int t = 0; t < 6; t++) {
      cv.g.drawLine(i + t, 0, i + t + 10, 10, gInk);
      cv.g.drawLine(i + t, kH - 11, i + t + 10, kH - 1, gInk);
    }
  }
  for (int j = 0; j < kH; j += 14) {
    for (int t = 0; t < 6; t++) {
      cv.g.drawLine(0, j + t, 10, j + t + 10, gInk);
      cv.g.drawLine(kW - 11, j + t, kW - 1, j + t + 10, gInk);
    }
  }
  cv.g.fillRect(11, 11, kW - 22, kH - 22, gPaper);

  const Lang L = s.c.lang;
  // Large Wi-Fi glyph with a slash
  for (int r = 16; r <= 52; r += 18) {
    for (int a = 222; a <= 318; a += 2) {
      const float c0 = std::cos(a * kDeg), s0 = std::sin(a * kDeg);
      const float c1 = std::cos((a + 2) * kDeg), s1 = std::sin((a + 2) * kDeg);
      thickLine(cv, 90 + r * c0, 150 + r * s0, 90 + r * c1, 150 + r * s1, 7, gInk);
    }
  }
  cv.g.fillCircle(90, 146, 6, gInk);
  thickLine(cv, 46, 72, 134, 160, 12, gPaper);
  thickLine(cv, 50, 76, 130, 156, 6, gInk);

  drawText(cv, F_GIANT, 160, 96, tr(L, S::OfflineTitle));
  const S reason = s.reason == OfflineReason::Wifi ? S::ReasonWifi
                   : s.reason == OfflineReason::Api ? S::ReasonApi
                   : s.reason == OfflineReason::Time ? S::ReasonTime
                                                     : S::ReasonNoLocation;
  char line[128];
  if (s.detail[0]) std::snprintf(line, sizeof(line), "%s  (%s)", tr(L, reason), s.detail);
  else std::snprintf(line, sizeof(line), "%s", tr(L, reason));
  const uint8_t* const reasonFonts[] = {F_MED_B, F_BODY_B};
  drawFit(cv, reasonFonts, 2, 162, 132, 600, line);
  std::snprintf(line, sizeof(line), tr(L, S::LastUpdate), s.lastUpdate[0] ? s.lastUpdate : tr(L, S::Never));
  drawText(cv, F_BODY, 162, 162, line);
  drawText(cv, F_BODY, 162, 188, tr(L, s.reason == OfflineReason::Wifi ? S::HoldForWifi : S::Retrying));
  cv.g.drawFastHLine(40, 214, kW - 80, gInk);
  drawText(cv, F_MED_B, kW / 2, 246, tr(L, S::NotLive), Align::Center);
}

void drawMessage(Canvas& cv, const MessageScreen& s) {
  beginFrame(cv, s.c);
  drawText(cv, F_BIG, kW / 2, 96, s.title, Align::Center);
  if (s.line1[0]) drawText(cv, F_MED, kW / 2, 140, s.line1, Align::Center);
  if (s.line2[0]) drawText(cv, F_BODY, kW / 2, 172, s.line2, Align::Center);
  if (s.progress >= 0) {
    const int x = 146, y = 200, w = 500, h = 20;
    cv.g.drawRect(x, y, w, h, gInk);
    cv.g.drawRect(x + 1, y + 1, w - 2, h - 2, gInk);
    cv.g.fillRect(x + 4, y + 4, (w - 8) * (s.progress > 100 ? 100 : s.progress) / 100, h - 8, gInk);
  }
}

}  // namespace ui

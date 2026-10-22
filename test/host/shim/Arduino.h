// Minimal Arduino shim so Adafruit-GFX, U8g2_for_Adafruit_GFX and QRCode build on the host.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include "Print.h"
#include "pgmspace.h"

using std::max;
using std::min;
typedef bool boolean;
#define radians(deg) ((deg) * 0.017453292519943295)
#define degrees(rad) ((rad) * 57.29577951308232)
typedef uint8_t byte;
#ifndef _swap_int16_t
#define _swap_int16_t(a, b) { int16_t t = a; a = b; b = t; }
#endif

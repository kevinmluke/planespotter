// Text helpers: font-safe transliteration and unit formatting.
// Pure C++ (no Arduino), unit-tested on the host.
#pragma once

#include <cstddef>
#include <cstdint>

namespace text {

// The display fonts cover Latin-1 (U+0000..U+00FF), which is enough for English
// and French. Rewrites UTF-8 `in` so every code point fits: common Latin
// Extended letters are transliterated (ł->l, ş->s, İ->I, œ->oe...), typographic
// quotes/dashes are mapped to ASCII, anything else becomes '?'. Output stays UTF-8.
void toFontSafe(const char* in, char* out, size_t outSize);

// Upper-cases ASCII and Latin-1 letters in a UTF-8 string (é -> É).
void toUpperLatin1(const char* in, char* out, size_t outSize);

enum class Units : uint8_t { Aviation = 0, Metric = 1, Imperial = 2 };

// "36,000 ft" (aviation and imperial) or "10 970 m" (metric).
void formatAltitude(int32_t feet, Units u, bool french, char* out, size_t n);
void formatSpeed(float knots, Units u, char* out, size_t n);
void formatDistance(float km, Units u, bool french, char* out, size_t n);
// Thousands separator: "," in English, narrow no-break space in French.
void formatThousands(long v, bool french, char* out, size_t n);

}  // namespace text

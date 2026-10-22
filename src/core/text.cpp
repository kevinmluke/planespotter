#include "text.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace text {
namespace {

struct Translit {
  uint32_t cp;
  const char* ascii;
};

// Latin Extended-A/B letters and punctuation seen in airport/city names.
const Translit kTable[] = {
    {0x0100, "A"}, {0x0101, "a"}, {0x0102, "A"}, {0x0103, "a"}, {0x0104, "A"}, {0x0105, "a"},
    {0x0106, "C"}, {0x0107, "c"}, {0x0108, "C"}, {0x0109, "c"}, {0x010A, "C"}, {0x010B, "c"},
    {0x010C, "C"}, {0x010D, "c"}, {0x010E, "D"}, {0x010F, "d"}, {0x0110, "D"}, {0x0111, "d"},
    {0x0112, "E"}, {0x0113, "e"}, {0x0116, "E"}, {0x0117, "e"}, {0x0118, "E"}, {0x0119, "e"},
    {0x011A, "E"}, {0x011B, "e"}, {0x011E, "G"}, {0x011F, "g"}, {0x0122, "G"}, {0x0123, "g"},
    {0x0126, "H"}, {0x0127, "h"}, {0x012A, "I"}, {0x012B, "i"}, {0x012E, "I"}, {0x012F, "i"},
    {0x0130, "I"}, {0x0131, "i"}, {0x0136, "K"}, {0x0137, "k"}, {0x0139, "L"}, {0x013A, "l"},
    {0x013B, "L"}, {0x013C, "l"}, {0x013D, "L"}, {0x013E, "l"}, {0x0141, "L"}, {0x0142, "l"},
    {0x0143, "N"}, {0x0144, "n"}, {0x0145, "N"}, {0x0146, "n"}, {0x0147, "N"}, {0x0148, "n"},
    {0x014C, "O"}, {0x014D, "o"}, {0x0150, "O"}, {0x0151, "o"}, {0x0152, "OE"}, {0x0153, "oe"},
    {0x0154, "R"}, {0x0155, "r"}, {0x0158, "R"}, {0x0159, "r"}, {0x015A, "S"}, {0x015B, "s"},
    {0x015E, "S"}, {0x015F, "s"}, {0x0160, "S"}, {0x0161, "s"}, {0x0162, "T"}, {0x0163, "t"},
    {0x0164, "T"}, {0x0165, "t"}, {0x016A, "U"}, {0x016B, "u"}, {0x016E, "U"}, {0x016F, "u"},
    {0x0170, "U"}, {0x0171, "u"}, {0x0172, "U"}, {0x0173, "u"}, {0x0178, "Y"}, {0x0179, "Z"},
    {0x017A, "z"}, {0x017B, "Z"}, {0x017C, "z"}, {0x017D, "Z"}, {0x017E, "z"}, {0x01A0, "O"},
    {0x01A1, "o"}, {0x01AF, "U"}, {0x01B0, "u"}, {0x0218, "S"}, {0x0219, "s"}, {0x021A, "T"},
    {0x021B, "t"}, {0x2010, "-"}, {0x2011, "-"}, {0x2012, "-"}, {0x2013, "-"}, {0x2014, "-"},
    {0x2018, "'"}, {0x2019, "'"}, {0x201A, ","}, {0x201C, "\""}, {0x201D, "\""}, {0x2026, "..."},
    {0x2032, "'"}, {0x02BB, "'"}, {0x02BC, "'"},
};

// Decodes one UTF-8 sequence; returns bytes consumed (0 at end). Invalid bytes -> U+FFFD.
size_t decode(const unsigned char* s, uint32_t& cp) {
  if (!*s) return 0;
  if (s[0] < 0x80) {
    cp = s[0];
    return 1;
  }
  int len = (s[0] & 0xE0) == 0xC0 ? 2 : (s[0] & 0xF0) == 0xE0 ? 3 : (s[0] & 0xF8) == 0xF0 ? 4 : 0;
  if (!len) {
    cp = 0xFFFD;
    return 1;
  }
  cp = s[0] & (0x7F >> len);
  for (int i = 1; i < len; i++) {
    if ((s[i] & 0xC0) != 0x80) {
      cp = 0xFFFD;
      return i;
    }
    cp = (cp << 6) | (s[i] & 0x3F);
  }
  return len;
}

bool append(char* out, size_t outSize, size_t& pos, const char* bytes, size_t len) {
  if (pos + len >= outSize) return false;
  std::memcpy(out + pos, bytes, len);
  pos += len;
  return true;
}

bool appendCp(char* out, size_t outSize, size_t& pos, uint32_t cp) {
  char buf[2];
  if (cp < 0x80) {
    buf[0] = static_cast<char>(cp);
    return append(out, outSize, pos, buf, 1);
  }
  buf[0] = static_cast<char>(0xC0 | (cp >> 6));
  buf[1] = static_cast<char>(0x80 | (cp & 0x3F));
  return append(out, outSize, pos, buf, 2);
}

}  // namespace

void toFontSafe(const char* in, char* out, size_t outSize) {
  if (!outSize) return;
  size_t pos = 0;
  const unsigned char* s = reinterpret_cast<const unsigned char*>(in ? in : "");
  uint32_t cp;
  while (size_t n = decode(s, cp)) {
    s += n;
    bool ok = true;
    if (cp <= 0xFF) {
      if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0)) continue;  // control characters
      ok = appendCp(out, outSize, pos, cp);
    } else {
      const char* rep = "?";
      for (const auto& t : kTable) {
        if (t.cp == cp) {
          rep = t.ascii;
          break;
        }
      }
      ok = append(out, outSize, pos, rep, std::strlen(rep));
    }
    if (!ok) break;
  }
  out[pos] = 0;
}

void toUpperLatin1(const char* in, char* out, size_t outSize) {
  if (!outSize) return;
  size_t pos = 0;
  const unsigned char* s = reinterpret_cast<const unsigned char*>(in ? in : "");
  uint32_t cp;
  while (size_t n = decode(s, cp)) {
    s += n;
    if (cp >= 'a' && cp <= 'z') cp -= 32;
    else if (cp >= 0xE0 && cp <= 0xFE && cp != 0xF7) cp -= 32;  // à..þ -> À..Þ (skip ÷)
    else if (cp == 0xFF) cp = 'Y';  // ÿ has no Latin-1 capital
    if (cp > 0xFF) cp = '?';
    if (!appendCp(out, outSize, pos, cp)) break;
  }
  out[pos] = 0;
}

void formatThousands(long v, bool french, char* out, size_t n) {
  char digits[24];
  std::snprintf(digits, sizeof(digits), "%ld", v < 0 ? -v : v);
  const size_t len = std::strlen(digits);
  // French uses a (narrow) space; U+00A0 is in Latin-1 so the fonts render it.
  const char* sep = french ? "\xC2\xA0" : ",";
  size_t pos = 0;
  if (v < 0 && pos + 1 < n) out[pos++] = '-';
  for (size_t i = 0; i < len && pos + 3 < n; i++) {
    if (i > 0 && (len - i) % 3 == 0) {
      for (const char* p = sep; *p && pos + 1 < n; p++) out[pos++] = *p;
    }
    out[pos++] = digits[i];
  }
  out[pos] = 0;
}

void formatAltitude(int32_t feet, Units u, bool french, char* out, size_t n) {
  char num[24];
  if (u == Units::Metric) {
    const long m = std::lround(feet * 0.3048 / 10.0) * 10;  // 10 m resolution is honest for baro alt
    formatThousands(m, french, num, sizeof(num));
    std::snprintf(out, n, "%s m", num);
  } else {
    formatThousands(std::lround(feet / 25.0) * 25, french, num, sizeof(num));
    std::snprintf(out, n, "%s ft", num);
  }
}

void formatSpeed(float knots, Units u, char* out, size_t n) {
  switch (u) {
    case Units::Metric: std::snprintf(out, n, "%ld km/h", std::lround(knots * 1.852)); break;
    case Units::Imperial: std::snprintf(out, n, "%ld mph", std::lround(knots * 1.15078)); break;
    default: std::snprintf(out, n, "%ld kt", std::lround(knots)); break;
  }
}

void formatDistance(float km, Units u, bool french, char* out, size_t n) {
  const char dec = french ? ',' : '.';
  double v = km;
  const char* unit = "km";
  if (u == Units::Aviation) {
    v = km / 1.852;
    unit = "NM";
  } else if (u == Units::Imperial) {
    v = km / 1.609344;
    unit = "mi";
  }
  if (v < 10) {
    const long tenths = std::lround(v * 10);
    std::snprintf(out, n, "%ld%c%ld %s", tenths / 10, dec, tenths % 10, unit);
  } else {
    char num[24];
    formatThousands(std::lround(v), french, num, sizeof(num));
    std::snprintf(out, n, "%s %s", num, unit);
  }
}

}  // namespace text

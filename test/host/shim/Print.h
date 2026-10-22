#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
class __FlashStringHelper;
class String {
 public:
  String(const char* s = "") : s_(s) {}
  const char* c_str() const { return s_; }
  unsigned length() const { return std::strlen(s_); }
 private:
  const char* s_;
};
class Print {
 public:
  virtual ~Print() {}
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t* b, size_t n) { size_t k = 0; while (n--) k += write(*b++); return k; }
  size_t write(const char* s) { return s ? write((const uint8_t*)s, std::strlen(s)) : 0; }
  size_t print(const char* s) { return write(s); }
};

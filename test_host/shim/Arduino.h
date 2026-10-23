// Arduino "de mentira" para rodar o lib/Modem no computador.
// Só o que o Modem usa: String, Stream/HardwareSerial, millis/delay, pinos.
#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
using std::max;
using std::min;

#define OUTPUT 1
#define INPUT_PULLUP 2
#define LOW 0
#define HIGH 1
#define SERIAL_8N1 0
#define constrain(x, a, b) ((x) < (a) ? (a) : ((x) > (b) ? (b) : (x)))

class String {
public:
  std::string s;
  String() {}
  String(const char* c) : s(c ? c : "") {}
  String(const std::string& x) : s(x) {}
  explicit String(char c) : s(1, c) {}
  String(int v) : s(std::to_string(v)) {}
  String(unsigned v) : s(std::to_string(v)) {}
  String(long v) : s(std::to_string(v)) {}
  String(unsigned long v) : s(std::to_string(v)) {}
  const char* c_str() const { return s.c_str(); }
  unsigned length() const { return (unsigned)s.size(); }
  char operator[](unsigned i) const { return s[i]; }
  char& operator[](unsigned i) { return s[i]; }
  bool startsWith(const String& p) const { return s.compare(0, p.s.size(), p.s) == 0; }
  int indexOf(char c, unsigned from = 0) const { auto p = s.find(c, from); return p == std::string::npos ? -1 : (int)p; }
  int indexOf(const String& x, unsigned from = 0) const { auto p = s.find(x.s, from); return p == std::string::npos ? -1 : (int)p; }
  int indexOf(const char* x, unsigned from = 0) const { return indexOf(String(x), from); }
  int lastIndexOf(char c) const { auto p = s.rfind(c); return p == std::string::npos ? -1 : (int)p; }
  String substring(unsigned a) const { return a >= s.size() ? String() : String(s.substr(a)); }
  String substring(unsigned a, unsigned b) const { if (a > s.size()) return String(); return String(s.substr(a, b > a ? b - a : 0)); }
  void trim() {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    s = a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
  }
  void toUpperCase() { for (auto& c : s) c = (char)toupper((unsigned char)c); }
  void toLowerCase() { for (auto& c : s) c = (char)tolower((unsigned char)c); }
  long toInt() const { return atol(s.c_str()); }
  float toFloat() const { return (float)atof(s.c_str()); }
  bool equalsIgnoreCase(const String& o) const {
    if (o.s.size() != s.size()) return false;
    for (size_t i = 0; i < s.size(); i++) if (tolower((unsigned char)s[i]) != tolower((unsigned char)o.s[i])) return false;
    return true;
  }
  void remove(unsigned i) { if (i < s.size()) s.erase(i); }
  String& operator+=(const String& o) { s += o.s; return *this; }
  String& operator+=(const char* c) { s += c; return *this; }
  String& operator+=(char c) { s += c; return *this; }
  bool operator==(const String& o) const { return s == o.s; }
  bool operator==(const char* c) const { return s == c; }
  bool operator!=(const String& o) const { return s != o.s; }
  bool operator!=(const char* c) const { return s != c; }
};
inline String operator+(const String& a, const String& b) { return String(a.s + b.s); }
inline String operator+(const char* a, const String& b) { return String(std::string(a) + b.s); }
inline String operator+(const String& a, const char* b) { return String(a.s + b); }
inline String operator+(const String& a, char c) { return String(a.s + c); }
inline String operator+(const String& a, int v) { return a + String(v); }

class Stream {
public:
  virtual ~Stream() {}
  virtual size_t write(uint8_t b) = 0;
  virtual size_t write(const uint8_t* d, size_t n) { for (size_t i = 0; i < n; i++) write(d[i]); return n; }
  size_t print(const String& x) { return write((const uint8_t*)x.c_str(), x.length()); }
  size_t print(const char* x) { return write((const uint8_t*)x, strlen(x)); }
  size_t print(char c) { return write((uint8_t)c); }
  size_t println(const String& x) { return print(x) + print("\r\n"); }
  size_t println(const char* x) { return print(x) + print("\r\n"); }
  size_t println() { return print("\r\n"); }
};

// Implementados no teste (relógio simulado e o modem falso)
uint32_t millis();
void delay(uint32_t ms);
inline void pinMode(int, int) {}
// Cada digitalWrite fica anotado (pino, nível, quando), para os testes conferirem.
struct EscritaPino { int pino, nivel; uint32_t quando; };
#include <vector>
inline std::vector<EscritaPino>& escritasDePinos() { static std::vector<EscritaPino> v; return v; }
inline void digitalWrite(int p, int n) { escritasDePinos().push_back({p, n, millis()}); }
inline int digitalRead(int) { return HIGH; }
inline long random(long a, long b) { return a + (rand() % (b - a)); }

class HardwareSerial : public Stream {
public:
  virtual void begin(uint32_t, int, int, int) {}
  virtual void setRxBufferSize(size_t) {}
  virtual int available() = 0;
  virtual int read() = 0;
  virtual void flush() {}
  using Stream::write;
};


#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#define PROGMEM
#define min(a,b) ((a)<(b)?(a):(b))
#define A0 0
typedef struct { uint8_t r,g,b; } RGB;
struct Cfg_ { uint32_t magic; uint8_t bright, mode, page, img, albumSec, albumOn, screenOn; };
struct Cfg_ cfg;
RGB stripBuf[98];
struct Strip_ {
  void begin(){}
  void show(){}
  void setPixelColor(int i, uint32_t c){ stripBuf[i].r=(c>>16)&255; stripBuf[i].g=(c>>8)&255; stripBuf[i].b=c&255; }
  uint32_t Color(uint8_t r, uint8_t g, uint8_t b){ return ((uint32_t)r<<16)|((uint32_t)g<<8)|b; }
} strip;
static uint32_t millis(){ static uint32_t t=12345; return t += 3; }
static void digitalWrite(int p, int v){}
static int digitalRead(int p){ return 1; }
static void analogWrite(int p, int v){}
static int analogRead(int p){ return 512; }
static void pinMode(int p, int v){}
static void delay(unsigned long ms){}
struct Serial1_ {
  int available(){ return 0; }
  char read(){ return 0; }
  void setRX(int p){}
  void begin(unsigned long baud){}
} Serial1;
struct Serial_ {
  void begin(unsigned long baud){}
  void printf(const char* f, ...){}
} Serial;
struct SPI_ {
  void setTX(int p){} void setSCK(int p){} void begin(){}
  void beginTransaction(uint32_t s){}
  void transfer(void* buf, int n){}
} SPI;

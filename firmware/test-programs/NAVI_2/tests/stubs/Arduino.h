// ============================================================================
// tests/stubs/Arduino.h — host-build stub for the TEMPLATES regression suite.
//
// Provides exactly the surface TEMPLATES.ino touches, nothing more. Time is
// SIMULATED: the harness advances hostNowMs explicitly, so dt-sensitive
// paths (conservation gate, quarantine floor, R3 timing/reachability) are
// driven deterministically.
// ============================================================================
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include <deque>
#include <vector>

// ---- simulated clock -------------------------------------------------------
extern unsigned long hostNowMs;
static inline unsigned long millis(){ return hostNowMs; }
static inline void delay(unsigned long ms){ hostNowMs += ms; }

typedef uint8_t byte;

// ---- arithmetic helpers (Arduino macro semantics) --------------------------
#ifndef min
template<typename A, typename B> static inline A min(A a, B b){ return a < (A)b ? a : (A)b; }
template<typename A, typename B> static inline A max(A a, B b){ return a > (A)b ? a : (A)b; }
#endif
#define constrain(x,lo,hi) ((x)<(lo)?(lo):((x)>(hi)?(hi):(x)))
using std::abs;

// ---- GPIO / ADC / PWM (inert) ---------------------------------------------
#define OUTPUT 1
#define INPUT 0
static inline void pinMode(int,int){}
static inline void digitalWrite(int,int){}
static inline void analogReadResolution(int){}
extern int hostAnalogValue;
static inline int analogRead(int){ return hostAnalogValue; }
static inline bool ledcAttach(int,double,int){ return true; }
static inline void ledcWrite(int,uint32_t){}
#define HIGH 1
#define LOW 0

// ---- String / IPAddress (minimal) -----------------------------------------
class String {
  std::string s_;
public:
  String():s_(){}
  String(const char* c):s_(c?c:""){}
  const char* c_str() const { return s_.c_str(); }
};
class IPAddress {
public:
  String toString() const { return String("0.0.0.0"); }
};

// ---- Serial ----------------------------------------------------------------
struct HostSerial {
  void begin(unsigned long){}
  int printf(const char* fmt, ...){
    va_list ap; va_start(ap, fmt);
    int r = vfprintf(stdout, fmt, ap);
    va_end(ap); return r;
  }
  void println(const char* s){ fputs(s, stdout); fputc('\n', stdout); }
  void println(){ fputc('\n', stdout); }
};
extern HostSerial Serial;

// ---- FreeRTOS queue/task surface ------------------------------------------
// Real (tiny) FIFO queues: eventQueue is load-bearing for the tests.
struct HostQueue {
  size_t itemSize; size_t cap;
  std::deque<std::vector<uint8_t>> items;
};
typedef HostQueue* QueueHandle_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef void* TaskHandle_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
static inline QueueHandle_t xQueueCreate(size_t n, size_t itemSize){
  HostQueue* q = new HostQueue(); q->itemSize=itemSize; q->cap=n; return q;
}
static inline BaseType_t xQueueSend(QueueHandle_t q, const void* item, int){
  if(!q || q->items.size() >= q->cap) return pdFALSE;
  std::vector<uint8_t> b((const uint8_t*)item, (const uint8_t*)item + q->itemSize);
  q->items.push_back(std::move(b)); return pdTRUE;
}
static inline BaseType_t xQueueReceive(QueueHandle_t q, void* out, int){
  if(!q || q->items.empty()) return pdFALSE;
  memcpy(out, q->items.front().data(), q->itemSize);
  q->items.pop_front(); return pdTRUE;
}
static inline BaseType_t xQueuePeek(QueueHandle_t q, void* out, int){
  if(!q || q->items.empty()) return pdFALSE;
  memcpy(out, q->items.front().data(), q->itemSize);
  return pdTRUE;
}
static inline UBaseType_t uxQueueMessagesWaiting(QueueHandle_t q){
  return q ? (UBaseType_t)q->items.size() : 0;
}
static inline void vTaskDelay(int){ }
#define pdMS_TO_TICKS(ms) (ms)
static inline BaseType_t xTaskCreatePinnedToCore(void(*)(void*),const char*,int,void*,int,TaskHandle_t*,int){ return pdPASS; }
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
static inline void portENTER_CRITICAL(portMUX_TYPE*){}
static inline void portEXIT_CRITICAL(portMUX_TYPE*){}

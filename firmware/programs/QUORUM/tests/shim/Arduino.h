// Host shim for Arduino.h — replay harness only, never flashed.
//
// Purpose: let QUORUM.ino compile and EXECUTE on the development host so the
// replay suite exercises the real adjudication code rather than a
// transcription of it. Everything here is the smallest stub that satisfies a
// call site in QUORUM.ino; nothing here models ESP32 behaviour beyond what the
// navigator actually observes.
//
// Two things are deliberately real, because the replay depends on them:
//   * the FreeRTOS queues are genuine in-memory FIFOs, so publications made by
//     the firmware can be drained and asserted on;
//   * millis() is host-controlled, so an event stream can be replayed at its
//     captured timing.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <deque>
#include <string>
#include <vector>

typedef uint8_t byte;
typedef bool    boolean;

// --- time ------------------------------------------------------------------
// Host-controlled. The harness sets this to the captured detection time before
// each event, so the timing gate sees the intervals the locomotive saw.
extern unsigned long g_hostMillis;
inline unsigned long millis(){ return g_hostMillis; }
inline unsigned long micros(){ return g_hostMillis * 1000UL; }
inline void delay(unsigned long){}
inline void delayMicroseconds(unsigned int){}

// --- GPIO / ADC / LEDC -----------------------------------------------------
#define INPUT        0x01
#define OUTPUT       0x03
#define INPUT_PULLUP 0x05
#define HIGH 1
#define LOW  0
inline void pinMode(uint8_t,uint8_t){}
inline void digitalWrite(uint8_t,uint8_t){}
inline int  digitalRead(uint8_t){ return 0; }
inline int  analogRead(uint8_t){ return 0; }
inline void analogReadResolution(uint8_t){}
inline bool ledcAttach(uint8_t,uint32_t,uint8_t){ return true; }
inline void ledcWrite(uint8_t,uint32_t){}

// --- math helpers ----------------------------------------------------------
#ifndef min
template<class T,class U> inline T min(T a,U b){ return a<(T)b?a:(T)b; }
#endif
#ifndef max
template<class T,class U> inline T max(T a,U b){ return a>(T)b?a:(T)b; }
#endif
#ifndef constrain
#define constrain(x,l,h) ((x)<(l)?(l):((x)>(h)?(h):(x)))
#endif
#ifndef abs
#define abs(x) ((x)>0?(x):-(x))
#endif

inline size_t strlcpy(char* d,const char* s,size_t n){
  size_t sl=strlen(s);
  if(n){ size_t c = sl>=n ? n-1 : sl; memcpy(d,s,c); d[c]='\0'; }
  return sl;
}

// --- Serial ----------------------------------------------------------------
// Routed to stderr so it never contaminates the harness's stdout protocol.
struct HostSerial {
  void begin(unsigned long){}
  int  printf(const char* f,...){
    va_list a; va_start(a,f); int n=vfprintf(stderr,f,a); va_end(a); return n;
  }
  void println(const char* s=""){ fprintf(stderr,"%s\n",s); }
  void print(const char* s){ fprintf(stderr,"%s",s); }
  operator bool() const { return true; }
};
extern HostSerial Serial;

// --- FreeRTOS: real queues, no-op tasks and critical sections --------------
typedef unsigned int UBaseType_t;
typedef unsigned int TickType_t;
typedef int BaseType_t;
#define pdTRUE  1
#define pdFALSE 0
#define pdPASS  1
#define pdFAIL  0
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define portTICK_PERIOD_MS 1
#define portMAX_DELAY 0xFFFFFFFF

struct HostQueue {
  std::deque<std::vector<uint8_t>> items;
  size_t itemSize;
  size_t capacity;
};
typedef HostQueue* QueueHandle_t;
typedef void* TaskHandle_t;

inline QueueHandle_t xQueueCreate(size_t len,size_t itemSize){
  HostQueue* q=new HostQueue(); q->itemSize=itemSize; q->capacity=len; return q;
}
inline BaseType_t xQueueSend(QueueHandle_t q,const void* item,TickType_t){
  if(!q || q->items.size()>=q->capacity) return pdFALSE;
  const uint8_t* p=(const uint8_t*)item;
  q->items.emplace_back(p,p+q->itemSize);
  return pdTRUE;
}
inline BaseType_t xQueueReceive(QueueHandle_t q,void* out,TickType_t){
  if(!q || q->items.empty()) return pdFALSE;
  memcpy(out,q->items.front().data(),q->itemSize);
  q->items.pop_front();
  return pdTRUE;
}
inline BaseType_t xQueuePeek(QueueHandle_t q,void* out,TickType_t){
  if(!q || q->items.empty()) return pdFALSE;
  memcpy(out,q->items.front().data(),q->itemSize);
  return pdTRUE;
}
inline UBaseType_t uxQueueMessagesWaiting(QueueHandle_t q){
  return q ? (UBaseType_t)q->items.size() : 0;
}
inline BaseType_t xTaskCreatePinnedToCore(void(*)(void*),const char*,uint32_t,
                                          void*,UBaseType_t,TaskHandle_t*,BaseType_t){
  return pdTRUE;   // the harness drives the loop thread directly; no task runs
}
inline void vTaskDelay(TickType_t){}

typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
inline void portENTER_CRITICAL(portMUX_TYPE*){}
inline void portEXIT_CRITICAL(portMUX_TYPE*){}

// --- PROGMEM ---------------------------------------------------------------
#define PROGMEM
#define F(x) (x)

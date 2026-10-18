#pragma once
#include <atomic>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <type_traits>

namespace navi_resource {
// ESP32's ABI reports SOMETIMES lock-free: aligned internal DRAM uses S32C1I;
// external-memory atomics take a fallback lock. These objects are static DRAM,
// never PSRAM/WordStorage. The target ELF placement is checked by validation.
static_assert(ATOMIC_INT_LOCK_FREE >= 1 && alignof(std::atomic<uint32_t>) >= 4,
              "diagnostics require aligned native 32-bit atomics");

// Fixed multi-producer storage. Exactly one lock attempt, including in a heap
// failure callback: preemption/re-entry/full storage loses diagnostics, never
// waits for their consumer. No RTOS object, allocation, or UART operation.
template<class T, size_t N> class TryRing {
  static_assert(std::is_trivially_copyable<T>::value, "fixed records only");
 public:
  bool push(const T& item) {
    uint32_t unlocked=0;
    if (!lock_.compare_exchange_strong(unlocked,1,std::memory_order_acquire,std::memory_order_relaxed)) { ++drops_; return false; }
    const bool room = count_ < N;
    if (room) {
      items_[head_] = item; head_ = (head_ + 1) % N; ++count_;
      if (count_ > high_.load(std::memory_order_relaxed)) high_.store(count_, std::memory_order_relaxed);
    } else ++drops_;
    lock_.store(0,std::memory_order_release);
    return room;
  }
  bool pop(T& item) {
    uint32_t unlocked=0;
    if (!lock_.compare_exchange_strong(unlocked,1,std::memory_order_acquire,std::memory_order_relaxed)) return false;
    const bool present = count_ != 0;
    if (present) { item = items_[tail_]; tail_ = (tail_ + 1) % N; --count_; }
    lock_.store(0,std::memory_order_release);
    return present;
  }
  uint32_t drops() const { return drops_.load(std::memory_order_relaxed); }
  uint32_t highWater() const { return high_.load(std::memory_order_relaxed); }
 private:
  T items_[N]{};
  size_t head_=0, tail_=0, count_=0;
  std::atomic<uint32_t> lock_{0};
  std::atomic<uint32_t> drops_{0}, high_{0};
};

struct ConsoleLine { uint16_t length=0; char text[256]{}; };
class Console {
 public:
  void printf(const char* format, ...) __attribute__((format(printf,2,3))) {
    ConsoleLine line;
    va_list args; va_start(args,format);
    const int n=vsnprintf(line.text,sizeof(line.text),format,args);
    va_end(args);
    if (n < 0) { ++formatErrors_; return; }
    if (n >= int(sizeof(line.text))) {
      ++truncated_; line.length=sizeof(line.text)-1;
      line.text[line.length-1]='\n';
    } else line.length=uint16_t(n);
    lines_.push(line);
  }
  bool take(ConsoleLine& line) { return lines_.pop(line); }
  uint32_t drops() const { return lines_.drops(); }
  uint32_t highWater() const { return lines_.highWater(); }
  uint32_t truncated() const { return truncated_.load(); }
  uint32_t formatErrors() const { return formatErrors_.load(); }
 private:
  TryRing<ConsoleLine,4> lines_;
  std::atomic<uint32_t> truncated_{0}, formatErrors_{0};
};

// Used only by the loop's warning path. All warnings still reach MQTT; only
// identical console repetitions within one second are coalesced.
class WarningGate {
 public:
  bool admit(const char* message, uint32_t now) {
    if (seen_ && !strcmp(last_,message) && uint32_t(now-lastMs_) < 1000) {
      ++suppressed_; return false;
    }
    snprintf(last_,sizeof(last_),"%s",message);
    seen_=true; lastMs_=now; return true;
  }
  uint32_t suppressed() const { return suppressed_; }
 private:
  char last_[128]{}; uint32_t lastMs_=0, suppressed_=0; bool seen_=false;
};

struct AllocationFailure {
  uint64_t timeUs=0;
  uint32_t requested=0, capabilities=0;
  uintptr_t task=0, allocatorName=0, hookReturnPc=0;
};

inline uint32_t boundedUs(uint64_t us) { return us>UINT32_MAX ? UINT32_MAX : uint32_t(us); }
// Single owner per window; resets only after its report has been attempted.
struct Window {
  uint32_t count=0, maximum=0, over1ms=0, over10ms=0, over25ms=0;
  void observe(uint64_t us) {
    const uint32_t v=boundedUs(us); ++count;
    if (v>maximum) maximum=v;
    if (v>=1000) ++over1ms;
    if (v>=10000) ++over10ms;
    if (v>=25000) ++over25ms;
  }
};
// One writer per maximum (or per-core while inside a recorder critical section).
inline void maximum(std::atomic<uint32_t>& out,uint64_t us) {
  const uint32_t value=boundedUs(us);
  if(value>out.load(std::memory_order_relaxed)) out.store(value,std::memory_order_relaxed);
}
enum class BootFault : uint8_t { None, ConsoleTask, Recorder, Queues, HallTask, MqttBuffer, NetworkTask };
}

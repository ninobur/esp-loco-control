#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <type_traits>
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_heap_caps.h>
#endif

namespace navi_sync {
// ESP32 IRAM permits aligned word access, not arbitrary packed-struct or byte
// access. Only these volatile uint32_t loads/stores touch the backing memory.
// Wire objects remain ordinary byte-addressable RAM at both API boundaries.
template<class T, size_t Count> class WordStorage {
  static_assert(std::is_trivially_copyable<T>::value, "wire bytes only");
  static constexpr size_t Words=(sizeof(T)+3)/4;
 public:
  WordStorage() {
#if defined(ARDUINO_ARCH_ESP32)
    words_=static_cast<uint32_t*>(heap_caps_malloc(Words*Count*4,
      MALLOC_CAP_EXEC|MALLOC_CAP_INTERNAL|MALLOC_CAP_32BIT));
#else
    words_=static_cast<uint32_t*>(malloc(Words*Count*4));
#endif
    if(words_) for(size_t i=0;i<Words*Count;++i) words_[i]=0;
  }
  ~WordStorage() {
#if defined(ARDUINO_ARCH_ESP32)
    heap_caps_free(const_cast<uint32_t*>(words_));
#else
    free(const_cast<uint32_t*>(words_));
#endif
  }
  WordStorage(const WordStorage&)=delete;
  WordStorage& operator=(const WordStorage&)=delete;
  bool ready() const { return words_!=nullptr; }
  bool put(size_t index,const T& value) {
    if(!words_ || index>=Count) return false;
    const auto* bytes=reinterpret_cast<const uint8_t*>(&value);
    for(size_t w=0;w<Words;++w) {
      const size_t offset=w*4, count=sizeof(T)-offset<4?sizeof(T)-offset:4;
      uint32_t word=0;memcpy(&word,bytes+offset,count);
      words_[index*Words+w]=word;
    }
    return true;
  }
  bool get(size_t index,T& value) const {
    if(!words_ || index>=Count) return false;
    auto* bytes=reinterpret_cast<uint8_t*>(&value);
    for(size_t w=0;w<Words;++w) {
      const size_t offset=w*4, count=sizeof(T)-offset<4?sizeof(T)-offset:4;
      const uint32_t word=words_[index*Words+w];
      memcpy(bytes+offset,&word,count);
    }
    return true;
  }
  static constexpr size_t allocatedBytes() { return Words*Count*4; }
 private:
  volatile uint32_t* words_=nullptr;
};
}

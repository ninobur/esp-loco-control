#include "../NaviWordStorage.h"
#include <assert.h>
#include <cstdio>
struct __attribute__((packed)) Odd { unsigned char bytes[231]; };
struct Even { unsigned char bytes[1208]; };
template<class T> void roundTrip() {
  navi_sync::WordStorage<T,24> store;
  assert(store.ready());
  for(unsigned round=0;round<4;++round) {
    for(unsigned i=0;i<24;++i) {
      T in{};for(size_t j=0;j<sizeof(T);++j) in.bytes[j]=(i*37+j+round*61)&255;
      assert(store.put(i,in));
    }
    for(unsigned i=0;i<24;++i) {
      T out{};assert(store.get(i,out));
      for(size_t j=0;j<sizeof(T);++j) assert(out.bytes[j]==((i*37+j+round*61)&255));
    }
  }
  T sentinel{};memset(&sentinel,0x5a,sizeof(sentinel));T before=sentinel;
  assert(!store.put(24,sentinel));assert(!store.get(24,sentinel));
  assert(memcmp(&sentinel,&before,sizeof(sentinel))==0);
}
int main() {
  roundTrip<Odd>();roundTrip<Even>();
  failWordAllocation=true;
  { navi_sync::WordStorage<Odd,24> failed;
    Odd out{};out.bytes[0]=71;
    assert(!failed.ready());assert(!failed.put(0,out));assert(!failed.get(0,out));
    assert(out.bytes[0]==71);
  }
  assert(wordAllocations==2 && wordFrees==2);
  puts("PASS: word-only storage, odd/even records, all bytes, wrap reuse, bounds, allocation failure and release");
}

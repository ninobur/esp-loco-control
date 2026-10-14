#pragma once
#include <stdlib.h>
#include <assert.h>
#define MALLOC_CAP_EXEC 1
#define MALLOC_CAP_32BIT 2
#define MALLOC_CAP_INTERNAL 2048
inline bool failWordAllocation=false;
inline unsigned wordAllocations=0,wordFrees=0;
inline void* heap_caps_malloc(size_t size,unsigned caps) {
  assert(caps==(MALLOC_CAP_EXEC|MALLOC_CAP_32BIT|MALLOC_CAP_INTERNAL));
  if(failWordAllocation) return nullptr;
  ++wordAllocations;return malloc(size);
}
inline void heap_caps_free(void* p) { if(p) ++wordFrees;free(p); }

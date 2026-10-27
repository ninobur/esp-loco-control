#pragma once
#include <stdint.h>
#include <limits.h>

namespace navi_eyes {
// The shell serializes these methods across callback/main tasks with one mux.
// Arrival order, not queue service order, determines whether a release is new.
class OrderedEstop {
 public:
  uint64_t received(bool assertion) {
    if (arrival_ == UINT64_MAX) { exhausted_ = asserted_ = true; return arrival_; }
    const uint64_t id = ++arrival_;
    if (assertion) { assertion_ = id; asserted_ = true; }
    return id;
  }
  bool apply(uint64_t id, bool assertion) {
    if (!id || exhausted_) return asserted_;
    if (assertion) {
      if (id > release_) {
        if (id > assertion_) assertion_ = id;
        asserted_ = true;
      }
    } else if (id > assertion_ && id > release_) {
      release_ = id;
      asserted_ = false;
    }
    return asserted_;
  }
  bool asserted() const { return asserted_; }
 private:
  uint64_t arrival_ = 0, assertion_ = 0, release_ = 0;
  bool asserted_ = false, exhausted_ = false;
};
} // namespace navi_eyes

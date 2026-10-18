#pragma once
#include <stdio.h>
#include <string.h>
namespace navi_eyes {
static constexpr const char* kIrLostWarning="IR Lost. Hall navigation only.";
// One existing dashboard warning field, with separate sentences for loss of IR
// and an operating intervention. Detailed sensor residuals stay in diagnostics.
class NaviWarnings {
 public:
  void set(const char* message) { snprintf(other_,sizeof(other_),"%s",message); }
  void clear() { other_[0]=0; }
  int format(char* out,size_t size,bool irLost) const {
    return snprintf(out,size,"%s%s%s",irLost?kIrLostWarning:"",
      irLost && other_[0]?"\n":"",other_);
  }
 private:
  char other_[192]{};
};
}

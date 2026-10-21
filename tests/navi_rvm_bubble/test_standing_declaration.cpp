// A declaration requested while the motor is off must take effect while still standing.
#include <cstdio>
#include "RvmContinuity.h"
using namespace rvm_shape;
int main() {
  static HallContinuity c; static Block st[CONT_K]; c.begin(st);
  int declared = 0, emitted = 0;
  auto emit = [&](const Block&) { ++emitted; };
  auto onDecl = [&](const ContDecl&) { ++declared; };
  uint32_t i = 0;
  for (; i < 200; ++i) { Block b{i, 1900.f, 1, 100, 60, 60}; c.admit(b, emit, onDecl); }      // running
  for (; i < 300; ++i) { Block b{i, 1940.f, 1, 100, 0, 0}; c.admit(b, emit, onDecl); }        // stopped
  c.requestDeclaration(40, 41, 1);                                                             // operator declares standing
  for (; i < 302; ++i) { Block b{i, 1940.f, 1, 100, 0, 0}; c.admit(b, emit, onDecl); }
  printf("declared while standing: %d (want 1), emitted %d\n", declared, emitted);
  return declared == 1 ? 0 : 1;
}

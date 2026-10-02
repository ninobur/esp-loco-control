#include "../NaviCompatibility.h"
#include <cstdio>

using namespace navi_eyes;

static ir_movement::WireSnapshot packet(uint32_t seq, uint64_t pulses,
                                        uint64_t boot = 42,
                                        uint8_t reason = ir_movement::TRACKING) {
  ir_movement::WireSnapshot w;
  w.bootId = boot;
  w.sequence = seq;
  w.capturedUs = uint64_t(seq) * 100000;
  w.completedPulses = w.observedRises = pulses;
  w.pitchUm = 1000;
  w.nominalUm = pulses * w.pitchUm;
  w.opticalReason = reason;
  return w;
}

int main() {
  NaviIntegratedCore n;
  n.observeIr(packet(1, 0), 120000, 40);
  n.observeIr(packet(2, 10), 220000, 40);
  n.declare(0, 1, 250000);
  for (uint32_t seq = 3; seq <= 12; ++seq)
    n.observeIr(packet(seq, 10 + 2 * (seq - 2)), uint64_t(seq) * 100000 + 20000, 40);
  char json[512];
  formatConsoleIr(json, sizeof(json), n, 1220000, true);
  std::puts(json);  // measured moving
  for (uint32_t seq = 13; seq <= 24; ++seq) {
    auto w = packet(seq, 30, 42, ir_movement::INADEQUATE_CONTRAST);
    ++w.openAborts;
    n.observeIr(w, uint64_t(seq) * 100000 + 20000, 0);
  }
  formatConsoleIr(json, sizeof(json), n, 2420000, true);
  std::puts(json);  // measured stopped, raw diagnostic retained
  formatConsoleIr(json, sizeof(json), n, 4000000, true);
  std::puts(json);  // stale is not zero
  n.observeIr(packet(25, 40), 2450000, 0);
  formatConsoleNav(json, sizeof(json), n, 1, 2450000);
  std::puts(json);  // handling retains diagnostic context; declaration required
  n.observeIr(packet(1, 0, 43), 2500000, 40);
  formatConsoleNav(json, sizeof(json), n, 1, 2500000);
  std::puts(json);  // genuine frame reset: context held, redeclare required
}

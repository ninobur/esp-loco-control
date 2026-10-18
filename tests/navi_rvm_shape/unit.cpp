// Deterministic software checks of the firmware headers (no railway claims).
//   c++ -std=c++17 -O2 -Wall -Wextra -Werror -Ifirmware/test-programs/NAVI_RVM_SHAPE_MANUAL tests/navi_rvm_shape/unit.cpp
#include <cstdio>
#include <cmath>
#include <vector>
#include <random>
#include "RvmTracing.h"
#include "RvmPause.h"
#include "RvmShape.h"
#include "RvmNavigator.h"
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)
using namespace rvm_shape;

static void navigatorTests() {
  Navigator n;
  CHECK(n.onEnsemble(1) == NavResult::NoClaim);                        // undeclared: no claim
  n.declare(63, 64, +1);                                               // start_interval 063-064, CW
  CHECK(n.state() == NavState::Tracking && n.nextMm() == 64);
  for (uint8_t mm = 64; mm <= 70; ++mm) {                              // MM-BL-MM pieces, each counted once
    CHECK(n.onEnsemble(expectedHallSign(mm)) == NavResult::Advanced);
    CHECK(n.lastMm() == mm && n.nextMm() == mm + 1);
  }
  CHECK(n.onEnsemble(int8_t(-expectedHallSign(71))) == NavResult::Contradiction);
  CHECK(n.state() == NavState::Contradiction && n.contradictedMm() == 71 && n.lastMm() == 70);
  CHECK(n.onEnsemble(expectedHallSign(71)) == NavResult::NoClaim);     // no claim until declared again
  CHECK(n.onEnsemble(expectedHallSign(72)) == NavResult::NoClaim);
  n.declare(1, 0, -1);                                                 // CCW across the seam: 0 -> 170
  CHECK(n.onEnsemble(expectedHallSign(0)) == NavResult::Advanced && n.nextMm() == 170);
  n.suspend();
  CHECK(n.state() == NavState::Unset && n.onEnsemble(expectedHallSign(170)) == NavResult::NoClaim);
  CHECK(expectedHallSign(64) == -1 && expectedHallSign(63) == 1);      // ROUTE_POLARITY: MM063 N, MM064 S
}

static void tracingTests() {
  Tracing t; std::vector<Block> out;
  auto emit = [&](const Block& b) { out.push_back(b); };
  uint32_t s = 0;
  for (int i = 0; i < 100; ++i) t.add(1000000 + i * 200, 2000, 1, ++s, uint8_t(i < 50 ? 10 : 40), emit);          // block 0: 100 reads
  for (int i = 0; i < 40; ++i)  t.add(1020000 + i * 200, 1900, 1, ++s, 0, emit);          // block 1: 40 reads -> gap
  for (int i = 0; i < 100; ++i) t.add(1060000 + i * 200, 1800, 2, ++s, 0, emit);          // block 2 empty, block 3 ok
  s += 5;                                                                              // lost reads
  for (int i = 0; i < 100; ++i) t.add(1080000 + i * 200, 1700, 2, ++s, 0, emit);          // block 4 after a jump
  t.add(1100000, 1, 0, ++s, 0, emit);                                                     // closes block 4
  CHECK(out.size() == 5);
  CHECK(out[0].n == 100 && fabsf(out[0].hall - 2000) < 1e-3f && out[0].dir == 1 && out[0].pwm == 40);
  CHECK(std::isnan(out[1].hall) && out[1].n == 40);
  CHECK(std::isnan(out[2].hall) && out[2].n == 0 && out[2].dir == 0);
  CHECK(std::isnan(out[3].hall));                                      // block before a serial jump
  CHECK(std::isnan(out[4].hall));                                      // block after a serial jump
}

static std::vector<float> bumpLine(double sign, int bumpAt, double width, std::mt19937& g) {
  std::normal_distribution<double> nz(0, 2.0);
  std::vector<float> v(1500);
  for (int i = 0; i < 1500; ++i) {
    const double x = (i - bumpAt) / width;
    v[i] = float(1890 + nz(g) + (bumpAt >= 0 ? sign * 170 * exp(-0.5 * x * x) : 0.0));
  }
  return v;
}
static int recognize(const std::vector<float>& v, int declareAt, std::vector<Ensemble>& got) {
  static Recognizer r; r = Recognizer();
  for (size_t i = 0; i < v.size(); ++i) {
    if (int(i) == declareAt) r.startBaseline(r.blocks());
    r.push(v[i], false);
    Ensemble e; while (r.take(e)) got.push_back(e);
  }
  return int(got.size());
}
static void recognizerTests() {
  std::mt19937 g(7);
  std::vector<Ensemble> got;
  CHECK(recognize(bumpLine(1, -1, 4, g), 0, got) == 0);                // flat noisy line: nothing
  got.clear();
  CHECK(recognize(bumpLine(1, 600, 4, g), -1, got) == 0);              // never declared: recognizer idle
  got.clear();
  CHECK(recognize(bumpLine(-1, 600, 4, g), 100, got) == 1);            // one bump, counted once
  if (got.size() == 1) { CHECK(got[0].pol == -1); CHECK(fabs(got[0].c - 600) <= 3); CHECK(got[0].blStart == 100); }
  got.clear();
  CHECK(recognize(bumpLine(1, 600, 12, g), 100, got) == 1);            // same bump three times broader
  if (got.size() == 1) { CHECK(got[0].pol == 1); CHECK(fabs(got[0].c - 600) <= 6); }
}


static void pauseTests() {
  WaveformPause p;
  CHECK(!p.admit(0));                       // never declared and stopped: not appended
  p.declared();
  CHECK(p.admit(0) && p.admit(14));         // startup: declared stationary baseline is appended
  CHECK(p.admit(15) && !p.paused());        // first moving block ends startup
  CHECK(!p.admit(14) && p.paused());        // STOPPED = PWM < 15: paused
  CHECK(!p.admit(0) && p.paused());
  CHECK(p.admit(90) && !p.paused());        // resumed
  p.declared(); CHECK(p.admit(0));          // a new declaration is a new startup
}

// The same bump with commanded stops inserted (PWM 0 blocks) must give exactly
// the same recognition as without them.
static void stopGoTests() {
  std::mt19937 g(11);
  const std::vector<float> v = bumpLine(1, 600, 5, g);
  auto run = [&](const std::vector<int>& stopAt, int stopLen, std::vector<Ensemble>& got) {
    static Recognizer r; r = Recognizer(); WaveformPause p; uint32_t id = 0;
    std::normal_distribution<double> nz(0, 2.0);
    for (size_t i = 0; i < v.size(); ++i) {
      for (int sa : stopAt) if (int(i) == sa) for (int k = 0; k < stopLen; ++k) {
        if (p.admit(0)) r.push(float(v[i] + nz(g)), false, id); ++id;   // held Hall while stopped
      }
      if (i == 100) { r.startBaseline(r.blocks(), id); p.declared(); }
      if (p.admit(i < 100 ? 0 : 90)) r.push(v[i], false, id);
      ++id;
      Ensemble e; while (r.take(e)) got.push_back(e);
    }
  };
  std::vector<Ensemble> ref; run({}, 0, ref);
  CHECK(ref.size() == 1);
  for (int at : {300, 585, 592, 600, 608, 615, 640}) for (int len : {50, 1500}) {
    std::vector<Ensemble> got; run({at}, len, got);
    CHECK(got.size() == 1);
    if (got.size() == 1 && ref.size() == 1) { CHECK(got[0].pol == ref[0].pol); CHECK(fabs(got[0].c - ref[0].c) < 1e-9); CHECK(fabsf(got[0].llr - ref[0].llr) < 1e-6f); }
  }
  std::vector<Ensemble> rep; run({590, 600, 610}, 100, rep);           // repeated STOP/GO in one ensemble
  CHECK(rep.size() == 1);
}

int main() {
  navigatorTests(); tracingTests(); recognizerTests(); pauseTests(); stopGoTests();
  printf("%s (%d failures)\n", fails ? "UNIT CHECKS FAILED" : "UNIT CHECKS PASSED", fails);
  return fails ? 1 : 0;
}

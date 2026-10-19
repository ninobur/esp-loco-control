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


static Block blk(uint32_t id, float h, uint8_t pwm) { Block b{}; b.index = id; b.hall = h; b.pwm = pwm; b.dir = 1; b.n = 100; return b; }

static void pauseTests() {
  WaveformPause p; Block o[2]; uint32_t id = 0;
  CHECK(p.admit(blk(id++, 1900, 0), o) == 0);                  // never declared, stopped: not appended
  p.declared();
  CHECK(p.admit(blk(id++, 1900, 0), o) == 1);                  // startup: declared stationary baseline appended
  CHECK(p.admit(blk(id++, 1900, 14), o) == 1);
  CHECK(p.admit(blk(id++, 1900, 15), o) == 1 && !p.paused());  // first moving block ends startup
  CHECK(p.admit(blk(id++, 1950, 90), o) == 1);
  // STOP over a magnet: stationary at 2000 +- 1
  for (int k = 0; k < 50; ++k) CHECK(p.admit(blk(id++, 2000.0f + (k % 3) - 1, 0), o) == 0);
  CHECK(p.paused() && p.stationaryLow() == 1999 && p.stationaryHigh() == 2001);
  // GO, but still stationary: PWM >= 15 alone does not resume
  for (int k = 0; k < 20; ++k) CHECK(p.admit(blk(id++, 2000, 40), o) == 0);
  CHECK(p.paused());
  CHECK(p.admit(blk(id++, 2003, 40), o) == 0);                 // one block outside ...
  CHECK(p.admit(blk(id++, 2000, 40), o) == 0);                 // ... then back: a fluctuation, withheld
  CHECK(p.paused());
  CHECK(p.admit(blk(id++, 2003, 40), o) == 0);                 // outside high ...
  CHECK(p.admit(blk(id++, 1996, 40), o) == 0);                 // ... then outside LOW: not the same side
  CHECK(p.paused());
  CHECK(p.admit(blk(id++, 1990, 0), o) == 0);                  // re-stop before resuming: same stop, reference grows
  CHECK(p.stationaryLow() == 1990 && p.stationaryHigh() == 2001);
  CHECK(p.admit(blk(id++, 2005, 40), o) == 0);                 // first block of the departure: pending
  const uint32_t first = id - 1;
  CHECK(p.admit(blk(id++, 2010, 40), o) == 2);                 // second on the same side: resumed, both appended
  CHECK(o[0].index == first && o[1].index == first + 1 && !p.paused());
  CHECK(p.admit(blk(id++, 2000, 40), o) == 1);                 // running: everything appended
  p.declared(); CHECK(p.admit(blk(id++, 1890, 0), o) == 1);    // a new declaration is a new startup
}

// The same bump with commanded stops inserted - the stop held at the bump's
// value with stationary noise, and Otto-like stationary blocks AFTER the GO -
// must give exactly the same recognition as without them.
static void stopGoTests() {
  std::mt19937 g(11);
  const std::vector<float> v = bumpLine(1, 600, 5, g);
  // centre of each recognition mapped back to the uninterrupted signal's sample index
  auto run = [&](const std::vector<int>& stopAt, int stopLen, int goStill, std::vector<double>& got) {
    static Recognizer r; r = Recognizer(); WaveformPause p; uint32_t id = 0; Block o[2];
    std::vector<int> orig;                                   // block id -> uninterrupted sample index (-1: inserted)
    std::normal_distribution<double> nz(0, 0.7);
    auto feed = [&](float h, uint8_t pwm, int i) {
      orig.push_back(i);
      const int na = p.admit(blk(id, h, pwm), o);
      for (int q = 0; q < na; ++q) r.push(o[q].hall, false, o[q].index);
      ++id;
      Ensemble e; while (r.take(e)) got.push_back(orig[e.cId] + (e.c - floor(e.c)));
    };
    for (size_t i = 0; i < v.size(); ++i) {
      for (int sa : stopAt) if (int(i) == sa) {
        for (int k = 0; k < stopLen; ++k) feed(float(v[i] + nz(g)), 0, -1);   // STOPPED
        for (int k = 0; k < goStill; ++k) feed(float(v[i] + nz(g)), 40, -1);  // GO, still stationary
      }
      if (i == 100) { r.startBaseline(r.blocks(), id); p.declared(); }
      feed(v[i], i < 100 ? 0 : 90, int(i));
    }
  };
  std::vector<double> ref; run({}, 0, 0, ref);
  CHECK(ref.size() == 1);
  // A long pause (30 s) establishes the stationary range; the 1 s pause is checked without
  // a stationary interval after GO (a short pause gives a narrow range: report s20).
  for (int at : {300, 585, 592, 600, 608, 615, 640}) for (int len : {50, 1500}) for (int still : {0, 165, 265}) {
    if (len == 50 && still) continue;
    std::vector<double> got; run({at}, len, still, got);
    if (got.size() != 1) printf("  stop at %d len %d still %d: %zu recognitions\n", at, len, still, got.size());
    CHECK(got.size() == 1);
    if (got.size() == 1 && ref.size() == 1 && fabs(got[0] - ref[0]) > 3.0) printf("  stop at %d len %d still %d: centre %.1f vs %.1f\n", at, len, still, got[0], ref[0]);
    if (got.size() == 1 && ref.size() == 1) CHECK(fabs(got[0] - ref[0]) <= 3.0);   // the block AT the stop point equals the stationary state
  }
  std::vector<double> rep; run({590, 600, 610}, 100, 165, rep);       // repeated STOP/GO in one ensemble
  CHECK(rep.size() == 1);
}

int main() {
  navigatorTests(); tracingTests(); recognizerTests(); pauseTests(); stopGoTests();
  printf("%s (%d failures)\n", fails ? "UNIT CHECKS FAILED" : "UNIT CHECKS PASSED", fails);
  return fails ? 1 : 0;
}

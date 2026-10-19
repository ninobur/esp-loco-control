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
  for (int i = 0; i < 100; ++i) t.add(1000000 + i * 200, 2000, 1, ++s, uint8_t(i < 50 ? 10 : 40), 77, emit);          // block 0: 100 reads
  for (int i = 0; i < 40; ++i)  t.add(1020000 + i * 200, 1900, 1, ++s, 0, 0, emit);          // block 1: 40 reads -> gap
  for (int i = 0; i < 100; ++i) t.add(1060000 + i * 200, 1800, 2, ++s, 0, 0, emit);          // block 2 empty, block 3 ok
  s += 5;                                                                              // lost reads
  for (int i = 0; i < 100; ++i) t.add(1080000 + i * 200, 1700, 2, ++s, 0, 0, emit);          // block 4 after a jump
  t.add(1100000, 1, 0, ++s, 0, 0, emit);                                                     // closes block 4
  CHECK(out.size() == 5);
  CHECK(out[0].n == 100 && fabsf(out[0].hall - 2000) < 1e-3f && out[0].dir == 1 && out[0].pwm == 40 && out[0].cmd == 77);
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


static Block blk(uint32_t id, float h, uint8_t pwm, uint8_t cmd = 255) { Block b{}; b.index = id; b.hall = h; b.pwm = pwm; b.cmd = cmd; b.dir = 1; b.n = 100; return b; }

static void pauseTests() {
  static Block storage[HOLD_MAX]; WaveformPause p; p.begin(storage); uint32_t id = 0; std::vector<uint32_t> out;
  auto sink = [&](const Block& a) { out.push_back(a.index); };
  // feed one block and then everything waiting (immediate feeding)
  auto feed = [&](float h, uint8_t pwm, uint8_t cmd = 255) { out.clear(); p.admit(blk(id++, h, pwm, cmd), sink); while (p.drainOne(sink)) {} return out.size(); };
  CHECK(feed(1900, 0) == 0);                                   // never declared, stopped: not appended
  p.declared([](const Block&) {});
  CHECK(feed(1900, 0) == 1 && feed(1900, 14) == 1);            // startup: declared stationary baseline appended
  CHECK(feed(1900, 15) == 1 && !p.paused());                   // first moving block ends startup
  CHECK(feed(1950, 90) == 1);
  // stop commanded then withdrawn before PWM < 15: everything released unchanged, in order
  CHECK(feed(1960, 60, 0) == 0 && feed(1970, 50, 0) == 0 && p.holding());
  CHECK(feed(1980, 55, 90) == 3 && out[0] == id - 3 && out[2] == id - 1 && !p.holding());
  // hold-back capacity: the oldest held blocks are released in order
  for (int k = 0; k < HOLD_MAX; ++k) CHECK(feed(1990, 40, 0) == 0);
  CHECK(feed(1990, 40, 0) == 1 && out[0] == id - 1 - HOLD_MAX && p.releasedEarly() == 1);
  CHECK(feed(1990, 40, 90) == HOLD_MAX + 1);                   // withdrawn: release the rest
  // arrival over a magnet: moving (rising) then standing still at 2000 while PWM is still >= 15
  CHECK(feed(1960, 50, 0) == 0 && feed(1980, 45, 0) == 0 && feed(1995, 40, 0) == 0);
  const uint32_t lastMoving = id - 1;
  for (int k = 0; k < 30; ++k) CHECK(feed(2000.0f + (k % 3) - 1, 30, 0) == 0);   // arrival plateau (held)
  for (int k = 0; k < 50; ++k) CHECK(feed(2000.0f + (k % 3) - 1, 0, 0) == 0);    // STOPPED
  CHECK(p.paused() && p.stationaryLow() == 1999 && p.stationaryHigh() == 2001);
  // GO: the plateau is trimmed, the moving blocks are released, this block awaits
  CHECK(feed(2000, 40) == 3 && out.back() == lastMoving && p.lastArrivalTrim() == 30);
  for (int k = 0; k < 20; ++k) CHECK(feed(2000, 40) == 0);     // still stationary after GO: withheld
  CHECK(feed(2003, 40) == 0 && feed(2000, 40) == 0);           // single excursion: fluctuation
  CHECK(feed(2003, 40) == 0 && feed(1996, 40) == 0);           // opposite sides: not a departure
  CHECK(feed(1990, 0) == 0);                                   // re-stop before resuming: same stop
  CHECK(p.stationaryLow() == 1990 && p.stationaryHigh() == 2001);
  CHECK(feed(2005, 40) == 0);                                  // first departing block: pending
  CHECK(feed(2010, 40) == 2 && out[0] == id - 2 && !p.paused());   // second on the same side: resumed
  CHECK(feed(2000, 40) == 1);
  CHECK(feed(1960, 50, 0) == 0 && feed(1970, 45, 0) == 0);    // a stop commanded, blocks held ...
  { int rel = 0; p.declared([&](const Block&) { ++rel; }); CHECK(rel == 2 && !p.holding() && !p.backlog()); }   // ... a declaration feeds them
  CHECK(feed(1890, 0) == 1);                                   // a new declaration is a new startup
}

// The same bump with commanded stops inserted: an arrival plateau (stationary,
// PWM >= 15, stop already commanded), the stop (PWM 0), and stationary blocks
// after GO (PWM >= 15) - must give the same recognition as without them.
static void stopGoTests() {
  std::mt19937 g(11);
  const std::vector<float> v = bumpLine(1, 600, 5, g);
  auto run = [&](const std::vector<int>& stopAt, int arrive, int stopLen, int goStill, std::vector<double>& got) {
    static Recognizer r; r = Recognizer(); static WaveformPause p; static Block storage[HOLD_MAX]; p = WaveformPause(); p.begin(storage); uint32_t id = 0;
    std::vector<int> orig;
    std::normal_distribution<double> nz(0, 0.7);
    auto feed = [&](float h, uint8_t pwm, uint8_t cmd, int i) {
      orig.push_back(i);
      auto push = [&](const Block& a) { r.push(a.hall, false, a.index); };
      p.admit(blk(id, h, pwm, cmd), push);
      p.drainOne(push);                                        // firmware schedule: one waiting block per pass
      ++id;
      Ensemble e; while (r.take(e)) got.push_back(orig[e.cId] + (e.c - floor(e.c)));
    };
    for (size_t i = 0; i < v.size(); ++i) {
      for (int sa : stopAt) if (int(i) == sa) {
        for (int k = 0; k < arrive; ++k) feed(float(v[i] + nz(g)), 30, 0, -1);    // arrival plateau, stop commanded
        for (int k = 0; k < stopLen; ++k) feed(float(v[i] + nz(g)), 0, 0, -1);    // STOPPED
        for (int k = 0; k < goStill; ++k) feed(float(v[i] + nz(g)), 40, 90, -1);  // GO, still stationary
      }
      if (i == 100) { p.declared([&](const Block& a) { r.push(a.hall, false, a.index); }); r.startBaseline(r.blocks(), id); }
      bool decel = false; for (int sa : stopAt) if (int(i) >= sa - 40 && int(i) < sa) decel = true;
      feed(v[i], i < 100 ? 0 : 90, decel ? 0 : 255, int(i));     // the stop is commanded 40 blocks before
    }
    while (p.drainOne([&](const Block& a) { r.push(a.hall, false, a.index); })) {
      Ensemble e; while (r.take(e)) got.push_back(orig[e.cId] + (e.c - floor(e.c)));
    }
  };
  std::vector<double> ref; run({}, 0, 0, 0, ref);
  CHECK(ref.size() == 1);
  for (int at : {300, 585, 592, 600, 608, 615, 640}) for (int arrive : {0, 150, 290}) for (int still : {0, 165, 265}) {
    std::vector<double> got; run({at}, arrive, 1500, still, got);
    if (got.size() != 1) printf("  stop at %d arrive %d still %d: %zu recognitions\n", at, arrive, still, got.size());
    CHECK(got.size() == 1);
    if (got.size() == 1 && ref.size() == 1 && fabs(got[0] - ref[0]) > 3.0) printf("  stop at %d arrive %d still %d: centre %.1f vs %.1f\n", at, arrive, still, got[0], ref[0]);
    if (got.size() == 1 && ref.size() == 1) CHECK(fabs(got[0] - ref[0]) <= 3.0);   // the block AT the stop point equals the stationary state
  }
  std::vector<double> rep; run({590, 600, 610}, 100, 300, 165, rep);   // repeated STOP/GO in one ensemble
  CHECK(rep.size() == 1);
}


// Bounded feeding: released blocks wait in order and are fed by drainOne; a block
// joining the waveform meanwhile goes behind them; a full ring feeds its oldest.
static void backlogTests() {
  static Block storage[HOLD_MAX]; WaveformPause p; p.begin(storage); uint32_t id = 0; std::vector<uint32_t> fed;
  auto sink = [&](const Block& a) { fed.push_back(a.index); };
  p.declared(sink); p.admit(blk(id++, 1900, 90), sink);                       // running
  for (int k = 0; k < 100; ++k) p.admit(blk(id++, 1950, 40, 0), sink);        // stop commanded: 100 held
  for (int k = 0; k < 20; ++k) p.admit(blk(id++, 1960 + k, 30, 0), sink);    // (moving on; held)
  p.admit(blk(id++, 1980, 0, 0), sink);                                       // STOPPED
  for (int k = 0; k < 10; ++k) p.admit(blk(id++, 1980.0f + (k % 2), 0, 0), sink);
  const size_t before = fed.size();
  p.admit(blk(id++, 1980, 40), sink);                                         // GO: trim + release, nothing fed yet
  CHECK(fed.size() == before && p.backlog() > 0);
  const uint16_t bl = p.backlog();
  p.admit(blk(id++, 1990, 40), sink); p.admit(blk(id++, 1995, 40), sink);    // resumption: joins BEHIND the backlog
  CHECK(fed.size() == before && p.backlog() == bl + 2);
  for (int k = 0; k < 5; ++k) CHECK(p.drainOne(sink));                        // bounded portions, oldest first
  CHECK(fed.size() == before + 5 && p.backlog() == bl - 3);
  while (p.drainOne(sink)) {}
  bool ordered = true; for (size_t k = 1; k < fed.size(); ++k) ordered &= fed[k] > fed[k - 1];
  CHECK(ordered && fed.back() == id - 1);                                    // every block once, in order
  // full ring: the oldest waiting block is fed at once
  WaveformPause q; q.begin(storage); fed.clear(); id = 1000;
  q.declared(sink); q.admit(blk(id++, 1900, 90), sink);
  for (int k = 0; k < HOLD_MAX + 3; ++k) q.admit(blk(id++, 1900, 40, 0), sink);
  CHECK(fed.size() == 1 + 3 && q.releasedEarly() == 3);
}

int main() {
  navigatorTests(); tracingTests(); recognizerTests(); pauseTests(); backlogTests(); stopGoTests();
  printf("%s (%d failures)\n", fails ? "UNIT CHECKS FAILED" : "UNIT CHECKS PASSED", fails);
  return fails ? 1 : 0;
}

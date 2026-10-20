// Deterministic software checks of the firmware headers (no railway claims).
//   c++ -std=c++17 -O2 -Wall -Wextra -Werror -Ifirmware/test-programs/NAVI_RVM_CONTINUITY_MANUAL tests/navi_rvm_continuity/unit.cpp
#include <cstdio>
#include <cmath>
#include <vector>
#include <random>
#include "RvmTracing.h"
#include "RvmContinuity.h"
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

// ----- HallContinuity (RvmContinuity.h) -----
static constexpr float BAND_TOP_MARGIN = 2 * CONT_BAND;
struct Run { std::vector<uint32_t> fed; std::vector<uint32_t> declaredAt; };
static Run runCont(const std::vector<float>& h, int declareAt, bool viaRequest = false) {
  static Block storage[CONT_K]; HallContinuity c; c.begin(storage); Run r;
  if (declareAt >= 0 && !viaRequest) c.declareAt(uint32_t(declareAt), 1, 2, 1);
  auto emit = [&](const Block& b) { r.fed.push_back(b.index); };
  auto dec = [&](const ContDecl& d) { r.declaredAt.push_back(d.index); CHECK(d.last == 1 && d.next == 2 && d.dir == 1); };
  for (size_t i = 0; i < h.size(); ++i) {
    if (viaRequest && int(i) == declareAt) c.requestDeclaration(1, 2, 1);
    const size_t before = r.fed.size();
    c.admit(blk(uint32_t(i), h[i], 0), emit, dec);
    CHECK(r.fed.size() - before <= 1);                                 // at most one block out per block in
    if (i + 1 < CONT_K) CHECK(r.fed.size() == 0);                      // nothing decided before the delay line fills
  }
  while (c.flushOne(emit, dec)) {}
  return r;
}
static bool fedHas(const Run& r, uint32_t id) { for (uint32_t x : r.fed) if (x == id) return true; return false; }

static void continuityTests() {
  std::mt19937 g(11); std::normal_distribution<double> nz(0, 0.6);
  // 1. A stall inside a magnet: rise to +130, stand still for 30 s (with slow drift), fall back.
  std::vector<float> h;
  for (int i = 0; i < 300; ++i) h.push_back(float(1890 + nz(g)));                       // declared baseline
  for (int i = 0; i < 40; ++i) h.push_back(float(1890 + 130.0 * i / 40 + nz(g)));        // rise (developing)
  for (int i = 0; i < 1500; ++i) h.push_back(float(2020 + 10.0 * i / 1500 + nz(g)));     // stall: drift 10 ADC in 30 s
  for (int i = 0; i < 40; ++i) h.push_back(float(2030 - 140.0 * i / 40 + nz(g)));        // fall (developing again)
  for (int i = 0; i < 300; ++i) h.push_back(float(1890 + nz(g)));
  Run r = runCont(h, 10);
  CHECK(r.declaredAt.size() == 1 && r.declaredAt[0] == 10);

  size_t withheld = h.size() - r.fed.size();
  CHECK(withheld >= 1450 && withheld <= 1545);                          // the plateau, no more
  for (uint32_t i = 0; i < 300; ++i) CHECK(fedHas(r, i));              // baseline before: all kept
  for (uint32_t i = 1880; i < 2180; ++i) CHECK(fedHas(r, i));          // baseline after: all kept
  CHECK(fedHas(r, 320) && fedHas(r, 1860));                            // the rise and the fall themselves are kept
  for (uint32_t i = 400; i < 1800; ++i) CHECK(!fedHas(r, i));          // the stationary middle is withheld
  for (size_t k = 1; k < r.fed.size(); ++k) CHECK(r.fed[k] > r.fed[k - 1]);   // chronological
  // 2. A long flat baseline (moving or stopped) is never withheld.
  std::vector<float> flat(3000); for (auto& x : flat) x = float(1890 + nz(g));
  r = runCont(flat, 0); CHECK(r.fed.size() == flat.size());
  // 3. Before any declaration nothing is withheld, even a plateau.
  r = runCont(h, -1); CHECK(r.fed.size() == h.size() && r.declaredAt.empty());
  // 4. A slow passage (4 s, amplitude 150): its developing flanks are kept. Withheld are only blocks
  //    inside a 1 s span that changes by <= BAND: the flattest top, and the first / last few blocks of
  //    the excursion where they still lie within BAND of the flat baseline before / after it
  //    (a property of the evaluated rule, identical in the Python model).
  std::vector<float> bump;
  for (int i = 0; i < 300; ++i) bump.push_back(float(1890 + nz(g)));
  for (int i = 0; i < 200; ++i) bump.push_back(float(1890 + 150.0 * sin(3.14159265 * i / 200.0) + nz(g)));  // 4 s passage
  for (int i = 0; i < 300; ++i) bump.push_back(float(1890 + nz(g)));
  r = runCont(bump, 0);
  size_t lost = bump.size() - r.fed.size();
  CHECK(lost > 0 && lost <= 110);
  for (uint32_t i = 0; i < bump.size(); ++i)
    if (!fedHas(r, i)) CHECK(bump[i] > 1890 + 150 - BAND_TOP_MARGIN || bump[i] - 1890 <= CONT_BAND + 2);
  CHECK(fedHas(r, 330) && fedHas(r, 470));                              // the developing flanks are kept
  // 5. A plateau with a gap (NaN) inside a span is not flat over that span.
  std::vector<float> g2 = h; for (int i = 900; i < 905; ++i) g2[i] = NAN;
  r = runCont(g2, 10); CHECK(fedHas(r, 900) && fedHas(r, 904));        // gap blocks themselves pass (NaN is never off-baseline)
  // 6. A declaration requested live applies to the next admitted block, after the delay line.
  r = runCont(h, 10, true); CHECK(r.declaredAt.size() == 1 && r.declaredAt[0] == 10);
  // 7. A stall ON the baseline (flat, within M*noise of it) is kept.
  std::vector<float> bs; for (int i = 0; i < 2000; ++i) bs.push_back(float(1890 + (i > 500 && i < 1500 ? 3.0 : 0.0) + nz(g)));
  r = runCont(bs, 0); CHECK(r.fed.size() == bs.size());
}

int main() {
  navigatorTests(); tracingTests(); recognizerTests(); continuityTests();
  if (fails) { printf("%d CHECK(S) FAILED\n", fails); return 1; }
  printf("ALL UNIT CHECKS PASSED\n"); return 0;
}

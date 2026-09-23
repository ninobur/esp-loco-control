// DOES THE TWO-SIDED VIEW TELL A MAGNET FROM AN ARTIFACT?
//
// calibrate.cpp measured what the halves of a REAL magnet do: they agree on
// amplitude to 2% median, 8% at p99. That is only half an answer. The other
// half is whether the things the recognizer must keep refusing -- the four
// adversarial non-magnets of gate 12 section E -- fail the same test, and
// whether the real interrupted Arches crossings of 2026-09-02 pass it.
//
// Every waveform here goes through the real capture and the real recognizer,
// then is split at the boundary the capture itself recorded (stitchAt) and
// each half is fitted with TwoSided.h's log-parabola.
//
//   c++ -std=c++17 -O1 -w tools/two_sided/separate.cpp -o /tmp/sep && /tmp/sep
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#define main gate12_main_unused
#include "../../firmware/programs/NAVI_ONE/variants/NAVI_ONE/tests/gate_interrupted.cpp"
#undef main
#include "../../firmware/programs/NAVI_ONE/variants/NAVI_ONE/TwoSided.h"
#include "../../firmware/programs/NAVI_ONE/variants/NAVI_ONE/tests/fixtures_arches.h"

struct Split {
  const char* what; bool wantMagnet;
  int n, boundary, peak; float whole;
  SideFit a, d; bool ok;
  float agree;
};

static void fitBoth(const int16_t* judged, int n, int pre, int boundary, int peak, Split& s) {
  // arrival [pre, boundary), departure [boundary, n). If there is no boundary
  // (an uninterrupted record) split at the apex instead, as calibrate.cpp does.
  int b = boundary;
  if (b <= 0) { int16_t pk = judged[pre]; b = pre;
    for (int i = pre + 1; i < n; ++i) if (judged[i] > pk) { pk = judged[i]; b = i; } }
  s.boundary = b;
  const bool okA = fitHalfGaussian(judged, (uint16_t)pre, (uint16_t)(boundary > 0 ? b : b + 1), 0.20f, (int16_t)peak, s.a);
  const bool okD = fitHalfGaussian(judged, (uint16_t)b, (uint16_t)n, 0.20f, (int16_t)peak, s.d);
  s.ok = okA && okD;
  const float mx = std::max(s.a.amp, s.d.amp);
  s.agree = (s.ok && mx > 0) ? std::fabs(s.a.amp - s.d.amp) / mx : 1.0f;
}

static void show(const Split& s) {
  printf("  %-46s %5d %5s", s.what, s.peak, s.whole > 0 ? "" : "");
  printf(" %6.4f %s |", s.whole, s.whole > 0.13f ? "REF" : "acc");
  if (!s.ok) { printf("   one half would not fit a Gaussian at all\n"); return; }
  printf(" %6.0f %6.0f %6.1f%% | %6.1f %6.1f | %6.4f %6.4f | %s\n",
         s.a.amp, s.d.amp, s.agree * 100, s.a.sigma, s.d.sigma,
         s.a.residual, s.d.residual,
         s.a.reach > 3 || s.d.reach > 3 ? "apex far outside" : "");
}

int main() {
  printf("SPLIT AT THE CAPTURE'S OWN BOUNDARY, EACH HALF FITTED ON ITS OWN\n");
  printf("  whole  = the unchanged recognizer's residual (acc <= 0.13 < REF)\n");
  printf("  Aarr/Adep = amplitude each half extrapolates to; agree = |Aarr-Adep|/max\n");
  printf("  sArr/sDep = each half's own width; rArr/rDep = each half's own residual\n\n");
  printf("  %-46s %5s %6s     | %6s %6s %7s | %6s %6s | %6s %6s |\n",
         "", "peak", "whole", "Aarr", "Adep", "agree", "sArr", "sDep", "rArr", "rDep");

  // ---- A. the real Arches records of 2026-09-02, split where they were joined
  printf("\n  REAL INTERRUPTED CROSSINGS (must be accepted -- they are magnets):\n");
  // fixtures_arches.h carries no sample count; these are the generator's own
  // reported lengths for each record.
  struct AF { const ArchesFixture* f; int n; };
  const AF AFS[] = {
    { &AX_A1967_CW,   179 }, { &AX_A1818_CW,  291 }, { &AX_A2680_CW,  317 },
    { &AX_A2548_CW,   318 }, { &AX_A1810_CW,  254 }, { &AX_A2695_CCW, 242 },
    { &AX_A5586_CCW,  280 },
  };
  for (const AF& x : AFS) {
    const ArchesFixture& F = *x.f;
    std::vector<int16_t> o(F.v, F.v + x.n), j(x.n);
    medianOfThree(o.data(), (uint16_t)x.n, j.data());
    Passage p; p.oriented = o.data(); p.judged = j.data();
    p.sampleCount = (uint16_t)x.n; p.preSamples = 12;
    int16_t pk = 0; for (int k = 12; k < x.n; ++k) pk = std::max(pk, j[k]);
    p.peakCounts = (uint16_t)pk;
    Split s{}; s.what = F.tag; s.wantMagnet = true; s.n = x.n; s.peak = pk;
    float w = 0; MagnetRecognizer::fitResidual(p, w); s.whole = w;
    fitBoth(j.data(), x.n, 12, F.splice, pk, s);
    char nm[80]; snprintf(nm, sizeof nm, "%s  (%s)", F.tag, F.splice >= 0 ? "at its join" : "ARRIVAL ONLY, no join");
    s.what = nm; show(s);
  }

  // ---- B. the four adversarial non-magnets, through the real capture
  printf("\n  ADVERSARIAL NON-MAGNETS (must stay refused):\n");
  static const char* NAMES[4] = {
    "DC ramp into an 80-count plateau",
    "electrical step, slow leading edge, parked",
    "shoulder: two overlapping lobes",
    "double-lobed non-magnet, stopped in the notch",
  };
  for (int c = 0; c < 4; ++c) {
    Rig rg; uint32_t t = 1;
    primeCapture(rg, t); rg.nav.declare(20, +1); primeLap(rg, 10, (int)AMP, t);
    rg.clearTrace();
    const int sign = polarityAt(nextMarker(rg.nav.status().navMm, rg.nav.status().navDir)) ? 1 : -1;
    std::vector<int> f;
    auto push = [&](int v, int n) { for (int i = 0; i < n; ++i) f.push_back(v); };
    switch (c) {
      case 0: for (int i = 0; i < 3000; ++i) f.push_back(80 * i / 3000); push(80, 12000);
              for (int i = 0; i < 3000; ++i) f.push_back(80 - 80 * i / 3000); push(0, 3000); break;
      case 1: for (int i = 0; i < 260; ++i) f.push_back(95 * i / 260); push(95, 12000);
              for (int i = 0; i < 200; ++i) f.push_back(95 - 95 * i / 200); push(0, 3000); break;
      case 2: for (int i = 0; i < 900; ++i) f.push_back((int)(gaussAt(i, 400, 90, 150) + gaussAt(i, 780, 90, 200)));
              push((int)(gaussAt(900, 400, 90, 150) + gaussAt(900, 780, 90, 200)), 12000);
              for (int i = 900; i < 1600; ++i) f.push_back((int)(gaussAt(i, 400, 90, 150) + gaussAt(i, 780, 90, 200)));
              push(0, 3000); break;
      case 3: for (int i = 0; i < 500; ++i) f.push_back((int)(gaussAt(i, 300, 130, 190) + gaussAt(i, 700, 130, 190)));
              push((int)(gaussAt(500, 300, 130, 190) + gaussAt(500, 700, 130, 190)), 12000);
              for (int i = 500; i < 1100; ++i) f.push_back((int)(gaussAt(i, 300, 130, 190) + gaussAt(i, 700, 130, 190)));
              push(0, 3000); break;
    }
    bool got = false; Split s{}; s.what = NAMES[c]; s.wantMagnet = false;
    std::vector<int16_t> o, j;
    for (size_t i = 0; i < f.size(); ++i, ++t) {
      const size_t tail = f.size() - 3200;
      rg.arm = (i < 300) ? StopArming::None : (i < tail ? StopArming::Decelerating : StopArming::Departing);
      rg.actualPwm = (rg.arm == StopArming::Decelerating) ? 0 : 60; rg.rampTarget = rg.actualPwm;
      rg.tick(t, (int16_t)(IDLE + sign * f[i]));
      if (!got && rg.cap.event() == HallEvent::Passage) {
        const Passage& p = rg.cap.passage();
        o.assign(p.oriented, p.oriented + p.sampleCount); j.assign(p.judged, p.judged + p.sampleCount);
        s.n = p.sampleCount; s.peak = p.peakCounts; s.whole = rg.lastResidual;
        fitBoth(j.data(), p.sampleCount, p.preSamples, p.stitchAt, p.peakCounts, s);
        got = true;
      }
    }
    if (!got) { printf("  %-46s   (no passage closed)\n", NAMES[c]); continue; }
    show(s);
  }
  return 0;
}

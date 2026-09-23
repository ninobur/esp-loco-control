// THE ARCHAEOLOGY, run over everything we have. This calls the firmware's own
// examineInterrupted() -- there is no second copy -- on:
//   * 312 real passages, made interrupted four ways: a rising tail against a
//     full departure; a full arrival against a falling tail; the apex EXCISED
//     (both fragments cut at 97%); both fragments through the apex
//   * the real Arches records of 2026-09-02 at their joins
//   * the four adversarial non-magnets, through the real capture
//
//   c++ -std=c++17 -O1 -w tools/two_sided/archaeology.cpp -o /tmp/arch && /tmp/arch passages.txt
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <sstream>
#include <fstream>
#define main gate12_main_unused
#include "../../firmware/programs/NAVI_ONE/variants/NAVI_ONE/tests/gate_interrupted.cpp"
#undef main
#include "../../firmware/programs/NAVI_ONE/variants/NAVI_ONE/TwoSided.h"
#include "../../firmware/programs/NAVI_ONE/variants/NAVI_ONE/tests/fixtures_arches.h"

static ArchVerdict judge(const std::vector<int16_t>& j, int pre, int join, int16_t peak) {
  Passage p; p.judged = j.data(); p.oriented = j.data();
  p.sampleCount = (uint16_t)j.size(); p.preSamples = (uint16_t)pre;
  p.stitchAt = (uint16_t)join; p.peakCounts = (uint16_t)peak;
  return examineInterrupted(p, 0.13f);
}

int main(int argc, char** argv) {
  if (argc < 2) { fprintf(stderr, "usage: archaeology <passages.txt>\n"); return 2; }
  std::ifstream in(argv[1]); std::string line;
  struct P { std::vector<int16_t> j; int pre, peakAt; int16_t peak; };
  std::vector<P> ps;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream ss(line); std::string tag; int N, pre; ss >> tag >> N >> pre;
    std::vector<int16_t> o(N); for (int i = 0; i < N; ++i) { int x; ss >> x; o[i] = (int16_t)x; }
    P p; p.j.resize(N); medianOfThree(o.data(), (uint16_t)N, p.j.data()); p.pre = pre;
    p.peakAt = pre; p.peak = p.j[pre]; for (int i = pre + 1; i < N; ++i) if (p.j[i] > p.peak) { p.peak = p.j[i]; p.peakAt = i; }
    ps.push_back(p);
  }
  auto cutAt = [](const P& p, float f, bool rising) {   // index where the arc crosses f*peak
    if (rising) { for (int i = p.pre; i <= p.peakAt; ++i) if (p.j[i] >= f * p.peak) return i; return p.peakAt; }
    for (int i = (int)p.j.size() - 1; i >= p.peakAt; --i) if (p.j[i] >= f * p.peak) return i + 1;
    return p.peakAt + 1;
  };
  struct Case { const char* name; float cutR, cutF; int kDep; };
  const Case CASES[] = {
    { "rising tail (45%) + full departure",          0.45f, 0.0f, 1 },
    { "rising tail (45%) + full departure at 3x",    0.45f, 0.0f, 3 },
    { "full arrival + falling tail (from 45%)",      0.0f, 0.45f, 1 },
    { "rising tail (75%) + full departure",          0.75f, 0.0f, 1 },
    { "apex EXCISED: both cut at 97%",               0.97f, 0.97f, 1 },
    { "apex EXCISED: both cut at 85%",               0.85f, 0.85f, 1 },
    { "apex EXCISED: both cut at 60%",               0.60f, 0.60f, 1 },
    { "both fragments through the apex (split there)", 1.0f, 1.0f, 1 },
  };
  printf("312 REAL PASSAGES, made interrupted:\n");
  printf("  %-48s %7s %7s %7s | %s\n", "", "MAGNET", "INSUFF", "WRONG", "why, when not magnet");
  for (const Case& c : CASES) {
    int mag = 0, ins = 0, wr = 0; std::map<std::string, int> whys; std::vector<float> res;
    for (const P& p : ps) {
      std::vector<int16_t> rec; int join;
      if (c.cutR >= 1.0f && c.cutF >= 1.0f) {            // split at the apex, both full
        rec.assign(p.j.begin() + p.pre, p.j.end()); join = p.peakAt - p.pre;
      } else if (c.cutR > 0 && c.cutF > 0) {             // excise the top
        const int a = cutAt(p, c.cutR, true), b = cutAt(p, c.cutF, false);
        rec.assign(p.j.begin() + p.pre, p.j.begin() + a); join = (int)rec.size();
        rec.insert(rec.end(), p.j.begin() + b, p.j.end());
      } else if (c.cutR > 0) {                           // rising tail + departure (from the cut on)
        const int a = cutAt(p, c.cutR, true);
        rec.assign(p.j.begin() + p.pre, p.j.begin() + a); join = (int)rec.size();
        for (int i = a; i < (int)p.j.size(); i += c.kDep) rec.push_back(p.j[i]);
      } else {                                           // arrival through the apex + falling tail
        const int b = cutAt(p, c.cutF, false);
        rec.assign(p.j.begin() + p.pre, p.j.begin() + b); join = (int)rec.size();   // hmm: no gap here
        // make the departure the far tail only: drop from the apex down to the cut
        rec.assign(p.j.begin() + p.pre, p.j.begin() + p.peakAt + 1); join = (int)rec.size();
        rec.insert(rec.end(), p.j.begin() + b, p.j.end());
      }
      if (join < 6 || (int)rec.size() - join < 6) continue;
      // the record's own peak, as the capture would report it
      int16_t pk = 0; for (auto v : rec) pk = std::max(pk, v);
      ArchVerdict v = judge(rec, 0, join, pk);
      if (v.outcome == Arch::Magnet) { ++mag; res.push_back(std::max(v.residArr, v.residDep)); }
      else if (v.outcome == Arch::Insufficient) { ++ins; ++whys[v.why]; }
      else { ++wr; ++whys[v.why]; }
    }
    std::sort(res.begin(), res.end());
    printf("  %-48s %7d %7d %7d |", c.name, mag, ins, wr);
    for (auto& w : whys) printf(" %s x%d;", w.first.c_str(), w.second);
    if (!res.empty()) printf("   worst accepted fragment residual %.4f", res.back());
    printf("\n");
  }

  printf("\nTHE REAL ARCHES RECORDS OF 2026-09-02 (all refused by the single fit):\n");
  struct AF { const ArchesFixture* f; int n; };
  const AF AFS[] = { { &AX_A1967_CW, 179 }, { &AX_A1818_CW, 291 }, { &AX_A1810_CW, 254 },
                     { &AX_A2695_CCW, 242 }, { &AX_A5586_CCW, 280 } };
  for (const AF& x : AFS) {
    std::vector<int16_t> o(x.f->v, x.f->v + x.n), j(x.n); medianOfThree(o.data(), (uint16_t)x.n, j.data());
    int16_t pk = 0; for (int i = 12; i < x.n; ++i) pk = std::max(pk, j[i]);
    if (x.f->splice < 0) { printf("  %-14s %-8s no join in the record: single fit refused it, archaeology never sees it\n", x.f->tag, ""); continue; }
    ArchVerdict v = judge(j, 12, x.f->splice, pk);
    printf("  %-14s %-12s %-40s amp %4.0f  sArr %5.1f sDep %5.1f  rArr %.4f rDep %.4f  trunk=%d\n",
           x.f->tag, archName(v.outcome), v.why, v.amp, v.sigmaArr, v.sigmaDep, v.residArr, v.residDep, v.trunk);
  }

  printf("\nTHE FOUR ADVERSARIAL NON-MAGNETS, through the real capture:\n");
  static const char* NAMES[4] = { "DC ramp into an 80-count plateau", "electrical step, slow leading edge, parked",
                                  "shoulder: two overlapping lobes", "double-lobed non-magnet, stopped in the notch" };
  for (int c = 0; c < 4; ++c) {
    Rig rg; uint32_t t = 1;
    primeCapture(rg, t); rg.nav.declare(20, +1); primeLap(rg, 10, (int)AMP, t); rg.clearTrace();
    const int sign = polarityAt(nextMarker(rg.nav.status().navMm, rg.nav.status().navDir)) ? 1 : -1;
    std::vector<int> f; auto push = [&](int v, int n) { for (int i = 0; i < n; ++i) f.push_back(v); };
    switch (c) {
      case 0: for (int i = 0; i < 3000; ++i) f.push_back(80 * i / 3000); push(80, 12000); for (int i = 0; i < 3000; ++i) f.push_back(80 - 80 * i / 3000); push(0, 3000); break;
      case 1: for (int i = 0; i < 260; ++i) f.push_back(95 * i / 260); push(95, 12000); for (int i = 0; i < 200; ++i) f.push_back(95 - 95 * i / 200); push(0, 3000); break;
      case 2: for (int i = 0; i < 900; ++i) f.push_back((int)(gaussAt(i, 400, 90, 150) + gaussAt(i, 780, 90, 200))); push((int)(gaussAt(900, 400, 90, 150) + gaussAt(900, 780, 90, 200)), 12000); for (int i = 900; i < 1600; ++i) f.push_back((int)(gaussAt(i, 400, 90, 150) + gaussAt(i, 780, 90, 200))); push(0, 3000); break;
      case 3: for (int i = 0; i < 500; ++i) f.push_back((int)(gaussAt(i, 300, 130, 190) + gaussAt(i, 700, 130, 190))); push((int)(gaussAt(500, 300, 130, 190) + gaussAt(500, 700, 130, 190)), 12000); for (int i = 500; i < 1100; ++i) f.push_back((int)(gaussAt(i, 300, 130, 190) + gaussAt(i, 700, 130, 190))); push(0, 3000); break;
    }
    bool got = false;
    for (size_t i = 0; i < f.size(); ++i, ++t) {
      const size_t tail = f.size() - 3200;
      rg.arm = (i < 300) ? StopArming::None : (i < tail ? StopArming::Decelerating : StopArming::Departing);
      rg.actualPwm = (rg.arm == StopArming::Decelerating) ? 0 : 60; rg.rampTarget = rg.actualPwm;
      rg.tick(t, (int16_t)(IDLE + sign * f[i]));
      if (!got && rg.cap.event() == HallEvent::Passage) {
        const Passage& p = rg.cap.passage(); got = true;
        if (!p.stitchAt) { printf("  %-46s not stitched (single fit: %s)\n", NAMES[c], rg.lastResidual > 0.13f ? "refused" : "ACCEPTED"); break; }
        ArchVerdict v = examineInterrupted(p, 0.13f);
        printf("  %-46s %-12s %-40s amp %4.0f rArr %.4f rDep %.4f\n", NAMES[c], archName(v.outcome), v.why, v.amp, v.residArr, v.residDep);
      }
    }
    if (!got) printf("  %-46s (no passage closed)\n", NAMES[c]);
  }
  return 0;
}

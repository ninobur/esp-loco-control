// THE TRUNK SETS THE SIZE; THE TAIL MUST FIT IT.
//
// twospeed.cpp showed the symmetric test cannot work: a flank cut at 45% of
// peak carries almost no curvature, so a free fit of its amplitude is
// ill-conditioned (median disagreement 45%, at 1x speed as much as at 4x).
// A tail on its own cannot say how big the elephant is.
//
// But given the size, it CAN say whether it belongs. So:
//
//   1. The half that CONTAINS the apex is fitted freely -- amplitude A,
//      centre, width. That is the trunk, and it is well-conditioned because
//      the curvature that pins A is right there.
//   2. The other half is fitted with A FIXED to the trunk's. Only its own
//      width and centre are free. Its residual against THAT Gaussian is the
//      test: does this tail fit an elephant of this size?
//   3. Ordering: the tail's centre must lie beyond its own segment on the
//      side of the trunk -- it never reached the apex. Two centres inside
//      their own segments is two lobes and is refused.
//
// Measured on the 312 real passages cut at 45% on the rising flank with the
// departure replayed at 1x..4x; on the real Arches records at their joins;
// on the electrical step and the double lobe.
//
//   c++ -std=c++17 -O1 -w tools/two_sided/consistency.cpp -o /tmp/cs && /tmp/cs passages.txt
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <fstream>
#include "../../firmware/programs/NAVI_ONE/variants/NAVI_ONE/MagnetRecognizer.h"
#include "../../firmware/programs/NAVI_ONE/variants/NAVI_ONE/TwoSided.h"
#include "../../firmware/programs/NAVI_ONE/variants/NAVI_ONE/tests/fixtures_arches.h"
using namespace navi_one;

static float pct(std::vector<float> v, double p) {
  if (v.empty()) return 0; std::sort(v.begin(), v.end());
  return v[(size_t)(p * (v.size() - 1) + 0.5)];
}

// Fit a flank with A fixed: for each candidate width the centre follows in
// closed form from every sample (a rising flank puts the apex AHEAD, a falling
// one BEHIND), take the median centre, score the residual, keep the best.
struct Tail { float sigma = 0, centre = 0, residual = 1; bool ok = false; };
static Tail fitTail(const int16_t* y, int from, int to, float A, bool rising, float floorFrac, int16_t peak) {
  Tail best;
  const float thr = floorFrac * (float)peak;
  std::vector<int> idx; for (int i = from; i < to; ++i) if (y[i] > thr && y[i] > 1 && y[i] < A) idx.push_back(i);
  if (idx.size() < 5) return best;
  for (float s = 6.0f; s <= 600.0f; s *= 1.06f) {
    std::vector<float> cs;
    for (int i : idx) {
      const float r = sqrtf(-2.0f * s * s * logf((float)y[i] / A));
      cs.push_back(rising ? (float)i + r : (float)i - r);
    }
    std::sort(cs.begin(), cs.end());
    const float c = cs[cs.size() / 2];
    double se = 0; int n = 0;
    for (int i = from; i < to; ++i) {
      if (y[i] <= thr || y[i] <= 1) continue;
      const float dx = ((float)i - c) / s;
      const float e = (float)y[i] - A * expf(-0.5f * dx * dx);
      se += (double)e * e; ++n;
    }
    const float res = (float)(sqrt(se / n) / A);
    if (!best.ok || res < best.residual) { best.ok = true; best.residual = res; best.sigma = s; best.centre = c; }
  }
  return best;
}

struct Verdict2 { bool ok; float trunkAmp, trunkRes, tailRes, widthRatio; bool oneApex; const char* trunk; };

// rec = arrival [0,b) then departure [b,n). Which half holds the apex?
static Verdict2 judge(const int16_t* rec, int b, int n, int16_t peak) {
  Verdict2 v{}; SideFit a, d;
  const bool okA = fitHalfGaussian(rec, 0, (uint16_t)b, 0.20f, peak, a);
  const bool okD = fitHalfGaussian(rec, (uint16_t)b, (uint16_t)n, 0.20f, peak, d);
  const bool aIn = okA && a.centre >= 0 && a.centre < b;
  const bool dIn = okD && d.centre >= b && d.centre < n;
  if (aIn && dIn) { v.ok = false; v.oneApex = false; v.trunk = "TWO APEXES"; return v; }
  v.oneApex = true;
  if (dIn) {            // trunk is the departure; arrival is a rising tail
    Tail t = fitTail(rec, 0, b, d.amp, true, 0.20f, peak);
    v.ok = t.ok; v.trunkAmp = d.amp; v.trunkRes = d.residual; v.tailRes = t.residual;
    v.widthRatio = t.sigma / d.sigma; v.trunk = "departure";
    if (t.ok && !(t.centre >= b - 1)) { v.ok = false; v.trunk = "tail apex misplaced"; }
  } else if (aIn) {     // trunk is the arrival; departure is a falling tail
    Tail t = fitTail(rec, b, n, a.amp, false, 0.20f, peak);
    v.ok = t.ok; v.trunkAmp = a.amp; v.trunkRes = a.residual; v.tailRes = t.residual;
    v.widthRatio = a.sigma / t.sigma; v.trunk = "arrival";
    if (t.ok && !(t.centre <= b)) { v.ok = false; v.trunk = "tail apex misplaced"; }
  } else {              // apex in neither: it sits at the join, or was excised
    v.ok = false; v.trunk = "NO APEX in either half";
  }
  return v;
}

int main(int argc, char** argv) {
  if (argc < 2) { fprintf(stderr, "usage: consistency <passages.txt>\n"); return 2; }
  std::ifstream in(argv[1]); std::string line;
  printf("312 REAL PASSAGES, cut at 45%% of peak on the rising flank, departure at k x speed\n");
  for (int k : { 1, 2, 4 }) {
    in.clear(); in.seekg(0);
    std::vector<float> tail, trunk, wr; int n = 0, fail = 0, twoApex = 0, noApex = 0;
    while (std::getline(in, line)) {
      if (line.empty() || line[0] == '#') continue;
      std::istringstream ss(line); std::string tag; int N, pre; ss >> tag >> N >> pre;
      std::vector<int16_t> o(N); for (int i = 0; i < N; ++i) { int x; ss >> x; o[i] = (int16_t)x; }
      std::vector<int16_t> j(N); medianOfThree(o.data(), (uint16_t)N, j.data());
      int peakAt = pre; int16_t peak = j[pre];
      for (int i = pre + 1; i < N; ++i) if (j[i] > peak) { peak = j[i]; peakAt = i; }
      int cut = pre; for (int i = pre; i < peakAt; ++i) if (j[i] >= 0.45f * peak) { cut = i; break; }
      if (cut <= pre + 4) continue;
      std::vector<int16_t> rec(j.begin() + pre, j.begin() + cut);
      for (int i = cut; i < N; i += k) rec.push_back(j[i]);
      const int b = cut - pre;
      Verdict2 v = judge(rec.data(), b, (int)rec.size(), peak);
      ++n;
      if (!v.oneApex) { ++twoApex; continue; }
      if (!v.ok) { ++fail; if (strstr(v.trunk, "NO APEX")) ++noApex; continue; }
      tail.push_back(v.tailRes); trunk.push_back(v.trunkRes); wr.push_back(v.widthRatio);
    }
    printf("\n  k=%d: %d passages -- tail fitted %zu, could not %d (no apex in either half %d), two apexes %d\n",
           k, n, tail.size(), fail, noApex, twoApex);
    printf("     TAIL residual against the trunk's amplitude  median %.4f  p95 %.4f  p99 %.4f  max %.4f\n",
           pct(tail,.5), pct(tail,.95), pct(tail,.99), pct(tail,1));
    printf("     trunk residual (free fit)                    median %.4f  p95 %.4f\n", pct(trunk,.5), pct(trunk,.95));
    printf("     width ratio arrival/departure                median %.2f   (expected about %d)\n", pct(wr,.5), k);
    int over = 0; for (float t : tail) if (t > 0.13f) ++over;
    printf("     tails over the 0.13 ceiling: %d of %zu\n", over, tail.size());
  }

  auto show = [&](const char* name, const int16_t* rec, int b, int n, int16_t peak) {
    Verdict2 v = judge(rec, b, n, peak);
    printf("  %-44s ", name);
    if (!v.oneApex) { printf("REFUSED: two apexes\n"); return; }
    if (!v.ok)      { printf("REFUSED: %s\n", v.trunk); return; }
    printf("trunk=%-9s A=%4.0f trunkRes %.4f  TAIL res %.4f  width x%.2f  -> %s\n",
           v.trunk, v.trunkAmp, v.trunkRes, v.tailRes, v.widthRatio,
           v.tailRes <= 0.13f ? "ACCEPT" : "REFUSED: tail does not fit");
  };

  printf("\nTHE REAL ARCHES RECORDS, at their joins (apex EXCISED by the capture in all of them):\n");
  struct AF { const ArchesFixture* f; int n; };
  const AF AFS[] = { { &AX_A1967_CW, 179 }, { &AX_A1818_CW, 291 }, { &AX_A1810_CW, 254 } };
  for (const AF& x : AFS) {
    std::vector<int16_t> o(x.f->v, x.f->v + x.n), j(x.n); medianOfThree(o.data(), (uint16_t)x.n, j.data());
    int16_t pk = 0; for (int i = 12; i < x.n; ++i) pk = std::max(pk, j[i]);
    show(x.f->tag, j.data() + 12, x.f->splice - 12, x.n - 12, pk);
  }

  printf("\nTHE ARTIFACTS (must be refused):\n");
  { std::vector<int16_t> f; for (int i = 0; i < 260; ++i) f.push_back((int16_t)(95 * i / 260));
    for (int i = 0; i < 200; ++i) f.push_back((int16_t)(95 - 95 * i / 200));
    std::vector<int16_t> j(f.size()); medianOfThree(f.data(), (uint16_t)f.size(), j.data());
    show("electrical step, slow leading edge (join at 260)", j.data(), 260, (int)j.size(), 95); }
  { auto g = [](double x, double c, double s, double A) { return A * exp(-0.5 * (x - c) * (x - c) / (s * s)); };
    std::vector<int16_t> f; for (int i = 0; i < 1100; ++i) f.push_back((int16_t)(g(i,300,130,190) + g(i,700,130,190)));
    std::vector<int16_t> j(f.size()); medianOfThree(f.data(), (uint16_t)f.size(), j.data());
    int16_t pk = 0; for (auto v : j) pk = std::max(pk, v);
    show("double lobe, split in the notch", j.data(), 500, (int)j.size(), pk); }
  { auto g = [](double x, double c, double s, double A) { return A * exp(-0.5 * (x - c) * (x - c) / (s * s)); };
    std::vector<int16_t> f; for (int i = 0; i < 1600; ++i) f.push_back((int16_t)(g(i,400,90,150) + g(i,780,90,200)));
    std::vector<int16_t> j(f.size()); medianOfThree(f.data(), (uint16_t)f.size(), j.data());
    int16_t pk = 0; for (auto v : j) pk = std::max(pk, v);
    show("shoulder: two overlapping lobes (join at 900)", j.data(), 900, (int)j.size(), pk); }
  { std::vector<int16_t> f; for (int i = 0; i < 3000; ++i) f.push_back((int16_t)(80 * i / 3000));
    for (int i = 0; i < 3000; ++i) f.push_back((int16_t)(80 - 80 * i / 3000));
    std::vector<int16_t> j(f.size()); medianOfThree(f.data(), (uint16_t)f.size(), j.data());
    show("DC ramp to 80 and back (join at 3000)", j.data(), 3000, (int)j.size(), 80); }
  return 0;
}

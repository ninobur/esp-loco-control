// THE POSITIVE CONTROL: one real magnet, two speeds, COMPLETE evidence.
//
// separate.cpp showed the two-sided fit does NOT rescue the real Arches
// records -- their halves disagree by 19% to 75% -- because the apex was
// excised from every one of them by the capture. That is the recording fault,
// not the model. This asks the other question: when the capture keeps the
// whole arc and only the SPEED differs between the halves, does the model see
// one magnet?
//
// Every one of the 312 real, accepted passages is cut on its rising flank at
// 45% of peak -- where Toby stalls -- and the departure side is then replayed
// at 2x, 3x and 4x the arrival speed by keeping every k-th sample. Nothing
// else is touched: the samples are the sensor's own. The amplitudes should
// still agree and the widths should differ by k.
//
// And the same treatment for the one artifact that passed amplitude agreement
// in separate.cpp -- the double lobe -- to show what catches it: two apexes.
//
//   c++ -std=c++17 -O1 -w tools/two_sided/twospeed.cpp -o /tmp/ts && /tmp/ts passages.txt
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
using namespace navi_one;

static float pct(std::vector<float> v, double p) {
  if (v.empty()) return 0; std::sort(v.begin(), v.end());
  return v[(size_t)(p * (v.size() - 1) + 0.5)];
}

// One apex across both halves: the arrival's centre may sit inside it only if
// the departure's does not, and vice versa. Two centres inside = two lobes.
static bool oneApex(const SideFit& a, int aFrom, int aTo, const SideFit& d, int dFrom, int dTo) {
  const bool aIn = a.centre >= aFrom && a.centre <= aTo;
  const bool dIn = d.centre >= dFrom && d.centre <= dTo;
  return !(aIn && dIn);
}

int main(int argc, char** argv) {
  if (argc < 2) { fprintf(stderr, "usage: twospeed <passages.txt>\n"); return 2; }
  std::ifstream in(argv[1]); std::string line;
  for (int k : { 1, 2, 3, 4 }) {
    in.clear(); in.seekg(0);
    std::vector<float> agree, sig, rA, rD; int n = 0, apexOK = 0, fitFail = 0;
    while (std::getline(in, line)) {
      if (line.empty() || line[0] == '#') continue;
      std::istringstream ss(line); std::string tag; int N, pre;
      ss >> tag >> N >> pre;
      std::vector<int16_t> o(N); for (int i = 0; i < N; ++i) { int x; ss >> x; o[i] = (int16_t)x; }
      std::vector<int16_t> j(N); medianOfThree(o.data(), (uint16_t)N, j.data());
      int peakAt = pre; int16_t peak = j[pre];
      for (int i = pre + 1; i < N; ++i) if (j[i] > peak) { peak = j[i]; peakAt = i; }
      // cut on the rising flank at 45% of peak
      int cut = pre; for (int i = pre; i < peakAt; ++i) if (j[i] >= 0.45f * peak) { cut = i; break; }
      if (cut <= pre + 4) continue;
      // arrival at native rate; departure keeps every k-th sample from the cut
      std::vector<int16_t> arr(j.begin() + pre, j.begin() + cut);
      std::vector<int16_t> dep; for (int i = cut; i < N; i += k) dep.push_back(j[i]);
      std::vector<int16_t> rec(arr); rec.insert(rec.end(), dep.begin(), dep.end());
      const int b = (int)arr.size();
      SideFit a, d;
      if (!fitHalfGaussian(rec.data(), 0, (uint16_t)b, 0.20f, peak, a) ||
          !fitHalfGaussian(rec.data(), (uint16_t)b, (uint16_t)rec.size(), 0.20f, peak, d)) { ++fitFail; continue; }
      ++n;
      // The trunk sets the size: whichever half holds the apex is the
      // better-determined amplitude, and the other must agree with IT.
      const float ref = (d.centre >= b && d.centre < (float)rec.size()) ? d.amp : a.amp;
      const float oth = (ref == d.amp) ? a.amp : d.amp;
      agree.push_back(std::fabs(ref - oth) / std::max(ref, oth));
      sig.push_back(d.sigma > 0 ? a.sigma / d.sigma : 0);
      rA.push_back(a.residual); rD.push_back(d.residual);
      if (oneApex(a, 0, b - 1, d, b, (int)rec.size() - 1)) ++apexOK;
    }
    printf("departure at %dx the arrival speed  (%d passages, %d would not fit)\n", k, n, fitFail);
    printf("   amplitude disagreement  median %.3f  p90 %.3f  p95 %.3f  p99 %.3f  max %.3f\n",
           pct(agree,.5), pct(agree,.9), pct(agree,.95), pct(agree,.99), pct(agree,1));
    printf("   width ratio arr/dep     median %.2f   (expected about %d)\n", pct(sig,.5), k);
    printf("   half residuals          arrival p95 %.4f   departure p95 %.4f\n", pct(rA,.95), pct(rD,.95));
    printf("   one apex across both    %d of %d\n\n", apexOK, n);
  }
  // the double lobe, split in its notch
  {
    std::vector<int16_t> f;
    auto g = [](double x, double c, double s, double A) { return A * exp(-0.5 * (x - c) * (x - c) / (s * s)); };
    for (int i = 0; i < 1100; ++i) f.push_back((int16_t)(g(i, 300, 130, 190) + g(i, 700, 130, 190)));
    std::vector<int16_t> j(f.size()); medianOfThree(f.data(), (uint16_t)f.size(), j.data());
    int16_t pk = 0; for (auto v : j) pk = std::max(pk, v);
    const int b = 500;
    SideFit a, d;
    fitHalfGaussian(j.data(), 0, b, 0.20f, pk, a); fitHalfGaussian(j.data(), b, (uint16_t)j.size(), 0.20f, pk, d);
    printf("double-lobed non-magnet split in the notch:\n");
    printf("   amplitudes %.0f / %.0f agree to %.1f%%  -- but arrival centre %.0f (inside 0..%d) and\n"
           "   departure centre %.0f (inside %d..%zu): TWO apexes -> one-rise-peak-fall FAILS -> refused\n",
           a.amp, d.amp, 100 * std::fabs(a.amp - d.amp) / std::max(a.amp, d.amp),
           a.centre, b - 1, d.centre, b, j.size() - 1);
  }
  return 0;
}

// HOW CLOSELY DO THE TWO HALVES OF A REAL MAGNET AGREE?
//
// The two-sided interpretation needs one number nobody has measured: when a
// clean, uninterrupted crossing is split at its apex and each half is fitted
// on its own, how far apart are the two amplitudes? That spread is the
// tolerance the model has to allow -- set from the railway's own passages,
// the way settleSpan was set from its own stationary plateaus, not picked.
//
//   c++ -std=c++17 -O1 -Wall tools/two_sided/calibrate.cpp -o /tmp/cal
//   /tmp/cal passages.txt
//
// The fit is TwoSided.h's log-parabola -- the same code the firmware would
// run -- and the whole-passage check is MagnetRecognizer::fitResidual, the
// unchanged recognizer. Only passages the unchanged recognizer would accept
// are measured: this is what magnets look like, not what artifacts look like.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <fstream>
#include "../../firmware/test-programs/NAVI_ONE/MagnetRecognizer.h"
#include "../../firmware/test-programs/NAVI_ONE/TwoSided.h"
using namespace navi_one;

struct Row {
  std::string tag; int n = 0;
  float whole = 0, wholeAmp = 0;
  SideFit a, d;
  float agree = 0, sigRatio = 0;
  int apex = 0, peak = 0;
};

static float pct(std::vector<float> v, double p) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  size_t i = (size_t)(p * (v.size() - 1) + 0.5);
  return v[i];
}

int main(int argc, char** argv) {
  if (argc < 2) { fprintf(stderr, "usage: calibrate <passages.txt>\n"); return 2; }
  std::ifstream in(argv[1]);
  std::string line;
  std::vector<Row> rows;
  int read = 0, notMagnet = 0, noFit = 0;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream ss(line);
    Row r; int pre;
    ss >> r.tag >> r.n >> pre;
    std::vector<int16_t> o(r.n);
    for (int i = 0; i < r.n; ++i) { int x; ss >> x; o[i] = (int16_t)x; }
    ++read;
    std::vector<int16_t> j(r.n);
    medianOfThree(o.data(), (uint16_t)r.n, j.data());
    Passage p;
    p.oriented = o.data(); p.judged = j.data();
    p.sampleCount = (uint16_t)r.n; p.preSamples = (uint16_t)pre;
    // the apex, and the whole-passage fit -- THE UNCHANGED RECOGNIZER
    int peakAt = pre; int16_t peak = j[pre];
    for (int i = pre + 1; i < r.n; ++i) if (j[i] > peak) { peak = j[i]; peakAt = i; }
    r.apex = peakAt; r.peak = peak; p.peakCounts = (uint16_t)peak;
    float whole = 0;
    if (!MagnetRecognizer::fitResidual(p, whole)) { ++noFit; continue; }
    r.whole = whole;
    if (whole > 0.13f) { ++notMagnet; continue; }      // not a clean magnet: skip
    // the two halves, split at the apex, each fitted on its own
    if (!fitHalfGaussian(j.data(), (uint16_t)pre, (uint16_t)(peakAt + 1), 0.20f, peak, r.a) ||
        !fitHalfGaussian(j.data(), (uint16_t)peakAt, (uint16_t)r.n, 0.20f, peak, r.d)) {
      ++noFit; continue;
    }
    const float mx = std::max(r.a.amp, r.d.amp);
    r.agree = mx > 0 ? std::fabs(r.a.amp - r.d.amp) / mx : 1.0f;
    r.sigRatio = r.d.sigma > 0 ? r.a.sigma / r.d.sigma : 0;
    rows.push_back(r);
  }
  printf("passages read %d, whole-fit refused %d, could not fit %d, MEASURED %zu\n\n",
         read, notMagnet, noFit, rows.size());
  if (rows.empty()) return 1;

  std::vector<float> agree, resA, resD, sig, ampVsPeak, whole;
  for (auto& r : rows) {
    agree.push_back(r.agree); resA.push_back(r.a.residual); resD.push_back(r.d.residual);
    sig.push_back(r.sigRatio); whole.push_back(r.whole);
    ampVsPeak.push_back(std::fabs(std::max(r.a.amp, r.d.amp) - (float)r.peak) / (float)r.peak);
  }
  auto show = [&](const char* name, std::vector<float>& v) {
    printf("  %-34s median %.4f  p90 %.4f  p95 %.4f  p99 %.4f  max %.4f\n",
           name, pct(v, .5), pct(v, .9), pct(v, .95), pct(v, .99), pct(v, 1.0));
  };
  printf("ON %zu CLEAN, ACCEPTED, UNINTERRUPTED MAGNET PASSAGES:\n", rows.size());
  show("amplitude DISAGREEMENT |Aa-Ad|/max", agree);
  show("arrival half residual", resA);
  show("departure half residual", resD);
  show("whole-passage residual (control)", whole);
  show("fitted amp vs measured peak", ampVsPeak);
  printf("  %-34s median %.3f  p05 %.3f  p95 %.3f   (1.0 = same speed both sides)\n",
         "sigma ratio arrival/departure", pct(sig, .5), pct(sig, .05), pct(sig, .95));

  // Where would a tolerance land?
  printf("\n  passages whose halves disagree by more than:");
  for (float t : { 0.05f, 0.10f, 0.15f, 0.20f, 0.25f, 0.30f }) {
    int over = 0; for (float a : agree) if (a > t) ++over;
    printf("  %.0f%%:%d", t * 100, over);
  }
  printf("   (of %zu)\n", rows.size());

  // The worst few, for inspection
  std::sort(rows.begin(), rows.end(), [](const Row& x, const Row& y) { return x.agree > y.agree; });
  printf("\n  the eight that disagree most:\n");
  printf("  %-28s %5s %6s %6s %7s %6s %6s %7s %7s\n",
         "tag", "peak", "Aarr", "Adep", "agree", "sArr", "sDep", "rArr", "rDep");
  for (size_t i = 0; i < rows.size() && i < 8; ++i) {
    const Row& r = rows[i];
    printf("  %-28s %5d %6.0f %6.0f %6.1f%% %6.1f %6.1f %7.4f %7.4f\n",
           r.tag.c_str(), r.peak, r.a.amp, r.d.amp, r.agree * 100,
           r.a.sigma, r.d.sigma, r.a.residual, r.d.residual);
  }
  return 0;
}

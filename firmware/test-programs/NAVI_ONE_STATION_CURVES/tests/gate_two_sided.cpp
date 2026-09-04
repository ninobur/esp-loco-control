// GATE 13 -- the archaeology, on the railway's own magnets.
//
// Every labelled primary magnet in the 2026-08-28 survey, untruncated -- 187
// of them, at cruise -- is made interrupted the ways a stop makes one, and
// judged by the firmware's own examineInterrupted(). The four adversarial
// non-magnets of gate 12 E are judged the same way. There is no second copy
// of the judgement here and no threshold of its own: the ceiling is the
// recognizer's 0.13 and the trust rules are the ones TwoSided.h carries,
// each with the measurement it rests on.
//
// What is asserted is the SHAPE of the result, not a count that noise could
// nudge: real magnets restored from a tail and a trunk, or from two halves
// over the top, are accepted almost without exception; a record whose top
// was cut away mostly comes back INSUFFICIENT, never as a confident magnet at
// the wrong scale in more than a small minority; and the non-magnets are
// refused on structure -- no apex, a reversal, two apexes -- never on a
// residual that more retained evidence could flatter.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <zlib.h>
#include "../MagnetRecognizer.h"
using namespace navi_one;

static int checks = 0, failures = 0;
static void ok(bool c, const char* what, const std::string& d = "") {
  ++checks; if (c) return; ++failures;
  printf("  *** FAIL  %s%s%s\n", what, d.empty() ? "" : " -- ", d.c_str());
}

static int b64v(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A'; if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52; if (c == '+') return 62; if (c == '/') return 63; return -1;
}
static std::string field(const std::string& s, const char* key) {
  std::string k = std::string("\"") + key + "\":"; size_t p = s.find(k); if (p == std::string::npos) return "";
  p += k.size(); if (s[p] == '"') { size_t e = s.find('"', p + 1); return s.substr(p + 1, e - p - 1); }
  size_t e = p; while (e < s.size() && s[e] != ',' && s[e] != '}') ++e; return s.substr(p, e - p);
}

struct P { std::vector<int16_t> j; int pre, peakAt; int16_t peak; };

static std::vector<P> survey(const char* path) {
  std::vector<P> out;
  gzFile f = gzopen(path, "rb"); if (!f) { fprintf(stderr, "cannot open %s\n", path); return out; }
  char line[16384];
  while (gzgets(f, line, sizeof line)) {
    std::string s(line); size_t b = s.find('{'); if (b == std::string::npos) continue; s = s.substr(b);
    if (field(s, "rej") != "0" || field(s, "tr") == "1") continue;
    const int sc = atoi(field(s, "sc").c_str()) ? atoi(field(s, "sc").c_str()) : 1;
    const bool north = field(s, "pol") == "N";
    std::vector<int16_t> o; int acc = 0, bits = 0;
    for (char c : field(s, "d")) { int v = b64v(c); if (v < 0) continue; acc = (acc << 6) | v; bits += 6;
      if (bits >= 8) { bits -= 8; int byte = (acc >> bits) & 0xff; o.push_back((int16_t)((byte - 128) * sc * (north ? 1 : -1))); } }
    P p; p.pre = atoi(field(s, "pre").c_str()); p.j.resize(o.size());
    medianOfThree(o.data(), (uint16_t)o.size(), p.j.data());
    p.peakAt = p.pre; p.peak = p.j[p.pre];
    for (int i = p.pre + 1; i < (int)p.j.size(); ++i) if (p.j[i] > p.peak) { p.peak = p.j[i]; p.peakAt = i; }
    if (p.j.size() > 40) out.push_back(p);
  }
  gzclose(f); return out;
}

static ArchVerdict judge(const std::vector<int16_t>& j, int join, int16_t peak, int16_t rest = 0) {
  Passage p; p.judged = j.data(); p.oriented = j.data();
  p.sampleCount = (uint16_t)j.size(); p.preSamples = 0; p.stitchAt = (uint16_t)join;
  p.peakCounts = (uint16_t)peak; p.restLevel = rest;
  return examineInterrupted(p, 0.13f);
}
static int cutAt(const P& p, float f, bool rising) {
  if (rising) { for (int i = p.pre; i <= p.peakAt; ++i) if (p.j[i] >= f * p.peak) return i; return p.peakAt; }
  for (int i = (int)p.j.size() - 1; i >= p.peakAt; --i) if (p.j[i] >= f * p.peak) return i + 1;
  return p.peakAt + 1;
}

int main(int argc, char** argv) {
  const char* path = argc > 1 ? argv[1] : "field-records/logs/20260828_survey/toby_1_13X_survey_waveforms.log.gz";
  printf("gate 13 -- the archaeology on the railway's own magnets\n\n");
  std::vector<P> ps = survey(path);
  printf("A. %zu primary magnets from the 2026-08-28 survey\n", ps.size());
  ok(ps.size() >= 150, "the survey loaded", std::to_string(ps.size()));

  struct Case { const char* name; float cutR, cutF; int kDep; int wantMagnetMinPct, wantMagnetMaxPct; };
  const Case CASES[] = {
    { "a rising tail (45%) and the whole departure",        0.45f, 0.0f, 1, 95, 100 },
    { "the same, departure three times faster",             0.45f, 0.0f, 3, 95, 100 },
    { "a rising tail (75%) and the whole departure",        0.75f, 0.0f, 1, 95, 100 },
    { "both halves over the top, split at the apex",        1.0f,  1.0f, 1, 98, 100 },
    { "the top cut away at 60% both sides (must NOT pass)", 0.60f, 0.60f, 1, 0,  15 },
    { "the top cut away at 85% both sides (must NOT pass)", 0.85f, 0.85f, 1, 0,  30 },
  };
  printf("\n   %-52s %6s %6s %6s\n", "", "MAGNET", "INSUFF", "WRONG");
  for (const Case& c : CASES) {
    int mag = 0, ins = 0, wr = 0, n = 0;
    for (const P& p : ps) {
      std::vector<int16_t> rec; int join;
      if (c.cutR >= 1.0f) { rec.assign(p.j.begin() + p.pre, p.j.end()); join = p.peakAt - p.pre; }
      else if (c.cutF > 0) { const int a = cutAt(p, c.cutR, true), b = cutAt(p, c.cutF, false);
        rec.assign(p.j.begin() + p.pre, p.j.begin() + a); join = (int)rec.size(); rec.insert(rec.end(), p.j.begin() + b, p.j.end()); }
      else { const int a = cutAt(p, c.cutR, true); rec.assign(p.j.begin() + p.pre, p.j.begin() + a); join = (int)rec.size();
        for (int i = a; i < (int)p.j.size(); i += c.kDep) rec.push_back(p.j[i]); }
      if (join < 6 || (int)rec.size() - join < 6) continue;
      int16_t pk = 0; for (auto v : rec) pk = std::max(pk, v);
      ArchVerdict v = judge(rec, join, pk);
      ++n; if (v.outcome == Arch::Magnet) ++mag; else if (v.outcome == Arch::Insufficient) ++ins; else ++wr;
    }
    const int pct = n ? 100 * mag / n : 0;
    printf("   %-52s %6d %6d %6d   (%d%% magnet)\n", c.name, mag, ins, wr, pct);
    ok(pct >= c.wantMagnetMinPct && pct <= c.wantMagnetMaxPct, c.name,
       std::to_string(pct) + "% magnet, wanted " + std::to_string(c.wantMagnetMinPct) + ".." + std::to_string(c.wantMagnetMaxPct));
    if (c.cutF == 0 || c.cutR >= 1.0f) ok(wr <= n / 50, "real magnets are not called WRONG", std::to_string(wr) + " of " + std::to_string(n));
  }

  printf("\nB. the four adversarial non-magnets, split where a stop would split them\n");
  auto g = [](double x, double c, double s, double A) { return A * exp(-0.5 * (x - c) * (x - c) / (s * s)); };
  struct Art { const char* name; std::vector<int16_t> f; int join; int16_t rest; };
  std::vector<Art> arts;
  { std::vector<int16_t> f; for (int i = 0; i < 3000; ++i) f.push_back((int16_t)(80 * i / 3000)); for (int i = 0; i < 3000; ++i) f.push_back((int16_t)(80 - 80 * i / 3000));
    arts.push_back({ "DC ramp to 80 and back, rested on the plateau", f, 3000, 80 }); }
  { std::vector<int16_t> f; for (int i = 0; i < 260; ++i) f.push_back((int16_t)(95 * i / 260)); for (int i = 0; i < 200; ++i) f.push_back((int16_t)(95 - 95 * i / 200));
    arts.push_back({ "electrical step, slow leading edge, parked on it", f, 260, 95 }); }
  { std::vector<int16_t> f; for (int i = 0; i < 1600; ++i) f.push_back((int16_t)(g(i, 400, 90, 150) + g(i, 780, 90, 200)));
    arts.push_back({ "shoulder: two overlapping lobes, stopped on the second", f, 900, 0 }); }
  { std::vector<int16_t> f; for (int i = 0; i < 1100; ++i) f.push_back((int16_t)(g(i, 300, 130, 190) + g(i, 700, 130, 190)));
    arts.push_back({ "double lobe, stopped in the notch", f, 500, 0 }); }
  for (const Art& a : arts) {
    std::vector<int16_t> j(a.f.size()); medianOfThree(a.f.data(), (uint16_t)a.f.size(), j.data());
    int16_t pk = 0; for (auto v : j) pk = std::max(pk, v);
    ArchVerdict v = judge(j, a.join, pk, a.rest);
    printf("   %-56s %-12s %s\n", a.name, archName(v.outcome), v.why);
    ok(v.outcome != Arch::Magnet, "refused", a.name);
  }

  printf("\n%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}

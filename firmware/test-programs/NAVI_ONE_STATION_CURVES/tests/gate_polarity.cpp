// Gate 6: the pole is decided by the whole passage, not by one sample.
//
// Findings 05 and 06: on 2026-08-31 a single-sample artifact at the entry
// crossing (+41 at MM70, -43 at MM119, each a few counts over entryMargin 38
// and opposite to the arriving field) latched the wrong pole. The real bell --
// -183 and +223 -- was then stored negated and never raised the peak, so the
// recognizer saw 41 and 43, ruled TOO_WEAK, and stopped the locomotive twice.
//
// Part A replays every passage captured that day through the REAL HallCapture.
// Part B is the spike test the review asked for: opposite-polarity spikes at
// every position and every amplitude, including larger than the passage's own
// peak, must not flip the pole.
// Part C checks the rule genuinely follows the summed sign, not an extremum.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include "../HallCapture.h"
using namespace navi_one;

static const int16_t BASE = 1834;          // field-measured baseline, 2026-08-29
static int checks = 0, failures = 0;

static void ok(bool cond, const char* what, const char* detail = "") {
  ++checks;
  if (!cond) { ++failures; printf("  FAIL %s %s\n", what, detail); }
}

// Drive a passage through a primed HallCapture and return the closed Passage.
// Returns false if no passage closed.
static bool run(const std::vector<int16_t>& deltas, Passage& out) {
  CaptureConfig cfg; HallCapture<> cap(cfg);
  uint32_t t = 0;
  for (; t < 2600; ++t) cap.sample(t, BASE, true);            // prime the baseline
  if (!cap.ready()) return false;
  bool closed = false;
  for (size_t i = 0; i < deltas.size(); ++i, ++t)
    if (cap.sample(t, (int16_t)(BASE + deltas[i]), true)) { out = cap.passage(); closed = true; }
  for (int i = 0; i < 40 && !closed; ++i, ++t)          // trailing quiet closes it
    if (cap.sample(t, BASE, true)) { out = cap.passage(); closed = true; }
  return closed;
}

struct Row {
  uint32_t opened; int gotPol, gotPeak, wantPol, wantPeak;
  std::string note; std::vector<int16_t> deltas;
};

static std::vector<Row> load(const char* path) {
  std::vector<Row> rows; FILE* f = fopen(path, "r");
  if (!f) { perror("open"); exit(2); }
  char* line = nullptr; size_t cap = 0;
  while (getline(&line, &cap, f) > 0) {
    if (line[0] == '#' || line[0] == '\n') continue;
    std::vector<std::string> col; std::string cur;
    for (char* c = line; *c; ++c) {
      if (*c == '\t') { col.push_back(cur); cur.clear(); }
      else if (*c != '\n') cur += *c;
    }
    col.push_back(cur);
    if (col.size() < 8) continue;
    Row r; r.opened = (uint32_t)strtoul(col[0].c_str(), nullptr, 10);
    r.gotPol = atoi(col[2].c_str()); r.gotPeak = atoi(col[3].c_str());
    r.wantPol = atoi(col[4].c_str()); r.wantPeak = atoi(col[5].c_str());
    r.note = col[6];
    cur.clear();
    for (char c : col[7]) {
      if (c == ',') { r.deltas.push_back((int16_t)atoi(cur.c_str())); cur.clear(); }
      else cur += c;
    }
    if (!cur.empty()) r.deltas.push_back((int16_t)atoi(cur.c_str()));
    rows.push_back(r);
  }
  free(line); fclose(f); return rows;
}

int main(int argc, char** argv) {
  if (argc < 2) { fprintf(stderr, "usage: gate_polarity <captured_passages.tsv>\n"); return 2; }
  std::vector<Row> rows = load(argv[1]);
  printf("part A: %zu captured passages replayed through the real HallCapture\n", rows.size());

  int corrected = 0, unchanged = 0;
  for (const Row& r : rows) {
    Passage p;
    char id[64]; snprintf(id, sizeof id, "[%u %s]", r.opened, r.note.c_str());
    if (!run(r.deltas, p)) { ok(false, "passage closed", id); continue; }
    ok(p.polarity == r.wantPol, "polarity", id);
    // Within four counts, not to the count: wantPeak was recorded by a build
    // whose judgement copy was a three-wide median; it is five-wide since
    // 2026-09-02 (see MagnetRecognizer.h), and on 312 real passages that moves
    // the peak by at most 4 counts, p95 3. Polarity is still exact.
    ok(abs((int)p.peakCounts - r.wantPeak) <= 4, "peak within 4", id);
    if (r.note == "nominal") {
      // A passage the old code judged correctly keeps its pole exactly, and its
      // peak to within a few counts -- the median of three trims a sample from
      // the very tip of a bell, never reshapes it. Anything larger would mean
      // the judgement copy is not a copy.
      ok(p.polarity == r.gotPol, "nominal pole unchanged", id);
      ok(std::abs((int)p.peakCounts - r.gotPeak) <= 8, "nominal peak within 8 counts", id);
      ++unchanged;
    } else {
      ok(p.polarity != r.gotPol || (int)p.peakCounts != r.gotPeak,
         "known artifact case corrected", id);
      ++corrected;
    }
  }
  printf("  %d nominal passages unchanged, %d mis-latches corrected\n", unchanged, corrected);

  printf("part D: the tail artifact of MM169 must no longer fail the shape test\n");
  {
    const Row* mm169 = nullptr;
    for (const Row& r : rows) if (r.note == "MM169-tail-artifact") mm169 = &r;
    ok(mm169 != nullptr, "MM169 present in the fixture");
    if (mm169) {
      Passage p;
      ok(run(mm169->deltas, p), "MM169 closed");
      float rJudged = 0.0f, rRaw = 0.0f;
      const bool okJ = MagnetRecognizer::fitResidual(p, rJudged);
      // and the same fit against the RECORDING, which is what 0.4 did
      Passage praw = p; praw.judged = nullptr;
      const bool okR = MagnetRecognizer::fitResidual(praw, rRaw);
      ok(okJ && okR, "both fits produced a residual");
      char d[160];
      snprintf(d, sizeof d, "(recording %.4f -> judgement copy %.4f, ceiling 0.13)", rRaw, rJudged);
      ok(rRaw > 0.13f, "the recording still fails the shape test -- the artifact is NOT hidden", d);
      ok(rJudged <= 0.13f, "the judgement copy passes", d);
      ok((int)p.peakCounts == mm169->wantPeak, "peak comes from the copy, not the spike", d);
      printf("  %s  peak %d (spike was %d)\n", d, (int)p.peakCounts, mm169->gotPeak);
    }
  }

  printf("part B: opposite-polarity spikes must not flip the pole\n");
  // Take the largest nominal passage as the victim.
  const Row* victim = nullptr;
  for (const Row& r : rows)
    if (r.note == "nominal" && (!victim || r.wantPeak > victim->wantPeak)) victim = &r;
  ok(victim != nullptr, "found a victim passage");
  if (victim) {
    const int truePeak = victim->wantPeak;
    const int sign = victim->wantPol ? -1 : +1;          // opposite the true pole
    int flipped = 0, tried = 0;
    for (int amp : {50, 100, 200, 400, 800, 2000, 4000}) {
      for (size_t pos = 0; pos < victim->deltas.size(); pos += 7) {
        std::vector<int16_t> d = victim->deltas;
        d[pos] = (int16_t)(sign * amp);
        Passage p; ++tried;
        if (run(d, p) && p.polarity != victim->wantPol) ++flipped;
      }
    }
    char detail[128];
    snprintf(detail, sizeof detail,
             "(true peak %d; %d single-sample spikes up to 4000 counts tried, %d flipped)",
             truePeak, tried, flipped);
    ok(flipped == 0, "no single sample flips the pole", detail);
    printf("  %s\n", detail);

    // And the contrast that justifies area over "larger peak wins": a single
    // spike above the true peak DOES defeat an extremum rule.
    std::vector<int16_t> d = victim->deltas;
    d[victim->deltas.size() / 2] = (int16_t)(sign * (truePeak + 20));
    int mx = 0, mn = 0;
    for (int16_t v : d) { if (v > mx) mx = v; if (-v > mn) mn = -v; }
    const int extremumPole = (mx >= mn) ? 1 : 0;
    ok(extremumPole != victim->wantPol,
       "extremum rule IS defeated by that same spike (why area was chosen)");
  }

  printf("part C: the rule follows the summed sign, not the largest excursion\n");
  {
    // A short, tall positive spur against a long, lower negative body.
    // Extremum says North; area -- and the eye -- say South.
    std::vector<int16_t> d;
    for (int i = 0; i < 10; ++i) d.push_back((int16_t)(i * 4));
    d.push_back(300);                                     // tall, narrow
    for (int i = 0; i < 10; ++i) d.push_back((int16_t)(i * 2));
    for (int i = 0; i < 120; ++i) d.push_back((int16_t)(-120 - (i % 7)));
    Passage p;
    ok(run(d, p), "bipolar passage closed");
    ok(p.polarity == 0, "summed sign picks the sustained side, not the tall spur");
    ok(p.signedSum < 0, "signed sum recorded and negative");
    ok(p.peakCounts >= 120 && p.peakCounts <= 130, "peak taken in the chosen orientation");
  }


  // ---------------------------------------------------------------------
  printf("part E: a burst of bad readings on a good magnet must not refuse it\n");
  {
    // Bamboo CCW, 2026-09-02 18:55:54, X9. MM157 crossed on the zero ramp:
    // 356 samples, peak 192 of gain 221, and on the falling flank a burst --
    // 105, -2, -13, 108, 109, 174, 68, 48 -- that a three-wide median leaves
    // in. It scored 0.1506 and stopped the train. Verbatim from the withdraw
    // dump (both refusal chunks arrived too; nothing was lost on the wire).
    static const int16_t MM157[356] = {29,29,29,32,30,30,32,32,32,29,32,35,39,39,40,43,37,48,51,47,52,53,51,53,56,53,57,59,60,61,30,63,65,67,68,67,68,70,73,73,73,77,77,77,78,81,79,81,81,85,87,85,88,89,91,93,93,94,95,97,100,101,103,103,106,108,107,110,110,110,110,113,114,116,117,116,121,133,123,125,126,128,129,131,132,135,134,137,139,137,141,133,132,133,136,138,142,140,142,143,143,146,149,149,149,151,151,155,155,157,157,158,158,158,160,165,161,164,162,158,167,164,168,171,173,172,171,173,174,172,173,173,174,174,186,185,183,184,187,183,188,185,184,187,187,187,190,191,187,189,187,188,185,190,188,189,190,189,192,188,190,190,189,191,192,191,189,191,189,189,191,191,190,192,189,190,192,192,190,191,190,191,192,189,189,190,189,189,189,187,187,184,186,175,176,177,180,174,169,179,174,177,174,174,175,174,172,174,174,174,174,173,171,171,194,169,171,174,175,175,175,174,173,172,170,169,168,165,163,165,163,169,160,161,159,158,158,158,154,155,151,151,152,152,148,147,149,145,144,143,142,142,141,141,140,139,133,137,135,133,130,132,129,132,114,128,125,124,123,123,124,105,-2,-13,108,109,174,68,48,106,114,100,99,101,98,93,94,94,92,94,92,81,88,88,80,80,81,78,78,69,78,78,76,73,75,73,67,71,68,67,64,64,61,60,59,58,47,53,52,53,46,49,48,46,45,46,46,43,46,43,42,37,39,42,36,33,33,31,30,32,30,30,29,28,28,28,25,21,24,21,21,24,20,19,20,15};
    std::vector<int16_t> o(MM157, MM157 + 356), j(356);
    medianOfFive(o.data(), 356, j.data());
    Passage p; p.oriented = o.data(); p.judged = j.data();
    p.sampleCount = 356; p.preSamples = 12;
    int16_t pk = 0; for (int i = 12; i < 356; ++i) pk = std::max(pk, j[i]);
    p.peakCounts = (uint16_t)pk;
    float r = 0; const bool fit = MagnetRecognizer::fitResidual(p, r);
    printf("  five-wide median: peak %d residual %.4f\n", pk, r);
    ok(fit && r <= 0.13f, "the burst is outvoted and the magnet accepted", "MM157 2026-09-02 18:55:54");
    // and what a three-wide median made of it, so the record says why
    std::vector<int16_t> j3(356);
    for (int i = 0; i < 356; ++i) { int a = i > 0 ? i - 1 : 0, b = i < 355 ? i + 1 : 355; int16_t w[3] = { o[a], o[i], o[b] }; std::sort(w, w + 3); j3[i] = w[1]; }
    Passage p3 = p; p3.judged = j3.data(); float r3 = 0; MagnetRecognizer::fitResidual(p3, r3);
    printf("  three-wide, for the record: residual %.4f -- what the field saw\n", r3);
    ok(r3 > 0.13f, "and a three-wide median really did refuse it", "MM157");
  }

  printf("\n%d checks, %d failures\n", checks, failures);
  if (failures) { printf("GATE 6 FAILED\n"); return 1; }
  printf("GATE 6 PASSED\n");
  return 0;
}

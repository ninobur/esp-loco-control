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
  for (; t < 2600; ++t) cap.sample(t, BASE);            // prime the baseline
  if (!cap.ready()) return false;
  bool closed = false;
  for (size_t i = 0; i < deltas.size(); ++i, ++t)
    if (cap.sample(t, (int16_t)(BASE + deltas[i]))) { out = cap.passage(); closed = true; }
  for (int i = 0; i < 40 && !closed; ++i, ++t)          // trailing quiet closes it
    if (cap.sample(t, BASE)) { out = cap.passage(); closed = true; }
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
    ok((int)p.peakCounts == r.wantPeak, "peak", id);
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

  printf("\n%d checks, %d failures\n", checks, failures);
  if (failures) { printf("GATE 6 FAILED\n"); return 1; }
  printf("GATE 6 PASSED\n");
  return 0;
}

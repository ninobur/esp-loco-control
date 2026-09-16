// Replay of a NAVI_ONE_STATION_CURVES run (every passage published) through
// the current recognizer, in field order, reporting every passage whose
// verdict differs from the one the locomotive reached. Input: one passage per
// line, TSV: seq open close pol outcome isMagnet shapeTested dec peak gain
// ratio resid gap stitchAt pre phase station mm dir samples(comma).
// The samples are the published record: oriented, entry-baseline-relative.
// truncated/clipped are not on the wire; shapeTested=0 is replayed as
// truncated so the abstention reproduces.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include "../LL_LocoConfig_9950012.h"
#include "../HallCapture.h"
#include "../MagnetRecognizer.h"
using namespace navi_one;
int main(int argc, char** argv) {
  if (argc < 2) { fprintf(stderr, "usage: replay_station_run <run.tsv>\n"); return 2; }
  FILE* f = fopen(argv[1], "r"); if (!f) { perror("open"); return 2; }
  RecognizerConfig cfg; MagnetRecognizer rec(cfg);
  char* line = nullptr; size_t cap = 0;
  int n = 0, changed = 0, wouldRefuse = 0; std::map<std::string,int> fieldTally, nowTally, shapeTally;
  while (getline(&line, &cap, f) > 0) {
    std::vector<std::string> c; char* p = strtok(line, "\t\n");
    while (p) { c.push_back(p); p = strtok(nullptr, "\t\n"); }
    if (c.size() < 20) continue;
    std::vector<int16_t> v; { std::string s = c[19]; size_t i = 0; while (i < s.size()) { size_t j = s.find(',', i); if (j == std::string::npos) j = s.size(); v.push_back((int16_t)atoi(s.substr(i, j - i).c_str())); i = j + 1; } }
    std::vector<int16_t> judged(v.size()); medianOfThree(v.data(), (uint16_t)v.size(), judged.data());
    Passage ps; ps.openedAtMs = strtoul(c[1].c_str(), 0, 10); ps.closedAtMs = strtoul(c[2].c_str(), 0, 10);
    ps.polarity = atoi(c[3].c_str()); ps.peakCounts = atoi(c[8].c_str()); ps.decimation = atoi(c[7].c_str());
    ps.stitchAt = atoi(c[13].c_str()); ps.preSamples = atoi(c[14].c_str());
    ps.truncated = atoi(c[6].c_str()) == 0; ps.oriented = v.data(); ps.judged = judged.data(); ps.sampleCount = (uint16_t)v.size();
    Verdict nv = rec.examine(ps);
    const int fieldOutcome = atoi(c[4].c_str()); const bool fieldMagnet = atoi(c[5].c_str()) != 0;
    ++n; fieldTally[outcomeName((Outcome)fieldOutcome)]++; nowTally[outcomeName(nv.outcome)]++;
    if (nv.wouldShapeRefuse) { ++wouldRefuse; shapeTally[outcomeName(nv.shapeOutcome)]++; }
    if (nv.isMagnet != fieldMagnet || (Outcome)fieldOutcome != nv.outcome) {
      ++changed;
      printf("  CHANGED seq %5s %-9s %-8s mm %3s %-3s field %-11s resid %s ratio %s -> now %-7s (shape says %s, resid %.4f, ratio %.3f)\n",
             c[0].c_str(), c[15].c_str(), c[16].c_str(), c[17].c_str(), c[18].c_str(),
             outcomeName((Outcome)fieldOutcome), c[11].c_str(), c[10].c_str(),
             outcomeName(nv.outcome), outcomeName(nv.shapeOutcome), (double)nv.residual, (double)nv.amplitudeRatio);
    }
  }
  printf("\n%d passages replayed in field order\n  field verdicts:", n);
  for (auto& kv : fieldTally) printf("  %s %d", kv.first.c_str(), kv.second);
  printf("\n  now:           ");
  for (auto& kv : nowTally) printf("  %s %d", kv.first.c_str(), kv.second);
  printf("\n  would_shape_refuse set on %d:", wouldRefuse);
  for (auto& kv : shapeTally) printf("  %s %d", kv.first.c_str(), kv.second);
  printf("\n  verdicts changed: %d\n", changed);
  return 0;
}

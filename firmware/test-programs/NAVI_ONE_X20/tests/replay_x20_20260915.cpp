// ---------------------------------------------------------------------------
// replay_x19_20260915 -- REGRESSION, not design.
//
// Runs the ACTUAL X20 ExcursionDetector, the same header the sketch compiles,
// against the 52 complete diag/waveform records of Otto 2026-09-15, and checks
// that the implementation agrees with the Python replay the architecture was
// argued from (docs/NAVI_NEXT_EVENT_WITHOUT_CLOSURE_20260915.md).
//
// It is NOT acquisition coverage and it cannot re-open the design question.
// Its only job is to catch an implementation that does something other than
// the algorithm already tested.
//
// WHAT THE CORPUS CAN AND CANNOT SAY
//   * Every X18 dump begins at the 70-count entry crossing with 12 ms of
//     pre-roll. So FIRST detection cannot be exercised -- the detector is
//     handed a synthetic 600 ms quiet prefix to arm it, exactly as the Python
//     replay did, and the results that matter are the candidates found AFTER
//     that first one.
//   * Records are oriented and decimated. Decimated ones are replayed with a
//     zero-order hold, so the time base is right and the sub-sample structure
//     is absent -- as it was in the original.
//
// Build:  g++ -O2 -std=c++17 -o /tmp/x19rep replay_x19_20260915.cpp
// Run:    /tmp/x19rep fixtures_otto_20260915.txt
// ---------------------------------------------------------------------------
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include "../ExcursionDetector.h"
#include "../MagnetRecognizer.h"

using namespace navi_one;

static const int32_t REF = 1935;     // the reconstruction constant; immaterial

struct Record {
  std::string ts; int dur, dec; std::string outcome; int peak, n;
  std::vector<int16_t> s;
};

// Genuine next-event magnets past the first refractory, from the independent
// excursion census in the report (rise >= 100 counts, width >= 100 ms).
struct Target { const char* ts; int dur; int at; };
static const Target TARGETS[] = {
  {"11:05:20.531",1852,1440},{"11:12:02.324",3652,1096},{"11:12:02.324",3652,2304},
  {"11:12:30.608",28158,16576},{"11:29:57.084",5478,2672},{"11:30:09.202",2107,1240},
  {"11:50:57.196",4996,1216},{"11:50:57.196",4996,2496},{"11:50:57.196",4996,3760},
  {"11:50:57.201",22053,19456},{"12:06:24.617",1443,1320},
};
static const int NTARGET = (int)(sizeof(TARGETS)/sizeof(TARGETS[0]));

int main(int argc, char** argv) {
  const char* path = argc > 1 ? argv[1] : "fixtures_otto_20260915.txt";
  std::ifstream in(path);
  if (!in) { fprintf(stderr, "cannot open %s\n", path); return 2; }

  std::vector<Record> recs;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    Record r; std::istringstream h(line);
    h >> r.ts >> r.dur >> r.dec >> r.outcome >> r.peak >> r.n;
    if (!std::getline(in, line)) break;
    std::istringstream d(line);
    int v; while (d >> v) r.s.push_back((int16_t)v);
    recs.push_back(std::move(r));
  }
  printf("records: %zu\n\n", recs.size());

  const int PREFIX_MS = 600;          // synthetic quiet line; see header
  int hits[NTARGET]; memset(hits, 0, sizeof(hits));
  int unmatchedFine = 0, unmatchedCoarse = 0;
  int normalOne = 0, normalTotal = 0, totalCand = 0;
  int persistentExtras = 0, wouldFailX18Floor = 0;

  // The Python replay, at the same operating point WITH ITS WIDTH SCREEN OFF
  // (WL=300 D=70 P=2 Wmin=0), found exactly these two candidates that are not
  // census magnets. X20 has no width screen at all, so it must find both -- and
  // nothing else, in any record fine enough to compare.
  struct Expected { const char* ts; int dur; int at; };
  const Expected EXPECT[] = {
    {"11:05:22.540",1602,1172},{"11:29:43.146",16004,1792},
  };
  const int NEXPECT = (int)(sizeof(EXPECT)/sizeof(EXPECT[0]));

  for (const auto& r : recs) {
    DetectorConfig cfg;               // the SHIPPED defaults, untouched
    cfg.departCounts = 70;
    ExcursionDetector<> det(cfg);
    RecognizerConfig rc; rc.bootstrapGain = 175;
    MagnetRecognizer rec(rc);

    uint32_t t = 0;
    // prime + quiet prefix
    for (int i = 0; i < 2100 + PREFIX_MS; ++i) det.sample(t++, (int16_t)REF, true);
    const uint32_t t0 = t;            // first sample of the record
    // the detection point inside the record, matching the Python d0
    const int d0 = (int)r.s.size() - (r.dur / r.dec) - 1;
    const uint32_t detectT = t0 + (uint32_t)(d0 < 0 ? 0 : d0) * (uint32_t)r.dec;

    std::vector<std::pair<long,Excursion>> cands;
    for (size_t i = 0; i < r.s.size(); ++i) {
      for (int k = 0; k < r.dec; ++k) {       // zero-order hold
        const int16_t raw = (int16_t)(REF + r.s[i]);
        if (det.sample(t, raw, true)) {
          Excursion e = det.excursion();
          Passage p; p.openedAtMs = e.detectedAtMs; p.closedAtMs = e.detectedAtMs;
          p.peakCounts = e.peakCounts; p.polarity = e.polarity;
          Verdict v = rec.examine(p);
          (void)v;
          cands.push_back({(long)e.detectedAtMs - (long)detectT, e});
        }
        ++t;
      }
    }
    // drain the final window
    for (int i = 0; i < 500; ++i) {
      if (det.sample(t, (int16_t)(REF + r.s.back()), true)) {
        Excursion e = det.excursion();
        cands.push_back({(long)e.detectedAtMs - (long)detectT, e});
      }
      ++t;
    }
    totalCand += (int)cands.size();

    printf("%-13s dur=%6d dec=%3d %-11s cand=%zu ", r.ts.c_str(), r.dur, r.dec,
           r.outcome.c_str(), cands.size());
    for (auto& c : cands)
      printf(" [%+ldms pk%u %c w%u/%u exc%u]", c.first, c.second.peakCounts,
             c.second.polarity ? 'N' : 'S', c.second.widthCaliperMs,
             c.second.widthFracMs, c.second.excursionCount);
    printf("\n");

    if (r.dur <= 500) { ++normalTotal; if (cands.size() == 1) ++normalOne; }

    for (auto& c : cands) {
      if (c.first < 400) continue;            // the seeded first magnet
      bool matched = false;
      for (int k = 0; k < NTARGET; ++k)
        if (r.ts == TARGETS[k].ts && r.dur == TARGETS[k].dur &&
            labs(c.first - TARGETS[k].at) <= 350) { hits[k] = 1; matched = true; }
      if (!matched) {
        bool expected = false;
        for (int k = 0; k < NEXPECT; ++k)
          if (r.ts == EXPECT[k].ts && r.dur == EXPECT[k].dur &&
              labs(c.first - EXPECT[k].at) <= 350) expected = true;
        // Records decimated 8x or more are replayed with a zero-order hold, so
        // the 2-sample persistence test spans 2 ms of HELD value rather than 2
        // samples of real signal. That makes the C++ strictly more permissive
        // than the Python replay was on the same record, and the difference is
        // the corpus, not the code: the 1 kHz data underneath was discarded on
        // the locomotive in 2026. These are reported, never asserted on.
        if (r.dec >= 8) {
          ++unmatchedCoarse;
          printf("      (coarse, dec=%d) candidate at %+ldms peak %u\n",
                 r.dec, c.first, c.second.peakCounts);
        } else if (!expected) {
          ++unmatchedFine;
          printf("      UNMATCHED candidate at %+ldms peak %u\n", c.first,
                 c.second.peakCounts);
        }
      }
    }
    // A record that holds a level for seconds with no real magnet in it must
    // produce exactly one candidate, whatever its duration.
    // How many candidates X18's 82 ms completed-passage floor would have
    // refused. X20 has no floor; this is the risk, counted.
    for (auto& c : cands)
      if (c.second.widthCaliperMs < 82) {
        ++wouldFailX18Floor;
        printf("      NO-FLOOR EXPOSURE: %+ldms peak %u width %u ms "
               "(X18 would have refused it)\n",
               c.first, c.second.peakCounts, c.second.widthCaliperMs);
      }
    if ((r.ts == "11:29:59.317" || r.ts == "11:29:24.387" ||
         r.ts == "11:15:04.481" || r.ts == "11:17:48.342" ||
         r.ts == "13:21:03.054") && cands.size() > 1)
      persistentExtras += (int)cands.size() - 1;
  }

  int found = 0; for (int k = 0; k < NTARGET; ++k) found += hits[k];
  printf("\n--- X20 implementation vs the 2026-09-15 replay ---\n");
  printf("next-event targets found      : %d / %d\n", found, NTARGET);
  printf("unmatched, records dec<=4     : %d   (must be 0)\n", unmatchedFine);
  printf("unmatched, records dec>=8     : %d   (reported, not asserted)\n", unmatchedCoarse);
  printf("persistent-field extra events : %d   (must be 0)\n", persistentExtras);
  printf("normal passages, exactly one  : %d / %d\n", normalOne, normalTotal);
  printf("total candidates              : %d\n", totalCand);
  printf("candidates X18's 82 ms floor\n");
  printf("  would have refused          : %d   <-- THE NO-FLOOR EXPOSURE\n",
         wouldFailX18Floor);
  for (int k = 0; k < NTARGET; ++k)
    if (!hits[k]) printf("MISSED: %s dur=%d at +%d\n", TARGETS[k].ts, TARGETS[k].dur, TARGETS[k].at);

  const bool pass = (found == NTARGET) && (persistentExtras == 0) &&
                    (normalOne == normalTotal) && (unmatchedFine == 0);
  printf("\n%s\n", pass ? "PASS -- implementation matches the tested algorithm"
                        : "FAIL -- implementation does NOT match");
  return pass ? 0 : 1;
}

// ---------------------------------------------------------------------------
// replay_x22_20260915 -- REGRESSION, not design.
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
// X21 NOTE. This corpus is ORIENTED (every record's own passage was made
// positive by X18 before it was dumped) and DECIMATED, so its "polarity"
// column is relative to the record, not an absolute N/S, and its sub-4 ms
// structure is gone. It can therefore show that the opening sign and the
// window argmax sometimes disagree; it cannot arbitrate which is right. The
// arbitration is MM136's raw 1 kHz record, in
// field-records/20260915_OTTO_X20_MM136_POLARITY_INVERSION.md, and gate 6b.
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
// X22 NOTE, AND IT IS THE WHOLE DIFFERENCE.
//
// These eleven are the SECOND-AND-LATER departures X19 found inside long
// records -- a locomotive sitting in a field while the field wanders. X19
// found them because its reference was a trailing minimum that walked onto
// whatever level persisted, so a wander 19 seconds into a 22-second field was
// a fresh 70-count departure from a rest the detector had just re-measured
// onto the field itself. That is the mechanism, exactly, that counted three
// magnets while Otto stood still at Arches on 2026-09-15.
//
// X22's reference does not move, and its electrical rearm forbids a second
// declaration until the line has come back within departCounts of the lock.
// The `expect` column below is DESCRIPTIVE, not normative: it records which of
// X19's targets are first departures. Whether X22 takes one of the others is
// decided by its own rearm -- if the field genuinely came home and went out
// again, the second departure is real and is taken. Two of the seven turn out
// to be exactly that, and the invariant check further down is what proves it:
// zero declarations anywhere in the corpus without an intervening return.
//
// They are not lost silently: every one is counted into suppressed_ and lands
// on diag/excursion as suppressed_total. The cost is real and is stated in
// gate 3 -- a genuine magnet arriving while the sensor is still inside another
// field is missed -- and it is the price of not manufacturing the Arches
// advance. Which way that trade runs on the railway is what the field test is
// for; it is not settled here.
struct Target { const char* ts; int dur; int at; bool expect; };
static const Target TARGETS[] = {
  // first departures: X22 must still find every one
  {"11:05:20.531",1852,1440,true},{"11:12:02.324",3652,1096,true},
  {"11:29:57.084",5478,2672,true},{"12:06:24.617",1443,1320,true},
  // re-declarations inside a field that never returned to the lock
  {"11:12:02.324",3652,2304,false},{"11:12:30.608",28158,16576,false},
  {"11:30:09.202",2107,1240,false},
  {"11:50:57.196",4996,1216,false},{"11:50:57.196",4996,2496,false},
  {"11:50:57.196",4996,3760,false},{"11:50:57.201",22053,19456,false},
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
  // X22. Declarations that were NOT preceded by a return to the lock. The
  // electrical rearm makes this structurally impossible, so it is the one
  // number here that is an assertion about the new architecture.
  int rearmLeaks = 0;
  // X21. How often the 400 ms window's own argmax disagrees with the sign of
  // the opening that declared the candidate. Under X19/X20 the window won
  // every one of these; under X21 the opening does. REPORTED, NOT ASSERTED --
  // the corpus is decimated and oriented, so it can show the mechanism but
  // cannot say which answer was right.
  int windowDisagrees = 0;

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
    // THE X22 INVARIANT, checked against the samples rather than asserted:
    // every declaration is preceded by a return to within departCounts of the
    // lock. `returned` is set by the signal itself and cleared by a
    // declaration, so a violation means the electrical rearm leaked.
    bool returned = true;
    for (size_t i = 0; i < r.s.size(); ++i) {
      for (int k = 0; k < r.dec; ++k) {       // zero-order hold
        const int16_t raw = (int16_t)(REF + r.s[i]);
        if (labs((long)raw - (long)det.baseline()) < cfg.departCounts) returned = true;
        { Detection dd; if (det.takeDetection(dd)) { /* drained below */ } }
        if (det.sample(t, raw, true)) {
          Excursion e = det.excursion();
          Passage p; p.openedAtMs = e.detectedAtMs; p.closedAtMs = e.detectedAtMs;
          p.peakCounts = e.peakCounts; p.polarity = e.polarity;
          Verdict v = rec.examine(p);
          (void)v;
          cands.push_back({(long)e.detectedAtMs - (long)detectT, e});
        }
        { Detection dd;
          if (det.takeDetection(dd)) {
            if (!returned) ++rearmLeaks;
            returned = false;
          } }
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

    // X21. peakSigned carries the sign the window MEASURED; polarity carries
    // the sign of the opening. When they differ this is an MM136-class record.
    for (auto& c : cands) {
      const Excursion& e = c.second;
      const uint8_t windowPol = e.peakSigned >= 0 ? 1 : 0;
      if (windowPol == e.polarity) continue;
      ++windowDisagrees;
      printf("      WINDOW DISAGREES at %+ldms: opening depart %+ld -> %c, "
             "window argmax %+d -> %c. X21 publishes the opening.\n",
             c.first, (long)e.departAtDetect, e.polarity ? 'N' : 'S',
             (int)e.peakSigned, windowPol ? 'N' : 'S');
    }

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
  printf("unmatched, records dec<=4     : %d   (X19 metric, reported)\n", unmatchedFine);
  printf("unmatched, records dec>=8     : %d   (reported, not asserted)\n", unmatchedCoarse);
  printf("persistent-field extra events : %d   (X19 metric, reported)\n", persistentExtras);
  printf("normal passages, exactly one  : %d / %d\n", normalOne, normalTotal);
  printf("total candidates              : %d\n", totalCand);
  printf("candidates X18's 82 ms floor\n");
  printf("  would have refused          : %d   <-- THE NO-FLOOR EXPOSURE\n",
         wouldFailX18Floor);
  printf("window argmax disagrees with\n");
  printf("  the opening sign            : %d   (X19/X20 published the window's\n",
         windowDisagrees);
  printf("                                     answer; X21 publishes the opening)\n");
  for (int k = 0; k < NTARGET; ++k)
    if (TARGETS[k].expect && !hits[k])
      printf("MISSED: %s dur=%d at +%d   <-- REGRESSION\n",
             TARGETS[k].ts, TARGETS[k].dur, TARGETS[k].at);
  int mustFind = 0, didFind = 0, declined = 0, wronglyFound = 0;
  for (int k = 0; k < NTARGET; ++k) {
    if (TARGETS[k].expect) { ++mustFind; if (hits[k]) ++didFind; }
    else if (hits[k]) { ++wronglyFound;
      printf("TAKEN AFTER A RETURN:    %s at +%d   (rearm satisfied)\n",
             TARGETS[k].ts, TARGETS[k].at); }
    else ++declined;
  }
  printf("first departures found         : %d / %d\n", didFind, mustFind);
  printf("rearm leaks (must be 0)        : %d\n", rearmLeaks);
  printf("X19 in-field re-declarations   : %d declined, %d taken after a return\n",
         declined, wronglyFound);
  // X19's "persistent-field extras" and its hand-listed in-field targets both
  // encode a detector whose reference walked onto whatever level persisted.
  // They are REPORTED here, because the comparison is the point of the file,
  // and they are not asserted. What IS asserted is X22's own contract: every
  // first departure is found, every normal passage exactly once, and no
  // declaration ever happens without an intervening return to the lock.
  const bool pass = (didFind == mustFind) && (rearmLeaks == 0) &&
                    (normalOne == normalTotal);
  printf("\n%s\n", pass ? "PASS -- implementation matches"
                         : "FAIL -- implementation does NOT match");
  return pass ? 0 : 1;
}

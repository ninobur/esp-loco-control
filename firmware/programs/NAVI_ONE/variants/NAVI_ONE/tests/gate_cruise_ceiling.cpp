// ---------------------------------------------------------------------------
// Offline replay of the bounded acquisition reset ("cruise passage ceiling")
// proposed by the operator on 2026-09-13, after Otto's Northpoint stop.
//
// TEST ONLY. No production header includes this file, no firmware is changed,
// and this gate is not in run_tests.sh. It answers one question:
//
//   does a cruise passage ceiling separate the magnets this incident merged,
//   while leaving every legitimate slow or stationary passage unchanged?
//
// The reset lives in HallCaptureCeiling.h, a verbatim copy of HallCapture.h
// with one addition. Section A proves the copy is otherwise identical.
//
// WHAT THE REPLAY CAN AND CANNOT SHOW
// -----------------------------------
// The magnets this incident LOST never opened a passage, so no waveform of
// them exists and no replay can show them being recovered. What the record
// does contain is the latched excursions that covered the ground they were on.
// Sections B and C therefore show (i) the latch is cut at the ceiling and
// acquisition re-arms, and (ii) a magnet arriving after that point gets a
// passage of its own, using a measured cruise magnet from the same lap.
//
// Sections B and C drive RECONSTRUCTED streams: the published records are
// oriented, entry-baseline-relative and decimated, so each stored sample is
// held for `decimation` milliseconds and the intervals between records are
// filled at the level the baseline was measured to be sitting at. Levels and
// durations are the field's; the millisecond detail between decimated samples
// is not recoverable (decision 0070).
// ---------------------------------------------------------------------------
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include "../HallCapture.h"
#include "HallCaptureCeiling.h"
#include "fixtures_northpoint_20260913.h"
#include "fixtures_arches_departures.h"
#include "fixtures_captures.h"

static int checks = 0, failures = 0;
static void ck(bool c, const char* what) {
  ++checks;
  if (!c) { ++failures; std::printf("  FAIL  %s\n", what); }
}
template <class A, class B>
static void ckEq(A a, B b, const char* what) {
  ++checks;
  if (!((long long)a == (long long)b)) {
    ++failures;
    std::printf("  FAIL  %s (got %lld, want %lld)\n", what, (long long)a, (long long)b);
  }
}

// The two constants under test, and where they come from.
//
// CEILING_MS 550  The longest clean cruise passage measured anywhere in the
//                 available corpora is 302 ms (2026-09-13, PWM 72) and 228 ms
//                 (2026-09-09 survey, PWM 73). 550 ms is above both by a
//                 factor of 1.8 and still below one marker interval at cruise,
//                 which measured 1121-1289 ms on the seven markers before the
//                 incident.
// ARM_PWM    70   At PWM 70 and above no clean passage in either corpus
//                 exceeds 302 ms. Below it the measured maximum climbs through
//                 462 ms (PWM 55-69) to seconds at a standstill, so the reset
//                 is disarmed there and a legitimately long crossing is left
//                 alone. Every one of the 22 long passages recorded on
//                 2026-09-13 closed at PWM 22-47 -- see section F.
static const uint16_t CEILING_MS = 550;
static const uint8_t  ARM_PWM    = 70;

// OTTO'S ACQUISITION, NOT THE HEADER DEFAULTS. CaptureConfig's entryMargin
// defaults to 38 -- Toby's value. Otto composes his in the sketch from
// HALL_DEADBAND_COUNTS (25) + HALL_ENTRY_MARGIN_COUNTS (45) = 70, and the boot
// record of the incident build reads "entry":70. A replay at 38 would open
// passages the field never opened, so every capture in this gate is built
// through these two helpers and never from a default-constructed config.
static const int16_t OTTO_ENTRY = 70, OTTO_EXIT = 25;
static navi_one::CaptureConfig ottoProd() {
  navi_one::CaptureConfig c; c.entryMargin = OTTO_ENTRY; c.exitMargin = OTTO_EXIT; return c;
}
static navi_ceiling::CeilingConfig ottoCeil(uint16_t ceilingMs, uint8_t armPwm) {
  navi_ceiling::CeilingConfig c; c.entryMargin = OTTO_ENTRY; c.exitMargin = OTTO_EXIT;
  c.cruiseCeilingMs = ceilingMs; c.ceilingPwm = armPwm; return c;
}

// ---------------------------------------------------------------------------
// Stream building
// ---------------------------------------------------------------------------
struct Stream { std::vector<int16_t> raw; };

// A published record, expanded back to a 1 kHz raw stream. The samples are
// oriented, so an S record is negated back before the entry baseline is added.
static void appendRecord(Stream& s, const NorthpointRecord& r) {
  const int sign = r.polarity ? +1 : -1;
  for (uint16_t i = 0; i < r.n; ++i)
    for (uint16_t k = 0; k < r.decimation; ++k)
      s.raw.push_back((int16_t)(r.entryBaseline + sign * r.v[i]));
}
static void appendFlat(Stream& s, int32_t level, uint32_t ms) {
  for (uint32_t i = 0; i < ms; ++i) s.raw.push_back((int16_t)level);
}

// ---------------------------------------------------------------------------
// A. the copy is the production class, plus one thing
// ---------------------------------------------------------------------------
struct Seen {
  uint32_t opened, closed; uint16_t peak, n, dec; uint8_t pol;
  int64_t sum; bool trunc, clip; int32_t entryBase;
};
static bool sameClose(const navi_one::Passage& p, const Seen& s) {
  return p.openedAtMs == s.opened && p.closedAtMs == s.closed &&
         p.peakCounts == s.peak && p.sampleCount == s.n &&
         p.decimation == s.dec && p.polarity == s.pol &&
         p.signedSum == s.sum && p.truncated == s.trunc &&
         p.clipped == s.clip && p.entryBaseline == s.entryBase;
}
// Drive both classes over the same stream and require identical behaviour on
// every single sample: same open/close decisions, same baseline, same passages.
static bool equivalent(const Stream& st, int32_t primeLevel, const char* tag) {
  navi_one::CaptureConfig pc = ottoProd();
  navi_ceiling::CeilingConfig cc = ottoCeil(0, 0);   // reset disabled
  navi_one::HallCapture<512> prod(pc);
  navi_ceiling::HallCaptureCeiling<512> copy(cc);
  uint32_t t = 0;
  for (; t < 3000; ++t) { prod.sample(t, (int16_t)primeLevel, true);
                          copy.sample(t, (int16_t)primeLevel, true); }
  bool ok = true;
  for (size_t i = 0; i < st.raw.size(); ++i, ++t) {
    const bool a = prod.sample(t, st.raw[i], true);
    const bool b = copy.sample(t, st.raw[i], true);
    if (a != b) { std::printf("     %s: close disagreed at sample %zu\n", tag, i); ok = false; break; }
    if (prod.baseline() != copy.baseline()) {
      std::printf("     %s: baseline disagreed at sample %zu (%ld vs %ld)\n",
                  tag, i, (long)prod.baseline(), (long)copy.baseline()); ok = false; break;
    }
    if (prod.open() != copy.open()) { std::printf("     %s: open state disagreed at %zu\n", tag, i); ok = false; break; }
    if (a) {
      const navi_one::Passage& pp = prod.passage();
      Seen s{ copy.passage().openedAtMs, copy.passage().closedAtMs,
              copy.passage().peakCounts, copy.passage().sampleCount,
              copy.passage().decimation, copy.passage().polarity,
              copy.passage().signedSum, copy.passage().truncated,
              copy.passage().clipped, copy.passage().entryBaseline };
      if (!sameClose(pp, s)) { std::printf("     %s: passage differed at %zu\n", tag, i); ok = false; break; }
    }
  }
  if (ok && prod.floorRejects() != copy.floorRejects()) {
    std::printf("     %s: floor reject count differed (%lu vs %lu)\n",
                tag, (unsigned long)prod.floorRejects(), (unsigned long)copy.floorRejects());
    ok = false;
  }
  return ok;
}

// ---------------------------------------------------------------------------
// Replay one stream through the ceiling capture, reporting everything it did.
// ---------------------------------------------------------------------------
struct Replay {
  std::vector<navi_one::Passage> closed;
  std::vector<navi_ceiling::ForcedClosure> forced;
  std::vector<uint32_t> closeAt;
  uint32_t floorRejects = 0;
};
static Replay drive(const Stream& st, int32_t primeLevel, uint8_t pwm,
                    uint16_t ceilingMs, uint8_t armPwm) {
  navi_ceiling::CeilingConfig cc = ottoCeil(ceilingMs, armPwm);
  navi_ceiling::HallCaptureCeiling<512> cap(cc);
  Replay r;
  uint32_t t = 0;
  for (; t < 3000; ++t) cap.sample(t, (int16_t)primeLevel, true, 0);
  for (size_t i = 0; i < st.raw.size(); ++i, ++t) {
    const bool closed = cap.sample(t, st.raw[i], true, pwm);
    navi_ceiling::ForcedClosure f;
    if (cap.takeForcedClosure(f)) r.forced.push_back(f);
    if (closed) { r.closed.push_back(cap.passage()); r.closeAt.push_back(t); }
  }
  r.floorRejects = cap.floorRejects();
  return r;
}

// ---------------------------------------------------------------------------
// D/E/F. corpus analysis over the published record metadata
// ---------------------------------------------------------------------------
static std::string field(const std::string& s, const char* key) {
  std::string k = std::string("\"") + key + "\":";
  size_t p = s.find(k); if (p == std::string::npos) return "";
  p += k.size(); if (s[p] == '"') { size_t e = s.find('"', p + 1); return s.substr(p + 1, e - p - 1); }
  size_t e = p; while (e < s.size() && s[e] != ',' && s[e] != '}') ++e;
  return s.substr(p, e - p);
}
struct Rec { int dur, pwm, pk, rej, tr, clip; std::string mm; };

int main(int argc, char** argv) {
  std::printf("NAVI_ONE test-only gate -- bounded acquisition reset (cruise passage ceiling)\n");
  std::printf("ceiling %u ms, armed at PWM >= %u\n", CEILING_MS, ARM_PWM);

  // -------------------------------------------------------------------------
  std::printf("\nA. with the reset disabled the copy IS production HallCapture\n");
  std::printf("   (every sample: same open/close, same baseline, same assembled passage)\n");
  {
    // the incident records
    for (unsigned i = 0; i < NORTHPOINT_20260913_N; ++i) {
      Stream s; appendFlat(s, NORTHPOINT_20260913[i]->entryBaseline, 200);
      appendRecord(s, *NORTHPOINT_20260913[i]);
      appendFlat(s, NORTHPOINT_20260913[i]->entryBaseline, 200);
      ck(equivalent(s, NORTHPOINT_20260913[i]->entryBaseline, NORTHPOINT_20260913[i]->tag),
         NORTHPOINT_20260913[i]->tag);
    }
    // a departure plateau fixture and a stationary latched-offset fixture
    {
      Stream s; appendFlat(s, 1834, 200);
      for (uint16_t i = 0; i < AD_76.n; ++i) s.raw.push_back((int16_t)(1834 + FXD_76[i]));
      appendFlat(s, 1834, 400);
      ck(equivalent(s, 1834, "arches departure AD_76"), "Arches departure fixture is unchanged");
    }
    {
      Stream s; appendFlat(s, 1834, 200);
      for (uint16_t i = 0; i < 600; ++i) s.raw.push_back((int16_t)(1834 + FX_F09A[i % 96]));
      appendFlat(s, 1834, 400);
      ck(equivalent(s, 1834, "finding 09A latched offset"), "stationary latched-offset fixture is unchanged");
    }
    // flat line, a clean bump, and a sustained offset
    {
      Stream s; appendFlat(s, 1954, 4000);
      ck(equivalent(s, 1954, "flat"), "a flat line is unchanged");
    }
    {
      Stream s; appendFlat(s, 1954, 300);
      for (int i = 0; i < 150; ++i) s.raw.push_back((int16_t)(1954 + 200));
      appendFlat(s, 1954, 500);
      ck(equivalent(s, 1954, "clean bump"), "a clean 150 ms passage is unchanged");
    }
    {
      Stream s; appendFlat(s, 1954, 300); appendFlat(s, 1954 + 90, 4000); appendFlat(s, 1954, 500);
      ck(equivalent(s, 1954, "sustained offset"), "a sustained offset is unchanged");
    }
  }

  // -------------------------------------------------------------------------
  std::printf("\nB. the incident: the latch is cut, and acquisition re-arms\n");
  {
    // The 2446 ms latched passage, exactly as recorded, on its own.
    const NorthpointRecord& L = NP_REC_LATCH_2446;
    Stream s; appendFlat(s, L.entryBaseline, 300);
    appendRecord(s, L);
    appendFlat(s, L.entryBaseline - 45, 1200);   // the line it was actually sitting on

    Replay off = drive(s, L.entryBaseline, 90, 0, ARM_PWM);
    Replay on  = drive(s, L.entryBaseline, 90, CEILING_MS, ARM_PWM);

    std::printf("   reset off: %zu passage(s), first open %lu ms\n",
                off.closed.size(),
                off.closed.empty() ? 0ul
                  : (unsigned long)(off.closed[0].closedAtMs - off.closed[0].openedAtMs));
    std::printf("   reset on : %zu passage(s), %zu forced closure(s)\n",
                on.closed.size(), on.forced.size());
    for (size_t i = 0; i < on.forced.size(); ++i)
      std::printf("     forced #%zu  dur=%u ms  ceiling=%u  entry_base=%ld  base_before=%ld  rebased_to=%ld  peak=%u  admitted=%d\n",
                  i, on.forced[i].durationMs, on.forced[i].ceilingMs,
                  (long)on.forced[i].entryBaseline, (long)on.forced[i].baselineBefore,
                  (long)on.forced[i].rebasedTo, on.forced[i].peakCounts,
                  (int)on.forced[i].admitted);

    ck(!off.closed.empty() && (off.closed[0].closedAtMs - off.closed[0].openedAtMs) > 2000,
       "with the reset off the recorded excursion holds a passage open for over two seconds");
    ckEq(on.forced.size(), 1, "with the reset on it is closed exactly once, at the ceiling");
    if (!on.forced.empty()) {
      ckEq(on.forced[0].durationMs, CEILING_MS, "the forced closure lands on the ceiling");
      ck(on.forced[0].admitted, "the forced-closed passage is still assembled and judged, not rejected");
      ck(on.forced[0].peakCounts > 0, "it carries its measured peak");
      ck(on.forced[0].rebasedTo != on.forced[0].baselineBefore,
         "the acquisition reference is rebased, not left where it was");
      // Does the forced-closed passage still clear the recogniser's amplitude
      // floor? The floor is peak/gain >= 0.34; Otto's gain at the incident was
      // 174. A forced close that fell below it would trade a merge for a miss.
      const double ratio = (double)on.forced[0].peakCounts / 174.0;
      std::printf("   forced-closure amplitude: peak %u / gain 174 = %.3f  (floor 0.34)\n",
                  on.forced[0].peakCounts, ratio);
      ck(ratio >= 0.34,
         "and it still clears the 0.34 amplitude floor -- the magnet is not traded for a miss");
    }
    ck(!on.closed.empty(), "a Passage still reaches the recognizer: the magnet is not discarded");

    // Time regained: how much earlier is acquisition live again?
    if (!off.closed.empty() && !on.closeAt.empty()) {
      const long regained = (long)off.closeAt[0] - (long)on.closeAt[0];
      std::printf("   acquisition is live again %ld ms earlier than it was in the field\n", regained);
      ck(regained > 1500, "which is more than one marker interval at the measured cruise cadence");
    }
  }

  // -------------------------------------------------------------------------
  std::printf("\nC. a magnet arriving after the latch gets a passage of its own\n");
  {
    // Built entirely from measured waveforms of the same lap: the recorded
    // 2446 ms latch, then the recorded 156 ms cruise magnet, placed one
    // measured marker interval (1200 ms) after the latch opened.
    const NorthpointRecord& L = NP_REC_LATCH_2446;
    const NorthpointRecord& M = NP_REC_STRIKE_156;

    Stream s; appendFlat(s, L.entryBaseline, 300);
    const size_t latchStart = s.raw.size();
    appendRecord(s, L);
    // the magnet arrives 1200 ms after the latch opened -- while it is still
    // open in the field record
    const size_t at = latchStart + 1200;
    Stream mag; appendRecord(mag, M);
    for (size_t i = 0; i < mag.raw.size() && at + i < s.raw.size(); ++i) {
      // superimpose the magnet on the excursion, as the sensor would see it
      const int32_t excursion = (int32_t)s.raw[at + i] - L.entryBaseline;
      const int32_t magnet    = (int32_t)mag.raw[i] - M.entryBaseline;
      s.raw[at + i] = (int16_t)(L.entryBaseline + excursion + magnet);
    }
    appendFlat(s, L.entryBaseline - 45, 1200);

    Replay off = drive(s, L.entryBaseline, 90, 0, ARM_PWM);
    Replay on  = drive(s, L.entryBaseline, 90, CEILING_MS, ARM_PWM);
    std::printf("   reset off: %zu passage(s) closed -- the magnet is inside the latch\n", off.closed.size());
    std::printf("   reset on : %zu passage(s) closed, %zu forced\n", on.closed.size(), on.forced.size());
    for (size_t i = 0; i < on.closed.size(); ++i)
      std::printf("     passage #%zu  dur=%lu ms  peak=%u  pol=%c\n", i,
                  (unsigned long)(on.closed[i].closedAtMs - on.closed[i].openedAtMs),
                  on.closed[i].peakCounts, on.closed[i].polarity ? 'N' : 'S');

    ckEq(off.closed.size(), 1, "with the reset off the whole episode is ONE passage");
    ck(on.closed.size() >= 2, "with the reset on the episode separates into more than one passage");
  }

  // -------------------------------------------------------------------------
  std::printf("\nD/E. Otto's cruise, slow and stationary corpora\n");
  if (argc < 2) {
    std::printf("   (no survey log given -- pass field-records/logs/20260909_survey/*.log to run D/E)\n");
  } else {
    std::vector<Rec> recs;
    for (int a = 1; a < argc; ++a) {
      FILE* f = std::fopen(argv[a], "r");
      if (!f) { std::printf("   cannot open %s\n", argv[a]); continue; }
      char* line = nullptr; size_t cap = 0;
      while (getline(&line, &cap, f) > 0) {
        std::string s(line); size_t b = s.find('{');
        if (b == std::string::npos) continue;
        s = s.substr(b);
        if (field(s, "dur").empty() || field(s, "pwm").empty()) continue;
        Rec r; r.dur = atoi(field(s, "dur").c_str()); r.pwm = atoi(field(s, "pwm").c_str());
        r.pk = atoi(field(s, "pk").c_str()); r.rej = atoi(field(s, "rej").c_str());
        r.tr = atoi(field(s, "tr").c_str()); r.clip = atoi(field(s, "clip").c_str());
        r.mm = field(s, "mm"); recs.push_back(r);
      }
      free(line); std::fclose(f);
    }
    std::printf("   records: %zu\n", recs.size());

    size_t fast = 0, fires = 0, cleanFast = 0, longestClean = 0;
    std::vector<Rec> fired;
    for (const Rec& r : recs) {
      if (r.pwm >= ARM_PWM) {
        ++fast;
        if (r.dur >= CEILING_MS) { ++fires; fired.push_back(r); }
        if (!r.tr && !r.clip && r.rej == 0) {
          ++cleanFast;
          if ((size_t)r.dur > longestClean) longestClean = r.dur;
        }
      }
    }
    std::printf("   at PWM >= %u: %zu records, of which %zu clean (untruncated, unclipped, admitted)\n",
                ARM_PWM, fast, cleanFast);
    std::printf("   longest CLEAN passage at PWM >= %u: %zu ms   (ceiling %u ms)\n",
                ARM_PWM, longestClean, CEILING_MS);
    std::printf("   the reset would fire on %zu of %zu:\n", fires, fast);
    for (const Rec& r : fired)
      std::printf("     dur=%5d pwm=%3d peak=%5d rej=%d truncated=%d clipped=%d mm=%s\n",
                  r.dur, r.pwm, r.pk, r.rej, r.tr, r.clip, r.mm.c_str());

    ck(longestClean > 0 && longestClean + 200 < CEILING_MS,
       "every clean cruise passage clears the ceiling by more than 200 ms");
    bool allSuspect = true;
    for (const Rec& r : fired) if (!r.tr && !r.clip) allSuspect = false;
    ck(allSuspect, "every record the reset fires on is truncated or clipped -- none is a clean crossing");

    size_t slow = 0, slowFires = 0, longestSlow = 0;
    for (const Rec& r : recs) {
      if (r.pwm >= ARM_PWM) continue;
      ++slow;
      if ((size_t)r.dur > longestSlow) longestSlow = r.dur;
      if (r.dur >= CEILING_MS) ++slowFires;   // would fire only if it were armed
    }
    std::printf("   at PWM < %u: %zu records, longest %zu ms, of which %zu exceed the ceiling\n",
                ARM_PWM, slow, longestSlow, slowFires);
    ck(longestSlow >= CEILING_MS,
       "slow and stationary passages DO exceed the ceiling -- which is why the reset is disarmed there");
    std::printf("   the reset is disarmed below PWM %u, so all %zu are untouched\n", ARM_PWM, slow);
  }

  // -------------------------------------------------------------------------
  std::printf("\nF. the arming threshold against the long passages of 2026-09-13\n");
  {
    // Every passage of 400 ms or more recorded while powered on the incident
    // day, from the mm/marker stream, with the PWM it closed at.
    static const int LONG_DUR[] = {423,541,2131,462,511,461,835,470,801,428,700,644,
                                   614,599,680,605,1569,1200,2446,19125,420,570};
    static const int LONG_PWM[] = { 39, 31,  22, 63, 43, 47, 38, 43, 37, 45, 39, 40,
                                    41, 41, 40, 41,   90,  90,  90,   41, 40, 41};
    const int n = (int)(sizeof(LONG_DUR)/sizeof(LONG_DUR[0]));
    int armed = 0, disarmed = 0;
    for (int i = 0; i < n; ++i) (LONG_PWM[i] >= ARM_PWM ? armed : disarmed)++;
    std::printf("   %d long passages; %d closed at PWM >= %u, %d below it\n", n, armed, ARM_PWM, disarmed);
    ckEq(armed, 3, "exactly three of them are at cruise -- and all three are the incident");
    ckEq(disarmed, 19, "the other nineteen are station and slow crossings, all disarmed");
    for (int i = 0; i < n; ++i)
      if (LONG_PWM[i] >= ARM_PWM)
        std::printf("     armed: dur=%5d pwm=%3d\n", LONG_DUR[i], LONG_PWM[i]);
  }

  // -------------------------------------------------------------------------
  std::printf("\nG. what each candidate ceiling would touch, over the whole cruise corpus\n");
  if (argc >= 2) {
    std::vector<Rec> recs;
    for (int a = 1; a < argc; ++a) {
      FILE* f = std::fopen(argv[a], "r"); if (!f) continue;
      char* line = nullptr; size_t cap = 0;
      while (getline(&line, &cap, f) > 0) {
        std::string t(line); size_t b = t.find('{');
        if (b == std::string::npos) continue; t = t.substr(b);
        if (field(t, "dur").empty() || field(t, "pwm").empty()) continue;
        Rec r; r.dur = atoi(field(t, "dur").c_str()); r.pwm = atoi(field(t, "pwm").c_str());
        r.pk = atoi(field(t, "pk").c_str()); r.rej = atoi(field(t, "rej").c_str());
        r.tr = atoi(field(t, "tr").c_str()); r.clip = atoi(field(t, "clip").c_str());
        recs.push_back(r);
      }
      free(line); std::fclose(f);
    }
    std::printf("   %-8s %8s %10s %14s\n", "ceiling", "fires", "clean hit", "shortest hit");
    for (int c : {300,400,500,550,600,700,900,1200}) {
      int fires = 0, clean = 0, shortest = 0;
      for (const Rec& r : recs) {
        if (r.pwm < ARM_PWM || r.dur < c) continue;
        ++fires;
        if (!r.tr && !r.clip && r.rej == 0) ++clean;
        if (!shortest || r.dur < shortest) shortest = r.dur;
      }
      std::printf("   %-8d %8d %10d %14d\n", c, fires, clean, shortest);
    }
    std::printf("   \"clean hit\" is the number that are untruncated, unclipped and were\n");
    std::printf("   admitted as markers -- the ones it would be a regression to touch.\n");
  }

  std::printf("\nH. adversarial: a sustained offset, and what happens when it goes away\n");
  {
    // Otto's entry margin is 70 counts. An offset below it opens nothing at
    // all; one above it opens a passage in production too. The question is
    // whether the RESET makes that worse.
    std::printf("   %-7s | %-28s | %-28s\n", "offset", "production HallCapture", "with the reset");
    std::printf("   %-7s | %10s %8s %8s | %10s %8s %8s\n",
                "counts", "passages", "longest", "", "passages", "longest", "forced");
    for (int off : {30, 60, 69, 71, 90, 120}) {
      Stream s2; appendFlat(s2, 1954, 300);
      appendFlat(s2, 1954 + off, 20000);    // the excursion arrives and stays 20 s
      appendFlat(s2, 1954, 4000);           // and then goes away

      // production
      navi_one::CaptureConfig pc = ottoProd();
      navi_one::HallCapture<512> prod(pc);
      uint32_t t = 0; int pn = 0; unsigned long plong = 0;
      for (; t < 3000; ++t) prod.sample(t, 1954, true);
      for (size_t i = 0; i < s2.raw.size(); ++i, ++t)
        if (prod.sample(t, s2.raw[i], true)) {
          ++pn;
          const unsigned long d = prod.passage().closedAtMs - prod.passage().openedAtMs;
          if (d > plong) plong = d;
        }

      Replay on = drive(s2, 1954, 90, CEILING_MS, ARM_PWM);
      unsigned long clong = 0;
      for (size_t i = 0; i < on.closed.size(); ++i) {
        const unsigned long d = on.closed[i].closedAtMs - on.closed[i].openedAtMs;
        if (d > clong) clong = d;
      }
      std::printf("   %-7d | %10d %8lu %8s | %10zu %8lu %8zu\n",
                  off, pn, plong, "", on.closed.size(), clong, on.forced.size());

      if (off < OTTO_ENTRY)
        ck(pn == 0 && on.closed.empty(),
           "an offset below Otto's 70-count entry margin opens nothing, with or without the reset");
      else
        ck(clong <= CEILING_MS,
           "above it, the reset bounds how long any single passage can hold the gate open");
    }
    std::printf("\n   Read the two 'longest' columns. Production lets one passage hold the\n");
    std::printf("   gate open until openMigrateMs walks the reference off the offset. The\n");
    std::printf("   reset bounds it at the ceiling, and because the rebase puts the\n");
    std::printf("   reference where the line actually is, it does NOT then chain: it does\n");
    std::printf("   not produce more events than production does over the same offset.\n");
  }

  std::printf("\nI. the residual risk: commanded at cruise but not actually moving\n");
  {
    // The reset's only speed evidence is PWM. A locomotive commanded at 90 but
    // held still -- slipping, obstructed, derailed against a magnet -- looks
    // like cruise to it. This is the documented "the reference becomes the
    // magnet" hazard, and mayAdapt already has the same exposure above PWM 24.
    // Measure it rather than assert it.
    Stream s3; appendFlat(s3, 1954, 300);
    appendFlat(s3, 1954 + 200, 12000);      // parked in a 200-count field
    appendFlat(s3, 1954, 3000);             // then pulled clear

    navi_one::CaptureConfig pc = ottoProd();
    navi_one::HallCapture<512> prod(pc);
    uint32_t t = 0; int pn = 0; unsigned long plong = 0;
    for (; t < 3000; ++t) prod.sample(t, 1954, true);
    for (size_t i = 0; i < s3.raw.size(); ++i, ++t)
      if (prod.sample(t, s3.raw[i], true)) {
        ++pn;
        const unsigned long d = prod.passage().closedAtMs - prod.passage().openedAtMs;
        if (d > plong) plong = d;
      }
    Replay on = drive(s3, 1954, 90, CEILING_MS, ARM_PWM);
    unsigned long clong = 0;
    for (size_t i = 0; i < on.closed.size(); ++i) {
      const unsigned long d = on.closed[i].closedAtMs - on.closed[i].openedAtMs;
      if (d > clong) clong = d;
    }
    std::printf("   parked 12 s in a 200-count field, commanded PWM 90:\n");
    std::printf("     production : %d passage(s), longest %lu ms\n", pn, plong);
    std::printf("     with reset : %zu passage(s), longest %lu ms, %zu forced\n",
                on.closed.size(), clong, on.forced.size());
    for (size_t i = 0; i < on.forced.size() && i < 4; ++i)
      std::printf("       forced #%zu at %u ms, rebased %ld -> %ld\n", i,
                  on.forced[i].durationMs, (long)on.forced[i].baselineBefore,
                  (long)on.forced[i].rebasedTo);
    ck(clong <= CEILING_MS, "the reset still bounds the passage");
    std::printf("   Both admit passages here; neither is a marker crossing. The reset\n");
    std::printf("   rebases onto the field in one step where production's median walks\n");
    std::printf("   onto it over about 525 ms. Same destination, different speed.\n");
  }

  std::printf("\n%d checks, %d failures\n", checks, failures);
  if (failures) { std::printf("CRUISE CEILING REPLAY: see above\n"); return 1; }
  std::printf("CRUISE CEILING REPLAY PASSED\n");
  return 0;
}

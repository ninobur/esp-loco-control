// Host replay of NaviCandidate + NaviEncounter + NaviTarget on the recorded
// native Hall stream used for the old recognizer's own boot-1 evaluation
// (analysis/navi_rvm_shape_20261018). Declarations are read directly from the
// recorded MQTT command log, not a pre-converted block-index CSV, so this
// exercises the same entry points as the real sketch (session_direction,
// start_interval/start_mm).
//
//   c++ -std=c++17 -O2 -Wall -Wextra -Werror -I<sketch dir> replay.cpp -o replay &&
//   ./replay <native.bin> <declarations.csv> <boot_id> <out_dir>
//
// native.bin: packed records {u32 serial, u64 us, i16 adc, u8 pwm, u8 dir}
//   (the hall-ground-20261015 extraction; pwm is read but not used — this
//   build gates on nothing PWM-related, which is the point).
// declarations.csv: boot,t_us,topic,payload (the navi-rvm-replay-20261016
//   extraction). Only rows matching boot_id are used.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include "NaviObservation.h"
#include "NaviBaseline.h"
#include "NaviCandidate.h"
#include "NaviEncounter.h"
#include "NaviTarget.h"

#pragma pack(push, 1)
struct Native { uint32_t serial; uint64_t us; int16_t adc; uint8_t pwm; uint8_t direction; };
#pragma pack(pop)
static_assert(sizeof(Native) == 16, "native record");

struct Cmd { uint64_t tUs; std::string leaf, payload; };

static bool parseInterval(const std::string& s, int& a, int& b) {
  const auto dash = s.find('-');
  if (dash == std::string::npos) return false;
  a = atoi(s.substr(0, dash).c_str()); b = atoi(s.substr(dash + 1).c_str());
  return true;
}

int main(int argc, char** argv) {
  if (argc < 5) { fprintf(stderr, "usage: replay native.bin declarations.csv boot_id outdir\n"); return 2; }
  FILE* f = fopen(argv[1], "rb"); if (!f) { perror(argv[1]); return 2; }
  const std::string bootId = argv[3];
  std::vector<Cmd> cmds;
  { FILE* d = fopen(argv[2], "r"); char line[512];
    if (d && fgets(line, sizeof line, d) == nullptr) { fclose(d); d = nullptr; }  // header
    while (d && fgets(line, sizeof line, d)) {
      char boot[64], topic[128], payload[64]; unsigned long long tUs;
      if (sscanf(line, "%63[^,],%llu,%127[^,],%63[^\n]", boot, &tUs, topic, payload) == 4 && bootId == boot) {
        const char* leaf = strrchr(topic, '/'); leaf = leaf ? leaf + 1 : topic;
        cmds.push_back({tUs, leaf, payload});
      }
    }
    if (d) fclose(d);
  }
  std::string out = argv[4];
  FILE* fe = fopen((out + "/encounters.csv").c_str(), "w");
  FILE* fn = fopen((out + "/nav.csv").c_str(), "w");
  FILE* fo = fopen((out + "/onsets.csv").c_str(), "w");
  fprintf(fe, "open_us,close_us,open_block,width_blocks,pol,peak_adc,meets_min_width,abandoned,timing_result,timing_measured_ms,timing_expected_ms,timing_tolerance_ms\n");
  fprintf(fn, "t_us,event,mm,expected,timing_result,timing_measured_ms\n");
  fprintf(fo, "onset_us,serial,pol,level_at_open\n");

  navi_power::Tracing tracing;
  navi_power::BaselineTracker baseline;
  navi_power::CandidateDetector candidate;
  navi_power::Encounter encounter;
  navi_power::TargetTracker target;
  baseline.beginBootCollection();
  uint32_t blockCounter = 0;
  uint32_t confirmed = 0, nonqualifying = 0, abandoned = 0, onsets = 0, baselineRetained = 0;
  size_t ci = 0;
  int8_t sessionDir = 0; bool forward = true;
  uint64_t lastUs = 0;

  auto applyCmd = [&](const Cmd& c) {
    if (c.leaf == "session_direction") { sessionDir = c.payload == "CW" ? 1 : -1; }
    else if (c.leaf == "direction") { forward = atoi(c.payload.c_str()) != 0; }
    else if (c.leaf == "start_interval" || c.leaf == "start_mm") {
      const int8_t dir = sessionDir * (forward ? 1 : -1);
      int last = -1, next = -1;
      if (c.leaf == "start_interval") {
        int a = -1, b = -1;
        if (!parseInterval(c.payload, a, b)) return;
        last = dir > 0 ? a : b; next = dir > 0 ? b : a;
      } else {
        const int mm = atoi(c.payload.c_str());
        last = mm; next = (mm + dir + navi_one::ROUTE_N) % navi_one::ROUTE_N;
      }
      target.declare(uint8_t(last), uint8_t(next), dir);
      fprintf(fn, "%llu,DECLARE,%d,%d\n", (unsigned long long)c.tUs, last, next);
    }
  };

  auto onBlock = [&](const navi_power::Block& b) {
    ++blockCounter;
    baseline.onBlock(b.hall, b.pwm > 0);   // the long measurement: always fed, inert while an encounter is active
    if (encounter.active()) {
      navi_power::CompletedEncounter done;
      if (encounter.push(b, done)) {
        const uint8_t expectedBefore = target.nextMm();
        const auto r = target.onEncounter(done);
        const auto& timing = target.lastTiming();
        fprintf(fe, "%llu,%llu,%u,%u,%d,%.1f,%u,%u,%u,%u,%u,%u\n", (unsigned long long)done.openUs, (unsigned long long)lastUs, done.openBlockId, done.widthBlocks,
                done.pol, double(done.peakAdc), unsigned(done.meetsMinWidth), unsigned(done.abandoned),
                unsigned(timing.result), timing.measuredMs, timing.expectedMs, timing.toleranceMs);
        if (r == navi_power::JudgementResult::Confirmed) {
          ++confirmed;
          fprintf(fn, "%llu,CONFIRMED,%u,%u,%u,%u\n", (unsigned long long)lastUs, target.lastMm(), expectedBefore, unsigned(timing.result), timing.measuredMs);
        } else if (done.abandoned) { ++abandoned; fprintf(fn, "%llu,ABANDONED,-1,%u,%u,%u\n", (unsigned long long)lastUs, expectedBefore, unsigned(timing.result), timing.measuredMs); }
        else { ++nonqualifying; fprintf(fn, "%llu,NONQUALIFYING,-1,%u,%u,%u\n", (unsigned long long)lastUs, expectedBefore, unsigned(timing.result), timing.measuredMs); }
        // No baseline.beginCollection() call: the long measurement resumes
        // watching for the next interval's entry point on its own.
      }
    }
  };

  Native r; uint64_t nrec = 0;
  while (fread(&r, sizeof r, 1, f) == 1) {
    lastUs = r.us;
    while (ci < cmds.size() && cmds[ci].tUs <= r.us) { applyCmd(cmds[ci]); ++ci; }
    if (baseline.haveLevel()) {
      navi_power::CandidateOnset onset;
      if (candidate.onSample(uint16_t(r.adc), baseline.level(), r.serial, r.us, r.pwm, onset)) {
        ++onsets;
        fprintf(fo, "%llu,%u,%d,%.1f\n", (unsigned long long)onset.openUs, onset.openSerial, onset.pol, double(onset.levelAtOpen));
        if (!encounter.active()) {
          uint32_t sampleCount = 0;
          if (!baseline.finalize(sampleCount)) ++baselineRetained;   // only the onset that actually opens a new encounter ends this interval
          encounter.open(onset, blockCounter);
        }
      }
    }
    tracing.add(r.us, uint16_t(r.adc), r.direction, r.serial, r.pwm, 255, onBlock);
    ++nrec;
  }
  fclose(f); fclose(fe); fclose(fn); fclose(fo);
  printf("{\"native_reads\":%llu,\"blocks\":%u,\"onsets\":%u,\"confirmed\":%u,\"nonqualifying\":%u,\"abandoned\":%u,\"baseline_retained\":%u}\n",
         (unsigned long long)nrec, blockCounter, onsets, confirmed, nonqualifying, abandoned, baselineRetained);
  return 0;
}

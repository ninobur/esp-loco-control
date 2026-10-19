// Host replay of the firmware recognition path on a recorded native Hall stream.
//   replay <native.bin> <declarations.csv> <out_dir> [--no-pause | --cmd cmd.bin]
// native.bin: packed records {u32 serial, u64 us, i16 adc, u8 pwm, u8 direction}
//   (the hall-ground-20261015 extraction). PWM is read only to be ignored.
// declarations.csv: block_index,last,next,dir  (tracing block index of each operator declaration)
// The STOP/GO WaveformPause (RvmPause.h) is applied as in the firmware unless --no-pause.
// The recordings carry applied PWM only; --cmd gives the commanded target per block (uint8).
// Writes out_dir/blocks.bin (float32 per block), out_dir/ensembles.csv, out_dir/nav.csv.
// Uses exactly the firmware headers: RvmTracing.h, RvmPause.h, RvmShape.h, RvmNavigator.h.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <chrono>
#include "RvmTracing.h"
#include "RvmPause.h"
#include "RvmShape.h"
#include "RvmNavigator.h"

#pragma pack(push, 1)
struct Native { uint32_t serial; uint64_t us; int16_t adc; uint8_t pwm; uint8_t direction; };
#pragma pack(pop)
static_assert(sizeof(Native) == 16, "native record");

struct Decl { uint32_t block; int last, next, dir; };

int main(int argc, char** argv) {
  if (argc < 4) { fprintf(stderr, "usage: replay native.bin decl.csv outdir\n"); return 2; }
  FILE* f = fopen(argv[1], "rb"); if (!f) { perror(argv[1]); return 2; }
  std::vector<Decl> decl;
  { FILE* d = fopen(argv[2], "r"); char line[256];
    while (d && fgets(line, sizeof line, d)) { Decl x; if (sscanf(line, "%u,%d,%d,%d", &x.block, &x.last, &x.next, &x.dir) == 4) decl.push_back(x); }
    if (d) fclose(d); }
  std::string out = argv[3];
  const bool usePause = !(argc > 4 && !strcmp(argv[4], "--no-pause"));
  std::vector<uint8_t> cmd;
  if (argc > 5 && !strcmp(argv[4], "--cmd")) { FILE* c = fopen(argv[5], "rb"); int ch; while (c && (ch = fgetc(c)) != EOF) cmd.push_back(uint8_t(ch)); if (c) fclose(c); }
  static rvm_shape::WaveformPause pause; uint64_t pausedBlocks = 0;
  static rvm_shape::Block holdStorage[rvm_shape::HOLD_MAX]; pause.begin(holdStorage);
  FILE* fb = fopen((out + "/blocks.bin").c_str(), "wb");
  FILE* fe = fopen((out + "/ensembles.csv").c_str(), "w");
  FILE* fn = fopen((out + "/nav.csv").c_str(), "w");
  fprintf(fe, "seq,c,w,s,e,decided,bl_start,llr,amp,level,noise,pol,c_id,s_id,e_id,decided_id,bl_start_id\n");
  fprintf(fn, "block,event,mm,expected,pol,seq\n");
  static rvm_shape::Recognizer rec;   // large: static storage, as in firmware
  rvm_shape::Navigator nav; rvm_shape::Tracing tr;
  size_t di = 0; uint64_t nblocks = 0;
  double maxStepUs = 0, totalUs = 0;
  // Append one block to the recognition waveform and log what it completes.
  auto pushOne = [&](const rvm_shape::Block& a) {
    auto t0 = std::chrono::steady_clock::now();
    rec.push(a.hall, a.dir == 2, a.index);
    double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    totalUs += us; if (us > maxStepUs) maxStepUs = us;
    rvm_shape::Ensemble e;
    while (rec.take(e)) {
      fprintf(fe, "%u,%.1f,%.0f,%.2f,%.2f,%.1f,%u,%.5f,%.3f,%.3f,%.4f,%d,%u,%u,%u,%u,%u\n", e.seq, e.c, e.w, e.s, e.e, e.decided, e.blStart,
              e.llr, e.amp, e.level, e.noise, e.pol, e.cId, e.sId, e.eId, e.decidedId, e.blStartId);
      const uint8_t expected = nav.nextMm();
      const auto r = nav.onEnsemble(e.pol);
      const char* ev = r == rvm_shape::NavResult::Advanced ? "MM" : r == rvm_shape::NavResult::Contradiction ? "CONTRADICTION" : "NO_CLAIM";
      fprintf(fn, "%u,%s,%d,%d,%d,%u\n", a.index, ev, r == rvm_shape::NavResult::Advanced ? nav.lastMm() : -1,
              r == rvm_shape::NavResult::NoClaim ? -1 : expected, e.pol, e.seq);
    }
  };
  auto onBlock = [&](const rvm_shape::Block& b) {
    while (di < decl.size() && decl[di].block == b.index) {
      if (usePause) pause.declared(pushOne);              // held blocks first, into the old context
      rec.startBaseline(rec.blocks(), b.index);
      nav.declare(uint8_t(decl[di].last), uint8_t(decl[di].next), int8_t(decl[di].dir));
      fprintf(fn, "%u,DECLARE,%d,%d,0,0\n", b.index, decl[di].last, decl[di].next);
      ++di;
    }
    fwrite(&b.hall, 4, 1, fb); ++nblocks;
    rvm_shape::Block bb = b; bb.cmd = b.index < cmd.size() ? cmd[b.index] : 255;
    int na = 0;
    if (usePause) pause.admit(bb, [&](const rvm_shape::Block& a) { ++na; pushOne(a); }); else { ++na; pushOne(bb); }
    if (!na) ++pausedBlocks;
  };

  Native r; uint64_t nrec = 0;
  while (fread(&r, sizeof r, 1, f) == 1) { tr.add(r.us, uint16_t(r.adc), r.direction, r.serial, r.pwm, 255, onBlock); ++nrec; }
  fclose(f); fclose(fb); fclose(fe); fclose(fn);
  printf("{\"pause\":%d,\"paused_blocks\":%llu,\"native_reads\":%llu,\"blocks\":%llu,\"windows_scored\":%u,\"push_mean_us\":%.2f,\"push_max_us\":%.1f,\"output_drops\":%u,\"first_block_us\":%llu,\"sizeof_recognizer\":%zu}\n",
         int(usePause), (unsigned long long)pausedBlocks, (unsigned long long)nrec, (unsigned long long)nblocks, rec.windowsScored(), totalUs / nblocks, maxStepUs, rec.outputDrops(),
         (unsigned long long)tr.firstBlockUs(), sizeof(rvm_shape::Recognizer));
  return 0;
}

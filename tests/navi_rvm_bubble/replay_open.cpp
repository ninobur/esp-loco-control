// Host replay of NAVI_RVM_CONTINUITY_MANUAL's recognition path on a recorded native Hall stream.
//   replay <native.bin> <declarations.csv> <out_dir>
// NAVI_RVM_AUTO_STATIONS_BUBBLE variant: continuity closes on every recognized ensemble (onEnsemble).
// native.bin: packed records {u32 serial, u64 us, i16 adc, u8 pwm, u8 direction} (the NSR native stream).
// declarations.csv: block_index,last,next,dir   (tracing block index of each operator declaration)
// Uses exactly the firmware headers: RvmTracing.h, RvmContinuity.h, RvmShape.h, RvmNavigator.h.
// Applied PWM in the records is carried in the blocks but read by nothing (Hall-only continuity).
// Writes out_dir/ensembles.csv, out_dir/nav.csv, out_dir/withheld.bin (u8 per block: 1 = withheld)
// and prints one JSON line of statistics.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <chrono>
#include "RvmTracing.h"
#include "RvmContinuity.h"
#include "RvmShape.h"
#include "RvmNavigator.h"

#pragma pack(push, 1)
struct Native { uint32_t serial; uint64_t us; int16_t adc; uint8_t pwm; uint8_t direction; };
#pragma pack(pop)
static_assert(sizeof(Native) == 16, "native record");

int main(int argc, char** argv) {
  if (argc < 4) { fprintf(stderr, "usage: replay native.bin decl.csv outdir\n"); return 2; }
  FILE* f = fopen(argv[1], "rb"); if (!f) { perror(argv[1]); return 2; }
  static rvm_shape::HallContinuity cont; static rvm_shape::Block storage[rvm_shape::CONT_K]; cont.begin(storage);
  struct D { unsigned blk; int a, b, dr; }; std::vector<D> decls; size_t di = 0;
  { FILE* d = fopen(argv[2], "r"); char line[256];
    while (d && fgets(line, sizeof line, d)) { D x; if (sscanf(line, "%u,%d,%d,%d", &x.blk, &x.a, &x.b, &x.dr) == 4) decls.push_back(x); }
    if (d) fclose(d); }
  // As on the locomotive, each declaration is handed over when its block arrives (the live
  // queue holds at most a few pending declarations).
  std::string out = argv[3];
  FILE* fe = fopen((out + "/ensembles.csv").c_str(), "w");
  FILE* fn = fopen((out + "/nav.csv").c_str(), "w");
  fprintf(fe, "seq,c,w,s,e,decided,bl_start,llr,amp,level,noise,pol,c_id,s_id,e_id,decided_id,bl_start_id\n");
  fprintf(fn, "block,event,mm,expected,pol,seq\n");
  static rvm_shape::Recognizer rec;
  rvm_shape::Navigator nav; rvm_shape::Tracing tr;
  std::vector<uint8_t> withheld;
  uint64_t nblocks = 0, nrec = 0; double maxStepUs = 0, totalUs = 0; uint32_t transitions = 0; bool wasW = false;
  auto pushOne = [&](const rvm_shape::Block& a) {
    auto t0 = std::chrono::steady_clock::now();
    rec.push(a.hall, a.dir == 2, a.index);
    double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    totalUs += us; if (us > maxStepUs) maxStepUs = us;
    rvm_shape::Ensemble e;
    while (rec.take(e)) {
      fprintf(fe, "%u,%.1f,%.0f,%.2f,%.2f,%.1f,%u,%.5f,%.3f,%.3f,%.4f,%d,%u,%u,%u,%u,%u\n", e.seq, e.c, e.w, e.s, e.e, e.decided, e.blStart,
              e.llr, e.amp, e.level, e.noise, e.pol, e.cId, e.sId, e.eId, e.decidedId, e.blStartId);
      cont.onEnsemble(e.level, e.noise);            // as the sketch: the ensemble is complete
      const uint8_t expected = nav.nextMm();
      const auto r = nav.onEnsemble(e.pol);
      const char* ev = r == rvm_shape::NavResult::Advanced ? "MM" : r == rvm_shape::NavResult::Contradiction ? "CONTRADICTION" : "NO_CLAIM";
      fprintf(fn, "%u,%s,%d,%d,%d,%u\n", a.index, ev, r == rvm_shape::NavResult::Advanced ? nav.lastMm() : -1,
              r == rvm_shape::NavResult::NoClaim ? -1 : expected, e.pol, e.seq);
    }
  };
  auto onDeclare = [&](const rvm_shape::ContDecl& d) {
    rec.startBaseline(rec.blocks(), d.index);
    nav.declare(d.last, d.next, d.dir);
    fprintf(fn, "%u,DECLARE,%d,%d,0,0\n", d.index, d.last, d.next);
  };
  auto track = [&]() {
    if (cont.withholding() != wasW) { ++transitions; wasW = cont.withholding(); }
    const uint32_t id = cont.lastDecidedId();
    if (withheld.size() <= id) withheld.resize(id + 1, 0);
    withheld[id] = cont.withholding() ? 1 : 0;
  };
  auto onBlock = [&](const rvm_shape::Block& b) {
    ++nblocks;
    while (di < decls.size() && decls[di].blk == b.index) {
      if (!cont.declareAt(b.index, uint8_t(decls[di].a), uint8_t(decls[di].b), int8_t(decls[di].dr))) fprintf(stderr, "declaration queue full at %u\n", b.index);
      ++di;
    }
    const uint16_t had = cont.delayBlocks();
    cont.admit(b, pushOne, onDeclare);
    if (had + 1 > cont.delayBlocks()) track();          // a block was decided
  };
  Native r;
  while (fread(&r, sizeof r, 1, f) == 1) { tr.add(r.us, uint16_t(r.adc), r.direction, r.serial, r.pwm, 255, onBlock); ++nrec; }
  while (cont.flushOne(pushOne, onDeclare)) track();     // end of the recording: decide the delay line
  fclose(f); fclose(fe); fclose(fn);
  FILE* fw = fopen((out + "/withheld.bin").c_str(), "wb"); fwrite(withheld.data(), 1, withheld.size(), fw); fclose(fw);
  printf("{\"native_reads\":%llu,\"blocks\":%llu,\"withheld_blocks\":%u,\"transitions\":%u,\"windows_scored\":%u,\"push_mean_us\":%.2f,\"push_max_us\":%.1f,"
         "\"output_drops\":%u,\"first_block_us\":%llu,\"sizeof_recognizer\":%zu,\"sizeof_continuity\":%zu,\"continuity_storage_bytes\":%zu}\n",
         (unsigned long long)nrec, (unsigned long long)nblocks, cont.withheldTotal(), transitions, rec.windowsScored(), totalUs / (nblocks ? nblocks : 1), maxStepUs,
         rec.outputDrops(), (unsigned long long)tr.firstBlockUs(), sizeof(rvm_shape::Recognizer), sizeof(rvm_shape::HallContinuity),
         sizeof(rvm_shape::Block) * rvm_shape::CONT_K);
  return 0;
}

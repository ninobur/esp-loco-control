// Firmware path on a 20 ms block sequence (synthetic STOP/GO cases).
//   blocks_replay <blocks.csv> <decl_block> <last> <next> <dir>
// blocks.csv: one line per block "hall,dir,pwm" (hall "nan" for a gap).
// Prints one line per recognized ensemble: seq,c_id,s_id,e_id,decided_id,pol,llr,nav_result,mm
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "RvmPause.h"
#include "RvmShape.h"
#include "RvmNavigator.h"
int main(int argc, char** argv) {
  if (argc < 6) { fprintf(stderr, "usage\n"); return 2; }
  FILE* f = fopen(argv[1], "r"); if (!f) return 2;
  const unsigned decl = unsigned(atoi(argv[2]));
  static rvm_shape::Recognizer rec; rvm_shape::Navigator nav; rvm_shape::WaveformPause pause;
  char line[128]; unsigned id = 0;
  while (fgets(line, sizeof line, f)) {
    char hs[32]; int dir, pwm;
    if (sscanf(line, "%31[^,],%d,%d", hs, &dir, &pwm) != 3) continue;
    const float hall = strtof(hs, nullptr);
    if (id == decl) { rec.startBaseline(rec.blocks(), id); pause.declared(); nav.declare(uint8_t(atoi(argv[3])), uint8_t(atoi(argv[4])), int8_t(atoi(argv[5]))); }
    rvm_shape::Block blk{}; blk.index = id; blk.hall = hall; blk.dir = uint8_t(dir); blk.pwm = uint8_t(pwm);
    rvm_shape::Block adm[2]; const int na = pause.admit(blk, adm);
    for (int q = 0; q < na; ++q) {
      rec.push(adm[q].hall, adm[q].dir == 2, adm[q].index);
      rvm_shape::Ensemble e;
      while (rec.take(e)) {
        const auto r = nav.onEnsemble(e.pol);
        printf("%u,%u,%u,%u,%u,%d,%.4f,%d,%d\n", e.seq, e.cId, e.sId, e.eId, e.decidedId, e.pol, e.llr, int(r),
               r == rvm_shape::NavResult::Advanced ? nav.lastMm() : -1);
      }
    }
    ++id;
  }
  fclose(f); return 0;
}

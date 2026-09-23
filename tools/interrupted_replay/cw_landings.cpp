// CW station landings, coast swept 60-140% of the measured fit.
//
// WHY THIS EXISTS: gate 12 section H is CCW ONLY -- declare(128, -1) --
// because findings 11 and 13 were both CCW. The clockwise stop offsets are
// different numbers (+1, -1, +1, +1 against the retuned CCW 0, -1, 0, -1)
// and had never been run through the pause/resume path at all.
//
// It includes gate_interrupted.cpp rather than copying its Rig, so there is
// exactly one reproduction of hallTask()/stationService()/loop() in the tree
// -- the same reason tools/interrupted_replay/proposed/ was deleted.
//
//   c++ -std=c++17 -O1 -w tools/interrupted_replay/cw_landings.cpp -o /tmp/cw && /tmp/cw
//
// NOT A GATE. It asserts nothing; it predicts. The coast sweep is a COVERAGE
// DEVICE, not a claim that the coast varies by 40% -- it walks the landing
// 390 mm, more than the 300 mm between markers, so every resting place is
// visited and the answer does not depend on the coast being exact.
#define main gate12_main_unused
#include "../../firmware/programs/NAVI_ONE/variants/NAVI_ONE/tests/gate_interrupted.cpp"
#undef main

struct Row { int pct; double nearest, field; int pauses, resumes, stitched, refused, adv;
             bool autoOn; float resid; uint8_t mm; bool ok; };

int main() {
  printf("CW STATION LANDINGS, COAST SWEPT 60%%-140%% OF THE MEASURED FIT\n");
  printf("The same sweep gate 12 section H applies CCW. It walks the landing\n");
  printf("further than the 300 mm between markers, so every resting place is\n");
  printf("visited and the answer does not depend on the coast being exact.\n");

  int totalStitch = 0, totalRefuse = 0, totalStop = 0;
  for (uint8_t si = 0; si < STATION_COUNT; ++si) {
    const StationDefinition& st = STATIONS[si];
    printf("\n  %s  (centre %u, CW stop offset %+d, departs to %u)\n",
           st.name, st.centre, (int)st.stopOffsetCW,
           st.departCW ? st.departCW : (unsigned)NAVI_AUTO_CRUISE_PWM);
    printf("   %5s %8s %8s %5s %5s %6s %8s  %s\n",
           "coast", "at rest", "field", "paus", "resu", "stitch", "resid", "outcome");
    for (int pct = 60; pct <= 140; pct += 10) {
      Rig rg; uint32_t t = 1;
      primeCapture(rg, t);
      rg.nav.declare(routeMod((int32_t)st.centre - 20), +1);
      primeLap(rg, 8, (int)AMP, t);
      rg.clearTrace();
      rg.useStations = true;
      rg.requestPwm(NAVI_AUTO_CRUISE_PWM, 62, 200);

      std::vector<double> mx; std::vector<int> mp;
      double acc = 0.0; uint8_t m = rg.nav.status().navMm;
      for (int k = 0; k < 40; ++k) {
        acc += (double)spanMm(m, +1); m = nextMarker(m, +1);
        mx.push_back(acc); mp.push_back(polarityAt(m));
      }
      double x = 0.0, restField = 0.0, nearest = 0.0;
      uint8_t restMm = 0; bool armedSeen = false, resting = false;
      for (uint32_t i = 0; i < 400000; ++i, ++t) {
        const uint8_t pwm = (uint8_t)rg.actualPwm;
        const double v = pwm > 25 ? (pct / 100.0) * 3.990 * (pwm - 25.1) : 0.0;
        x += v / 1000.0;
        double field = 0.0;
        for (size_t k = 0; k < mx.size(); ++k) {
          const double d = x - mx[k];
          if (d > -90 && d < 90) field += (mp[k] ? 1 : -1) * gaussAt(d, 0, SIGMA_MM, AMP);
        }
        rg.tick(t, (int16_t)(IDLE + (int)field));
        if (!armedSeen && rg.armedCount == 1) armedSeen = true;
        if (rg.dwellBeginCount == 1 && !resting) {
          resting = true; restField = field; restMm = rg.nav.status().navMm;
          double best = 1e9;
          for (size_t k = 0; k < mx.size(); ++k)
            if (std::fabs(x - mx[k]) < std::fabs(best)) best = x - mx[k];
          nearest = best;
        }
        if (!rg.autoRunning && armedSeen) break;
        if (rg.departSeen && t > rg.dwellTo + 25000) break;
      }
      char rs[16]; snprintf(rs, sizeof(rs), "%.4f", (double)rg.lastResidual);
      const char* out =
        (!rg.autoRunning && armedSeen && !resting)
          ? "*** STOPPED ON THE APPROACH, BEFORE THE DWELL ***"
        : !rg.autoRunning ? "*** STOPPED -- refused stitched waveform ***"
        : !resting ? "*** never reached the dwell ***"
        : rg.stitched ? "stitched, ACCEPTED"
        : "rests clear";
      char rp[16], fp[16];
      if (resting) { snprintf(rp,sizeof rp,"%+.0fmm",nearest); snprintf(fp,sizeof fp,"%+.0f",restField); }
      else { snprintf(rp,sizeof rp,"  --  "); snprintf(fp,sizeof fp,"  --  "); }
      printf("   %4d%% %8s %8s %5d %5d %6d %8s  %s\n",
             pct, rp, fp, rg.pauses, rg.resumes, rg.stitched,
             rg.stitched ? rs : "   --   ", out);
      if (pct == 70 && si == 0) { printf("      --- trace of this one ---\n");
        for (const auto& e : rg.log) printf("      %s\n", e.c_str()); }
      if (rg.stitched) ++totalStitch;
      if (rg.stitchedRefusals) ++totalRefuse;
      if (!rg.autoRunning && armedSeen) ++totalStop;
      (void)restMm;
    }
  }
  printf("\n  ACROSS ALL 36 CW LANDINGS: %d produce a stitched waveform, %d of\n"
         "  those are refused, %d stop the locomotive.\n",
         totalStitch, totalRefuse, totalStop);
  return 0;
}

// Gate 10: the station machine — approach, stop, dwell, departure.
//
// Pure, so it can be driven marker by marker with a fake clock. Everything the
// operator specified on 2026-09-01 is asserted here: the per-direction stop
// offsets, the 30 s dwell, station speed held to the stop trigger with no
// intermediate step, the approach ramping from whatever the locomotive is
// actually doing, and the pacing coming from marker times so one table serves
// a 6-count and a 9-count approach alike.
#include <cstdio>
// Toby's profile carries NAVI_APPROACH_MARKER_MS, and this gate tests HIS
// numbers, so it includes the profile directly rather than a synthetic table.
#include "../LL_LocoConfig_9950012.h"
#include "../RouteMap.h"
#include "../Stations.h"
using namespace navi_one;

static int checks = 0, failures = 0;
static void ok(bool c, const char* what, const char* d = "") {
  ++checks; if (!c) { ++failures; printf("  FAIL %s %s\n", what, d); }
}

// Walk the locomotive marker by marker, collecting the orders it is given.
struct Step { int16_t off; uint8_t pwm; uint16_t stepMs; const char* ev; };

int main() {
  printf("gate 10 -- station approach, stop, dwell, departure\n\n");

  printf("A. Patio CW from cruise 90: the approach profile\n");
  {
    StationMachine m; uint32_t t = 0;
    const uint8_t centre = 15;
    uint8_t pwm = 90;
    const uint8_t wantPwm[5]  = { 84, 78, 72, 66, 60 };
    const uint16_t wantPace[5] = { 231, 256, 287, 326, 378 };  // marker ms / 6
    int seen = 0;
    for (int mm = centre - 12; mm <= centre + 4; ++mm, t += 1500) {
      StationOrder o = m.tick((uint8_t)mm, +1, pwm, 90, t);
      if (o.setThrottle) pwm = o.pwm;
      const int16_t off = offsetToCentre((uint8_t)mm, +1, centre);
      if (off >= -10 && off <= -6 && o.setThrottle) {
        const int i = off + 10;
        ok(o.pwm == wantPwm[i], "approach target", o.station);
        ok(o.stepMs == wantPace[i], "approach pacing", o.station);
        if (o.pwm != wantPwm[i] || o.stepMs != wantPace[i])
          printf("       off %d: got pwm %u pace %u, want %u / %u\n",
                 off, o.pwm, o.stepMs, wantPwm[i], wantPace[i]);
        ++seen;
      }
    }
    ok(seen == 5, "five approach markers issued an order");
  }

  printf("B. station speed is HELD to the stop trigger -- no second step down\n");
  {
    StationMachine m; uint32_t t = 0; uint8_t pwm = 90;
    bool sawBelow60BeforeStop = false, sawZeroAt = false;
    for (int mm = 3; mm <= 17; ++mm, t += 1500) {
      StationOrder o = m.tick((uint8_t)mm, +1, pwm, 90, t);
      if (!o.setThrottle) continue;
      const int16_t off = offsetToCentre((uint8_t)mm, +1, 15);
      if (off >= -5 && off < 1 && o.pwm != 0 && o.pwm < 60) sawBelow60BeforeStop = true;
      if (o.pwm == 0) { sawZeroAt = true; ok(off == 1, "zero ramp starts at stopOffset +1"); }
      pwm = o.pwm;
    }
    ok(!sawBelow60BeforeStop, "no intermediate speed between the zone and the stop");
    ok(sawZeroAt, "a zero ramp was ordered");
  }

  printf("C. the dwell is 30 s, and departure returns to cruise at 200 ms/count\n");
  {
    StationMachine m; uint32_t t = 0; uint8_t pwm = 90;
    for (int mm = 3; mm <= 16; ++mm, t += 1500) {
      StationOrder o = m.tick((uint8_t)mm, +1, pwm, 90, t);
      if (o.setThrottle) pwm = o.pwm;
    }
    ok(m.phase() == StPhase::Ramp, "in the zero ramp after the trigger");
    pwm = 0;                                   // the ramp completes
    m.tick(16, +1, pwm, 90, t);
    ok(m.phase() == StPhase::Dwell, "dwell begins when the throttle reaches zero");
    const uint32_t dwellFrom = t;
    m.tick(16, +1, 0, 90, dwellFrom + 29000);
    ok(m.phase() == StPhase::Dwell, "still dwelling at 29 s");
    StationOrder d = m.tick(16, +1, 0, 90, dwellFrom + 30001);
    ok(m.phase() == StPhase::Depart, "departs after 30 s");
    ok(d.setThrottle && d.pwm == 90, "departs to cruise");
    ok(d.stepMs == 200, "departure paced at 200 ms per count");
  }

  printf("D. Patio CCW arrives at 105 off the curve -- 9 counts a marker\n");
  {
    StationMachine m; uint32_t t = 0; uint8_t pwm = 105;
    const uint8_t wantPwm[5]  = { 96, 87, 78, 69, 60 };
    const uint16_t wantPace[5] = { 154, 171, 191, 217, 252 };  // marker ms / 9
    int seen = 0;
    for (int mm = 27; mm >= 12; --mm, t += 1500) {
      StationOrder o = m.tick((uint8_t)mm, -1, pwm, 105, t);
      if (!o.setThrottle) continue;
      const int16_t off = offsetToCentre((uint8_t)mm, -1, 15);
      if (off >= -10 && off <= -6) {
        const int i = off + 10;
        ok(o.pwm == wantPwm[i], "CCW approach target");
        ok(o.stepMs == wantPace[i], "CCW approach pacing");
        if (o.pwm != wantPwm[i] || o.stepMs != wantPace[i])
          printf("       off %d: got %u / %u, want %u / %u\n",
                 off, o.pwm, o.stepMs, wantPwm[i], wantPace[i]);
        ++seen;
      }
      pwm = o.pwm;
    }
    ok(seen == 5, "five CCW approach markers");
  }

  printf("E. Grillers is the one asymmetric platform: 72 CCW, 60 CW\n");
  {
    const StationDefinition* g = nullptr;
    for (uint8_t i = 0; i < STATION_COUNT; ++i)
      if (STATIONS[i].centre == 63) g = &STATIONS[i];
    ok(g != nullptr, "Grillers present");
    if (g) {
      ok(stationPwm(*g, -1) == 72, "Grillers CCW is 72");
      ok(stationPwm(*g, +1) == 60, "Grillers CW is 60");
    }
  }

  printf("F. stop offsets are per station AND per direction\n");
  {
    // The whole table, asserted literally, because every one of these is a
    // measurement the operator took from an observed landing and any drift in
    // them is a change to where the locomotive stops in front of visitors.
    // Order: Patio, Grillers, Arches, Bamboo.
    // Arches CW moved three times on 2026-09-02, once for the firmware and
    // twice for the track: +1 -> +2 to take a slipping departure off the slight
    // grade (field finding 14), +2 -> +1 when the operator levelled that track
    // by hand and the longer coast carried the stop too far forward, +1 -> 0
    // when he went back and took the dip out as well. Each number was measured
    // from where Toby actually landed. See the note in Stations.h; none of the
    // three is a revert of another.
    const int8_t wantCW[4]  = {  1, -1,  0,  1 };
    const int8_t wantCCW[4] = {  0, -1,  0, -1 };
    for (uint8_t i = 0; i < STATION_COUNT; ++i) {
      ok(stopOffsetFor(STATIONS[i], +1) == wantCW[i],  "CW stop offset");
      ok(stopOffsetFor(STATIONS[i], -1) == wantCCW[i], "CCW stop offset");
    }
  }
  ok(STATION_COUNT == 4, "four platforms");
  {
    const uint8_t want[4] = { 15, 63, 108, 157 };
    for (uint8_t i = 0; i < 4; ++i)
      ok(STATIONS[i].centre == want[i], "station centre");
  }

  printf("G. an overshoot stands the machine down instead of chasing it\n");
  {
    StationMachine m; uint32_t t = 0;
    m.tick(5, +1, 90, 90, t);                       // arm at -10
    ok(m.phase() == StPhase::Approach, "armed");
    StationOrder o = m.tick(25, +1, 90, 90, t + 5000);   // now well past centre
    ok(m.phase() == StPhase::Idle, "stood down after overshoot");
    ok(o.event != nullptr, "and said so");
  }

  printf("H. an unset direction cannot arm a station\n");
  {
    StationMachine m;
    for (int mm = 0; mm < ROUTE_N; ++mm) m.tick((uint8_t)mm, 0, 90, 90, 0);
    ok(m.phase() == StPhase::Idle, "never armed with dir 0");
  }

  printf("I. Grillers CW: stops a marker SHORT of centre and departs to 110\n");
  {
    // 2026-09-01, after the wheels spun leaving Grillers: "The problem was Toby
    // was attempting to start the grade while on the grade." Centre is 63, so a
    // -1 stop puts the zero ramp at MM62, off the climb, with a run at it. The
    // departure asks for 110 outright; the section cruise reaches 110 at MM65
    // by itself, so the two agree and nothing steps between them.
    const StationDefinition& g = STATIONS[1];
    ok(g.centre == 63, "Grillers centre");
    ok(stopOffsetFor(g, +1) == -1, "CW stop moved to -1");
    ok(stopOffsetFor(g, -1) == -1, "CCW stop is also -1: the descent overshot too");

    StationMachine m; uint32_t t = 0; uint8_t pwm = 90;
    bool rampedAt62 = false;
    for (int mm = 53; mm <= 62; ++mm, t += 1500) {
      StationOrder o = m.tick((uint8_t)mm, +1, pwm, cruisePwmAt((uint8_t)mm, +1, 90), t);
      if (o.setThrottle) pwm = o.pwm;
      if (mm == 62 && m.phase() == StPhase::Ramp) rampedAt62 = true;
    }
    ok(rampedAt62, "the zero ramp starts at MM62, one marker before centre");
    pwm = 0; t += 1500;
    m.tick(62, +1, pwm, cruisePwmAt(62, +1, 90), t);
    ok(m.phase() == StPhase::Dwell, "dwell begins");
    const uint32_t from = t;
    StationOrder d = m.tick(62, +1, 0, cruisePwmAt(62, +1, 90), from + 30001);
    ok(m.phase() == StPhase::Depart, "departs after 30 s");
    ok(cruisePwmAt(62, +1, 90) == 90, "the section cruise at the stop is still 90");
    ok(d.setThrottle && d.pwm == 110, "but the departure asks for 110");
    ok(d.stepMs == 200, "at the unchanged 200 ms pacing");

    // and 110 is sustained to MM80, where the existing ramp-down takes over.
    for (int mm = 65; mm < 80; ++mm)
      ok(cruisePwmAt((uint8_t)mm, +1, 90) == 110, "110 held across the climb");
    ok(cruisePwmAt(80, +1, 90) == 106, "MM80 begins the existing ramp down");
    ok(cruisePwmAt(85, +1, 90) == 90,  "and reaches 90 by MM85, untouched");
  }

  printf("I2. Grillers CCW ramps at MM64 -- two markers earlier than it did\n");
  {
    // Counter-clockwise is the descent. At station speed 72 he carried two
    // markers past the ramp point on 2026-09-01: ZERO_RAMP MM62, rest MM60,
    // which parked the sensor in MM59's field. Finding 11. The ramp now starts
    // at MM64 and the departure is as per routing, not the CW 110.
    StationMachine m; uint32_t t = 0; uint8_t pwm = 90;
    bool rampedAt64 = false;
    for (int mm = 73; mm >= 64; --mm, t += 1500) {
      StationOrder o = m.tick((uint8_t)mm, -1, pwm, cruisePwmAt((uint8_t)mm, -1, 90), t);
      if (o.setThrottle) pwm = o.pwm;
      if (mm == 64 && m.phase() == StPhase::Ramp) rampedAt64 = true;
    }
    ok(rampedAt64, "the zero ramp starts at MM64");
    ok(offsetToCentre(64, -1, 63) == -1, "which is offset -1 counter-clockwise");
    ok(offsetToCentre(62, -1, 63) == +1, "where MM62, the old ramp point, is +1");
    pwm = 0; t += 1500;
    m.tick(64, -1, pwm, cruisePwmAt(64, -1, 90), t);
    ok(m.phase() == StPhase::Dwell, "dwell begins");
    StationOrder d = m.tick(64, -1, 0, cruisePwmAt(64, -1, 90), t + 30001);
    ok(m.phase() == StPhase::Depart, "departs after 30 s");
    ok(d.setThrottle && d.pwm == 90, "CCW departs as per routing, NOT the CW 110");
  }

  printf("J. the DEPARTURE change reaches Grillers CW and nothing else\n");
  {
    // The 110 departure is Grillers CW alone; every other platform-direction
    // departs as per routing. TWO clockwise exceptions to the +1 standard --
    // Grillers at -1 for the grade, Arches at 0 over track the operator re-laid
    // twice on 2026-09-02 -- and this asserts exactly those two and no third.
    for (uint8_t i = 0; i < STATION_COUNT; ++i) {
      const StationDefinition& s2 = STATIONS[i];
      const bool grillersCW = (i == 1);
      const bool archesCW   = (i == 2);
      ok(departPwmFor(s2, +1, 90) == (grillersCW ? 110 : 90), "CW departure target");
      ok(departPwmFor(s2, -1, 90) == 90, "CCW departs as per routing everywhere");
      ok(departPwmFor(s2, -1, 105) == 105, "including off the Patio curve at 105");
      ok(stopOffsetFor(s2, +1) == (grillersCW ? -1 : (archesCW ? 0 : 1)),
         "CW stop offset");
      ok(departPwmFor(s2, -1, 72) == 72, "CCW departure follows the cruise it is handed");
    }
  }

  printf("\n%d checks, %d failures\n", checks, failures);
  if (failures) { printf("GATE 10 FAILED\n"); return 1; }
  printf("GATE 10 PASSED\n");
  return 0;
}

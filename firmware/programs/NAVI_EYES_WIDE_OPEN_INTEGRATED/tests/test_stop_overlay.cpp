#include "../NaviStopOverlay.h"
#include <cassert>
#include <cstdio>

using namespace navi_eyes;

static StopGeography remaining(int32_t distance, int8_t direction = 1,
                               int32_t target = 45 * kStopMm + kStopMm / 2) {
  return stopGeography(stopRouteMod(int64_t(target) - direction * distance,
                                   kStopCircuit), target, direction);
}

int main() {
  // These are operator-specified values at independent positions, including
  // the two different slopes in the final MM. The plateau is exactly five MM.
  struct Point { int32_t distance; uint8_t pwm; StopSection section; };
  const Point points[] = {
    {11000000,90,StopSection::Approach}, {10900000,89,StopSection::Approach},
    {10500000,85,StopSection::Approach}, {10000000,80,StopSection::Approach},
    {9000000,70,StopSection::Approach}, {8000000,60,StopSection::Approach},
    {7000000,50,StopSection::Fifty}, {6500000,50,StopSection::Fifty},
    {5500000,50,StopSection::Fifty}, {4500000,50,StopSection::Fifty},
    {3500000,50,StopSection::Fifty}, {2500000,50,StopSection::Fifty},
    {2000000,50,StopSection::Penultimate}, {1500000,40,StopSection::Penultimate},
    {1000000,30,StopSection::Final}, {750000,25,StopSection::Final},
    {500000,20,StopSection::Final}, {250000,10,StopSection::Final},
    {0,0,StopSection::Final},
  };
  for (int8_t dir : {int8_t(1), int8_t(-1)}) {
    for (const auto& p : points) {
      // All entries work directly: no earlier start/admission event is needed.
      const auto g = remaining(p.distance, dir);
      assert(g.applicable() && g.pwm == p.pwm && g.section == p.section);
      NaviStopOverlay cold;
      const auto command = cold.command(g, true, dir, 90, 50, 50, 0, true);
      assert(command.overlay);
      assert(command.pwm == (p.section == StopSection::Final ? 0 : p.pwm));
      // A portable target near the route wrap yields the same instruction.
      const auto wrapped = remaining(p.distance, dir, kStopMm / 2);
      assert(wrapped.pwm == p.pwm && wrapped.section == p.section);
    }
    assert(!remaining(11000001, dir).applicable());
    assert(!remaining(-1, dir).applicable());
  }

  // Physical target is identical from either direction; route spacing, rather
  // than an assumed universal 300 mm, maps offsets to fractions of a MM.
  const int32_t targetUm = stopMarkerUm(45) + 150000;
  assert(navi_one::spanMm(45, 1) == 300);
  assert(stopPositionUm(45, 1, 150000) == targetUm);
  assert(stopPositionUm(46, -1, 150000) == targetUm);
  assert(stopCoordinate(targetUm) == 45500000);
  for (uint8_t mm = 0; mm < navi_one::ROUTE_N; ++mm) {
    const auto half = int32_t(navi_one::spanMm(mm, 1)) * 500;
    assert(stopCoordinate(stopMarkerUm(mm) + half) == mm * kStopMm + kStopMm / 2);
    assert(stopPositionUm(mm, 1, half) ==
           stopPositionUm(navi_one::nextMarker(mm, 1), -1, half));
    assert(stopPositionUm(mm, -1, -half) == stopPositionUm(mm, 1, half));
  }

  NaviStopOverlay stop;
  auto held = stop.command(remaining(9500000), true, 1, 90, 80, 80, 0, true);
  assert(held.overlay && held.pwm == 75);
  held = stop.command({}, false, 1, 90, 78, 75, 1, true);
  assert(held.overlay && held.pwm == 75); // An IR gap preserves authority as well as the command.
  stop.reset();
  const auto final = remaining(750000);
  auto command = stop.command(final, true, 1, 90, 25, 25, 100, true);
  assert(command.overlay && command.pwm == 0);
  // Loss of geography does not interrupt an established final ramp.
  command = stop.command({}, false, 1, 90, 20, 0, 200, true);
  assert(command.overlay && command.pwm == 0 && command.execution == StopExecution::Braking);
  command = stop.command({}, false, 1, 90, 0, 0, 1000, true);
  assert(command.execution == StopExecution::Dwell && !command.releasedNow);
  command = stop.command({}, false, 1, 90, 0, 0, 5999, true);
  assert(command.overlay && !command.releasedNow);
  // No IR availability or target-attainment condition gates completion.
  command = stop.command({}, false, 1, 90, 0, 0, 6000, true);
  assert(!command.overlay && command.releasedNow && command.pwm == 90);
  // An early physical stop cannot reissue itself during departure.
  for (uint32_t t = 6100; t < 10000; t += 100)
    assert(!stop.command(final, true, 1, 90, 10, 90, t, true).overlay);
  stop.command(remaining(-1000), true, 1, 90, 90, 90, 10000, true);
  command = stop.command(remaining(11000000), true, 1, 90, 90, 90, 20000, true);
  assert(command.overlay && command.pwm == 90);  // Next circuit repeats.

  // Manual STOP/GO preserves an unfinished stop in its footprint, and a
  // manual nonzero PWM invalidates previously accrued continuous zero dwell.
  stop.reset();
  stop.command(final, true, 1, 90, 20, 20, 0, true);
  stop.command(final, true, 1, 90, 0, 0, 100, false);
  command = stop.command(final, true, 1, 90, 0, 0, 5100, false);
  assert(!command.releasedNow && command.pwm == 0); // Disabled AUTO cannot depart.
  command = stop.command(final, true, 1, 90, 0, 0, 5101, true);
  assert(command.releasedNow && command.pwm == 90);
  // Leaving the footprint under Manual also completes departure. Re-entry
  // on GO is a fresh visit, not suppression by an old completed operation.
  stop.command(remaining(-1000), true, 1, 90, 40, 40, 5200, false);
  command = stop.command(final, true, 1, 90, 20, 20, 5300, true);
  assert(command.overlay && command.pwm == 0);
  stop.reset();
  stop.command(final, true, 1, 90, 0, 0, 100, true);
  stop.command(final, true, 1, 90, 10, 10, 4000, false);
  command = stop.command(final, true, 1, 90, 0, 0, 5000, true);
  assert(!command.releasedNow);
  assert(stop.command(final, true, 1, 90, 0, 0, 10000, true).releasedNow);

  // Manual relocation / reversal uses the current requirement on GO.
  stop.reset();
  stop.command(final, true, 1, 90, 10, 0, 0, true);
  stop.command(final, true, 1, 90, 0, 0, 100, false);
  command = stop.command(remaining(5000000), true, 1, 90, 0, 0, 200, true);
  assert(command.pwm == 50 && command.execution == StopExecution::Geographic);
  command = stop.command(remaining(12000000,-1), true, -1, 90, 0, 0, 300, true);
  assert(!command.overlay && command.pwm == 90);

  // The MCU millisecond counter wraps; the five-second duration remains exact.
  stop.reset();
  stop.command(final, true, 1, 90, 0, 0, UINT32_MAX - 1000, true);
  assert(!stop.command(final, true, 1, 90, 0, 0, 3998, true).releasedNow);
  assert(stop.command(final, true, 1, 90, 0, 0, 3999, true).releasedNow);
  std::puts("PASS: portable STOP geometry, both directions, cold entry, timed final/dwell, interruption, departure and repeat");
}

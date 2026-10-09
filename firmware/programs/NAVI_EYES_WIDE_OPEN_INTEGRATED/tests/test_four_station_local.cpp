#include <assert.h>
#include <stdio.h>
#include "../NaviFourStationLocal.h"
using namespace navi_eyes;
int main() {
  for (int d : {1, -1}) for (uint8_t c : FourStationGeography::kCentres) {
    assert(FourStationGeography::evaluate(navi_one::routeMod(c - 10 * d), d).instruction == LocalInstruction::Approach);
    assert(FourStationGeography::evaluate(navi_one::routeMod(c - 5 * d), d).instruction == LocalInstruction::StationSpeed);
    assert(FourStationGeography::evaluate(c, d).instruction == LocalInstruction::FinalStop);
    assert(FourStationGeography::evaluate(navi_one::routeMod(c + 2 * d), d).instruction == LocalInstruction::StopTile);
  }
  FourStationLocalController c; LocalControlInput i{}; i.mm=40; i.direction=1; i.actualPwm=90; i.irSpeedValid=true; i.speedPkph=45;
  for (unsigned n=1;n<=5;++n) { i.irSequence=n; i.nowUs=n*100000; (void)c.tick(i,90); }
  i.speedPkph=50; for (unsigned n=6;n<=10;++n) { i.irSequence=n; i.nowUs=n*100000; auto o=c.tick(i,90); if(n==10) assert(o.pwmTarget==89); }
  // A stop tile needs actual PWM=0 and 50 ms without wheel displacement;
  // ordinary same-position IR packets do not postpone dwell.
  c.reset(); i.mm=navi_one::routeMod(15+2); i.actualPwm=0; i.speedPkph=0; i.irUm=100; i.irSequence=20; i.nowUs=1;
  assert(c.tick(i,90).pwmTarget == 0);
  i.irSequence=21; i.nowUs=50001; assert(c.tick(i,90).dwelling);
  i.nowUs=5050002; assert(c.tick(i,90).pwmTarget == 90);
  // A glide is paced as a monotonic one-count request; losing IR does not add
  // a corrective increase or withdraw the current geographic instruction.
  c.reset(); i.mm=5; i.actualPwm=90; i.irUm=200; i.irSequence=30; i.speedPkph=45; i.nowUs=6000000;
  assert(c.tick(i,90).pwmTarget == 89);
  i.irSpeedValid=false; i.irSequence=31; i.nowUs=6100000;
  assert(c.tick(i,90).pwmTarget == 89);
  puts("PASS: four-station geography and five-beat homeostasis");
}

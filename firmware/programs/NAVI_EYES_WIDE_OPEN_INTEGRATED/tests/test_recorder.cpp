#include "../NaviSyncRecorder.h"
#include "../NaviIntegratedCore.h"

#include <cassert>
#include <climits>
#include <cstdio>

using namespace navi_sync;

int main() {
  Recorder recorder;
  recorder.begin(9950012, 7, 42);
  Context context;
  context.navMm = 40;
  context.navDir = 1;
  for (unsigned group = 0; group < HALL_BATCH_SAMPLES; ++group) {
    HallSample sample{};
    sample.median = INT16_MIN;
    for (unsigned i = 0; i < 5; ++i)
      sample.raw[i] = 100 + group * 5 + i;
    recorder.addHall(1000000 + group * 1000, 1000 + group, sample, context);
  }
  HallWire hall;
  assert(recorder.popHall(hall));
  assert(hall.header.recType == REC_HALL &&
         hall.header.nItems == HALL_BATCH_SAMPLES &&
         hall.header.firstItemSeq == 0);
  for (unsigned group = 0; group < HALL_BATCH_SAMPLES; ++group) {
    assert(hall.samples[group].median == INT16_MIN);
    for (unsigned i = 0; i < 5; ++i)
      assert(hall.samples[group].raw[i] == 100 + group * 5 + i);
  }
  assert(hall.header.crc32 == recordCrc(hall.header,
    reinterpret_cast<const uint8_t*>(hall.samples),
    HALL_BATCH_SAMPLES * sizeof(HallSample)));

  for (unsigned i = 0; i < NATIVE_HALL_BATCH_ITEMS; ++i) {
    NativeHallItem item{};
    item.serial = i + 1;
    item.tUs = 1000000 + i * 180;
    item.raw = 100 + i;
    item.pwm = 40;
    item.direction = 1;
    recorder.addNativeHall(item, context);
  }
  NativeHallWire native;
  assert(recorder.popNativeHall(native));
  assert(native.header.recType == REC_HALL_NATIVE &&
         native.header.firstItemSeq == 1 &&
         native.header.nItems == NATIVE_HALL_BATCH_ITEMS);
  for (unsigned i = 0; i < NATIVE_HALL_BATCH_ITEMS; ++i)
    assert(native.items[i].serial == i + 1 &&
           native.items[i].tUs == 1000000 + i * 180 &&
           native.items[i].raw == static_cast<int16_t>(100 + i));
  assert(native.header.crc32 == recordCrc(native.header,
    reinterpret_cast<const uint8_t*>(native.items),
    NATIVE_HALL_BATCH_ITEMS * sizeof(NativeHallItem)));

  ir_movement::WireSnapshot ir;
  ir.bootId = 99;
  ir.sequence = 8;
  ir.completedPulses = 123;
  ir.nominalUm = ir.completedPulses * ir.pitchUm;
  const uint8_t mac[6] = {2, 3, 4, 5, 6, 7};
  recorder.addIr(2000000, mac, 1, ir, 0, 0, 0, 0, context);
  IrWire irRecord;
  assert(recorder.popIr(irRecord));
  assert(irRecord.snapshot.wire.completedPulses == 123 &&
         irRecord.snapshot.sourceMac[0] == 2 &&
         irRecord.header.crc32 == recordCrc(irRecord.header,
           reinterpret_cast<const uint8_t*>(&irRecord.snapshot),
           sizeof(irRecord.snapshot)));

  NaviSnapshot decision{};
  decision.tUs = 2000100;
  decision.kind = static_cast<uint8_t>(navi_eyes::EwoEventKind::PwmZeroDisplacement);
  decision.mm = 40;
  decision.target = 41;
  decision.irUm = ir.nominalUm;
  decision.positionReliable = 0;
  decision.hallQueueDrops = 3;
  recorder.addNavi(decision, context);
  NaviWire nav;
  assert(recorder.popNavi(nav));
  assert(nav.header.recType == REC_NAVI &&
         nav.snapshot.kind == decision.kind && nav.snapshot.irUm == ir.nominalUm &&
         nav.snapshot.hallQueueDrops == 3 &&
         nav.header.crc32 == recordCrc(nav.header,
           reinterpret_cast<const uint8_t*>(&nav.snapshot),
           sizeof(nav.snapshot)));

  for (unsigned i = 0; i < NAVI_RING_RECORDS + 1; ++i)
    recorder.addNavi(decision, context);
  assert(recorder.naviDrops() == 1);
  std::puts("PASS: EWO NSR1 observation and decision records");
}

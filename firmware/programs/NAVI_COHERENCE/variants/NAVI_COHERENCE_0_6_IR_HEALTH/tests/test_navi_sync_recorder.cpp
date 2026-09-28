#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../NaviSyncRecorder.h"

using namespace navi_sync;

static HallSample sample(uint16_t value, uint8_t flags = 0) {
  HallSample s{};
  for (unsigned i = 0; i < 5; ++i) s.raw[i] = (uint16_t)(value + i);
  s.median = (int16_t)(value + 2);
  s.pwmActual = 90; s.pwmCommanded = 90; s.flags = flags;
  return s;
}

int main() {
  Recorder r;
  r.begin(9950012, 0x12345678, 0x1122334455667788ULL);
  Context c; c.navMm = 41; c.navDir = 1; c.flags = CTX_NAV_KNOWN;

  for (unsigned i = 0; i < HALL_BATCH_SAMPLES; ++i)
    r.addHall(1000000ULL + i * 1000ULL, 1000 + i,
              sample((uint16_t)(500 + i)), c);

  HallWire hall{};
  assert(r.popHall(hall));
  assert(hall.header.recType == REC_HALL);
  assert(hall.header.nItems == HALL_BATCH_SAMPLES);
  assert(hall.header.firstItemSeq == 0);
  assert(hall.header.t0Us == 1000000ULL);
  assert(hall.header.locoId == 9950012);
  assert(hall.samples[0].dtUs == 0);
  assert(hall.samples[1].dtUs == 1000);
  assert(hall.samples[1].median == 503);
  assert(hall.samples[1].raw[0] == 501);
  assert(hall.header.crc32 == recordCrc(
    hall.header, reinterpret_cast<const uint8_t*>(hall.samples),
    HALL_BATCH_SAMPLES * sizeof(HallSample)));
  assert(r.maxHallGapUs() == 1000);

  ir_movement::WireSnapshot wire{};
  wire.magic = 0x4952; wire.version = 1; wire.type = 5;
  wire.sequence = 7; wire.bootId = 0xABC; wire.capturedUs = 2000000;
  wire.completedPulses = 10; wire.observedRises = 10; wire.pitchUm = 9652;
  wire.nominalUm = wire.completedPulses * wire.pitchUm;
  uint8_t mac[6] = {0x38,0x18,0x2B,0x30,0x8C,0x2C};
  r.addIr(2000123, mac, 1, wire, 2, 3, 90, 90, c);
  IrWire ir{};
  assert(r.popIr(ir));
  assert(ir.header.recType == REC_IR && ir.header.nItems == 1);
  assert(ir.header.firstItemSeq == 7 && ir.snapshot.rxUs == 2000123);
  assert(ir.snapshot.sourceMac[0] == 0x38 && ir.snapshot.wire.bootId == 0xABC);
  assert(ir.header.crc32 == recordCrc(
    ir.header, reinterpret_cast<const uint8_t*>(&ir.snapshot), sizeof(ir.snapshot)));

  Status status{}; status.hallSamples = r.hallSamples(); status.irAccepted = r.irAccepted();
  StatusWire sw = r.makeStatus(3000, 3000000, c, status);
  assert(sw.header.recType == REC_STATUS && sw.header.firstItemSeq == SEQ_NA);
  assert(sw.header.crc32 == recordCrc(
    sw.header, reinterpret_cast<const uint8_t*>(&sw.status), sizeof(sw.status)));

  // A full ring is allowed to lose whole records, but the loss is counted.
  for (unsigned batch = 0; batch < HALL_RING_BATCHES + 2; ++batch)
    for (unsigned i = 0; i < HALL_BATCH_SAMPLES; ++i)
      r.addHall(4000000ULL + (batch * HALL_BATCH_SAMPLES + i) * 1000ULL,
                4000, sample(700), c);
  assert(r.hallDrops() > 0);
  puts("NAVI NSR1 recorder checks passed");
  return 0;
}

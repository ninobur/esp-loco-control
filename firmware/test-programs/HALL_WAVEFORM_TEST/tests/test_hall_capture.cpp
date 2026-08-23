// ============================================================================
// test_hall_capture.cpp — host tests for the HALL_WAVEFORM_TEST capture engine
//
// INVESTIGATORY / UNAPPROVED.
//
// Build and run:  ./run_tests.sh
//
// These run the REAL engine (HallCapture.h, the same header the sketch
// compiles) on the host. What they check is what the instrument claims:
// samples come out in order, nothing is invented, losses are declared, and
// the ring is bounded.
// ============================================================================
#include "../HallCapture.h"

#include <stdio.h>
#include <vector>
#include <string>

static int failures = 0;
static int checks   = 0;

static void ck(bool cond, const char* what) {
  checks++;
  if (!cond) { failures++; printf("  FAIL  %s\n", what); }
}

static void ckEq(long long got, long long want, const char* what) {
  checks++;
  if (got != want) {
    failures++;
    printf("  FAIL  %s (got %lld, want %lld)\n", what, got, want);
  }
}

// A capture instance is ~63 kB; keep it off the stack.
static HwtCapture cap;

// Feed n samples on a perfect 1 kHz grid starting at tUs.
static uint64_t feedGrid(uint32_t n, uint64_t tUs, uint16_t rawStart = 100) {
  for (uint32_t i = 0; i < n; i++) {
    cap.addSample(hwtPackChannel((uint16_t)(rawStart + i), HWT_ANN_NONE, true),
                  hwtPackChannel(0, HWT_ANN_NONE, false),
                  tUs, 0, 0, HWT_DIR_FORWARD);
    tUs += 1000;
  }
  return tUs;
}

// Drain everything the ring holds into a vector of batches.
static std::vector<HwtBatch> drain() {
  std::vector<HwtBatch> out;
  HwtBatch b;
  while (cap.popBatch(&b)) out.push_back(b);
  return out;
}

// ---------------------------------------------------------------------------
// 1. Sequence ordering — every sample, once, in order, across batch borders.
// ---------------------------------------------------------------------------
static void testSequenceOrdering() {
  printf("sequence ordering\n");
  cap.begin(9950011, 0xABCD, 1000);
  feedGrid(HWT_BATCH_SAMPLES * 4, 1000000);

  std::vector<HwtBatch> bs = drain();
  ckEq((long long)bs.size(), 4, "four full batches emitted");

  uint32_t expectSample = 0, expectBatch = 1;
  uint16_t expectRaw = 100;
  for (size_t i = 0; i < bs.size(); i++) {
    ckEq(bs[i].hdr.batchSeq, expectBatch++, "batchSeq is consecutive");
    ckEq(bs[i].hdr.firstSampleSeq, expectSample, "firstSampleSeq continues");
    ckEq(bs[i].hdr.nSamples, HWT_BATCH_SAMPLES, "batch is full");
    ckEq(bs[i].hdr.missedBefore, 0, "no missed slots on a clean grid");
    for (uint16_t k = 0; k < bs[i].hdr.nSamples; k++) {
      ck((bs[i].s[k].ch0 & HWT_CH_RAW_MASK) == (expectRaw & HWT_CH_RAW_MASK),
         "sample values are in acquisition order");
      expectRaw++;
    }
    expectSample += bs[i].hdr.nSamples;
  }
  ckEq(cap.sampleSeq(), HWT_BATCH_SAMPLES * 4, "sample counter matches feed");
  ckEq(cap.cumMissed(), 0, "nothing declared missed");
  ckEq(cap.cumQueueDrops(), 0, "nothing dropped");
}

// ---------------------------------------------------------------------------
// 2. Raw fidelity — the recorded value IS the reading. No averaging anywhere.
// ---------------------------------------------------------------------------
static void testRawFidelityAndAnnotationIsPassive() {
  printf("raw fidelity, annotation is passive\n");
  cap.begin(9950011, 1, 1000);
  // A deliberately spiky sequence: an averaging pipeline would smear it.
  const uint16_t vals[] = {1900, 4095, 0, 1901, 4000, 5, 1902};
  uint64_t t = 500000;
  for (size_t i = 0; i < sizeof(vals) / sizeof(vals[0]); i++) {
    // Annotate half of them as NORTH: the annotation must not alter the raw.
    cap.addSample(hwtPackChannel(vals[i], (i % 2) ? HWT_ANN_NORTH : HWT_ANN_NONE, true),
                  hwtPackChannel(0, HWT_ANN_NONE, false),
                  t, 42, 90, HWT_DIR_REVERSE);
    t += 1000;
  }
  cap.flushPartial();
  std::vector<HwtBatch> bs = drain();
  ckEq((long long)bs.size(), 1, "partial batch flushed on demand");
  ckEq(bs[0].hdr.nSamples, 7, "all seven samples present");
  for (uint16_t k = 0; k < 7; k++) {
    ckEq(bs[0].s[k].ch0 & HWT_CH_RAW_MASK, vals[k], "raw ADC stored verbatim");
    ckEq(bs[0].s[k].pwmActual, 42, "actual PWM rides with the sample");
    ckEq(bs[0].s[k].pwmCommanded, 90, "commanded PWM rides with the sample");
    ckEq(bs[0].s[k].ctx & HWT_CTX_DIR_MASK, HWT_DIR_REVERSE, "direction rides along");
  }
  ckEq((bs[0].s[1].ch0 >> HWT_CH_ANN_SHIFT) & 0x03, HWT_ANN_NORTH, "annotation is carried");
  ckEq(bs[0].s[1].ch0 & HWT_CH_RAW_MASK, 4095, "annotated sample keeps its raw value");
}

// ---------------------------------------------------------------------------
// 3. Missed slots are declared, never fabricated.
// ---------------------------------------------------------------------------
static void testMissedSlotsAreNotFabricated() {
  printf("missed slots declared, not fabricated\n");
  cap.begin(9950011, 2, 1000);
  uint64_t t = 100000;
  t = feedGrid(10, t);
  t += 5000;                 // a 6 ms gap: five slots never acquired
  feedGrid(10, t);
  cap.flushPartial();

  std::vector<HwtBatch> bs = drain();
  ckEq((long long)bs.size(), 2, "the gap closes the open batch short");
  ckEq(bs[0].hdr.nSamples, 10, "pre-gap batch holds exactly what was acquired");
  ckEq(bs[1].hdr.missedBefore, 5, "five slots declared missing");
  ckEq(bs[1].hdr.firstSampleSeq, 15, "sequence numbers of missing slots are skipped");
  ckEq(cap.cumMissed(), 5, "cumulative missed count is exposed");
  ckEq(bs[0].hdr.nSamples + bs[1].hdr.nSamples, 20, "no filler samples invented");
  ck(cap.maxGapUs() >= 6000, "worst acquisition gap is measured");
}

// ---------------------------------------------------------------------------
// 4. Queue overflow — bounded, drops the oldest, and says so.
// ---------------------------------------------------------------------------
static void testQueueOverflow() {
  printf("queue overflow is bounded and reported\n");
  cap.begin(9950011, 3, 1000);
  const uint32_t batchesToMake = HWT_QUEUE_BATCHES + 7;   // nothing drained
  feedGrid(HWT_BATCH_SAMPLES * batchesToMake, 0);

  ckEq(cap.queued(), HWT_QUEUE_BATCHES, "ring never exceeds its bound");
  ckEq(cap.cumQueueDrops(), 7, "the overflow is counted");

  std::vector<HwtBatch> bs = drain();
  ckEq((long long)bs.size(), HWT_QUEUE_BATCHES, "only the bound survives");
  // Oldest dropped: the first surviving batch is #8, and the run is contiguous.
  ckEq(bs[0].hdr.batchSeq, 8, "the OLDEST batches were the ones dropped");
  for (size_t i = 1; i < bs.size(); i++)
    ckEq(bs[i].hdr.batchSeq, bs[i - 1].hdr.batchSeq + 1, "survivors stay consecutive");
  ck(bs[0].hdr.cumQueueDrops <= bs.back().hdr.cumQueueDrops,
     "every header carries the running drop count");
  // A batch is sealed before it is pushed, so the drop it causes is reported
  // by the NEXT header and by the 2 s status record — never lost, but one
  // batch late. Asserted here so the lag stays a documented property.
  ckEq(bs.back().hdr.cumQueueDrops, 6, "the last drop is reported one batch later");
  ckEq(cap.cumQueueDrops(), 7, "the engine's own counter is exact");
}

// ---------------------------------------------------------------------------
// 5. Transport loss — a batch lost on the wire is detectable from the headers
//    alone, and a corrupted one fails its CRC.
// ---------------------------------------------------------------------------
static void testTransportLossAndCorruption() {
  printf("transport loss and corruption are detectable\n");
  cap.begin(9950011, 4, 1000);
  feedGrid(HWT_BATCH_SAMPLES * 5, 0);
  std::vector<HwtBatch> bs = drain();

  // Drop batch index 2 "in the network".
  std::vector<HwtBatch> wire;
  for (size_t i = 0; i < bs.size(); i++) if (i != 2) wire.push_back(bs[i]);

  bool sawGap = false;
  uint32_t missingSamples = 0;
  for (size_t i = 1; i < wire.size(); i++) {
    uint32_t step = wire[i].hdr.batchSeq - wire[i - 1].hdr.batchSeq;
    if (step != 1) {
      sawGap = true;
      missingSamples += wire[i].hdr.firstSampleSeq
                      - (wire[i - 1].hdr.firstSampleSeq + wire[i - 1].hdr.nSamples);
    }
  }
  ck(sawGap, "a wire loss shows as a batchSeq gap");
  ckEq(missingSamples, HWT_BATCH_SAMPLES, "the exact lost sample range is recoverable");

  // CRC check: seal, verify, corrupt one byte, verify again.
  HwtBatch b = bs[0];
  uint32_t stored = b.hdr.crc32;
  b.hdr.crc32 = 0;
  uint32_t recomputed = hwtCrc32((const uint8_t*)&b.hdr, sizeof(b.hdr), 0);
  recomputed = hwtCrc32((const uint8_t*)b.s,
                        (uint32_t)b.hdr.nSamples * sizeof(HwtSample), recomputed);
  ckEq(recomputed, stored, "CRC of an intact batch matches");

  b.s[3].ch0 ^= 0x0001;
  b.hdr.crc32 = 0;
  uint32_t bad = hwtCrc32((const uint8_t*)&b.hdr, sizeof(b.hdr), 0);
  bad = hwtCrc32((const uint8_t*)b.s,
                 (uint32_t)b.hdr.nSamples * sizeof(HwtSample), bad);
  ck(bad != stored, "a single flipped bit fails the CRC");
}

// ---------------------------------------------------------------------------
// 6. Timestamp and counter rollover.
// ---------------------------------------------------------------------------
static void testRollover() {
  printf("timestamp and counter rollover\n");

  // millis()-domain wrap: the helper every deadline in the sketch uses.
  ckEq(hwtElapsedMs(5, 0xFFFFFFF0u), 21, "ms elapsed is correct across the wrap");
  ckEq(hwtElapsedMs(0, 0xFFFFFFFFu), 1, "ms elapsed handles the wrap boundary");
  ckEq(hwtElapsedMs(1000, 400), 600, "ms elapsed is ordinary away from the wrap");

  // The 64-bit microsecond source does not wrap in any practical session, but
  // a large timestamp must still produce sane dt and no bogus missed count.
  cap.begin(9950011, 5, 1000);
  const uint64_t big = 0xFFFFFFFFull + 1000000ull;   // past the 32-bit horizon
  feedGrid(3, big);
  cap.flushPartial();
  std::vector<HwtBatch> bs = drain();
  ckEq(bs[0].hdr.t0Us, big, "64-bit microsecond stamp survives intact");
  ckEq(bs[0].s[1].dtUs, 1000, "dt is correct above the 32-bit boundary");
  ckEq(cap.cumMissed(), 0, "no phantom missed slots at large timestamps");

  // dtUs saturates rather than wrapping into a small, plausible lie.
  cap.begin(9950011, 6, 1000);
  cap.addSample(hwtPackChannel(1, 0, true), hwtPackChannel(0, 0, false), 0, 0, 0, 0);
  cap.addSample(hwtPackChannel(2, 0, true), hwtPackChannel(0, 0, false),
                200000, 0, 0, 0);   // 200 ms stall
  cap.flushPartial();
  bs = drain();
  ckEq(bs.back().s[bs.back().hdr.nSamples - 1].dtUs, 0xFFFF,
       "dtUs saturates instead of wrapping");
  ckEq(cap.cumMissed(), 199, "the stall is declared as 199 missing slots");
}

// ---------------------------------------------------------------------------
// 7. Dual-channel alignment — the format's promise, checked in both builds.
// ---------------------------------------------------------------------------
static void testChannelAlignment() {
  printf("channel alignment and presence flag\n");
  cap.begin(9950011, 7, 1000);
  uint64_t t = 0;
  for (uint16_t i = 0; i < 20; i++) {
    // Two channels acquired in one slot: B is A's mirror about 2048.
    uint16_t a = (uint16_t)(1000 + i);
    uint16_t b = (uint16_t)(4095 - a);
    cap.addSample(hwtPackChannel(a, HWT_ANN_NONE, true),
                  hwtPackChannel(b, HWT_ANN_SOUTH, true), t, 0, 0, 0);
    t += 1000;
  }
  cap.flushPartial();
  std::vector<HwtBatch> bs = drain();
  for (uint16_t k = 0; k < bs[0].hdr.nSamples; k++) {
    uint16_t a = bs[0].s[k].ch0 & HWT_CH_RAW_MASK;
    uint16_t b = bs[0].s[k].ch1 & HWT_CH_RAW_MASK;
    ckEq(a + b, 4095, "both channels come from the same slot, in step");
    ck((bs[0].s[k].ch0 & HWT_CH_PRESENT) != 0, "channel A marked present");
    ck((bs[0].s[k].ch1 & HWT_CH_PRESENT) != 0, "channel B marked present");
  }

  // Single-sensor build (this instrument's default): B is marked ABSENT, and
  // its slot must never be mistaken for a reading of zero flux.
  cap.begin(9950011, 8, 1000);
  cap.addSample(hwtPackChannel(1234, HWT_ANN_NONE, true),
                hwtPackChannel(0, HWT_ANN_NONE, false), 0, 0, 0, 0);
  cap.flushPartial();
  bs = drain();
  ck((bs[0].s[0].ch1 & HWT_CH_PRESENT) == 0, "absent channel is flagged absent");
  ckEq(bs[0].s[0].ch0 & HWT_CH_RAW_MASK, 1234, "the present channel is unaffected");
}

// ---------------------------------------------------------------------------
// 8. Wire-format stability. The decoder parses these sizes by hand.
// ---------------------------------------------------------------------------
static void testWireFormat() {
  printf("wire format sizes\n");
  ckEq((long long)sizeof(HwtSample), 10, "sample record is 10 bytes");
  ckEq((long long)sizeof(HwtBatchHeader), 52, "batch header is 52 bytes");
  ckEq((long long)(sizeof(HwtBatchHeader) + HWT_BATCH_SAMPLES * sizeof(HwtSample)),
       1302, "a full batch fits one unfragmented UDP datagram");
  // zlib.crc32(b"123456789") == 0xCBF43926 — the decoder relies on this match.
  ckEq(hwtCrc32((const uint8_t*)"123456789", 9, 0), 0xCBF43926u,
       "CRC-32 matches Python zlib.crc32");
}

int main() {
  printf("HALL_WAVEFORM_TEST — capture engine tests (investigatory)\n\n");
  testSequenceOrdering();
  testRawFidelityAndAnnotationIsPassive();
  testMissedSlotsAreNotFabricated();
  testQueueOverflow();
  testTransportLossAndCorruption();
  testRollover();
  testChannelAlignment();
  testWireFormat();
  printf("\n%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}

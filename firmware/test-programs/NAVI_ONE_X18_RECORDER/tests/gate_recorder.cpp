// ============================================================================
// gate 14 — the continuous Hall recorder's ring, batching and wire format.
//
// This gate compiles the SHIPPED HallRecorder.h and drives it exactly as
// hallTask does, then writes the datagrams it produced into a real capture
// file. tools/tests/test_xhr_decoder.py reads that file with the real
// tools/xhr_format.py. Neither side is checked against a description of the
// other: the firmware emits bytes and the decoder consumes them, so a layout
// drift, a CRC-polynomial disagreement or an off-by-one in the sequence
// arithmetic fails a test instead of silently corrupting a field capture.
//
// Run standalone:   ./gate_recorder [out.xhr]
// ============================================================================
#include "../HallRecorder.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>

static int failures = 0, checks = 0;
static void ok(bool cond, const char* what) {
  ++checks;
  if (!cond) { ++failures; printf("  FAIL  %s\n", what); }
}
template <typename A, typename B>
static void eq(A got, B want, const char* what) {
  ++checks;
  if (!((long long)got == (long long)want)) {
    ++failures;
    printf("  FAIL  %s: got %lld, want %lld\n", what, (long long)got, (long long)want);
  }
}

// ---------------------------------------------------------------------------
// The capture container the Pi receiver writes, so this gate's output is
// indistinguishable from a real capture. XHRCAP01 + per-frame (recv_us, len).
// ---------------------------------------------------------------------------
struct Capture {
  FILE* fh = nullptr;
  uint64_t clock = 1700000000000000ULL;
  void open(const char* path) {
    fh = fopen(path, "wb");
    if (!fh) { printf("  FAIL  cannot write %s\n", path); ++failures; return; }
    const char magic[8] = {'X','H','R','C','A','P','0','1'};
    uint64_t started = clock, reserved = 0;
    fwrite(magic, 1, 8, fh);
    fwrite(&started, 8, 1, fh);
    fwrite(&reserved, 8, 1, fh);
  }
  void frame(const void* data, uint16_t len) {
    if (!fh) return;
    clock += 100000;                       // 100 ms apart, like the real stream
    fwrite(&clock, 8, 1, fh);
    fwrite(&len, 2, 1, fh);
    fwrite(data, 1, len, fh);
  }
  void close() { if (fh) { fclose(fh); fh = nullptr; } }
};

int main(int argc, char** argv) {
  const char* out = argc > 1 ? argv[1] : "/tmp/gate_recorder.xhr";
  printf("gate 14: continuous Hall recorder — ring, batching, wire format\n");

  // -- 1. wire layout ------------------------------------------------------
  // The static_asserts in the header already stop the build on a mismatch;
  // these restate the numbers so a reader of the output knows what the
  // decoder is being held to.
  eq(sizeof(XhrSample), 8, "XhrSample wire size");
  eq(sizeof(XhrHeader), 44, "XhrHeader wire size");
  eq(sizeof(XhrRulingRec), 44, "XhrRulingRec wire size");
  eq(sizeof(XhrStatusRec), 52, "XhrStatusRec wire size");
  eq(sizeof(XhrHeader) + XHR_SAMPLE_BATCH * sizeof(XhrSample), 844,
     "one sample datagram");
  ok(sizeof(XhrHeader) + XHR_SAMPLE_BATCH * sizeof(XhrSample) <= 1472,
     "a sample datagram fits one unfragmented UDP packet");

  // -- 2. a clean 1 kHz run ------------------------------------------------
  // 12,000 ticks = 12 s of trace = 120 batches, drained as it goes exactly as
  // networkTask drains it.
  Capture cap;
  cap.open(out);
  XhrSampleRing ring;
  ring.begin(9950011u, 0xA1B2C3D4u);
  eq(ring.lastCompletedSeq(), (uint32_t)XHR_SAMPLE_SEQ_NA,
     "no completed sample before the first tick");

  static XhrSampleBatch pop;
  uint32_t sent = 0;
  uint32_t us = 5000000u, ms = 5000u;
  for (uint32_t i = 0; i < 12000u; ++i) {
    // A synthetic magnet every 2 s so the trace has structure a decoder can
    // be seen to preserve: 1950 resting, a 170-count north arc over 140 ms.
    int16_t raw = 1950;
    const uint32_t ph = i % 2000u;
    if (ph < 140u) {
      const double x = (double)ph / 140.0 - 0.5;
      raw = (int16_t)(1950.0 + 170.0 * (1.0 - 4.0 * x * x));
    }
    uint8_t f = XHR_F_DIR_FWD | XHR_F_AUTO | XHR_F_MAY_ADAPT;
    if (ph < 140u) f |= XHR_F_PASSAGE;
    ring.addSample(ms, us, raw, 90, 90, f, 1948, (uint8_t)(i / 2000u), +1, 0,
                   XHR_CTX_POS_KNOWN | XHR_CTX_AUTO | XHR_CTX_MQTT_UP);
    us += 1000u; ms += 1u;
    while (ring.popBatch(&pop)) { cap.frame(&pop, (uint16_t)xhrSampleBatchWireLen(&pop)); ++sent; }
  }
  eq(sent, 120, "120 batches produced by 12,000 samples");
  eq(ring.cumSamples(), 12000, "every sample counted");
  eq(ring.cumRingDrops(), 0, "a drained ring drops nothing");
  eq(ring.lastCompletedSeq(), 11999, "last completed sample seq");

  // -- 3. a ruling, sealed as its own datagram -----------------------------
  XhrRulingRing rulings;
  rulings.begin(9950011u, 0xA1B2C3D4u);
  XhrRulingRec r;
  memset(&r, 0, sizeof(r));
  r.tMs = 17000; r.openedAtMs = 16860; r.closedAtMs = 17000;
  r.sampleSeq = ring.lastCompletedSeq();
  r.peakCounts = 173; r.durationMs = 140; r.gapMs = 1860;
  r.entryBaseline = 1948; r.closeBaseline = 1948; r.shadowBaseline = 1951;
  r.amplitudeRatio = 1.02f;
  r.polarity = 1; r.ruling = XHR_RULING_ADVANCED; r.outcome = 0; r.isMagnet = 1;
  r.navMmBefore = 41; r.navMmAfter = 42; r.navDir = 1; r.stPhase = 0;
  r.pwmActual = 90; r.postStopSuccessor = 0;
  rulings.push(r);
  uint8_t small[sizeof(XhrHeader) + 64];
  uint32_t n = rulings.popSealed(small, sizeof(small));
  eq(n, sizeof(XhrHeader) + sizeof(XhrRulingRec), "a ruling datagram's length");
  cap.frame(small, (uint16_t)n);
  eq(rulings.popSealed(small, sizeof(small)), 0, "an empty ruling ring yields nothing");

  // -- 4. status -----------------------------------------------------------
  XhrStatusRec st;
  memset(&st, 0, sizeof(st));
  st.tMs = 17000; st.uptimeMs = 17000; st.cumSamples = ring.cumSamples();
  st.maxGapUs = 1000; st.maxTickBodyUs = 260; st.freeHeap = 180000;
  st.hallStackFreeBytes = 2100; st.netStackFreeBytes = 5200;
  st.measuredHzX10 = 10000; st.ringHighWater = 1; st.rssi = -58;
  st.mqttConnected = 1; st.wifiConnected = 1;
  n = xhrSealStatus(small, sizeof(small), 9950011u, 0xA1B2C3D4u, 1, st,
                    1948, 42, +1, 0,
                    XHR_CTX_POS_KNOWN | XHR_CTX_AUTO | XHR_CTX_MQTT_UP, 0);
  eq(n, sizeof(XhrHeader) + sizeof(XhrStatusRec), "a status datagram's length");
  cap.frame(small, (uint16_t)n);

  // -- 5. ring overflow drops the OLDEST, and counts it --------------------
  // The radio is gone: nothing is drained for 6 s against a 4 s ring.
  XhrSampleRing starved;
  starved.begin(9950011u, 0xA1B2C3D4u);
  uint32_t sus = 0, sms = 0;
  for (uint32_t i = 0; i < 6000u; ++i) {
    starved.addSample(sms, sus, 1950, 0, 0, 0, 1948, XHR_MM_NA, 0, 0, 0);
    sus += 1000u; sms += 1u;
  }
  eq(starved.cumRingDrops(), 60u - XHR_SAMPLE_RING_BATCHES,
     "6 s into a 4 s ring drops exactly the overflow");
  // What survives must be the NEWEST, because when the radio is gone the
  // interesting evidence is what is happening now.
  static XhrSampleBatch first;
  ok(starved.popBatch(&first), "the starved ring still has batches");
  eq(first.hdr.firstSampleSeq, (60u - XHR_SAMPLE_RING_BATCHES) * XHR_SAMPLE_BATCH,
     "the oldest surviving batch is the one after the drops");
  ok(first.hdr.ringDropsLo > 0, "the surviving batch carries the drop count");

  // -- 6. dt saturation and micros() wrap ----------------------------------
  XhrSampleRing gappy;
  gappy.begin(9950011u, 0x5E5510Du);
  gappy.addSample(0, 0xFFFFF000u, 1950, 0, 0, 0, 1948, XHR_MM_NA, 0, 0, 0);
  // 8 ms later, ACROSS the 32-bit micros() wrap. Unsigned subtraction must
  // give 8000, not four billion.
  gappy.addSample(8, 0x00000F40u, 1951, 0, 0, 0, 1948, XHR_MM_NA, 0, 0, 0);
  // then a 200 ms stall, which saturates
  gappy.addSample(208, 0x00032D40u, 1952, 0, 0, 0, 1948, XHR_MM_NA, 0, 0, 0);
  for (uint32_t i = 3; i < XHR_SAMPLE_BATCH; ++i)
    gappy.addSample(208 + i, 0x00032D40u + (i - 2) * 1000u, 1950, 0, 0, 0,
                    1948, XHR_MM_NA, 0, 0, 0);
  static XhrSampleBatch gb;
  ok(gappy.popBatch(&gb), "the gappy batch completed");
  eq(gb.s[0].dtUs, 0, "the very first sample has no predecessor, so dt 0");
  eq(gb.s[1].dtUs, 8000, "8 ms measured correctly across the micros() wrap");
  eq(gb.s[2].dtUs, 65535, "a 200 ms stall saturates rather than wrapping");
  ok((gb.s[2].flags & XHR_F_LATE) != 0, "and is flagged late");
  ok((gb.s[1].flags & XHR_F_LATE) != 0, "an 8 ms gap is flagged late too");
  ok((gb.s[3].flags & XHR_F_LATE) == 0, "an on-time sample is not flagged late");
  eq(gappy.maxGapUs(), 65535, "maxGapUs holds the worst gap");
  cap.frame(&gb, (uint16_t)xhrSampleBatchWireLen(&gb));

  // -- 7. a SECOND SESSION, appended to the same file ----------------------
  // The locomotive rebooted. Sequence numbers and clocks restart at zero, and
  // nothing downstream may join the two.
  XhrSampleRing reboot;
  reboot.begin(9950011u, 0x0B007EDu);
  uint32_t rus = 0, rms = 0;
  for (uint32_t i = 0; i < 300u; ++i) {
    reboot.addSample(rms, rus, (int16_t)(1900 + (int)(i % 7)), 0, 0, 0,
                     1902, XHR_MM_NA, 0, 0, 0);
    rus += 1000u; rms += 1u;
    while (reboot.popBatch(&pop)) cap.frame(&pop, (uint16_t)xhrSampleBatchWireLen(&pop));
  }

  // -- 8. CRC actually covers the payload ----------------------------------
  // Flip one bit in a sealed batch and the seal must fail. (The decoder's own
  // CRC check is exercised from Python; this proves the firmware's seal is
  // not vacuous.)
  static XhrSampleBatch tamper;
  XhrSampleRing one;
  one.begin(1u, 2u);
  for (uint32_t i = 0; i < XHR_SAMPLE_BATCH; ++i)
    one.addSample(i, i * 1000u, 1950, 0, 0, 0, 1948, XHR_MM_NA, 0, 0, 0);
  ok(one.popBatch(&tamper), "a sealed batch to tamper with");
  const uint32_t stored = tamper.hdr.crc32;
  tamper.hdr.crc32 = 0;
  const uint32_t recomputed0 = xhrCrc32((const uint8_t*)&tamper.hdr, sizeof(XhrHeader), 0);
  const uint32_t recomputed = xhrCrc32((const uint8_t*)tamper.s,
                                       XHR_SAMPLE_BATCH * sizeof(XhrSample), recomputed0);
  eq(recomputed, stored, "the seal recomputes over header+payload");
  tamper.s[50].raw ^= 1;
  const uint32_t after0 = xhrCrc32((const uint8_t*)&tamper.hdr, sizeof(XhrHeader), 0);
  const uint32_t after = xhrCrc32((const uint8_t*)tamper.s,
                                  XHR_SAMPLE_BATCH * sizeof(XhrSample), after0);
  ok(after != stored, "one flipped bit in the payload breaks the seal");

  cap.close();
  printf("  wrote %s\n", out);
  printf("%s  %d checks, %d failures\n", failures ? "FAILED" : "PASSED", checks, failures);
  return failures ? 1 : 0;
}

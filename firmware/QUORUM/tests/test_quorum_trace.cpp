// ============================================================================
// test_quorum_trace.cpp — host tests for QuorumTrace.h's ring/batch engine.
//
// INVESTIGATORY / UNAPPROVED.
//
// Build and run:  ./run_tests.sh
//
// These run the REAL engine (QuorumTrace.h, the same header QUORUM.ino
// compiles when QUORUM_TRACE is defined) on the host with g++. QUORUM_TRACE
// is defined here (before the #include) exactly so this header's content
// exists to test -- QUORUM.ino itself defines it via a build flag instead.
// ============================================================================
#define QUORUM_TRACE
#include "../QuorumTrace.h"

#include <stdio.h>
#include <vector>

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

// ---------------------------------------------------------------------------
// 1. Wire format sizes and CRC agreement with Python's zlib.crc32.
// ---------------------------------------------------------------------------
static void testWireFormat() {
  printf("wire format sizes\n");
  ckEq((long long)sizeof(QtHeader), 32,
       "batch header is 32 bytes (magic4+ver1+recType1+nItems2+locoId4+"
       "sessionId4+batchSeq4+firstSampleSeq4+t0Ms4+crc32(4)=32)");
  ckEq((long long)sizeof(QtSample), 14, "sample record is 14 bytes");
  ckEq((long long)sizeof(QtDecision), 40, "decision record is 40 bytes");
  ck((long long)(sizeof(QtHeader) + QT_SAMPLE_BATCH * sizeof(QtSample)) <= 1472,
     "a full sample batch fits one unfragmented UDP datagram");
  ck((long long)(sizeof(QtHeader) + sizeof(QtDecision)) <= 1472,
     "a decision datagram fits one unfragmented UDP datagram");
  ckEq(qtCrc32((const uint8_t*)"123456789", 9, 0), 0xCBF43926u,
       "CRC-32 matches Python zlib.crc32 (same table as HallCapture.h)");
}

// ---------------------------------------------------------------------------
// 2. QtSampleRing — sequence ordering across batch borders.
// ---------------------------------------------------------------------------
static QtSampleRing sampleRing;

static std::vector<QtSampleBatch> drainSamples() {
  std::vector<QtSampleBatch> out;
  QtSampleBatch b;
  while (sampleRing.popBatch(&b)) out.push_back(b);
  return out;
}

static void testSampleSequenceOrdering() {
  printf("sample sequence ordering across batch borders\n");
  sampleRing.begin(9950012, 0xABCD);
  for (uint32_t i = 0; i < QT_SAMPLE_BATCH * 3; i++) {
    sampleRing.addSample(1, (int16_t)(1000 + i), 500, 0, 0, 90, 90, 0);
  }
  std::vector<QtSampleBatch> bs = drainSamples();
  ckEq((long long)bs.size(), 3, "three full batches emitted");
  uint32_t expectSeq = 0, expectBatch = 1;
  int16_t expectRaw = 1000;
  for (size_t i = 0; i < bs.size(); i++) {
    ckEq(bs[i].hdr.batchSeq, expectBatch++, "batchSeq is consecutive");
    ckEq(bs[i].hdr.firstSampleSeq, expectSeq, "firstSampleSeq continues");
    ckEq(bs[i].hdr.nItems, QT_SAMPLE_BATCH, "batch is full");
    ckEq(bs[i].hdr.recType, QT_REC_SAMPLE, "recType is QT_REC_SAMPLE");
    for (uint16_t k = 0; k < bs[i].hdr.nItems; k++) {
      ckEq(bs[i].s[k].raw, expectRaw, "samples are in acquisition order");
      expectRaw++;
    }
    expectSeq += bs[i].hdr.nItems;
  }
  ckEq(sampleRing.sampleSeq(), QT_SAMPLE_BATCH * 3, "sample counter matches feed");
  ckEq(sampleRing.cumRingDrops(), 0, "nothing dropped on an undersized feed");
}

// ---------------------------------------------------------------------------
// 3. QtSampleRing — partial-batch flush, and every field survives verbatim.
// ---------------------------------------------------------------------------
static void testSampleFieldsSurviveVerbatim() {
  printf("sample fields (raw/baseline/peaks/pwm/flags) survive verbatim\n");
  sampleRing.begin(9950012, 1);
  sampleRing.addSample(7, -50, 1900, 120, -30, 42, 45,
                       (uint8_t)(QT_SAMPLE_FLAG_ACTIVE | QT_SAMPLE_FLAG_POLE |
                                 (1 << QT_SAMPLE_DIR_SHIFT) | QT_SAMPLE_FLAG_LATE));
  sampleRing.flushPartial();
  std::vector<QtSampleBatch> bs = drainSamples();
  ckEq((long long)bs.size(), 1, "partial batch flushed on demand");
  ckEq(bs[0].hdr.nItems, 1, "exactly one sample present");
  const QtSample& s = bs[0].s[0];
  ckEq(s.dtUs, 7, "dt survives verbatim");
  ckEq(s.raw, -50, "raw survives verbatim, including negative baseline-relative range");
  ckEq(s.baseline, 1900, "baseline survives verbatim");
  ckEq(s.evPeakN, 120, "north peak survives verbatim");
  ckEq(s.evPeakS, -30, "south peak survives verbatim");
  ckEq(s.pwmActual, 42, "actual PWM survives verbatim");
  ckEq(s.pwmCommanded, 45, "commanded PWM survives verbatim");
  ck((s.flags & QT_SAMPLE_FLAG_ACTIVE) != 0, "active flag set");
  ck((s.flags & QT_SAMPLE_FLAG_POLE) != 0, "pole flag set");
  ckEq((s.flags >> QT_SAMPLE_DIR_SHIFT) & QT_SAMPLE_DIR_MASK, 1, "direction bits carry CW (1)");
  ck((s.flags & QT_SAMPLE_FLAG_ESTOP) == 0, "estop flag clear when not requested");
  ck((s.flags & QT_SAMPLE_FLAG_LATE) != 0, "late flag set");
}

// ---------------------------------------------------------------------------
// 4. QtSampleRing — bounded, drop-oldest, counted overflow (spec requirement
//    5: "overflow is bounded and counted").
// ---------------------------------------------------------------------------
static void testSampleRingOverflow() {
  printf("sample ring overflow is bounded, drops the oldest, and counts it\n");
  sampleRing.begin(9950012, 2);
  const uint32_t batchesToMake = QT_SAMPLE_RING_BATCHES + 5;   // nothing drained meanwhile
  for (uint32_t i = 0; i < QT_SAMPLE_BATCH * batchesToMake; i++)
    sampleRing.addSample(1, 0, 0, 0, 0, 0, 0, 0);

  ckEq(sampleRing.queued(), QT_SAMPLE_RING_BATCHES, "ring never exceeds its bound");
  ckEq(sampleRing.cumRingDrops(), 5, "the overflow is counted");

  std::vector<QtSampleBatch> bs = drainSamples();
  ckEq((long long)bs.size(), QT_SAMPLE_RING_BATCHES, "only the bound survives");
  ckEq(bs[0].hdr.batchSeq, 6, "the OLDEST batches were the ones dropped");
  for (size_t i = 1; i < bs.size(); i++)
    ckEq(bs[i].hdr.batchSeq, bs[i - 1].hdr.batchSeq + 1, "survivors stay consecutive (no wraparound corruption)");
}

// ---------------------------------------------------------------------------
// 5. QtDecisionRing — ordering, and bounded/drop-oldest/counted overflow.
// ---------------------------------------------------------------------------
static QtDecisionRing decisionRing;

static QtDecision makeDecision(uint8_t kind, uint8_t navMmAfter) {
  QtDecision d; memset(&d, 0, sizeof(d));
  d.kind = kind; d.navMmAfter = navMmAfter;
  d.leaderOffset = QT_OFFSET_NA; d.runnerUpOffset = QT_OFFSET_NA;
  return d;
}

static void testDecisionRingOrderingAndOverflow() {
  printf("decision ring ordering and wraparound\n");
  decisionRing.begin(9950012, 3);
  for (uint8_t i = 0; i < 10; i++)
    decisionRing.push(makeDecision(QTD_ACCEPT_EVENT, i));
  for (uint8_t i = 0; i < 10; i++) {
    QtDecision d;
    ck(decisionRing.pop(&d), "ten pushed, ten pop successfully");
    ckEq(d.navMmAfter, i, "decisions pop in push (FIFO) order");
  }
  QtDecision d;
  ck(!decisionRing.pop(&d), "the ring is empty after draining exactly what was pushed");

  printf("decision ring overflow is bounded, drops the oldest, and counts it\n");
  decisionRing.begin(9950012, 4);
  const uint32_t toPush = QT_DECISION_RING + 9;
  for (uint32_t i = 0; i < toPush; i++)
    decisionRing.push(makeDecision(QTD_ACCEPT_EVENT, (uint8_t)(i % 256)));
  ckEq(decisionRing.cumRingDrops(), 9, "nine overflow drops counted");
  ckEq(decisionRing.decisionSeq(), toPush, "decisionSeq counts every push, including dropped ones");
  uint32_t popped = 0;
  uint8_t expect = (uint8_t)(9 % 256);   // the oldest surviving entry
  while (decisionRing.pop(&d)) {
    ckEq(d.navMmAfter, expect, "surviving entries are the newest ones, oldest-first");
    expect = (uint8_t)((expect + 1) % 256);
    popped++;
  }
  ckEq(popped, QT_DECISION_RING, "exactly the ring's bound survived");
}

// Wraparound specifically: push/pop past the ring boundary repeatedly and
// confirm no corruption or misordering at the wrap point (requirement 4).
static void testRingWraparound() {
  printf("ring wraparound (repeated push/pop cycles crossing the boundary)\n");
  decisionRing.begin(9950012, 5);
  uint8_t nextPush = 0, nextPop = 0;
  for (int cycle = 0; cycle < 3; cycle++) {
    for (uint16_t i = 0; i < QT_DECISION_RING - 1; i++)
      decisionRing.push(makeDecision(QTD_ACCEPT_EVENT, nextPush++));
    for (uint16_t i = 0; i < QT_DECISION_RING - 1; i++) {
      QtDecision d;
      ck(decisionRing.pop(&d), "pop succeeds through repeated wraps");
      ckEq(d.navMmAfter, nextPop++, "order survives repeated wraparound");
    }
  }
  ckEq(decisionRing.cumRingDrops(), 0, "no drops when never over-filled");
}

int main() {
  printf("QUORUM TRACE — ring/batch engine tests (investigatory)\n\n");
  testWireFormat();
  testSampleSequenceOrdering();
  testSampleFieldsSurviveVerbatim();
  testSampleRingOverflow();
  testDecisionRingOrderingAndOverflow();
  testRingWraparound();
  printf("\n%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}

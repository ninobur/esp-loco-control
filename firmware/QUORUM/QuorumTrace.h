// ============================================================================
// QuorumTrace.h — passive, synchronized Hall-waveform + navigation-decision
// trace for QUORUM.ino.
//
// INVESTIGATORY / UNAPPROVED. Compiled in only when QUORUM_TRACE is defined
// at build time (default: undefined -- OFF). Every symbol in this file that
// is not the QT_SAMPLE/QT_DECISION/QT_STATUS macro pair lives inside
// #ifdef QUORUM_TRACE, so an OFF build carries none of it -- not as dead
// code, not as an unused buffer, not as a disabled branch. See
// firmware/QUORUM/README_TRACE.md for the full design and its boundaries.
//
// NO NAVIGATION OR MOTOR AUTHORITY. Nothing in this file is ever read by
// QUORUM's control path -- every function here is called FROM the control
// path (a one-way tap), never the reverse, and every value carried here is a
// COPY. Losing every trace record, at any time, changes nothing QUORUM does.
//
// Format identity: magic "QTR1", version 1. This is NOT HALL_WAVEFORM_TEST's
// HWT1 format wearing a new name -- the sample fields, the decision-record
// class, and the transport model (QUORUM already runs the detector; a
// separate task may not touch the radio) are all different. See the format
// doc for the four record classes this file distinguishes: physical
// measurement, detector interpretation, navigation decision, operator
// anchor -- kept apart exactly as CAPTURE_FORMAT.md keeps HWT1's three apart.
//
// Pure C++ (stdint + string.h only) below the Arduino-specific transport
// glue at the bottom of the file, so the ring/batch engine is host-testable
// with g++ exactly like HallCapture.h -- see firmware/QUORUM/tests/.
// ============================================================================
#pragma once

#ifdef QUORUM_TRACE

#include <stdint.h>
#include <string.h>

#ifndef QT_ENTER_CRITICAL
  #define QT_ENTER_CRITICAL() do {} while (0)
  #define QT_EXIT_CRITICAL()  do {} while (0)
#endif

// ---------------------------------------------------------------------------
// Wire constants
// ---------------------------------------------------------------------------
#define QT_MAGIC0 'Q'
#define QT_MAGIC1 'T'
#define QT_MAGIC2 'R'
#define QT_MAGIC3 '1'
#define QT_FORMAT_VERSION 1

enum QtRecType : uint8_t {
  QT_REC_SAMPLE   = 1,   // physical measurement + detector interpretation, batched
  QT_REC_DECISION = 2,   // one navigation decision, unbatched
  QT_REC_ANCHOR   = 3,   // operator anchor. Producer: the trace-only
                         // ngr/loco/<id>/cmd/trace_anchor MQTT command
                         // (Option A of README_TRACE.md's "Anchor
                         // mechanism"), which passes through QUORUM's
                         // existing cmdQueue/handleCommand() path exactly
                         // like cmd/force_lost. Same record shape as
                         // originally reserved -- no version bump needed.
  QT_REC_STATUS   = 4    // periodic health, ring/loss counters, threshold context
};

// What kind of decision QT_REC_DECISION carries. Almost every value maps
// directly onto an existing QUORUM concept -- this file introduces no new
// decision, it taps decisions QUORUM already makes.
enum QtDecisionKind : uint8_t {
  QTD_EVENT_OPENED        = 1,   // detectorSample(): evActive false->true
  QTD_EVENT_FLOOR_REJECT  = 2,   // detectorSample(): dur < EVENT_FLOOR_MS
  QTD_EVENT_CLOSED        = 3,   // detectorSample(): MarkerEvent queued
  QTD_NAV_ON_MARKER_ENTRY = 4,   // navOnMarker(): before the timing gate runs
  QTD_TIMING_GATE_RESULT  = 5,   // navOnMarker(): lastTimingGate just set
  QTD_ACCEPT_EVENT        = 6,   // acceptEvent(): navMm advanced, ring pushed
  QTD_COMPARISON_AGREE    = 7,   // navPublishState("AGREE", ...)
  QTD_COMPARISON_DISAGREE = 8,   // navPublishState("DISAGREE", ...)
  QTD_QUORUM_EVENT        = 9    // publishQuorumDecision(ev, ...) -- ev names
                                 // which one; see QtQuorumEvent below
};

// publishQuorumDecision()'s `ev` string, classified once at the trace hook so
// no call site of publishQuorumDecision() itself needs to change.
enum QtQuorumEvent : uint8_t {
  QTQ_OTHER            = 0,
  QTQ_OPEN             = 1,   // "QUORUM_OPEN"
  QTQ_TIED             = 2,   // "QUORUM_TIED"
  QTQ_ADOPTED          = 3,   // "QUORUM_ADOPTED"
  QTQ_REOPENED         = 4,   // "QUORUM_REOPENED"
  QTQ_CLOSED           = 5,   // "QUORUM_CLOSED"
  QTQ_NO_QUORUM        = 6,   // "NO_QUORUM"
  QTQ_PHANTOM_REJECTED = 7,   // "PHANTOM_REJECTED"
  QTQ_FIXTURE_REJECTED = 8,   // "FIXTURE_REJECTED"
  QTQ_FORCED_OFFSET    = 9    // "FORCED_OFFSET"
};

// navOnMarker()'s lastTimingGate string, classified the same way.
enum QtTimingGate : uint8_t {
  QTG_NO_POSITION = 0, QTG_NO_DIR = 1, QTG_LOW_PWM = 2, QTG_RAMP = 3,
  QTG_NO_PREV = 4, QTG_ACTIVE_PHANTOM = 5, QTG_ACTIVE_ACCEPTED = 6
};

#define QT_POLARITY_NA 0xFF   // "not applicable" sentinel for the polarity fields
#define QT_OFFSET_NA   (-128) // "no leader/runner-up" sentinel (int8_t)
#define QT_SAMPLE_SEQ_NA 0xFFFFFFFFu   // "no Hall sample has completed yet this session"

// ---------------------------------------------------------------------------
// QT_REC_SAMPLE — one per hallTask tick (up to ~1 kHz). 16 bytes.
//
// Deliberately does NOT carry northEnter/northExit/southEnter/southExit:
// under this firmware they are always baseline +/- a compile-time constant
// (recomputeThresholds()), so QtStatusRecord carries HALL_DEADBAND_COUNTS and
// HALL_ENTRY_MARGIN_COUNTS once and a decoder reconstructs every threshold
// from baseline exactly, not approximately. That is the "compactly" in the
// spec, not a missing field.
// ---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
  uint16_t dtUs;         // ms since the previous SAMPLE, saturating (see note)
  int16_t  raw;          // QUORUM's own averaged Hall value (readAveragedADC())
  int16_t  baseline;     // baselineCounts at this tick
  int16_t  evPeakN;      // running north peak of the OPEN event, 0 if none
  int16_t  evPeakS;      // running south peak of the OPEN event, 0 if none
  uint8_t  pwmActual;
  uint8_t  pwmCommanded;
  // bit0 evActive, bit1 evOpenPole, bits2-3 direction (0 REV,1 NEU,2 FWD,3
  // UNSET), bit4 estop, bit5 late (gap > 1.25x nominal tick)
  uint8_t  flags;
  uint8_t  pad;          // zero
} QtSample;
// NOTE: "dtUs" is named for wire-format parity with HWT1's field of the same
// role, but hallTask measures in milliseconds (millis()), not microseconds
// -- QUORUM has no per-sample microsecond timer. The unit actually carried
// is MILLISECONDS; a decoder must read it as such. Kept as uint16 (same
// saturating-at-65535 behaviour as HWT1) since a >65 s hallTask stall is
// already visible from taskMaxGapMs in the existing (non-trace) loopstat.

#define QT_SAMPLE_FLAG_ACTIVE 0x01
#define QT_SAMPLE_FLAG_POLE   0x02
#define QT_SAMPLE_DIR_SHIFT   2
#define QT_SAMPLE_DIR_MASK    0x03
#define QT_SAMPLE_FLAG_ESTOP  0x10
#define QT_SAMPLE_FLAG_LATE   0x20

// ---------------------------------------------------------------------------
// QT_REC_DECISION — one per navigation decision (bounded by real marker
// rate; no batching, sent one per datagram). ~40 bytes.
// ---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
  uint32_t tMs;                // millis() at the decision
  uint8_t  kind;                // QtDecisionKind
  uint8_t  quorumEvent;         // QtQuorumEvent, meaningful only for QTD_QUORUM_EVENT
  uint8_t  timingGate;          // QtTimingGate, meaningful only for
                                // QTD_NAV_ON_MARKER_ENTRY/QTD_TIMING_GATE_RESULT
  uint8_t  navMmBefore;
  uint8_t  navMmAfter;
  uint8_t  navStateBefore;      // NavState enum value
  uint8_t  navStateAfter;
  uint8_t  observedPolarity;    // 0/1, or QT_POLARITY_NA
  uint8_t  expectedPolarity;    // 0/1, or QT_POLARITY_NA
  uint8_t  missStreak;
  uint8_t  evalCount;
  int8_t   leaderOffset;        // QUORUM_OFFSETS[leaderIdx], or QT_OFFSET_NA
  int8_t   runnerUpOffset;      // QUORUM_OFFSETS[runnerUpIdx], or QT_OFFSET_NA
  int8_t   quorumMargin;
  int8_t   scores[6];           // QUORUM_CANDIDATES; only [0..QUORUM_CANDIDATES)
                                // meaningful -- QUORUM_CANDIDATES is fixed at 6
                                // in this firmware, so no length prefix is needed
  uint16_t dt;                  // ms since the previous RECEIVED event
  uint16_t dtExpectedMs;
  float    dtConserveRatio;     // -1.0 when not computed (mirrors lastDtConserveRatio)
  uint16_t eventPeak;
  uint16_t eventDurationMs;
  uint8_t  pwmActual;
  uint8_t  pwmCommanded;
  uint8_t  ringInserted;        // bool: this decision pushed the evidence ring
  uint8_t  pad;
} QtDecision;

// ---------------------------------------------------------------------------
// QT_REC_ANCHOR — one per accepted ngr/loco/<id>/cmd/trace_anchor command.
// Shape mirrors HWT1's anchor payload for the same reasons (operator text,
// the sample it names, motor context at that instant).
//
// sampleSeq names the MOST RECENTLY COMPLETED Hall trace sample at anchor
// execution -- QtSampleRing::lastCompletedSeq(), NOT sampleSeq() (which is a
// forward-looking count, one past the last completed sample; see that
// accessor's own comment). QT_SAMPLE_SEQ_NA if no sample has completed yet
// this session (a boot-time anchor, before hallTask's first tick lands).
// ---------------------------------------------------------------------------
#define QT_ANCHOR_TEXT_MAX 40

typedef struct __attribute__((packed)) {
  uint32_t anchorId;
  uint32_t sampleSeq;
  uint32_t tMs;
  uint8_t  dir;
  uint8_t  pwmActual;
  uint8_t  pwmCommanded;
  uint8_t  textLen;
  char     text[QT_ANCHOR_TEXT_MAX];
} QtAnchor;

// ---------------------------------------------------------------------------
// qtDecideAnchorText — the ONE place the accept/truncate/reject rule for
// operator anchor text lives. Pure and host-testable (see
// test_quorum_trace.cpp), same discipline as the ring/batch engine below:
// QUORUM.ino's handleCommand() calls this and only acts on the answer, so
// the rule itself runs under g++, not just under a static source audit.
//
// Rule (documented here because this is the one place it is enforced):
//   1. Trim leading/trailing ASCII space/tab/CR/LF.
//   2. Empty after trimming -> REJECTED. An anchor with no operator text
//      carries no evidence; matches this sketch's existing cmd/force_lost
//      precedent of rejecting an empty payload outright.
//   3. Longer than QT_ANCHOR_TEXT_MAX bytes -> TRUNCATED to exactly that
//      many bytes (byte-level, not UTF-8-aware -- same as the rest of this
//      sketch's text handling), accepted, truncated=true.
//   4. Otherwise -> accepted verbatim, truncated=false.
// ---------------------------------------------------------------------------
struct QtAnchorTextDecision {
  bool    accepted;
  bool    truncated;
  uint8_t len;
  char    text[QT_ANCHOR_TEXT_MAX];
};

static inline QtAnchorTextDecision qtDecideAnchorText(const char* raw) {
  QtAnchorTextDecision r; memset(&r, 0, sizeof(r));
  const char* p = raw;
  while (*p == ' ' || *p == '\t') p++;
  size_t len = strlen(p);
  while (len && (p[len - 1] == ' ' || p[len - 1] == '\t' ||
                p[len - 1] == '\r' || p[len - 1] == '\n')) len--;
  if (len == 0) { r.accepted = false; r.truncated = false; return r; }
  r.truncated = len > QT_ANCHOR_TEXT_MAX;
  if (r.truncated) len = QT_ANCHOR_TEXT_MAX;
  r.accepted = true;
  r.len = (uint8_t)len;
  memcpy(r.text, p, len);
  return r;
}

// ---------------------------------------------------------------------------
// QT_REC_STATUS — periodic (see QT_STATUS_INTERVAL_MS), one per datagram.
// ---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
  uint32_t uptimeMs;
  uint32_t sampleSeq;
  uint32_t decisionSeq;
  uint32_t cumSampleRingDrops;     // THIS ring's own overflow, distinct from
  uint32_t cumDecisionRingDrops;   // QUORUM's existing queueDrops/floorRejects
  uint32_t cumAnchorRingDrops;     // QtAnchorRing's own overflow
  uint32_t cumHallQueueDrops;      // mirrors the existing eventQueue queueDrops
  uint32_t cumFloorRejects;        // mirrors the existing floorRejects
  uint32_t freeHeap;
  uint32_t udpSendFailures;
  uint16_t hallDeadbandCounts;     // compile-time; lets a decoder reconstruct
  uint16_t hallEntryMarginCounts;  // every SAMPLE's thresholds from baseline
  uint8_t  quorumTrigger, quorumMargin, quorumMax, quorumCandidates;
} QtStatus;

// ---------------------------------------------------------------------------
// Batch header — precedes SAMPLE, DECISION, ANCHOR and STATUS payloads alike
// (recType says which). crc32 covers the whole header (zeroed) + payload.
// ---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
  char     magic[4];        // "QTR1"
  uint8_t  version;
  uint8_t  recType;         // QtRecType
  uint16_t nItems;          // samples in payload for QT_REC_SAMPLE; else 1
  uint32_t locoId;
  uint32_t sessionId;       // random per boot; never join across sessions
  uint32_t batchSeq;        // monotonic WITHIN this record type's own stream.
                            // Unlike HallCapture.h, SAMPLE/DECISION/STATUS do
                            // NOT share one counter: they come from separate
                            // rings with separate producers, and forcing a
                            // shared counter would need re-sealing the CRC
                            // after assignment. A gap in one type's batchSeq
                            // still means exactly one thing -- a lost
                            // datagram of THAT type -- just not "of any type".
  uint32_t firstSampleSeq;  // meaningful for QT_REC_SAMPLE only
  uint32_t t0Ms;            // millis() of payload item 0
  uint32_t crc32;
} QtHeader;

static const uint32_t QT_CRC_NIBBLE[16] = {
  0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
  0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
  0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
  0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu
};
static inline uint32_t qtCrc32(const uint8_t* data, uint32_t len, uint32_t crc) {
  crc = ~crc;
  for (uint32_t i = 0; i < len; i++) {
    crc ^= data[i];
    crc = (crc >> 4) ^ QT_CRC_NIBBLE[crc & 0x0F];
    crc = (crc >> 4) ^ QT_CRC_NIBBLE[crc & 0x0F];
  }
  return ~crc;
}
static inline void qtSeal(QtHeader* h, const uint8_t* payload, uint32_t payloadLen) {
  h->crc32 = 0;
  uint32_t c = qtCrc32((const uint8_t*)h, (uint32_t)sizeof(QtHeader), 0);
  if (payloadLen) c = qtCrc32(payload, payloadLen, c);
  h->crc32 = c;
}
static inline void qtStampMagic(QtHeader* h) {
  h->magic[0]=QT_MAGIC0; h->magic[1]=QT_MAGIC1; h->magic[2]=QT_MAGIC2; h->magic[3]=QT_MAGIC3;
  h->version = QT_FORMAT_VERSION;
}

#ifndef QT_SAMPLE_BATCH
  #define QT_SAMPLE_BATCH 100        // 100*16=1600B payload+16B hdr=1616B, under 1472? NO --
#endif
// 100*16=1600 payload + 16 header = 1616 > 1472: would IP-fragment. Sized down
// to keep one batch inside one unfragmented UDP datagram, matching HWT1's
// same reasoning (CAPTURE_FORMAT.md §2.2).
#undef QT_SAMPLE_BATCH
#define QT_SAMPLE_BATCH 90           // 90*16=1440 + 16 = 1456B, inside 1472
#ifndef QT_SAMPLE_RING_BATCHES
  #define QT_SAMPLE_RING_BATCHES 16   // ~1.6 s of buffering at 1 kHz/90-batch
#endif
#ifndef QT_DECISION_RING
  #define QT_DECISION_RING 64         // individual records, not batched
#endif
#ifndef QT_ANCHOR_RING
  #define QT_ANCHOR_RING 8            // operator-paced (a handful per run);
                                      // generous against networkTask's drain
#endif

typedef struct {
  QtHeader hdr;
  QtSample s[QT_SAMPLE_BATCH];
} QtSampleBatch;

static inline uint32_t qtSampleBatchWireLen(const QtSampleBatch* b) {
  return (uint32_t)sizeof(QtHeader) + (uint32_t)b->hdr.nItems * (uint32_t)sizeof(QtSample);
}

// ---------------------------------------------------------------------------
// QtSampleRing — hallTask calls addSample() up to ~1000x/s; only the
// per-BATCH ring push (every QT_SAMPLE_BATCH samples, ~11x/s at 1 kHz) takes
// the critical section, exactly mirroring HwtCapture's design: the current
// batch is single-producer/uncontended, only the completed-batch handoff to
// the ring is shared with networkTask's popBatch().
// ---------------------------------------------------------------------------
class QtSampleRing {
public:
  void begin(uint32_t locoId, uint32_t sessionId) {
    memset(this, 0, sizeof(*this));
    locoId_ = locoId; sessionId_ = sessionId;
    startBatch();
  }

  // dtMs: milliseconds since the previous sample (0 for the first).
  void addSample(uint16_t dtMs, int16_t raw, int16_t baseline,
                 int16_t peakN, int16_t peakS, uint8_t pwmActual,
                 uint8_t pwmCommanded, uint8_t flags) {
    if (n_ == 0) { cur_.hdr.firstSampleSeq = sampleSeq_; cur_.hdr.t0Ms = lastTMs_; }
    QtSample* s = &cur_.s[n_];
    s->dtUs = dtMs; s->raw = raw; s->baseline = baseline;
    s->evPeakN = peakN; s->evPeakS = peakS;
    s->pwmActual = pwmActual; s->pwmCommanded = pwmCommanded;
    s->flags = flags; s->pad = 0;
    n_++; sampleSeq_++;
    if (n_ >= QT_SAMPLE_BATCH) flushBatch();
  }
  void noteTimestamp(uint32_t nowMs) { lastTMs_ = nowMs; }

  // The most recently COMPLETED sample's own sequence number -- NOT
  // sampleSeq() below, which is a forward-looking count (equal to the seq
  // that will be assigned to the NEXT sample; STATUS carries it that way
  // for continuity with how QUORUM's own pre-existing counters already
  // read as "how many so far"). QT_SAMPLE_SEQ_NA before the first sample.
  uint32_t lastCompletedSeq() const { return sampleSeq_ ? sampleSeq_ - 1 : QT_SAMPLE_SEQ_NA; }

  bool popBatch(QtSampleBatch* out) {
    bool got = false;
    QT_ENTER_CRITICAL();
    if (count_) {
      memcpy(out, &ring_[tail_], sizeof(QtSampleBatch));
      tail_ = (uint16_t)((tail_ + 1u) % QT_SAMPLE_RING_BATCHES);
      count_--; got = true;
    }
    QT_EXIT_CRITICAL();
    return got;
  }

  void flushPartial() { if (n_) flushBatch(); }
  uint32_t sampleSeq()     const { return sampleSeq_; }
  uint32_t cumRingDrops()  const { return ringDrops_; }
  uint32_t cumBatchSeq()   const { return batchSeq_; }
  uint16_t queued()        const { return count_; }

private:
  void startBatch() { memset(&cur_.hdr, 0, sizeof(cur_.hdr)); qtStampMagic(&cur_.hdr);
                      cur_.hdr.recType = QT_REC_SAMPLE; n_ = 0; }
  void flushBatch() {
    cur_.hdr.nItems  = n_;
    cur_.hdr.locoId  = locoId_;
    cur_.hdr.sessionId = sessionId_;
    QT_ENTER_CRITICAL();
    cur_.hdr.batchSeq = ++batchSeq_;
    QT_EXIT_CRITICAL();
    qtSeal(&cur_.hdr, (const uint8_t*)cur_.s, (uint32_t)n_ * (uint32_t)sizeof(QtSample));
    QT_ENTER_CRITICAL();
    if (count_ >= QT_SAMPLE_RING_BATCHES) {
      tail_ = (uint16_t)((tail_ + 1u) % QT_SAMPLE_RING_BATCHES);
      count_--; ringDrops_++;   // oldest dropped, counted -- never silent
    }
    memcpy(&ring_[head_], &cur_, sizeof(QtSampleBatch));
    head_ = (uint16_t)((head_ + 1u) % QT_SAMPLE_RING_BATCHES);
    count_++;
    QT_EXIT_CRITICAL();
    startBatch();
  }

  QtSampleBatch cur_;
  QtSampleBatch ring_[QT_SAMPLE_RING_BATCHES];
  uint16_t head_=0, tail_=0, count_=0;
  uint16_t n_=0;
  uint32_t locoId_=0, sessionId_=0, sampleSeq_=0, batchSeq_=0, ringDrops_=0;
  uint32_t lastTMs_=0;
};

// ---------------------------------------------------------------------------
// QtDecisionRing — pushed from EITHER hallTask (event open/close/floor-
// reject) OR the loop thread (everything else). Two producers, one
// consumer (networkTask) -- unlike QtSampleRing, every push takes the
// critical section; acceptable because decision-rate is bounded by real
// marker traffic, not by the 1 kHz Hall tick.
// ---------------------------------------------------------------------------
class QtDecisionRing {
public:
  void begin(uint32_t locoId, uint32_t sessionId) {
    memset(this, 0, sizeof(*this));
    locoId_ = locoId; sessionId_ = sessionId;
  }
  void push(const QtDecision& d) {
    QT_ENTER_CRITICAL();
    decisionSeq_++;
    if (count_ >= QT_DECISION_RING) {
      tail_ = (uint16_t)((tail_ + 1u) % QT_DECISION_RING);
      count_--; ringDrops_++;
    }
    ring_[head_] = d;
    head_ = (uint16_t)((head_ + 1u) % QT_DECISION_RING);
    count_++;
    QT_EXIT_CRITICAL();
  }
  bool pop(QtDecision* out) {
    bool got = false;
    QT_ENTER_CRITICAL();
    if (count_) {
      *out = ring_[tail_];
      tail_ = (uint16_t)((tail_ + 1u) % QT_DECISION_RING);
      count_--; got = true;
    }
    QT_EXIT_CRITICAL();
    return got;
  }
  uint32_t locoId()       const { return locoId_; }
  uint32_t sessionId()    const { return sessionId_; }
  uint32_t decisionSeq()  const { return decisionSeq_; }
  uint32_t cumRingDrops() const { return ringDrops_; }

private:
  QtDecision ring_[QT_DECISION_RING];
  uint16_t head_=0, tail_=0, count_=0;
  uint32_t locoId_=0, sessionId_=0, decisionSeq_=0, ringDrops_=0;
};

// ---------------------------------------------------------------------------
// QtAnchorRing — pushed from the LOOP thread only (handleCommand(), via
// qtSubmitAnchor(), on an operator's ngr/loco/<id>/cmd/trace_anchor
// command), popped by networkTask. Cross-core like QtDecisionRing, so every
// push/pop takes the critical section; acceptable because anchors are
// operator-paced (at most a handful per run), nowhere near hallTask's 1 kHz
// rate. Overflow is bounded, drops the oldest, and is counted -- never
// silent, exactly like the other two rings; visible in QtStatus as
// cumAnchorRingDrops.
// ---------------------------------------------------------------------------
class QtAnchorRing {
public:
  void begin(uint32_t locoId, uint32_t sessionId) {
    memset(this, 0, sizeof(*this));
    locoId_ = locoId; sessionId_ = sessionId;
  }
  void push(const QtAnchor& a) {
    QT_ENTER_CRITICAL();
    if (count_ >= QT_ANCHOR_RING) {
      tail_ = (uint16_t)((tail_ + 1u) % QT_ANCHOR_RING);
      count_--; ringDrops_++;
    }
    ring_[head_] = a;
    head_ = (uint16_t)((head_ + 1u) % QT_ANCHOR_RING);
    count_++;
    QT_EXIT_CRITICAL();
  }
  bool pop(QtAnchor* out) {
    bool got = false;
    QT_ENTER_CRITICAL();
    if (count_) {
      *out = ring_[tail_];
      tail_ = (uint16_t)((tail_ + 1u) % QT_ANCHOR_RING);
      count_--; got = true;
    }
    QT_EXIT_CRITICAL();
    return got;
  }
  uint32_t cumRingDrops() const { return ringDrops_; }

private:
  QtAnchor ring_[QT_ANCHOR_RING];
  uint16_t head_=0, tail_=0, count_=0;
  uint32_t locoId_=0, sessionId_=0, ringDrops_=0;
};

#endif // QUORUM_TRACE

// ============================================================================
// Macro layer — ALWAYS defined, regardless of QUORUM_TRACE. This is the one
// place trace conditionality lives: every call site in QUORUM.ino reads as a
// single, unconditional-looking line (QT_SAMPLE_TICK(...), QT_DECISION_...),
// and expands to nothing at all when QUORUM_TRACE is undefined -- not a
// runtime branch, not a call to an empty function, nothing emitted. The
// functions these call when tracing IS on (qtSampleTick, qtDecisionXxx,
// qtNetworkDrain, ...) are defined in QUORUM.ino itself, beside the rest of
// the trace glue (WiFiUDP, the two rings, the portMUX) -- this header stays
// pure C++ and host-testable; QUORUM.ino is where Arduino/FreeRTOS meet it.
// ============================================================================
#ifdef QUORUM_TRACE
  #define QT_TRACE_BEGIN(locoId, sessionId)          qtTraceBegin((locoId), (sessionId))
  #define QT_SAMPLE_TICK(nowMs, gapMs)                qtSampleTick((nowMs), (gapMs))
  #define QT_DECISION_EVENT_OPENED(polarity, atMs)    qtDecisionEventOpened((polarity), (atMs))
  #define QT_DECISION_EVENT_FLOOR_REJECT(durMs)       qtDecisionEventFloorReject((durMs))
  #define QT_DECISION_EVENT_CLOSED(ev)                qtDecisionEventClosed((ev))
  #define QT_DECISION_NAV_ENTRY(ev, mmBefore)         qtDecisionNavEntry((ev), (mmBefore))
  #define QT_DECISION_GATE_RESULT(mmBefore, gateCode) qtDecisionGateResult((mmBefore), (gateCode))
  #define QT_DECISION_ACCEPT(ev, mmBefore, stBefore)  qtDecisionAccept((ev), (mmBefore), (stBefore))
  #define QT_DECISION_NAV_STATE(evName, evPtr)        qtDecisionNavState((evName), (evPtr))
  #define QT_DECISION_QUORUM(evName)                  qtDecisionQuorum((evName))
  #define QT_DIR_MIRROR_UPDATE(dirValue)               qtDirMirrorUpdate((dirValue))
  #define QT_STATUS_SERVICE()                          qtStatusService()
  #define QT_NETWORK_DRAIN()                           qtNetworkDrain()
#else
  #define QT_TRACE_BEGIN(locoId, sessionId)            do {} while (0)
  #define QT_SAMPLE_TICK(nowMs, gapMs)                 do {} while (0)
  #define QT_DECISION_EVENT_OPENED(polarity, atMs)     do {} while (0)
  #define QT_DECISION_EVENT_FLOOR_REJECT(durMs)        do {} while (0)
  #define QT_DECISION_EVENT_CLOSED(ev)                 do {} while (0)
  #define QT_DECISION_NAV_ENTRY(ev, mmBefore)          do {} while (0)
  #define QT_DECISION_GATE_RESULT(mmBefore, gateCode)  do {} while (0)
  #define QT_DECISION_ACCEPT(ev, mmBefore, stBefore)   do {} while (0)
  #define QT_DECISION_NAV_STATE(evName, evPtr)         do {} while (0)
  #define QT_DECISION_QUORUM(evName)                   do {} while (0)
  #define QT_DIR_MIRROR_UPDATE(dirValue)                do {} while (0)
  #define QT_STATUS_SERVICE()                           do {} while (0)
  #define QT_NETWORK_DRAIN()                            do {} while (0)
#endif

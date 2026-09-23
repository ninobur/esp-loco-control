// ============================================================================
// HallRecorder.h — continuous 1 kHz Hall record for X18, streamed to the Pi.
//
// OBSERVATION BUILD. Nothing in this file is ever read by X18's control path.
// Every function here is called FROM that path (a one-way tap), never the
// reverse, and every value carried here is a COPY taken after X18 has already
// decided what to do with it. Losing every record, at any time, in any order,
// changes nothing the locomotive does. There is no route by which a recorded
// value re-enters acquisition, recognition, navigation, the station machine,
// the throttle or the safety layer, and the diff against 242109e is additive
// only -- see docs/NAVI_X18_RECORDER_20260916.md §3.
//
// NO NEW REMOTE SURFACE. The locomotive's recorder socket is SEND-ONLY. It
// binds no port, subscribes to no topic and parses no inbound datagram, so
// this build adds nothing that can be commanded from the network. The
// receiver on the Pi is listen-only for the same reason.
//
// Format identity: magic "XHR1", version 1. This is NOT HWT1 or QTR1 wearing
// a new name -- the sample record, the per-batch navigation context, and the
// ruling record are all different, because what is being measured is
// different: HWT1 recorded a bench sweep, QTR1 recorded QUORUM's detector,
// and XHR1 records the exact value X18 navigates by plus enough map context
// to tie every millisecond of it to a surveyed magnet. The three formats are
// deliberately not interchangeable and their decoders refuse each other's
// magic.
//
// WHAT IS RECORDED IS THE VALUE X18 USED. addSample() is called with the
// median-of-five int16 returned by hallRead(), on the same tick, before
// capture.sample() has seen it. Not a re-read, not a second conversion, not
// an average of its own -- the identical number. A recorded trace and X18's
// own rulings therefore cannot disagree about what the sensor said.
//
// NO BASELINE RULE LIVES HERE. This file proposes no measurement boundary,
// no ordinary-track window and no interval segmentation. It carries X18's
// operative baseline_ per batch as CONTEXT -- what the flown firmware was
// actually using at that moment, needed to align this trace with X18's own
// telemetry -- and nothing else. Choosing measurement boundaries is the
// offline analysis's job, from the raw trace, after the fact.
//
// Pure C++ (stdint + string.h) throughout. The ring/batch engine compiles and
// runs on the host exactly as it does on the ESP32, so tests/gate_recorder.cpp
// emits REAL datagram bytes that tools/xhr_format.py parses -- the firmware
// and the decoder are checked against each other, not each against a
// description of the other. The Arduino/FreeRTOS glue (WiFiUDP, the portMUX,
// the task hooks) lives in the .ino, not here.
//
// ARDUINO PROTOTYPE HAZARD. Every type this build adds is declared in THIS
// header, and the .ino includes it above the first function that names one.
// The Arduino preprocessor inserts its generated prototypes after the last
// #include line, so a type defined here is always visible before any
// prototype that mentions it. A struct defined in the .ino itself would not
// be -- that is exactly how the draft recorder failed to compile, with
// "'HallRecord' does not name a type" pointing at a line nobody wrote. Do not
// move these definitions into the sketch.
// ============================================================================
#pragma once

#include <stdint.h>
#include <string.h>

// The critical section is supplied by the .ino (portMUX). On the host it is a
// no-op: gate_recorder.cpp drives the engine single-threaded on purpose, so a
// failure there is a format or accounting failure and never a race artefact.
#ifndef XHR_ENTER_CRITICAL
  #define XHR_ENTER_CRITICAL() do {} while (0)
  #define XHR_EXIT_CRITICAL()  do {} while (0)
#endif

// ---------------------------------------------------------------------------
// Wire constants
// ---------------------------------------------------------------------------
#define XHR_MAGIC0 'X'
#define XHR_MAGIC1 'H'
#define XHR_MAGIC2 'R'
#define XHR_MAGIC3 '1'
#define XHR_FORMAT_VERSION 1

enum XhrRecType : uint8_t {
  XHR_REC_SAMPLES = 1,   // the continuous 1 kHz trace, batched
  XHR_REC_RULING  = 2,   // one X18 marker ruling, unbatched
  XHR_REC_STATUS  = 3    // periodic health and loss accounting
};

// X18's Ruling enum, copied by value at the tap so this header does not
// include Navigator.h and the decoder does not depend on its numbering.
enum XhrRuling : uint8_t {
  XHR_RULING_NONE        = 0,
  XHR_RULING_ADVANCED    = 1,
  XHR_RULING_WRONG       = 2,   // Ruling::WrongMagnet   -> DISAGREE
  XHR_RULING_CONTRADICT  = 3,   // Ruling::Contradicted
  XHR_RULING_NOT_MAGNET  = 4,   // Ruling::NotAMagnet
  XHR_RULING_NO_POSITION = 5,   // anything else -> NO_POSITION
  XHR_RULING_STALE       = 6    // epoch mismatch: never reached navigator.judge()
};

#define XHR_SAMPLE_SEQ_NA 0xFFFFFFFFu   // no sample has completed yet this session
#define XHR_MM_NA         0xFFu         // position not known
#define XHR_POLARITY_NA   0xFFu

// ---------------------------------------------------------------------------
// XHR_REC_SAMPLES payload item — one per hallTask tick, nominally 1 kHz.
// 8 bytes. Physical measurement and motor context only; no interpretation.
// ---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
  uint16_t dtUs;         // microseconds since the previous sample, SATURATING
                         // at 65535 (65.5 ms). A saturated value means "at
                         // least this long" and is never silently a wrap --
                         // the LATE flag is set with it, and a stall longer
                         // than 65 ms is visible in maxGapUs in STATUS too.
  int16_t  raw;          // THE median-of-five hallRead() value X18 navigated
                         // by on this tick. Signed because HallCapture works
                         // in signed counts; the ADC range is 0..4095.
  uint8_t  pwmActual;
  uint8_t  pwmCommanded;
  uint8_t  flags;
  uint8_t  pad;          // zero
} XhrSample;

#define XHR_F_DIR_FWD    0x01   // motorDirection == 1
#define XHR_F_ESTOP      0x02   // estopped || estopAsserted
#define XHR_F_AUTO       0x04   // autoRunning
#define XHR_F_MAY_ADAPT  0x08   // X18's mayAdapt for THIS tick (pwm > adapt pwm)
#define XHR_F_PASSAGE    0x10   // capture.open() as X18 saw it on this tick
#define XHR_F_LATE       0x20   // dtUs > 1250: the tick did not land on time
#define XHR_F_LOW_VOLT   0x40   // lowVoltage

// ---------------------------------------------------------------------------
// Batch header — precedes every record type (recType says which).
// crc32 covers the whole header with the crc field zeroed, then the payload.
// 44 bytes.
//
// The navigation context fields (baselineAtT0 .. stPhase) are mirrors written
// by loop() and read by hallTask. They are single-writer, naturally aligned,
// and at most 16 bits wide, so a read is never torn on this core; they are
// carried per BATCH (every ~100 ms) rather than per sample because none of
// them can move meaningfully faster than that, and because an offline
// analysis needs them to say WHICH mapped interval a stretch of trace sits
// in, not to reconstruct them at 1 kHz.
// ---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
  char     magic[4];         // "XHR1"
  uint8_t  version;
  uint8_t  recType;          // XhrRecType
  uint16_t nItems;           // samples in payload for XHR_REC_SAMPLES; else 1
  uint32_t locoId;
  uint32_t sessionId;        // random per boot. NEVER join two sessions.
  uint32_t batchSeq;         // monotonic WITHIN this record type's own stream
  uint32_t firstSampleSeq;   // meaningful for XHR_REC_SAMPLES only
  uint32_t t0Ms;             // millis() of payload item 0 -- aligns this trace
                             // with every other X18 telemetry record, all of
                             // which are stamped in millis()
  uint32_t t0Us;             // micros() of payload item 0. WRAPS every 71.6
                             // minutes; a decoder reconstructs intra-batch
                             // time from dtUs and uses t0Ms across batches.
  int16_t  baselineAtT0;     // X18's OPERATIVE baseline_ -- context, not a rule
  uint16_t ringDropsLo;      // low 16 bits of the cumulative ring-drop count at
                             // the moment this batch was sealed. A batchSeq gap
                             // WITH an increase here was dropped on the
                             // locomotive; a gap WITHOUT one was lost in
                             // transit. The decoder never has to guess which.
  uint8_t  navMm;            // XHR_MM_NA when position is not known
  int8_t   navDir;           // +1 CW, -1 CCW, 0 unset
  uint8_t  stPhase;          // StPhase: 0 Idle 1 Approach 2 Zone 3 Ramp 4 Dwell 5 Depart
  uint8_t  ctxFlags;         // XHR_CTX_*
  uint32_t crc32;
} XhrHeader;

#define XHR_CTX_POS_KNOWN  0x01
#define XHR_CTX_AUTO       0x02
#define XHR_CTX_ESTOP      0x04
#define XHR_CTX_MQTT_UP    0x08

// ---------------------------------------------------------------------------
// XHR_REC_RULING payload — one per marker X18 ruled on, sent unbatched. 44 B.
// This is X18's ruling COPIED, not a second opinion: every field is
// read out of the Judged/Verdict/Ruling the navigator has already produced.
// ---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
  uint32_t tMs;              // millis() at the tap, just after navigator.judge()
  uint32_t openedAtMs;       // X18's passage open  (its own clock)
  uint32_t closedAtMs;       // X18's passage close
  uint32_t sampleSeq;        // most recently COMPLETED trace sample at the tap,
                             // or XHR_SAMPLE_SEQ_NA. This is the join key: it
                             // names the exact position in the continuous
                             // record that this ruling belongs beside.
  uint16_t peakCounts;
  uint16_t durationMs;       // closedAtMs - openedAtMs, as X18 computed it
  uint16_t gapMs;
  int16_t  entryBaseline;
  int16_t  closeBaseline;
  int16_t  shadowBaseline;
  float    amplitudeRatio;
  uint8_t  polarity;         // 0 south, 1 north, or XHR_POLARITY_NA
  uint8_t  ruling;           // XhrRuling
  uint8_t  outcome;          // Verdict::outcome, unmapped
  uint8_t  isMagnet;
  uint8_t  navMmBefore;
  uint8_t  navMmAfter;
  int8_t   navDir;
  uint8_t  stPhase;
  uint8_t  pwmActual;
  uint8_t  postStopSuccessor;
  uint8_t  pad[2];
} XhrRulingRec;

// ---------------------------------------------------------------------------
// XHR_REC_STATUS payload — health and loss accounting, ~1 Hz. 52 bytes.
// Every counter here is CUMULATIVE since boot, so a decoder that missed a
// status datagram still recovers the totals from the next one.
// ---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
  uint32_t tMs;
  uint32_t uptimeMs;
  uint32_t cumSamples;          // samples handed to addSample() since boot
  uint32_t cumSampleRingDrops;  // whole BATCHES dropped by the ring, oldest first
  uint32_t cumRulingRingDrops;
  uint32_t cumUdpFailures;      // sendto() refused or short
  uint32_t maxGapUs;            // worst tick-to-tick gap since boot
  uint32_t maxTickBodyUs;       // worst time spent inside the hallTask body
  uint32_t freeHeap;
  uint32_t hallStackFreeBytes;  // BYTES. uxTaskGetStackHighWaterMark returns
                                // bytes on ESP-IDF (portSTACK_TYPE is uint8_t),
                                // not words. Labelled wrong once already.
  uint32_t netStackFreeBytes;
  uint16_t measuredHzX10;       // measured sample rate over the last second
  uint16_t ringHighWater;       // deepest the batch ring has ever been
  int8_t   rssi;
  uint8_t  mqttConnected;
  uint8_t  wifiConnected;
  uint8_t  pad;
} XhrStatusRec;

// ---------------------------------------------------------------------------
// CRC-32 (the standard reflected polynomial, so Python's zlib.crc32 agrees).
// Nibble table: 64 bytes of rodata instead of 1 KiB, and the whole 844-byte
// datagram seals in about 30 us on this core -- done on networkTask, never on
// hallTask.
// ---------------------------------------------------------------------------
static const uint32_t XHR_CRC_NIBBLE[16] = {
  0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
  0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
  0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
  0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu
};
static inline uint32_t xhrCrc32(const uint8_t* data, uint32_t len, uint32_t crc) {
  crc = ~crc;
  for (uint32_t i = 0; i < len; i++) {
    crc ^= data[i];
    crc = (crc >> 4) ^ XHR_CRC_NIBBLE[crc & 0x0F];
    crc = (crc >> 4) ^ XHR_CRC_NIBBLE[crc & 0x0F];
  }
  return ~crc;
}
static inline void xhrStampMagic(XhrHeader* h) {
  h->magic[0]=XHR_MAGIC0; h->magic[1]=XHR_MAGIC1;
  h->magic[2]=XHR_MAGIC2; h->magic[3]=XHR_MAGIC3;
  h->version = XHR_FORMAT_VERSION;
}
static inline void xhrSeal(XhrHeader* h, const uint8_t* payload, uint32_t payloadLen) {
  h->crc32 = 0;
  uint32_t c = xhrCrc32((const uint8_t*)h, (uint32_t)sizeof(XhrHeader), 0);
  if (payloadLen) c = xhrCrc32(payload, payloadLen, c);
  h->crc32 = c;
}

// ---------------------------------------------------------------------------
// Sizing.
//
// 100 samples x 8 B + 44 B header = 844 B, one unfragmented UDP datagram
// (well inside the 1472 B Ethernet/IP/UDP payload), one every 100 ms.
//
// Why 100 and not 180 (which would also fit): a lost datagram costs exactly
// its own span of trace, and 100 ms is already comparable to a 140 ms magnet
// passage. Making batches bigger to save packet overhead would make each loss
// swallow a whole magnet. 10 datagrams/s is negligible Wi-Fi airtime and the
// arithmetic is exact: sampleSeq == batchSeq*100 when nothing has been lost.
//
// RING: 40 batches = 4.0 s of buffering. Otto's longest observed MQTT stall is
// well under that, and 40 x 844 = 33.8 kB of the ESP32's 320 kB DRAM. X18 flew
// at 18% RAM; §5 of the report gives the measured figure with this added.
// ---------------------------------------------------------------------------
#ifndef XHR_SAMPLE_BATCH
  #define XHR_SAMPLE_BATCH 100
#endif
#ifndef XHR_SAMPLE_RING_BATCHES
  #define XHR_SAMPLE_RING_BATCHES 40
#endif
#ifndef XHR_RULING_RING
  #define XHR_RULING_RING 32
#endif

typedef struct {
  XhrHeader hdr;
  XhrSample s[XHR_SAMPLE_BATCH];
} XhrSampleBatch;

static inline uint32_t xhrSampleBatchWireLen(const XhrSampleBatch* b) {
  return (uint32_t)sizeof(XhrHeader) + (uint32_t)b->hdr.nItems * (uint32_t)sizeof(XhrSample);
}

// ---------------------------------------------------------------------------
// XhrSampleRing
//
// hallTask calls addSample() up to 1000x/s. That path is a bounds check, an
// 8-byte store and an increment -- no allocation, no lock, no formatting, no
// float, no blocking call. Only the completed-BATCH handoff (10x/s) takes the
// critical section, because that is the only moment the current batch becomes
// visible to networkTask's popBatch().
//
// Overflow drops the OLDEST batch and counts it. Never silent, and never the
// newest: when the radio is gone the interesting evidence is what is happening
// NOW, and a decoder that sees a batchSeq gap plus a ringDrops increase knows
// precisely what it lost and where.
// ---------------------------------------------------------------------------
class XhrSampleRing {
public:
  void begin(uint32_t locoId, uint32_t sessionId) {
    locoId_ = locoId; sessionId_ = sessionId;
    head_ = tail_ = count_ = 0;
    sampleSeq_ = 0; batchSeq_ = 0; ringDrops_ = 0; highWater_ = 0;
    n_ = 0;
    startBatch();
  }

  // Called from hallTask. nowMs/nowUs are the tick's own clocks; the context
  // arguments are the mirrors, sampled once here rather than per field so a
  // batch's header describes one consistent instant.
  void addSample(uint32_t nowMs, uint32_t nowUs, int16_t raw,
                 uint8_t pwmActual, uint8_t pwmCommanded, uint8_t flags,
                 int16_t baseline, uint8_t navMm, int8_t navDir,
                 uint8_t stPhase, uint8_t ctxFlags) {
    if (n_ == 0) {
      cur_.hdr.t0Ms = nowMs;
      cur_.hdr.t0Us = nowUs;
      cur_.hdr.firstSampleSeq = sampleSeq_;
      cur_.hdr.baselineAtT0 = baseline;
      cur_.hdr.navMm = navMm;
      cur_.hdr.navDir = navDir;
      cur_.hdr.stPhase = stPhase;
      cur_.hdr.ctxFlags = ctxFlags;
    }
    uint32_t dt = nowUs - lastUs_;          // unsigned: immune to micros() wrap
    if (!haveLast_) { dt = 0; haveLast_ = true; }
    if (dt > 65535u) dt = 65535u;           // saturate, and say so in the flags
    if (dt > 1250u) flags |= XHR_F_LATE;
    if (dt > maxGapUs_) maxGapUs_ = dt;
    lastUs_ = nowUs;

    XhrSample* s = &cur_.s[n_];
    s->dtUs = (uint16_t)dt;
    s->raw = raw;
    s->pwmActual = pwmActual;
    s->pwmCommanded = pwmCommanded;
    s->flags = flags;
    s->pad = 0;
    ++n_;
    ++sampleSeq_;
    ++cumSamples_;
    if (n_ >= XHR_SAMPLE_BATCH) flushBatch();
  }

  // Called from networkTask. Copies one batch out and SEALS IT HERE, at send
  // time, not when it was filled.
  //
  // That is the whole point of doing it here. ringDropsLo has to mean "how
  // many batches the locomotive had thrown away as of the moment this
  // datagram left", because that is the only reading that lets a receiver
  // attribute a sequence gap. Sealing at fill time gave the opposite: when
  // the radio came back, every surviving batch in the ring had been stamped
  // BEFORE the drops that emptied it, so the counter did not move across the
  // gap it was supposed to explain, and on-board loss looked exactly like
  // transport loss. Gate 14 caught it.
  //
  // The CRC runs on networkTask, ~30 us per datagram, ten times a second.
  // Never on hallTask.
  bool popBatch(XhrSampleBatch* out) {
    bool got = false;
    XHR_ENTER_CRITICAL();
    if (count_) {
      memcpy(out, &ring_[tail_], sizeof(XhrSampleBatch));
      tail_ = (uint16_t)((tail_ + 1) % XHR_SAMPLE_RING_BATCHES);
      --count_;
      got = true;
    }
    const uint32_t drops = ringDrops_;
    XHR_EXIT_CRITICAL();
    if (got) {
      out->hdr.ringDropsLo = (uint16_t)(drops & 0xFFFFu);
      xhrSeal(&out->hdr, (const uint8_t*)out->s,
              (uint32_t)out->hdr.nItems * (uint32_t)sizeof(XhrSample));
    }
    return got;
  }

  // The last sample seq that has actually been written, for the ruling join
  // key. sampleSeq_ is forward-looking (one past the last written), so this is
  // one less -- and XHR_SAMPLE_SEQ_NA before the first tick has landed.
  uint32_t lastCompletedSeq() const {
    return sampleSeq_ ? (sampleSeq_ - 1) : XHR_SAMPLE_SEQ_NA;
  }
  uint32_t cumSamples()   const { return cumSamples_; }
  uint32_t cumRingDrops() const { return ringDrops_; }
  uint32_t maxGapUs()     const { return maxGapUs_; }
  uint16_t highWater()    const { return highWater_; }
  uint16_t depth()        const { return count_; }
  uint32_t sessionId()    const { return sessionId_; }

private:
  void startBatch() {
    memset(&cur_.hdr, 0, sizeof(cur_.hdr));
    xhrStampMagic(&cur_.hdr);
    cur_.hdr.recType = XHR_REC_SAMPLES;
    cur_.hdr.locoId = locoId_;
    cur_.hdr.sessionId = sessionId_;
    cur_.hdr.navMm = XHR_MM_NA;
    n_ = 0;
  }
  // Fills in the batch's own identity and hands it to the ring. NOT sealed
  // here -- popBatch() seals at send time so the drop counter is current; see
  // its comment. This runs on hallTask, so it does no CRC at all.
  void flushBatch() {
    if (!n_) return;
    cur_.hdr.nItems = n_;
    cur_.hdr.batchSeq = ++batchSeq_;
    XHR_ENTER_CRITICAL();
    if (count_ == XHR_SAMPLE_RING_BATCHES) {
      tail_ = (uint16_t)((tail_ + 1) % XHR_SAMPLE_RING_BATCHES);
      --count_;
      ++ringDrops_;                        // oldest dropped, counted, never silent
    }
    memcpy(&ring_[head_], &cur_, sizeof(XhrSampleBatch));
    head_ = (uint16_t)((head_ + 1) % XHR_SAMPLE_RING_BATCHES);
    ++count_;
    if (count_ > highWater_) highWater_ = count_;
    XHR_EXIT_CRITICAL();
    startBatch();
  }

  XhrSampleBatch cur_;
  XhrSampleBatch ring_[XHR_SAMPLE_RING_BATCHES];
  uint16_t head_ = 0, tail_ = 0, count_ = 0, n_ = 0, highWater_ = 0;
  uint32_t locoId_ = 0, sessionId_ = 0;
  uint32_t sampleSeq_ = 0, batchSeq_ = 0, ringDrops_ = 0, cumSamples_ = 0;
  uint32_t lastUs_ = 0, maxGapUs_ = 0;
  bool     haveLast_ = false;
};

// ---------------------------------------------------------------------------
// XhrRulingRing — one record per ruling, sent unbatched. Rulings arrive at the
// marker rate (a handful per second at most), so 32 slots is many seconds of
// buffering; overflow still drops the oldest and counts it.
// ---------------------------------------------------------------------------
class XhrRulingRing {
public:
  void begin(uint32_t locoId, uint32_t sessionId) {
    locoId_ = locoId; sessionId_ = sessionId;
    head_ = tail_ = count_ = 0; seq_ = 0; ringDrops_ = 0;
  }
  void push(const XhrRulingRec& r) {
    XHR_ENTER_CRITICAL();
    if (count_ == XHR_RULING_RING) {
      tail_ = (uint16_t)((tail_ + 1) % XHR_RULING_RING);
      --count_; ++ringDrops_;
    }
    ring_[head_] = r;
    head_ = (uint16_t)((head_ + 1) % XHR_RULING_RING);
    ++count_;
    XHR_EXIT_CRITICAL();
  }
  // Pops one record and seals a complete datagram into buf. Returns the wire
  // length, or 0 when the ring is empty or the buffer is too small.
  uint32_t popSealed(uint8_t* buf, uint32_t cap) {
    if (cap < sizeof(XhrHeader) + sizeof(XhrRulingRec)) return 0;
    XhrRulingRec r;
    bool got = false;
    XHR_ENTER_CRITICAL();
    if (count_) {
      r = ring_[tail_];
      tail_ = (uint16_t)((tail_ + 1) % XHR_RULING_RING);
      --count_; got = true;
    }
    XHR_EXIT_CRITICAL();
    if (!got) return 0;
    XhrHeader* h = (XhrHeader*)buf;
    memset(h, 0, sizeof(XhrHeader));
    xhrStampMagic(h);
    h->recType = XHR_REC_RULING;
    h->nItems = 1;
    h->locoId = locoId_;
    h->sessionId = sessionId_;
    h->batchSeq = ++seq_;
    h->firstSampleSeq = r.sampleSeq;
    h->t0Ms = r.tMs;
    h->baselineAtT0 = r.closeBaseline;
    h->ringDropsLo = (uint16_t)(ringDrops_ & 0xFFFFu);
    h->navMm = r.navMmAfter;
    h->navDir = r.navDir;
    h->stPhase = r.stPhase;
    memcpy(buf + sizeof(XhrHeader), &r, sizeof(XhrRulingRec));
    xhrSeal(h, buf + sizeof(XhrHeader), (uint32_t)sizeof(XhrRulingRec));
    return (uint32_t)(sizeof(XhrHeader) + sizeof(XhrRulingRec));
  }
  uint32_t cumRingDrops() const { return ringDrops_; }
  uint16_t depth()        const { return count_; }

private:
  XhrRulingRec ring_[XHR_RULING_RING];
  uint16_t head_ = 0, tail_ = 0, count_ = 0;
  uint32_t locoId_ = 0, sessionId_ = 0, seq_ = 0, ringDrops_ = 0;
};

// Seals a STATUS datagram into buf the same way. Kept a free function because
// status has no ring: it is produced and sent in the same breath, on
// networkTask, and a status record that cannot be sent is worth nothing later.
static inline uint32_t xhrSealStatus(uint8_t* buf, uint32_t cap,
                                     uint32_t locoId, uint32_t sessionId,
                                     uint32_t seq, const XhrStatusRec& st,
                                     int16_t baseline, uint8_t navMm,
                                     int8_t navDir, uint8_t stPhase,
                                     uint8_t ctxFlags, uint16_t ringDropsLo) {
  if (cap < sizeof(XhrHeader) + sizeof(XhrStatusRec)) return 0;
  XhrHeader* h = (XhrHeader*)buf;
  memset(h, 0, sizeof(XhrHeader));
  xhrStampMagic(h);
  h->recType = XHR_REC_STATUS;
  h->nItems = 1;
  h->locoId = locoId;
  h->sessionId = sessionId;
  h->batchSeq = seq;
  h->firstSampleSeq = XHR_SAMPLE_SEQ_NA;
  h->t0Ms = st.tMs;
  h->baselineAtT0 = baseline;
  h->ringDropsLo = ringDropsLo;
  h->navMm = navMm;
  h->navDir = navDir;
  h->stPhase = stPhase;
  h->ctxFlags = ctxFlags;
  memcpy(buf + sizeof(XhrHeader), &st, sizeof(XhrStatusRec));
  xhrSeal(h, buf + sizeof(XhrHeader), (uint32_t)sizeof(XhrStatusRec));
  return (uint32_t)(sizeof(XhrHeader) + sizeof(XhrStatusRec));
}

// Compile-time guards on the wire layout. If a compiler ever pads one of
// these, the build stops here rather than producing a capture that the
// decoder silently misreads.
#if defined(__cplusplus) && __cplusplus >= 201103L
static_assert(sizeof(XhrSample) == 8,      "XhrSample must be 8 bytes on the wire");
static_assert(sizeof(XhrHeader) == 44,     "XhrHeader must be 44 bytes on the wire");
static_assert(sizeof(XhrRulingRec) == 44,  "XhrRulingRec must be 44 bytes on the wire");
static_assert(sizeof(XhrStatusRec) == 52,  "XhrStatusRec must be 52 bytes on the wire");
static_assert(sizeof(XhrHeader) + XHR_SAMPLE_BATCH * sizeof(XhrSample) <= 1472,
              "one sample batch must fit in one unfragmented UDP datagram");
#endif

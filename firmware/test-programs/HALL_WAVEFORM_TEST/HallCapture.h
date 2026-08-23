// ============================================================================
// HallCapture.h — HALL_WAVEFORM_TEST capture engine
//
// INVESTIGATORY / UNAPPROVED. Diagnostic instrument only.
//
// Pure C++ (stdint + string.h only). No Arduino, no FreeRTOS, no ADC, no
// motor, no navigation. That is deliberate: the whole engine compiles and
// runs on the host, so ordering, rollover, overflow and dual-channel
// alignment are tested with g++ instead of being argued about.
//
// What it does:
//   - accepts one sample PAIR at a time (ch0, ch1, microsecond timestamp,
//     motor context) and stores it verbatim — no averaging, no filtering,
//     no threshold, no rejection;
//   - assigns every sample a monotonic sequence number;
//   - derives missed-slot counts from the timestamp gap and RECORDS them as
//     absent (skipped sequence numbers), never as fabricated samples;
//   - fills fixed-size batches and pushes them into a bounded ring;
//   - on overflow drops the OLDEST batch and counts it, so the loss is
//     always visible as a batchSeq gap at the receiver.
//
// What it must never do: decide what is worth recording.
// ============================================================================
#pragma once

#include <stdint.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Critical section. On the ESP32 the sampler task pushes and the network task
// pops, so the ring needs a lock; on the host the tests are single-threaded
// and these compile away.
// ---------------------------------------------------------------------------
#ifndef HWT_ENTER_CRITICAL
  #define HWT_ENTER_CRITICAL() do {} while (0)
  #define HWT_EXIT_CRITICAL()  do {} while (0)
#endif

// ---------------------------------------------------------------------------
// Wire constants
// ---------------------------------------------------------------------------
#define HWT_MAGIC0 'H'
#define HWT_MAGIC1 'W'
#define HWT_MAGIC2 'T'
#define HWT_MAGIC3 '1'
#define HWT_FORMAT_VERSION 1

enum HwtRecType : uint8_t {
  HWT_REC_SAMPLES = 1,
  HWT_REC_ANCHOR  = 2,
  HWT_REC_STATUS  = 3
};

// Per-channel annotation: what the OLD Module C threshold rule WOULD have
// said about this sample. Annotation only — it gates nothing, on the
// locomotive or in the recorder.
enum HwtAnn : uint8_t {
  HWT_ANN_NONE  = 0,
  HWT_ANN_NORTH = 1,
  HWT_ANN_SOUTH = 2
};

// Direction codes as carried on the wire (LL_LocoConfig values).
enum HwtDir : uint8_t {
  HWT_DIR_REVERSE = 0,
  HWT_DIR_NEUTRAL = 1,
  HWT_DIR_FORWARD = 2
};

// ---------------------------------------------------------------------------
// One sample PAIR — 10 bytes, little-endian, both channels acquired inside
// the same slot from the same timestamp. Channel alignment is structural:
// there is no way to store ch0 without ch1's slot.
//
//   ch0/ch1 : bits 0-11 raw 12-bit ADC exactly as read (NEVER averaged)
//             bits 12-13 HwtAnn annotation
//             bit  14    channel present (ch1 clear when compiled out)
//             bit  15    reserved, zero
//   dtUs    : microseconds since the previous ACQUIRED sample, saturating
//   ctx     : bits 0-1 direction, bit 2 e-stop, bit 3 fixed-PWM mode,
//             bit 4 test sequence running, bit 5 this sample was late
// ---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
  uint16_t ch0;
  uint16_t ch1;
  uint16_t dtUs;
  uint8_t  pwmActual;
  uint8_t  pwmCommanded;
  uint8_t  ctx;
  uint8_t  pad;          // zero
} HwtSample;

#define HWT_CTX_DIR_MASK   0x03
#define HWT_CTX_ESTOP      0x04
#define HWT_CTX_FIXED      0x08
#define HWT_CTX_SEQRUN     0x10
#define HWT_CTX_LATE       0x20

#define HWT_CH_RAW_MASK    0x0FFF
#define HWT_CH_ANN_SHIFT   12
#define HWT_CH_PRESENT     0x4000

static inline uint16_t hwtPackChannel(uint16_t raw12, uint8_t ann, bool present) {
  uint16_t w = (uint16_t)(raw12 & HWT_CH_RAW_MASK);
  w = (uint16_t)(w | ((uint16_t)(ann & 0x03) << HWT_CH_ANN_SHIFT));
  if (present) w = (uint16_t)(w | HWT_CH_PRESENT);
  return w;
}

// ---------------------------------------------------------------------------
// Batch header — 52 bytes, little-endian, prefixes every packet.
// crc32 covers the whole header (with crc32 itself zeroed) plus the payload.
// ---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
  char     magic[4];          // "HWT1"
  uint8_t  version;           // HWT_FORMAT_VERSION
  uint8_t  recType;           // HwtRecType
  uint16_t nSamples;          // samples in payload (0 for anchor/status)
  uint32_t locoId;
  uint32_t sessionId;         // random per boot; never join across sessions
  uint32_t batchSeq;          // monotonic per session; a gap IS a lost batch
  uint32_t firstSampleSeq;    // sequence of payload[0] (u32, wraps — see doc)
  uint64_t t0Us;              // esp_timer microseconds of payload[0]
  uint32_t missedBefore;      // slots skipped immediately before firstSampleSeq
  uint32_t cumMissed;         // cumulative skipped slots this session
  uint32_t cumQueueDrops;     // cumulative batches dropped by the ring
  uint32_t maxGapUs;          // worst acquisition gap seen this session
  uint32_t crc32;
} HwtBatchHeader;

// ---------------------------------------------------------------------------
// CRC-32 (IEEE, reflected) — 16-entry nibble table. Matches Python zlib.crc32.
// Nibble-at-a-time keeps the per-batch cost near 45 us on a 240 MHz core, which
// matters: it runs inside a 1 ms acquisition slot.
// ---------------------------------------------------------------------------
static const uint32_t HWT_CRC_NIBBLE[16] = {
  0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
  0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
  0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
  0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu
};

static inline uint32_t hwtCrc32(const uint8_t* data, uint32_t len, uint32_t crc) {
  crc = ~crc;
  for (uint32_t i = 0; i < len; i++) {
    crc ^= data[i];
    crc = (crc >> 4) ^ HWT_CRC_NIBBLE[crc & 0x0F];
    crc = (crc >> 4) ^ HWT_CRC_NIBBLE[crc & 0x0F];
  }
  return ~crc;
}

// ---------------------------------------------------------------------------
// Unsigned-wrap-safe elapsed time. Used for every millisecond deadline in the
// sketch so the 49.7-day millis() rollover cannot stall a ramp or an anchor.
// ---------------------------------------------------------------------------
static inline uint32_t hwtElapsedMs(uint32_t now, uint32_t then) {
  return (uint32_t)(now - then);
}

// ---------------------------------------------------------------------------
// Sizing. 125 samples x 10 bytes = 1250 B payload + 52 B header = 1302 B,
// comfortably inside a 1472-byte UDP datagram, so no IP fragmentation.
// ---------------------------------------------------------------------------
#ifndef HWT_BATCH_SAMPLES
  #define HWT_BATCH_SAMPLES 125
#endif
#ifndef HWT_QUEUE_BATCHES
  #define HWT_QUEUE_BATCHES 48
#endif

typedef struct {
  HwtBatchHeader hdr;
  HwtSample      s[HWT_BATCH_SAMPLES];
} HwtBatch;

static inline uint32_t hwtBatchWireLen(const HwtBatch* b) {
  return (uint32_t)sizeof(HwtBatchHeader) + (uint32_t)b->hdr.nSamples * (uint32_t)sizeof(HwtSample);
}

// Finalize a packet in place: zero the crc field, then checksum header+payload.
static inline void hwtSeal(HwtBatchHeader* h, const uint8_t* payload, uint32_t payloadLen) {
  h->crc32 = 0;
  uint32_t c = hwtCrc32((const uint8_t*)h, (uint32_t)sizeof(HwtBatchHeader), 0);
  if (payloadLen) c = hwtCrc32(payload, payloadLen, c);
  h->crc32 = c;
}

// ---------------------------------------------------------------------------
// HwtCapture — batch assembly plus the bounded ring.
//
// Acquisition calls addSample(). Transport calls popBatch(). Nothing else
// touches the data, and nothing in here can refuse a sample.
// ---------------------------------------------------------------------------
class HwtCapture {
public:
  void begin(uint32_t locoId, uint32_t sessionId, uint32_t nominalPeriodUs) {
    memset(this, 0, sizeof(*this));
    locoId_    = locoId;
    sessionId_ = sessionId;
    periodUs_  = nominalPeriodUs ? nominalPeriodUs : 1;
    havePrev_  = false;
    startBatch();
  }

  // One acquired sample pair. tUs must be monotonic (esp_timer_get_time()).
  void addSample(uint16_t ch0Word, uint16_t ch1Word, uint64_t tUs,
                 uint8_t pwmActual, uint8_t pwmCommanded, uint8_t ctxBase) {
    uint32_t dt = 0;
    uint32_t missed = 0;
    bool late = false;

    if (havePrev_) {
      uint64_t d = tUs - prevUs_;                 // monotonic source: no wrap
      dt = (d > 0xFFFFFFFFull) ? 0xFFFFFFFFu : (uint32_t)d;
      if (dt > maxGapUs_) maxGapUs_ = dt;
      // A gap of two or more nominal periods means slots were NOT acquired.
      // Their sequence numbers are skipped so the absence is explicit; no
      // sample is ever invented to fill them.
      if (dt >= 2u * periodUs_) missed = (dt / periodUs_) - 1u;
      // "Late" is a quarter period off the nominal grid — an observation
      // about cadence, recorded per sample, never acted on.
      late = (dt > periodUs_ + periodUs_ / 4u);
    }
    prevUs_   = tUs;
    havePrev_ = true;

    if (missed) {
      // Skipped slots end the open batch short: a batch is always a
      // contiguous run of really-acquired samples.
      if (n_) flushBatch();
      sampleSeq_    += missed;
      cumMissed_    += missed;
      pendingMissed_ = missed;
    }

    if (n_ == 0) {
      cur_.hdr.firstSampleSeq = sampleSeq_;
      cur_.hdr.t0Us           = tUs;
      cur_.hdr.missedBefore   = pendingMissed_;
      pendingMissed_          = 0;
    }

    HwtSample* s   = &cur_.s[n_];
    s->ch0          = ch0Word;
    s->ch1          = ch1Word;
    s->dtUs         = (dt > 0xFFFFu) ? 0xFFFFu : (uint16_t)dt;
    s->pwmActual    = pwmActual;
    s->pwmCommanded = pwmCommanded;
    s->ctx          = (uint8_t)(ctxBase | (late ? HWT_CTX_LATE : 0));
    s->pad          = 0;

    n_++;
    sampleSeq_++;
    if (n_ >= HWT_BATCH_SAMPLES) flushBatch();
  }

  // Transport side. Returns false when the ring is empty.
  bool popBatch(HwtBatch* out) {
    bool got = false;
    HWT_ENTER_CRITICAL();
    if (count_) {
      memcpy(out, &ring_[tail_], sizeof(HwtBatch));
      tail_ = (uint16_t)((tail_ + 1u) % HWT_QUEUE_BATCHES);
      count_--;
      got = true;
    }
    HWT_EXIT_CRITICAL();
    return got;
  }

  // Statistics. Every one of these also rides in every batch header, so a
  // recorder that never sees a status packet still learns the loss counts.
  uint32_t sampleSeq()      const { return sampleSeq_; }
  uint32_t cumMissed()      const { return cumMissed_; }
  uint32_t cumQueueDrops()  const { return queueDrops_; }
  uint32_t cumBatches()     const { return batchSeq_; }
  uint32_t maxGapUs()       const { return maxGapUs_; }
  uint16_t queued()         const { return count_; }
  uint16_t queueHighWater() const { return highWater_; }
  uint32_t sessionId()      const { return sessionId_; }

  // Fill a header for a non-sample record (anchor, status). Same session,
  // same batchSeq space, so a lost anchor is as visible as a lost batch.
  // Anchors and status records share the batchSeq space, so a lost one is as
  // visible at the receiver as a lost sample batch.
  void fillAuxHeader(HwtBatchHeader* h, uint8_t recType) {
    memset(h, 0, sizeof(*h));
    stampMagic(h);
    h->recType        = recType;
    h->nSamples       = 0;
    h->locoId         = locoId_;
    h->sessionId      = sessionId_;
    h->batchSeq       = nextBatchSeq();
    h->firstSampleSeq = sampleSeq_;
    h->missedBefore   = 0;
    h->cumMissed      = cumMissed_;
    h->cumQueueDrops  = queueDrops_;
    h->maxGapUs       = maxGapUs_;
  }

  // Force the open batch out (used at session end / on operator command).
  void flushPartial() { if (n_) flushBatch(); }

private:
  uint32_t nextBatchSeq() {
    HWT_ENTER_CRITICAL();
    uint32_t v = ++batchSeq_;
    HWT_EXIT_CRITICAL();
    return v;
  }

  void stampMagic(HwtBatchHeader* h) {
    h->magic[0] = HWT_MAGIC0; h->magic[1] = HWT_MAGIC1;
    h->magic[2] = HWT_MAGIC2; h->magic[3] = HWT_MAGIC3;
    h->version  = HWT_FORMAT_VERSION;
  }

  void startBatch() {
    memset(&cur_.hdr, 0, sizeof(cur_.hdr));
    stampMagic(&cur_.hdr);
    cur_.hdr.recType = HWT_REC_SAMPLES;
    n_ = 0;
  }

  void flushBatch() {
    cur_.hdr.nSamples      = n_;
    cur_.hdr.locoId        = locoId_;
    cur_.hdr.sessionId     = sessionId_;
    cur_.hdr.batchSeq      = nextBatchSeq();
    cur_.hdr.cumMissed     = cumMissed_;
    cur_.hdr.cumQueueDrops = queueDrops_;
    cur_.hdr.maxGapUs      = maxGapUs_;
    hwtSeal(&cur_.hdr, (const uint8_t*)cur_.s, (uint32_t)n_ * (uint32_t)sizeof(HwtSample));

    HWT_ENTER_CRITICAL();
    if (count_ >= HWT_QUEUE_BATCHES) {
      // Bounded ring, full: drop the OLDEST batch and count it. The receiver
      // sees the batchSeq gap and the cumQueueDrops step, so nothing is lost
      // quietly. Keeping the newest is deliberate — during a transport outage
      // the waveform happening now is the one worth keeping.
      //
      // REPORTING LAG, by construction: this batch's header was sealed before
      // the push, so the drop it causes appears in the NEXT header (and in the
      // 2 s status record). The batchSeq gap is independent evidence either
      // way, so no loss can hide behind the lag.
      tail_ = (uint16_t)((tail_ + 1u) % HWT_QUEUE_BATCHES);
      count_--;
      queueDrops_++;
    }
    memcpy(&ring_[head_], &cur_, sizeof(HwtBatch));
    head_ = (uint16_t)((head_ + 1u) % HWT_QUEUE_BATCHES);
    count_++;
    if (count_ > highWater_) highWater_ = count_;
    HWT_EXIT_CRITICAL();

    startBatch();
  }

  HwtBatch cur_;
  HwtBatch ring_[HWT_QUEUE_BATCHES];
  uint16_t head_, tail_, count_, highWater_;
  uint16_t n_;

  uint32_t locoId_, sessionId_;
  uint32_t periodUs_;
  uint32_t sampleSeq_, batchSeq_;
  uint32_t cumMissed_, queueDrops_, maxGapUs_, pendingMissed_;
  uint64_t prevUs_;
  bool     havePrev_;
};

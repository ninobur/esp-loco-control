#pragma once

// NSR1 records raw Hall/IR evidence and NAVI's resulting decisions for the
// integrated EWO build. It has no reader and no path back into acquisition,
// navigation, stations, throttle, or safety. The .ino supplies
// critical-section macros on ESP32; host tests use the no-op defaults below.

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../../common/IrMovementWire.h"

#ifndef NAVI_SYNC_ENTER_CRITICAL
#define NAVI_SYNC_ENTER_CRITICAL() do {} while (0)
#define NAVI_SYNC_EXIT_CRITICAL()  do {} while (0)
#endif

namespace navi_sync {

static constexpr char MAGIC[4] = {'N','S','R','1'};
static constexpr uint8_t FORMAT_VERSION = 1;
static constexpr uint8_t REC_HALL = 1;
static constexpr uint8_t REC_IR = 2;
static constexpr uint8_t REC_STATUS = 3;
static constexpr uint8_t REC_NAVI = 4; // EWO extension: NAVI conclusion, not acquisition
static constexpr uint8_t REC_HALL_NATIVE = 5;
static constexpr uint32_t SEQ_NA = 0xFFFFFFFFu;
static constexpr uint8_t MM_NA = 0xFFu;

static constexpr uint8_t HALL_F_DIR_FWD       = 0x01;
static constexpr uint8_t HALL_F_ESTOP         = 0x02;
static constexpr uint8_t HALL_F_AUTO           = 0x04;
static constexpr uint8_t HALL_F_NAV_KNOWN      = 0x08;
static constexpr uint8_t HALL_F_HOLD           = 0x10;
static constexpr uint8_t HALL_F_LOW_VOLT       = 0x20;
static constexpr uint8_t HALL_F_LATE           = 0x40;

static constexpr uint8_t CTX_NAV_KNOWN         = 0x01;
static constexpr uint8_t CTX_AUTO_ENROLLED     = 0x02;
static constexpr uint8_t CTX_AUTO_RUNNING      = 0x04;
static constexpr uint8_t CTX_ESTOP             = 0x08;
static constexpr uint8_t CTX_LOW_VOLT          = 0x10;
static constexpr uint8_t CTX_HOLD              = 0x20;

struct Context {
  uint8_t navMm = MM_NA;
  int8_t navDir = 0;
  uint8_t stationPhase = 0;
  uint8_t flags = 0;
};

// One NSR1 header precedes every record.  t0Us is always the locomotive's
// esp_timer_get_time() clock.  t0Ms is retained only as a convenient join to
// existing MQTT/serial diagnostics; it is never used to reconstruct timing.
struct __attribute__((packed)) Header {
  char magic[4];
  uint8_t version;
  uint8_t recType;
  uint16_t nItems;
  uint32_t locoId;
  uint32_t sessionId;
  uint32_t batchSeq;
  uint32_t firstItemSeq;
  uint32_t t0Ms;
  uint64_t t0Us;
  uint64_t locoBootId;
  uint32_t ringDrops;
  uint8_t navMm;
  int8_t navDir;
  uint8_t stationPhase;
  uint8_t ctxFlags;
  uint32_t crc32;
};

// Five ordered ADC results are carried only as a wire batching format. Each
// result is delivered separately to NAVI. median is INT16_MIN: no acquisition
// statistic exists and no detector consumes this recorder field.
struct __attribute__((packed)) HallSample {
  uint16_t dtUs;
  uint16_t raw[5];
  int16_t median;
  uint8_t pwmActual;
  uint8_t pwmCommanded;
  uint8_t flags;
  uint8_t pad;
};

// The IR wire snapshot is copied intact. rxUs is the local ESP-NOW receive
// time, not a reconstructed time. TX capturedUs and sequence remain in wire.
struct __attribute__((packed)) IrSnapshot {
  uint64_t rxUs;
  uint8_t sourceMac[6];
  uint8_t acceptKind;      // legacy NSR1 field; 1 = valid type-5 wire envelope
  uint8_t healthFault;
  uint8_t readiness;
  uint8_t pwmActual;
  uint8_t pwmCommanded;
  uint8_t navMm;
  int8_t navDir;
  uint8_t stationPhase;
  uint8_t ctxFlags;
  ir_movement::WireSnapshot wire;
};

struct __attribute__((packed)) Status {
  uint32_t tMs;
  uint64_t tUs;
  uint32_t hallSamples;
  uint32_t hallRingDrops;
  uint32_t irAccepted;
  uint32_t irRingDrops;
  uint32_t irInputQueueDrops;
  uint32_t udpFailures;
  uint32_t datagramsSent;
  uint32_t maxHallGapUs;
  uint16_t hallHighWater;
  uint16_t irHighWater;
  uint8_t wifiConnected;
  uint8_t mqttConnected;
  uint8_t pad[2];
};

struct __attribute__((packed)) StatusWire {
  Header header;
  Status status;
};

// EWO decision trace. Kind values follow EwoEventKind in NaviIntegratedCore.h.
// The event snapshots its own context at decision time, not when UDP drains.
struct __attribute__((packed)) NaviSnapshot {
  uint64_t tUs;
  uint64_t irUm;
  uint64_t openingIrUm;
  uint32_t hallSerial;
  uint32_t openingSerial;
  uint32_t hallQueueDrops;
  uint32_t irQueueDrops;
  int16_t median;
  int16_t reference;
  uint8_t kind;
  uint8_t mm;
  uint8_t target;
  int8_t direction;
  uint8_t degraded;
  uint8_t positionReliable;
  uint8_t spatialPhase;
};
struct __attribute__((packed)) NaviWire {
  Header header;
  NaviSnapshot snapshot;
};
struct __attribute__((packed)) NativeHallItem {
  uint32_t serial;
  uint64_t tUs;
  int16_t raw;
  uint8_t pwm;
  uint8_t direction;
};
static constexpr uint16_t NATIVE_HALL_BATCH_ITEMS = 48;
struct __attribute__((packed)) NativeHallWire {
  Header header;
  NativeHallItem items[NATIVE_HALL_BATCH_ITEMS];
};

static_assert(sizeof(Header) == 56, "NSR1 header layout changed");
static_assert(sizeof(HallSample) == 18, "NSR1 Hall sample layout changed");
static_assert(sizeof(IrSnapshot) == 133, "NSR1 IR snapshot layout changed");
static_assert(sizeof(Status) == 52, "NSR1 status layout changed");
static_assert(sizeof(StatusWire) == 108, "NSR1 status wire layout changed");
static_assert(sizeof(NaviSnapshot) == 51, "EWO NAVI snapshot layout changed");
static_assert(sizeof(NativeHallItem) == 16, "EWO native Hall item layout changed");

static constexpr uint16_t HALL_BATCH_SAMPLES = 48;  // 48 kHz-equivalent ms
static constexpr uint16_t HALL_RING_BATCHES = 24;   // nominally >1 s of Hall
static constexpr uint16_t IR_RING_RECORDS = 48;     // nominally >4 s of IR
static constexpr uint16_t NAVI_RING_RECORDS = 64;
static constexpr uint16_t NATIVE_HALL_RING_BATCHES = 24;

static const uint32_t CRC_NIBBLE[16] = {
  0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
  0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
  0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
  0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu
};

inline uint32_t crc32(const uint8_t* data, uint32_t len, uint32_t crc = 0) {
  crc = ~crc;
  for (uint32_t i = 0; i < len; ++i) {
    crc ^= data[i];
    crc = (crc >> 4) ^ CRC_NIBBLE[crc & 0x0F];
    crc = (crc >> 4) ^ CRC_NIBBLE[crc & 0x0F];
  }
  return ~crc;
}

inline uint32_t recordCrc(const Header& h, const uint8_t* payload,
                          uint32_t payloadLen) {
  Header copy = h;
  copy.crc32 = 0;
  uint32_t c = crc32(reinterpret_cast<const uint8_t*>(&copy), sizeof(copy));
  return crc32(payload, payloadLen, c);
}

struct __attribute__((packed)) HallWire {
  Header header;
  HallSample samples[HALL_BATCH_SAMPLES];
};

struct __attribute__((packed)) IrWire {
  Header header;
  IrSnapshot snapshot;
};

class Recorder {
 public:
  void begin(uint32_t locoId, uint32_t sessionId, uint64_t locoBootId) {
    NAVI_SYNC_ENTER_CRITICAL();
    locoId_ = locoId; sessionId_ = sessionId; locoBootId_ = locoBootId;
    hallHead_ = hallTail_ = hallCount_ = 0;
    irHead_ = irTail_ = irCount_ = 0;
    naviHead_ = naviTail_ = naviCount_ = 0;
    nativeHead_ = nativeTail_ = nativeCount_ = 0;
    hallDrops_ = irDrops_ = hallSamples_ = irAccepted_ = 0;
    maxHallGapUs_ = 0; lastHallUs_ = previousHallUs_ = 0;
    lastDtUs_ = 0; hallSeq_ = 0;
    hallBatchSeq_ = irBatchSeq_ = statusSequence_ = 0;
    naviSequence_ = naviDrops_ = 0;
    nativeBatchSeq_ = nativeDrops_ = nativeSamples_ = 0;
    nativeCurrent_.header.nItems = 0;
    hallCurrent_.header.nItems = 0;
    hallHighWater_ = irHighWater_ = 0;
    NAVI_SYNC_EXIT_CRITICAL();
  }

  void addHall(uint64_t tUs, uint32_t tMs, const HallSample& sample,
              const Context& context) {
    // hallTask is the sole writer of the current batch. The completed batch
    // handoff is protected; no wait or allocation occurs at the 1 kHz tap.
    if (lastHallUs_ && tUs >= lastHallUs_) {
      const uint64_t gap = tUs - lastHallUs_;
      if (gap > maxHallGapUs_) maxHallGapUs_ =
        gap > 0xFFFFFFFFULL ? 0xFFFFFFFFu : (uint32_t)gap;
    }
    lastHallUs_ = tUs;
    HallSample copy = sample;
    copy.dtUs = hallSamples_ && lastDtUs_ ? lastDtUs_ : 0;
    // The caller supplies the native timestamp; compute the actual delta here
    // rather than assuming the FreeRTOS 1 ms schedule was exact.
    if (hallSamples_ && previousHallUs_) {
      const uint64_t d = tUs - previousHallUs_;
      copy.dtUs = d > 65535ULL ? 65535u : (uint16_t)d;
      if (d > 1250ULL) copy.flags = (uint8_t)(copy.flags | HALL_F_LATE);
    }
    previousHallUs_ = tUs;
    lastDtUs_ = copy.dtUs;
    copy.pad = 0;

    if (!hallCurrent_.header.nItems) {
      initHeader(hallCurrent_.header, REC_HALL, tMs, tUs, hallBatchSeq_,
                 hallSeq_, context, hallDrops_);
    }
    const uint16_t index = hallCurrent_.header.nItems++;
    hallCurrent_.samples[index] = copy;
    ++hallSeq_;
    ++hallSamples_;
    if (hallCurrent_.header.nItems == HALL_BATCH_SAMPLES) sealHall();
  }

  // Call after technical wire validation; no IR movement qualification is
  // performed here. The complete type-5 snapshot is copied as evidence.
  void addIr(uint64_t rxUs, const uint8_t* sourceMac, uint8_t acceptKind,
             const ir_movement::WireSnapshot& wire, uint8_t healthFault,
             uint8_t readiness, uint8_t pwmActual, uint8_t pwmCommanded,
             const Context& context) {
    IrWire out{};
    const uint32_t batchSeq = irBatchSeq_++;
    initHeader(out.header, REC_IR, (uint32_t)(rxUs / 1000ULL), rxUs,
               batchSeq, wire.sequence, context, irDrops_);
    out.header.nItems = 1;
    IrSnapshot& r = out.snapshot;
    r.rxUs = rxUs;
    if (sourceMac) memcpy(r.sourceMac, sourceMac, sizeof(r.sourceMac));
    r.acceptKind = acceptKind; r.healthFault = healthFault;
    r.readiness = readiness; r.pwmActual = pwmActual;
    r.pwmCommanded = pwmCommanded; r.navMm = context.navMm;
    r.navDir = context.navDir; r.stationPhase = context.stationPhase;
    r.ctxFlags = context.flags; r.wire = wire;
    out.header.crc32 = recordCrc(out.header,
      reinterpret_cast<const uint8_t*>(&out.snapshot), sizeof(out.snapshot));

    NAVI_SYNC_ENTER_CRITICAL();
    ++irAccepted_;
    if (irCount_ == IR_RING_RECORDS) {
      ++irDrops_; irTail_ = (uint16_t)((irTail_ + 1) % IR_RING_RECORDS);
      --irCount_;
    }
    irRing_[irHead_] = out;
    irHead_ = (uint16_t)((irHead_ + 1) % IR_RING_RECORDS);
    ++irCount_;
    if (irCount_ > irHighWater_) irHighWater_ = irCount_;
    NAVI_SYNC_EXIT_CRITICAL();
  }

  void addNavi(const NaviSnapshot& snapshot, const Context& context) {
    NaviWire out{};
    initHeader(out.header, REC_NAVI, uint32_t(snapshot.tUs / 1000),
               snapshot.tUs, naviSequence_++, snapshot.hallSerial,
               context, naviDrops_);
    out.header.nItems = 1;
    out.snapshot = snapshot;
    out.header.crc32 = recordCrc(out.header,
      reinterpret_cast<const uint8_t*>(&out.snapshot), sizeof(out.snapshot));
    NAVI_SYNC_ENTER_CRITICAL();
    if (naviCount_ == NAVI_RING_RECORDS) {
      ++naviDrops_;
      naviTail_ = (naviTail_ + 1) % NAVI_RING_RECORDS;
      --naviCount_;
    }
    naviRing_[naviHead_] = out;
    naviHead_ = (naviHead_ + 1) % NAVI_RING_RECORDS;
    ++naviCount_;
    NAVI_SYNC_EXIT_CRITICAL();
  }

  void addNativeHall(const NativeHallItem& item, const Context& context) {
    if (!nativeCurrent_.header.nItems)
      initHeader(nativeCurrent_.header, REC_HALL_NATIVE,
                 uint32_t(item.tUs / 1000), item.tUs, nativeBatchSeq_,
                 item.serial, context, nativeDrops_);
    nativeCurrent_.items[nativeCurrent_.header.nItems++] = item;
    ++nativeSamples_;
    if (nativeCurrent_.header.nItems == NATIVE_HALL_BATCH_ITEMS)
      sealNativeHall();
  }

  bool popHall(HallWire& out) {
    NAVI_SYNC_ENTER_CRITICAL();
    if (!hallCount_) { NAVI_SYNC_EXIT_CRITICAL(); return false; }
    out = hallRing_[hallTail_];
    hallTail_ = (uint16_t)((hallTail_ + 1) % HALL_RING_BATCHES);
    --hallCount_;
    NAVI_SYNC_EXIT_CRITICAL();
    return true;
  }

  bool popIr(IrWire& out) {
    NAVI_SYNC_ENTER_CRITICAL();
    if (!irCount_) { NAVI_SYNC_EXIT_CRITICAL(); return false; }
    out = irRing_[irTail_];
    irTail_ = (uint16_t)((irTail_ + 1) % IR_RING_RECORDS);
    --irCount_;
    NAVI_SYNC_EXIT_CRITICAL();
    return true;
  }

  bool popNavi(NaviWire& out) {
    NAVI_SYNC_ENTER_CRITICAL();
    if (!naviCount_) { NAVI_SYNC_EXIT_CRITICAL(); return false; }
    out = naviRing_[naviTail_];
    naviTail_ = (naviTail_ + 1) % NAVI_RING_RECORDS;
    --naviCount_;
    NAVI_SYNC_EXIT_CRITICAL();
    return true;
  }
  bool popNativeHall(NativeHallWire& out) {
    NAVI_SYNC_ENTER_CRITICAL();
    if (!nativeCount_) { NAVI_SYNC_EXIT_CRITICAL(); return false; }
    out = nativeRing_[nativeTail_];
    nativeTail_ = (nativeTail_ + 1) % NATIVE_HALL_RING_BATCHES;
    --nativeCount_;
    NAVI_SYNC_EXIT_CRITICAL();
    return true;
  }

  StatusWire makeStatus(uint32_t tMs, uint64_t tUs, const Context& context,
                        const Status& status) const {
    StatusWire out{};
    const uint32_t sequence = statusSequence_++;
    initHeader(out.header, REC_STATUS, tMs, tUs, sequence, SEQ_NA,
               context, hallDrops_);
    out.header.nItems = 1;
    out.status = status;
    out.header.crc32 = recordCrc(out.header,
      reinterpret_cast<const uint8_t*>(&out.status), sizeof(out.status));
    return out;
  }

  uint32_t locoId() const { return locoId_; }
  uint32_t sessionId() const { return sessionId_; }
  uint64_t locoBootId() const { return locoBootId_; }
  uint32_t hallSamples() const { return hallSamples_; }
  uint32_t hallDrops() const { return hallDrops_; }
  uint32_t irAccepted() const { return irAccepted_; }
  uint32_t irDrops() const { return irDrops_; }
  uint32_t naviDrops() const { return naviDrops_; }
  uint32_t nativeDrops() const { return nativeDrops_; }
  uint32_t nativeSamples() const { return nativeSamples_; }
  uint32_t maxHallGapUs() const { return maxHallGapUs_; }
  uint16_t hallHighWater() const { return hallHighWater_; }
  uint16_t irHighWater() const { return irHighWater_; }
  uint16_t hallDepth() const { return hallCount_; }
  uint16_t irDepth() const { return irCount_; }

 private:
  void initHeader(Header& h, uint8_t type, uint32_t tMs, uint64_t tUs,
                  uint32_t batchSeq, uint32_t firstSeq, const Context& c,
                  uint32_t drops) const {
    memcpy(h.magic, MAGIC, sizeof(h.magic)); h.version = FORMAT_VERSION;
    h.recType = type; h.nItems = 0; h.locoId = locoId_; h.sessionId = sessionId_;
    h.batchSeq = batchSeq; h.firstItemSeq = firstSeq; h.t0Ms = tMs;
    h.t0Us = tUs; h.locoBootId = locoBootId_; h.ringDrops = drops;
    h.navMm = c.navMm; h.navDir = c.navDir; h.stationPhase = c.stationPhase;
    h.ctxFlags = c.flags; h.crc32 = 0;
  }

  void sealHall() {
    hallCurrent_.header.crc32 = recordCrc(
      hallCurrent_.header, reinterpret_cast<const uint8_t*>(hallCurrent_.samples),
      (uint32_t)hallCurrent_.header.nItems * sizeof(HallSample));
    NAVI_SYNC_ENTER_CRITICAL();
    if (hallCount_ == HALL_RING_BATCHES) {
      ++hallDrops_; hallTail_ = (uint16_t)((hallTail_ + 1) % HALL_RING_BATCHES);
      --hallCount_;
    }
    hallRing_[hallHead_] = hallCurrent_;
    hallHead_ = (uint16_t)((hallHead_ + 1) % HALL_RING_BATCHES);
    ++hallCount_;
    if (hallCount_ > hallHighWater_) hallHighWater_ = hallCount_;
    NAVI_SYNC_EXIT_CRITICAL();
    hallCurrent_.header.nItems = 0;
    ++hallBatchSeq_;
  }
  void sealNativeHall() {
    nativeCurrent_.header.crc32 = recordCrc(
      nativeCurrent_.header,
      reinterpret_cast<const uint8_t*>(nativeCurrent_.items),
      uint32_t(nativeCurrent_.header.nItems) * sizeof(NativeHallItem));
    NAVI_SYNC_ENTER_CRITICAL();
    if (nativeCount_ == NATIVE_HALL_RING_BATCHES) {
      ++nativeDrops_;
      nativeTail_ = (nativeTail_ + 1) % NATIVE_HALL_RING_BATCHES;
      --nativeCount_;
    }
    nativeRing_[nativeHead_] = nativeCurrent_;
    nativeHead_ = (nativeHead_ + 1) % NATIVE_HALL_RING_BATCHES;
    ++nativeCount_;
    NAVI_SYNC_EXIT_CRITICAL();
    nativeCurrent_.header.nItems = 0;
    ++nativeBatchSeq_;
  }

  uint32_t locoId_ = 0, sessionId_ = 0;
  uint64_t locoBootId_ = 0, previousHallUs_ = 0, lastHallUs_ = 0;
  uint16_t lastDtUs_ = 0;
  uint32_t hallSeq_ = 0, hallBatchSeq_ = 0, irBatchSeq_ = 0;
  uint32_t naviSequence_ = 0, naviDrops_ = 0;
  uint32_t nativeBatchSeq_ = 0, nativeDrops_ = 0, nativeSamples_ = 0;
  mutable uint32_t statusSequence_ = 0;
  uint32_t hallSamples_ = 0, hallDrops_ = 0, irAccepted_ = 0, irDrops_ = 0;
  uint32_t maxHallGapUs_ = 0;
  uint16_t hallHead_ = 0, hallTail_ = 0, hallCount_ = 0, hallHighWater_ = 0;
  uint16_t irHead_ = 0, irTail_ = 0, irCount_ = 0, irHighWater_ = 0;
  uint16_t naviHead_ = 0, naviTail_ = 0, naviCount_ = 0;
  uint16_t nativeHead_ = 0, nativeTail_ = 0, nativeCount_ = 0;
  HallWire hallCurrent_{};
  HallWire hallRing_[HALL_RING_BATCHES]{};
  IrWire irRing_[IR_RING_RECORDS]{};
  NaviWire naviRing_[NAVI_RING_RECORDS]{};
  NativeHallWire nativeCurrent_{};
  NativeHallWire nativeRing_[NATIVE_HALL_RING_BATCHES]{};
};

} // namespace navi_sync

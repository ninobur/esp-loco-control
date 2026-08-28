// ============================================================================
// IRSpeedWire.h — the Stage-A IR speed wire contract, shared verbatim by the
// sender (IR_TEST_CAR_ESPNOW) and the receiver (QUORUM IR Test A).
//
// FROZEN per docs/IR_TEST_CAR_ESPNOW_FIRMWARE_SPEC.md §5. Exactly 72 bytes,
// packed, little-endian on both ESP32 endpoints. Neither endpoint duplicates
// this struct; both include this header, so the contract cannot drift in one
// place silently (the CtoPeerPacket lesson, applied from birth).
//
// CANONICAL SPEED ENCODING — structural, so an invalid measurement can never
// be mistaken for a measured stop (decisions 0005 + 0036):
//   validity == IR_V_VALID   : speedMmpsX100 is the measured value, and is
//                              never IR_SPEED_INVALID_X100;
//   validity == IR_V_STOPPED : speedMmpsX100 == 0, exactly;
//   anything else            : speedMmpsX100 == IR_SPEED_INVALID_X100.
//
// The validity enum extends the proven IR_SPEED_LOCAL_1_2 acquisition states
// (0-4, unchanged) with one WIRE-ONLY state: STOPPED (5), derived by the
// sender's packet-production layer when cumulative pulses have not advanced
// for IR_STOPPED_MS. It is an observation, not proof of cause — a blinded
// sensor on a moving wheel can imitate it — which is why 0036 forbids any
// future governor from raising PWM in response to a zero.
// ============================================================================
#pragma once
#include <stdint.h>

static constexpr uint8_t  IR_SPEED_MAGIC        = 0xC6;
static constexpr uint8_t  IR_SPEED_VERSION      = 1;
static constexpr uint32_t IR_SENSOR_ID_IR01     = 0x49523031UL;   // "IR01"
static constexpr uint32_t IR_SPEED_INVALID_X100 = 0xFFFFFFFFUL;

// Wire validity enum — CLOSED. 0-4 are IR_SPEED_LOCAL_1_2's acquisition
// states with their exact meanings; 5 exists only on the wire.
enum IrWireValidity : uint8_t {
  IR_V_INVALID_CONTRAST = 0,
  IR_V_MARGINAL         = 1,
  IR_V_REACQUIRING      = 2,
  IR_V_VALID            = 3,
  IR_V_STALE            = 4,
  IR_V_STOPPED          = 5,   // wire-only: no pulse progress for IR_STOPPED_MS
};
static constexpr uint8_t IR_V_MAX = IR_V_STOPPED;

struct __attribute__((packed)) IrSpeedPacketV1 {
  uint8_t  magic;                     // 0xC6
  uint8_t  version;                   // 1
  uint16_t bytes;                     // always 72 — length rides inside too
  uint32_t sensorId;                  // IR_SENSOR_ID_IR01
  uint32_t targetLocoId;              // intended recipient (9950012)
  uint32_t bootId;                    // nonzero esp_random() per sensor boot
  uint32_t txSequence;                // radio-report counter — NEVER a pulse count
  uint32_t capturedMs;                // sensor millis() at snapshot — sensor clock,
                                      // never subtracted from the receiver's millis()
  uint32_t completedPulses;           // cumulative physical pulses — THE distance fact
  uint32_t lastIntervalMs;
  uint32_t speedMmpsX100;             // canonical encoding above
  uint16_t opticalSpan;
  uint8_t  validity;                  // IrWireValidity
  uint8_t  reserved;                  // must be zero
  // eight monotonic health/radio counters, 32 bytes
  uint32_t sampleMissedSlots;
  uint32_t openAborts;
  uint32_t contrastInvalidEpisodes;
  uint32_t saturatedSamples;
  uint32_t sampleMaxGapUs;
  uint32_t txAttempts;                // intentions (the 1.16Ra lesson: named as such)
  uint32_t txImmediateErrors;         // esp_now_send() returned non-OK
  uint32_t txDeliveryFailures;        // send callback reported non-success
};
static_assert(sizeof(IrSpeedPacketV1) == 72, "IR wire size changed");

// Normative field offsets (spec §5). Pinned so a compiler-padding surprise on
// either endpoint fails the build rather than corrupting the field split.
static_assert(offsetof(IrSpeedPacketV1, bytes)            ==  2, "IR wire offset drift");
static_assert(offsetof(IrSpeedPacketV1, sensorId)         ==  4, "IR wire offset drift");
static_assert(offsetof(IrSpeedPacketV1, targetLocoId)     ==  8, "IR wire offset drift");
static_assert(offsetof(IrSpeedPacketV1, bootId)           == 12, "IR wire offset drift");
static_assert(offsetof(IrSpeedPacketV1, txSequence)       == 16, "IR wire offset drift");
static_assert(offsetof(IrSpeedPacketV1, capturedMs)       == 20, "IR wire offset drift");
static_assert(offsetof(IrSpeedPacketV1, completedPulses)  == 24, "IR wire offset drift");
static_assert(offsetof(IrSpeedPacketV1, lastIntervalMs)   == 28, "IR wire offset drift");
static_assert(offsetof(IrSpeedPacketV1, speedMmpsX100)    == 32, "IR wire offset drift");
static_assert(offsetof(IrSpeedPacketV1, opticalSpan)      == 36, "IR wire offset drift");
static_assert(offsetof(IrSpeedPacketV1, validity)         == 38, "IR wire offset drift");
static_assert(offsetof(IrSpeedPacketV1, reserved)         == 39, "IR wire offset drift");
static_assert(offsetof(IrSpeedPacketV1, sampleMissedSlots)== 40, "IR wire offset drift");
static_assert(offsetof(IrSpeedPacketV1, txDeliveryFailures)==68, "IR wire offset drift");

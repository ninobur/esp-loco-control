// ============================================================================
// HALL_WAVEFORM_TEST_0_1
// DIAGNOSTIC ONLY — NO NAVIGATION AUTHORITY
//
// INVESTIGATORY AND UNAPPROVED. Not operational firmware. Not QUORUM. Not a
// Module C successor. It is a data recorder that happens to be able to drive
// the locomotive, because the waveforms only exist while the locomotive moves.
//
// BASE
//   LL_PM_Loco_ModuleC_v0_4_1.ino (operator-selected). Kept from that base:
//   Blynk throttle, Blynk direction, motor ramping, local E-STOP, safe boot
//   at PWM 0, and Otto/Toby profile support.
//
//   CHANGED from that base — CODEX safety review, 2026-08-24: the base wrote
//   MOTOR_DIR_PIN unconditionally on a direction command, so a reversal could
//   be commanded while PWM was still applied. Every direction request — FORWARD,
//   REVERSE, and NEUTRAL alike — is now REFUSED (both the Blynk control and
//   the DIR command) unless the motor is fully at rest (rampCurrent == 0 and
//   rampTarget == 0): matches established operator behavior (Blynk has
//   refused every direction change while moving, F/R/N alike, for about a
//   year; the Flask console's own omission of NEUTRAL never changed that). A
//   refusal changes nothing — not manualDirection, not the pin, not PWM —
//   and does not itself begin stopping the locomotive. Bring PWM to zero
//   with STOP or E-STOP first, then reissue the direction request. Selecting
//   NEUTRAL while stopped sets manualDirection and blocks new throttle until
//   FORWARD or REVERSE is chosen, exactly as before; it still never touches
//   the pin and is still never an automatic stop. Brake (VPIN_BRAKE) was
//   never in the preserve list above; the base's silent accept-and-ignore is
//   replaced with an audible refusal and a reset control, so the app cannot
//   show Brake as engaged when nothing is stopping the locomotive. Use STOP
//   or E-STOP.
//
//   DELETED from that base, deliberately and completely:
//     - PROTECTED / ACC / AOP modes and the mode framework
//     - block decoding (decodeBlock, BlockId, block labels)
//     - magnet-pair navigation (the two-magnet state machine)
//     - pre-acquisition reverse lockout
//     - reversal detection and observation
//     - CTO / dispatcher packet behaviour, MSG_TYPE_BLOCK / MSG_TYPE_STATUS
//       broadcasts, and ESP-NOW entirely (transport here is UDP)
//     - every Hall-derived influence on motor control, of any kind
//
// WHAT IT RECORDS
//   The Hall channel, continuously: one raw ADC reading per slot at a nominal
//   1 kHz, timestamped in microseconds, with the motor context of that slot,
//   streamed as bounded binary batches to a host recorder over UDP.
//
//   ONE SENSOR (operator direction, 2026-08-23). The record carries a second
//   channel slot, flagged absent, and HWT_SECOND_CHANNEL 1 turns it on — a
//   clean extension, not a promise about hardware that is not installed.
//
//   NO averaging. NO amplitude threshold on what gets recorded. NO CFAR, no
//   matched filter, no dead time, no event rejection, no onboard
//   classification. Every acquired sample is transmitted or explicitly
//   accounted as lost.
//
// THE ONE ANNOTATION
//   Each sample carries two bits per channel saying what the OLD Module C
//   threshold rule (deadband 120 + margin 40, with its enter/exit
//   hysteresis) WOULD have called it. It is a label travelling beside the
//   measurement. It gates nothing: not recording, not transport, not the
//   motor. Offline analysis is free to ignore it, and should not treat it as
//   ground truth.
//
// GROUND TRUTH
//   Only operator anchors. Not the old block decoder, not QUORUM's declared
//   position, not an inferred event count. Anchors may be inserted as often
//   as the operator likes, mid-lap, from Blynk, the host recorder, or serial.
//
// HARDWARE
//   ESP32. Hall A on GPIO 33 (ADC1_CH5) — the existing sensor, unchanged and
//   the only one this build reads.
//
//   The disabled second channel is pinned to GPIO 35 (ADC1_CH7), NOT GPIO 34:
//   34 is the IR wheel sensor in IR_DIAG, IR_SCOPE, IR_SPEED_LOCAL, IR_TEST
//   and Spoke_IR_RSSI_survey, and QUORUM's pin block warns about exactly this
//   confusion. Both are ADC1 because ADC2 cannot be read with WiFi up. GPIO 35
//   is UNVERIFIED on these locomotives — no sensor is installed there and this
//   build never reads it. Motor PWM/DIR per LL_LocoConfig.h (PWM 4, DIR 2).
//
// OPERATING
//   See README.md. Nothing moves at boot; every fixed-PWM step needs an
//   explicit GO; E-STOP overrides everything, always.
// ============================================================================

#include "LL_LocoConfig.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <BlynkSimpleEsp32.h>
#include <esp_timer.h>
#include <esp_random.h>

// The capture engine needs a real critical section on target; define these
// BEFORE including it so the host-test defaults are overridden.
static portMUX_TYPE hwtMux = portMUX_INITIALIZER_UNLOCKED;
#define HWT_ENTER_CRITICAL() portENTER_CRITICAL(&hwtMux)
#define HWT_EXIT_CRITICAL()  portEXIT_CRITICAL(&hwtMux)
#include "HallCapture.h"
#include "DirectionGate.h"

// ============================================================================
// BUILD CONFIGURATION — the whole instrument's tunables, in one block
// ============================================================================
#define SKETCH_NAME          "HALL_WAVEFORM_TEST_0_1"

// Second Hall channel. OPERATOR DIRECTION 2026-08-23: ONE sensor only, so
// this is 0 and channel B is not read, not wired and not assumed to exist.
// Setting it to 1 is the single compile-time change that enables the second
// channel; the record layout already carries channel B's slot with its
// "present" bit clear, so no format, decoder or plot change is needed when
// that day comes.
#define HWT_SECOND_CHANNEL   0

static const int HALL_PIN_A = 33;    // installed sensor (ADC1_CH5)
static const int HALL_PIN_B = 35;    // second channel (ADC1_CH7) — see header

// Acquisition. 1000 us nominal slot = 1 kHz per sensor. Both channels are
// read inside one slot, so their alignment is structural.
static const uint32_t HWT_PERIOD_US = 1000;

// Host recorder (tools/hwt_receiver.py). The Pi at 192.168.68.142 per
// docs/STATUS.md §11; change here or by the SETHOST command.
#define HWT_HOST_DEFAULT     "192.168.68.142"
static const uint16_t HWT_HOST_PORT  = 47600;   // batches out
static const uint16_t HWT_LOCAL_PORT = 47601;   // operator commands in

// Bench fallback: 1 = also mirror every batch as hex over Serial. Off by
// default; it costs far more time per batch than UDP and is for tethered
// bench work only. It never blocks acquisition either way.
#define HWT_SERIAL_STREAM    0

// The OLD Module C threshold rule, reproduced verbatim for annotation only.
// (The current production profile values are HALL_DEADBAND_COUNTS 25 /
// HALL_ENTRY_MARGIN_COUNTS 13; set HWT_ANN_USE_PROFILE to 1 to annotate with
// those instead. Neither choice changes one recorded sample.)
#define HWT_ANN_USE_PROFILE  0
#if HWT_ANN_USE_PROFILE
  static const int ANN_DEADBAND = HALL_DEADBAND_COUNTS;
  static const int ANN_MARGIN   = HALL_ENTRY_MARGIN_COUNTS;
#else
  static const int ANN_DEADBAND = 120;   // Module C DEFAULT_DEADBAND_COUNTS
  static const int ANN_MARGIN   = 40;    // Module C TRIGGER_MARGIN_COUNTS
#endif

static const uint32_t CALIBRATION_MS   = 2000UL;
static const uint32_t STATUS_PERIOD_MS = 2000UL;

// Programmed fixed-PWM test sequence (operator-stepped, never automatic).
static const uint8_t TEST_PWM_STEPS[] = {50, 60, 70, 80, 90, 100, 110, 120};
static const uint8_t TEST_PWM_STEP_N  = sizeof(TEST_PWM_STEPS);

// ============================================================================
// BLYNK VIRTUAL PINS
// V0/V1/V2/V3 keep their base meaning (direction, throttle, E-stop, brake).
// The rest are this instrument's.
// ============================================================================
#define VPIN_FIXED_PWM       V15   // numeric: arm a fixed PWM (does NOT move)
#define VPIN_GO              V16   // switch: 1 = GO to armed PWM, 0 = STOP
#define VPIN_SEQ_NEXT        V17   // button: arm the next sequence step
#define VPIN_ANCHOR          V18   // button: insert an anchor now
#define VPIN_ANCHOR_TEXT     V19   // string: the anchor label
#define VPIN_STATUS_LABEL    V11   // reused from the base's warning label

// ============================================================================
// STATE
// ============================================================================
// --- motor (the ONLY code allowed to touch the motor lives in this section
// --- and in serviceRamp(); the recorder never calls into it) ---
static volatile int rampCurrent = 0;
static volatile int rampTarget  = 0;
static uint32_t lastRampMs    = 0;
static volatile int  manualDirection   = DIRECTION_FORWARD;
static volatile bool localEStopEngaged = false;

// --- fixed-PWM test driving ---
static volatile bool fixedMode  = false;
static volatile bool seqRunning = false;   // a step is under way (GO given)
static uint8_t  fixedTarget   = 0;       // armed PWM, awaiting GO
static int8_t   seqIndex      = -1;      // -1 = sequence not armed

// --- capture ---
static HwtCapture   cap;
static uint32_t     sessionId = 0;
static TaskHandle_t samplerHandle = nullptr;
static volatile uint32_t samplerLoops = 0;
static volatile uint32_t samplerMaxSlotUs = 0;   // worst in-slot work time

// --- annotation state (per channel, Module C classifyHall hysteresis) ---
static int baselineA = -1, baselineB = -1;
static int annNorthEnterA = 0, annNorthExitA = 0, annSouthEnterA = 0, annSouthExitA = 0;
static int annNorthEnterB = 0, annNorthExitB = 0, annSouthEnterB = 0, annSouthExitB = 0;
static uint8_t annPrevA = HWT_ANN_NONE, annPrevB = HWT_ANN_NONE;

// --- transport ---
static WiFiUDP   udp;
static IPAddress hostIp;
static bool      hostIpValid = false;
static uint32_t  udpSendFailures = 0;
static uint32_t  anchorCounter   = 0;
static char      anchorText[40]  = "MARK";

// --- operator commands cross from the network/serial side to the control
// --- side as values on a queue. The network task never writes the motor. ---
typedef struct { char line[64]; } HwtCmd;
static QueueHandle_t cmdQueue = nullptr;

// Anchor and status records are BUILT on the control side and SENT by the
// network task. One task owns the UDP socket; that is the whole rule.
typedef struct { uint16_t len; uint8_t buf[128]; } HwtAux;
static QueueHandle_t auxQueue = nullptr;
static uint32_t auxDrops = 0;

// ============================================================================
// ANCHOR AND STATUS PAYLOADS
// ============================================================================
typedef struct __attribute__((packed)) {
  uint32_t anchorId;
  uint32_t sampleSeq;
  uint64_t tUs;
  uint8_t  dir;
  uint8_t  pwmActual;
  uint8_t  pwmCommanded;
  uint8_t  textLen;
  char     text[40];
} HwtAnchorPayload;

typedef struct __attribute__((packed)) {
  uint32_t uptimeMs;
  uint32_t sampleSeq;
  uint32_t cumMissed;
  uint32_t cumQueueDrops;
  uint32_t cumBatches;
  uint32_t maxGapUs;
  uint32_t measuredMilliHz;   // measured cadence x1000, from sample counting
  uint32_t freeHeap;
  uint32_t udpSendFailures;
  uint32_t maxSlotUs;
  uint16_t queueHighWater;
  uint16_t baselineA;
  uint16_t baselineB;
  uint8_t  channels;
  uint8_t  dir;
  uint8_t  pwmActual;
  uint8_t  pwmCommanded;
  uint8_t  estop;
  uint8_t  fixedMode;
  uint8_t  seqRunning;
} HwtStatusPayload;

// ============================================================================
// TIMESTAMP HELPER (base sketch's log format)
// ============================================================================
static String ts() {
  uint32_t ms = millis(), s = ms / 1000, m = s / 60, h = m / 60;
  s %= 60; m %= 60;
  char buf[16]; sprintf(buf, "%02lu:%02lu:%02lu",
                        (unsigned long)h, (unsigned long)m, (unsigned long)s);
  return String(buf);
}

static const char* dirName(int d) {
  return d == DIRECTION_FORWARD ? "FWD" : d == DIRECTION_REVERSE ? "REV" : "NEUTRAL";
}

// ============================================================================
// ANNOTATION — the old Module C rule, reproduced. Observation only.
// ============================================================================
static uint8_t classifyAnn(int raw, uint8_t prev,
                           int nEnter, int nExit, int sEnter, int sExit) {
  switch (prev) {
    case HWT_ANN_NORTH:
      if (raw < nExit) return (raw <= sEnter) ? HWT_ANN_SOUTH : HWT_ANN_NONE;
      return HWT_ANN_NORTH;
    case HWT_ANN_SOUTH:
      if (raw > sExit) return (raw >= nEnter) ? HWT_ANN_NORTH : HWT_ANN_NONE;
      return HWT_ANN_SOUTH;
    default:
      if (raw >= nEnter) return HWT_ANN_NORTH;
      if (raw <= sEnter) return HWT_ANN_SOUTH;
      return HWT_ANN_NONE;
  }
}

static void setAnnThresholds(int base, int& nEnter, int& nExit, int& sEnter, int& sExit) {
  nEnter = base + ANN_DEADBAND + ANN_MARGIN;
  nExit  = base + ANN_DEADBAND;
  sEnter = base - ANN_DEADBAND - ANN_MARGIN;
  sExit  = base - ANN_DEADBAND;
}

// ============================================================================
// SAMPLER TASK — core 0, priority 3, vTaskDelayUntil grid.
//
// This is the only task that reads the ADC. It never touches WiFi, Blynk,
// Serial, UDP or the motor, and it never blocks on any of them: it hands
// finished batches to a bounded ring and returns to the grid.
// ============================================================================
static void samplerTask(void*) {
  TickType_t last = xTaskGetTickCount();
  const TickType_t periodTicks = pdMS_TO_TICKS(HWT_PERIOD_US / 1000);

  for (;;) {
    const uint64_t slotStart = (uint64_t)esp_timer_get_time();

    // One acquisition per channel per slot. NOT averaged: the recorded value
    // is the reading, and 12 stacked reads with 300 us settling (the base
    // sketch's readAveragedADC) would have cost 3.6 ms and destroyed the
    // waveform this instrument exists to see.
    const int rawA = analogRead(HALL_PIN_A);
#if HWT_SECOND_CHANNEL
    const int rawB = analogRead(HALL_PIN_B);
#else
    const int rawB = 0;
#endif

    annPrevA = classifyAnn(rawA, annPrevA, annNorthEnterA, annNorthExitA,
                           annSouthEnterA, annSouthExitA);
#if HWT_SECOND_CHANNEL
    annPrevB = classifyAnn(rawB, annPrevB, annNorthEnterB, annNorthExitB,
                           annSouthEnterB, annSouthExitB);
#else
    annPrevB = HWT_ANN_NONE;
#endif

    uint8_t ctx = (uint8_t)(manualDirection & HWT_CTX_DIR_MASK);
    if (localEStopEngaged) ctx |= HWT_CTX_ESTOP;
    if (fixedMode)         ctx |= HWT_CTX_FIXED;
    if (seqRunning)        ctx |= HWT_CTX_SEQRUN;

    cap.addSample(hwtPackChannel((uint16_t)rawA, annPrevA, true),
                  hwtPackChannel((uint16_t)rawB, annPrevB, HWT_SECOND_CHANNEL != 0),
                  slotStart,
                  (uint8_t)rampCurrent, (uint8_t)rampTarget, ctx);

    samplerLoops++;
    const uint32_t work = (uint32_t)((uint64_t)esp_timer_get_time() - slotStart);
    if (work > samplerMaxSlotUs) samplerMaxSlotUs = work;

    vTaskDelayUntil(&last, periodTicks);
  }
}

// ============================================================================
// TRANSPORT — network task, core 0, priority 1 (below the sampler, always)
// ============================================================================
static void sendPacket(const uint8_t* data, uint32_t len) {
  if (!hostIpValid || WiFi.status() != WL_CONNECTED) { udpSendFailures++; return; }
  if (!udp.beginPacket(hostIp, HWT_HOST_PORT)) { udpSendFailures++; return; }
  udp.write(data, len);
  if (!udp.endPacket()) udpSendFailures++;
}

static void queueAux(const HwtBatchHeader* h, const uint8_t* payload, uint32_t payloadLen) {
  HwtAux a;
  a.len = (uint16_t)(sizeof(HwtBatchHeader) + payloadLen);
  if (a.len > sizeof(a.buf)) { auxDrops++; return; }
  memcpy(a.buf, h, sizeof(HwtBatchHeader));
  memcpy(a.buf + sizeof(HwtBatchHeader), payload, payloadLen);
  if (!auxQueue || xQueueSend(auxQueue, &a, 0) != pdTRUE) auxDrops++;
}

#if HWT_SERIAL_STREAM
static void mirrorSerial(const uint8_t* data, uint32_t len) {
  Serial.print("#HWT:");
  static const char* hex = "0123456789ABCDEF";
  for (uint32_t i = 0; i < len; i++) {
    Serial.write(hex[data[i] >> 4]);
    Serial.write(hex[data[i] & 0x0F]);
  }
  Serial.println();
}
#endif

static void sendAnchor(const char* text) {
  HwtBatchHeader h;
  HwtAnchorPayload p;
  memset(&p, 0, sizeof(p));
  cap.fillAuxHeader(&h, HWT_REC_ANCHOR);
  p.anchorId     = ++anchorCounter;
  p.sampleSeq    = h.firstSampleSeq;
  p.tUs          = (uint64_t)esp_timer_get_time();
  p.dir          = (uint8_t)manualDirection;
  p.pwmActual    = (uint8_t)rampCurrent;
  p.pwmCommanded = (uint8_t)rampTarget;
  size_t n = strnlen(text, sizeof(p.text) - 1);
  memcpy(p.text, text, n);
  p.textLen = (uint8_t)n;
  hwtSeal(&h, (const uint8_t*)&p, sizeof(p));

  queueAux(&h, (const uint8_t*)&p, sizeof(p));

  Serial.printf("%s [HWT] ANCHOR #%lu \"%s\"  sampleSeq=%lu dir=%s pwm=%d/%d\n",
                ts().c_str(), (unsigned long)p.anchorId, p.text,
                (unsigned long)p.sampleSeq, dirName(manualDirection),
                rampCurrent, rampTarget);
}

static void sendStatus() {
  static uint32_t lastLoops = 0, lastMs = 0;
  const uint32_t nowMs = millis();
  const uint32_t loops = samplerLoops;
  const uint32_t dMs   = hwtElapsedMs(nowMs, lastMs);
  const uint32_t dL    = loops - lastLoops;
  lastLoops = loops; lastMs = nowMs;

  HwtBatchHeader h;
  HwtStatusPayload p;
  memset(&p, 0, sizeof(p));
  cap.fillAuxHeader(&h, HWT_REC_STATUS);
  p.uptimeMs        = nowMs;
  p.sampleSeq       = cap.sampleSeq();
  p.cumMissed       = cap.cumMissed();
  p.cumQueueDrops   = cap.cumQueueDrops();
  p.cumBatches      = cap.cumBatches();
  p.maxGapUs        = cap.maxGapUs();
  // MEASURED cadence, counted from acquisitions over wall time. This is not
  // the telemetry rate and is not derived from it.
  p.measuredMilliHz = dMs ? (uint32_t)(((uint64_t)dL * 1000000ull) / dMs) : 0;
  p.freeHeap        = ESP.getFreeHeap();
  p.udpSendFailures = udpSendFailures;
  p.maxSlotUs       = samplerMaxSlotUs;
  p.queueHighWater  = cap.queueHighWater();
  p.baselineA       = (uint16_t)(baselineA < 0 ? 0 : baselineA);
  p.baselineB       = (uint16_t)(baselineB < 0 ? 0 : baselineB);
  p.channels        = HWT_SECOND_CHANNEL ? 2 : 1;
  p.dir             = (uint8_t)manualDirection;
  p.pwmActual       = (uint8_t)rampCurrent;
  p.pwmCommanded    = (uint8_t)rampTarget;
  p.estop           = localEStopEngaged ? 1 : 0;
  p.fixedMode       = fixedMode ? 1 : 0;
  p.seqRunning      = seqRunning ? 1 : 0;
  hwtSeal(&h, (const uint8_t*)&p, sizeof(p));

  queueAux(&h, (const uint8_t*)&p, sizeof(p));

  Serial.printf("%s [HWT] %lu Hz meas  seq=%lu missed=%lu qdrop=%lu qhw=%u "
                "maxgap=%lu us slot=%lu us udpfail=%lu auxdrop=%lu heap=%lu\n",
                ts().c_str(),
                (unsigned long)(p.measuredMilliHz / 1000),
                (unsigned long)p.sampleSeq, (unsigned long)p.cumMissed,
                (unsigned long)p.cumQueueDrops, (unsigned)p.queueHighWater,
                (unsigned long)p.maxGapUs, (unsigned long)p.maxSlotUs,
                (unsigned long)p.udpSendFailures, (unsigned long)auxDrops,
                (unsigned long)p.freeHeap);
}

static void networkTask(void*) {
  static HwtBatch out;
  uint8_t rx[96];
  for (;;) {
    // Drain the ring. Transport delay never reaches the sampler: worst case
    // the ring fills and drops OLDEST, counted and visible.
    int sent = 0;
    while (sent < 16 && cap.popBatch(&out)) {
      const uint32_t len = hwtBatchWireLen(&out);
      sendPacket((const uint8_t*)&out, len);
#if HWT_SERIAL_STREAM
      mirrorSerial((const uint8_t*)&out, len);
#endif
      sent++;
    }

    HwtAux a;
    while (auxQueue && xQueueReceive(auxQueue, &a, 0) == pdTRUE) {
      sendPacket(a.buf, a.len);
#if HWT_SERIAL_STREAM
      mirrorSerial(a.buf, a.len);
#endif
    }

    // Operator commands from the host recorder. Parsed here, APPLIED on the
    // control side — this task never writes the motor.
    int n = udp.parsePacket();
    if (n > 0) {
      int got = udp.read(rx, sizeof(rx) - 1);
      if (got > 0 && cmdQueue) {
        rx[got] = 0;
        HwtCmd c;
        strncpy(c.line, (const char*)rx, sizeof(c.line) - 1);
        c.line[sizeof(c.line) - 1] = 0;
        xQueueSend(cmdQueue, &c, 0);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

// ============================================================================
// MOTOR — ramp and E-STOP. The recorder cannot reach any of this.
// ============================================================================
static void serviceRamp() {
  if (localEStopEngaged) {
    rampTarget = 0;
    if (rampCurrent != 0) { rampCurrent = 0; ledcWrite(MOTOR_PWM_PIN, 0); }
    return;
  }
  const uint32_t now    = millis();
  const uint32_t stepMs = (rampTarget > rampCurrent) ? RAMP_UP_DELAY_MS : RAMP_DOWN_DELAY_MS;
  if (hwtElapsedMs(now, lastRampMs) < stepMs) return;
  lastRampMs = now;

  if      (rampCurrent < rampTarget) rampCurrent++;
  else if (rampCurrent > rampTarget) rampCurrent--;
  else {
    if (seqRunning && rampTarget == 0) seqRunning = false;
    return;
  }
  ledcWrite(MOTOR_PWM_PIN, rampCurrent);
}

static void engageEStop(bool on) {
  localEStopEngaged = on;
  if (on) {
    rampTarget = 0; rampCurrent = 0; seqRunning = false;
    ledcWrite(MOTOR_PWM_PIN, 0);
    Serial.printf("%s [HWT] LOCAL E-STOP ON — all driving cancelled\n", ts().c_str());
    sendAnchor("ESTOP_ON");
  } else {
    Serial.printf("%s [HWT] LOCAL E-STOP OFF (PWM stays 0 until a new command)\n",
                  ts().c_str());
    sendAnchor("ESTOP_OFF");
  }
  if (Blynk.connected()) Blynk.virtualWrite(VPIN_THROTTLE, 0);
}

// ============================================================================
// FIXED-PWM TEST DRIVING
// Nothing here moves the locomotive without an explicit GO, and E-STOP wins.
// ============================================================================
static void armFixed(int pwm) {
  fixedMode   = true;
  fixedTarget = (uint8_t)constrain(pwm, 0, 255);
  Serial.printf("%s [HWT] FIXED armed at PWM %u — send GO to start\n",
                ts().c_str(), (unsigned)fixedTarget);
}

static void armSeqStep(int8_t idx) {
  if (idx < 0 || idx >= (int8_t)TEST_PWM_STEP_N) {
    Serial.printf("%s [HWT] sequence complete (%u steps)\n",
                  ts().c_str(), (unsigned)TEST_PWM_STEP_N);
    seqIndex = -1;
    return;
  }
  seqIndex = idx;
  rampTarget = 0;                 // always come back to rest between steps
  seqRunning = false;
  armFixed(TEST_PWM_STEPS[idx]);
  Serial.printf("%s [HWT] sequence step %d/%u armed\n",
                ts().c_str(), idx + 1, (unsigned)TEST_PWM_STEP_N);
}

static void testGo() {
  if (localEStopEngaged) {
    Serial.printf("%s [HWT] GO refused — E-STOP engaged\n", ts().c_str());
    return;
  }
  if (manualDirection == DIRECTION_NEUTRAL) {
    Serial.printf("%s [HWT] GO refused — direction NEUTRAL\n", ts().c_str());
    return;
  }
  if (!fixedMode || fixedTarget == 0) {
    Serial.printf("%s [HWT] GO refused — nothing armed (send FIXED <pwm> or SEQ)\n",
                  ts().c_str());
    return;
  }
  rampTarget = fixedTarget;
  seqRunning = true;
  char label[40];
  snprintf(label, sizeof(label), "GO_PWM_%u_%s", (unsigned)fixedTarget,
           dirName(manualDirection));
  sendAnchor(label);
  Serial.printf("%s [HWT] GO — ramping to PWM %u %s\n",
                ts().c_str(), (unsigned)fixedTarget, dirName(manualDirection));
}

static void testStop() {
  rampTarget = 0;
  sendAnchor("STOP");
  Serial.printf("%s [HWT] STOP — ramping to 0\n", ts().c_str());
}

// ============================================================================
// COMMANDS — one parser for host UDP, Blynk and Serial alike
//   ANCHOR <text>   insert an operator anchor (repeatable, any time)
//   FIXED <pwm>     arm a fixed PWM; does NOT move
//   SEQ             arm sequence step 1 (50)
//   NEXT            arm the next sequence step
//   GO / STOP       start the armed step / ramp to zero
//   DIR F|R|N       set direction
//   ESTOP 1|0       engage / release local E-STOP
//   MANUAL          leave fixed mode, hand the throttle back to Blynk
//   SETHOST a.b.c.d point the stream at another recorder
//   STATUS / HELP
// ============================================================================
static void applyCommand(const char* raw) {
  char line[64];
  strncpy(line, raw, sizeof(line) - 1);
  line[sizeof(line) - 1] = 0;
  for (char* p = line; *p; p++) if (*p == '\r' || *p == '\n') { *p = 0; break; }

  char verb[16] = {0};
  const char* arg = "";
  size_t i = 0;
  while (line[i] && line[i] != ' ' && i < sizeof(verb) - 1) { verb[i] = toupper(line[i]); i++; }
  verb[i] = 0;
  if (line[i] == ' ') arg = line + i + 1;

  if      (!strcmp(verb, "ANCHOR")) {
    if (*arg) { strncpy(anchorText, arg, sizeof(anchorText) - 1); anchorText[sizeof(anchorText) - 1] = 0; }
    sendAnchor(anchorText);
  }
  else if (!strcmp(verb, "FIXED"))  armFixed(atoi(arg));
  else if (!strcmp(verb, "SEQ"))    armSeqStep(0);
  else if (!strcmp(verb, "NEXT"))   armSeqStep((int8_t)(seqIndex + 1));
  else if (!strcmp(verb, "GO"))     testGo();
  else if (!strcmp(verb, "STOP"))   testStop();
  else if (!strcmp(verb, "MANUAL")) {
    fixedMode = false; seqRunning = false; seqIndex = -1; rampTarget = 0;
    Serial.printf("%s [HWT] MANUAL — Blynk throttle has the motor\n", ts().c_str());
  }
  else if (!strcmp(verb, "DIR")) {
    char d = toupper(arg[0]);
    int nd = (d == 'F') ? DIRECTION_FORWARD : (d == 'R') ? DIRECTION_REVERSE : DIRECTION_NEUTRAL;
    // Same decideDirectionRequest() the Blynk handler uses -- the two call
    // sites cannot diverge because they share this one function.
    DirectionOutcome o = decideDirectionRequest(nd, rampCurrent, rampTarget,
                                                manualDirection, DIRECTION_NEUTRAL);
    if (!o.applied) {
      Serial.printf("%s [HWT] DIRECTION REFUSED -- PWM not at zero (cur=%d tgt=%d); "
                    "STOP or E-STOP first\n", ts().c_str(), rampCurrent, rampTarget);
      return;
    }
    manualDirection = nd;
    if (o.writePin) digitalWrite(MOTOR_DIR_PIN, nd);
    Serial.printf("%s [HWT] DIRECTION -> %s\n", ts().c_str(), dirName(nd));
  }
  else if (!strcmp(verb, "ESTOP"))  engageEStop(atoi(arg) != 0);
  else if (!strcmp(verb, "SETHOST")) {
    IPAddress ip;
    if (ip.fromString(arg)) { hostIp = ip; hostIpValid = true;
      Serial.printf("%s [HWT] stream host -> %s\n", ts().c_str(), arg); }
    else Serial.printf("%s [HWT] SETHOST: bad address \"%s\"\n", ts().c_str(), arg);
  }
  else if (!strcmp(verb, "STATUS") || !strcmp(verb, "P")) sendStatus();
  else if (!strcmp(verb, "HELP") || !strcmp(verb, "H")) {
    Serial.println("[HWT] ANCHOR <t> | FIXED <pwm> | SEQ | NEXT | GO | STOP | MANUAL");
    Serial.println("[HWT] DIR F|R|N | ESTOP 1|0 | SETHOST <ip> | STATUS | HELP");
    Serial.println("[HWT] DIR F/R/N refused unless PWM is at zero -- STOP or ESTOP first.");
    Serial.println("[HWT] Brake is NOT implemented -- use STOP or ESTOP to stop.");
  }
  else if (verb[0]) {
    Serial.printf("%s [HWT] unknown command \"%s\" (HELP for the list)\n", ts().c_str(), verb);
  }
}

static void serviceCommands() {
  HwtCmd c;
  while (cmdQueue && xQueueReceive(cmdQueue, &c, 0) == pdTRUE) applyCommand(c.line);

  static char sbuf[64];
  static uint8_t sn = 0;
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (sn) { sbuf[sn] = 0; applyCommand(sbuf); sn = 0; }
    } else if (sn < sizeof(sbuf) - 1) {
      sbuf[sn++] = ch;
    }
  }
}

// ============================================================================
// BLYNK HANDLERS — base behaviour preserved, minus every deleted gate
// ============================================================================
BLYNK_WRITE(VPIN_THROTTLE) {
  int t = constrain(param.asInt(), 0, 255);
  if (localEStopEngaged) {
    if (Blynk.connected()) Blynk.virtualWrite(VPIN_THROTTLE, 0);
    return;
  }
  if (manualDirection == DIRECTION_NEUTRAL) {
    if (Blynk.connected()) Blynk.virtualWrite(VPIN_THROTTLE, 0);
    rampTarget = 0;
    return;
  }
  // A manual throttle movement takes the locomotive out of fixed-PWM testing;
  // the operator's hand always wins over the test script.
  if (fixedMode) { fixedMode = false; seqRunning = false; seqIndex = -1; }
  rampTarget = t;
  Serial.printf("%s [HWT] THROTTLE -> %d  dir=%s\n",
                ts().c_str(), t, dirName(manualDirection));
}

BLYNK_WRITE(VPIN_DIRECTION) {
  int nd = param.asInt();
  // CODEX safety review, 2026-08-24: the base sketch wrote MOTOR_DIR_PIN
  // unconditionally, so a direction command under power could reverse the
  // H-bridge while current was flowing.
  //
  // CODEX correction, 2026-08-24 (established operator behavior: Blynk has
  // refused every direction selection while moving, F/R/N alike, for about
  // a year -- the Flask console's own omission of NEUTRAL never changed
  // that). The fully-stopped gate now covers ALL THREE choices, not just
  // F/R: any direction request -- including NEUTRAL -- is refused unless
  // the motor is fully at rest (rampCurrent == 0 AND rampTarget == 0, not
  // merely commanded to zero). A refusal changes nothing: not
  // manualDirection, not MOTOR_DIR_PIN, not PWM, and it does not itself
  // start stopping the locomotive -- STOP or E-STOP first, then reissue.
  DirectionOutcome o = decideDirectionRequest(nd, rampCurrent, rampTarget,
                                              manualDirection, DIRECTION_NEUTRAL);
  if (!o.applied) {
    Serial.printf("%s [HWT] DIRECTION REFUSED -- PWM not at zero (cur=%d tgt=%d); "
                  "STOP or E-STOP first\n", ts().c_str(), rampCurrent, rampTarget);
    // Echo the ACCEPTED direction back to the app -- a refused request must
    // never leave the Blynk control showing a direction that was never
    // applied. o.echoDirection == manualDirection whenever refused; using it
    // rather than manualDirection directly keeps this call in step with the
    // pure decision the host tests exercise.
    if (Blynk.connected()) Blynk.virtualWrite(VPIN_DIRECTION, o.echoDirection);
    return;
  }
  manualDirection = nd;
  // NEUTRAL never touches the pin: it deliberately does not command a
  // physical rest state, only a software gate against new throttle.
  // Selecting NEUTRAL is never itself an automatic stop.
  if (o.writePin) digitalWrite(MOTOR_DIR_PIN, manualDirection);
  Serial.printf("%s [HWT] DIRECTION -> %s\n", ts().c_str(), dirName(manualDirection));
}

BLYNK_WRITE(VPIN_BRAKE) {
  // CODEX safety review, 2026-08-24: Brake was never in this instrument's
  // preserve list (throttle, direction, ramping, E-STOP, safe boot, profiles
  // only) and silently accepting it let an operator believe it stopped the
  // locomotive when nothing happened. Clearly disabled instead of silently
  // ignored: refuse audibly and reset the control so the app cannot show
  // Brake as engaged. Use STOP or E-STOP to actually stop.
  if (param.asInt() != 0) {
    Serial.printf("%s [HWT] BRAKE NOT IMPLEMENTED -- use STOP or E-STOP\n", ts().c_str());
  }
  if (Blynk.connected()) Blynk.virtualWrite(VPIN_BRAKE, 0);
}

BLYNK_WRITE(VPIN_ESTOP) { engageEStop(param.asInt() == 1); }

BLYNK_WRITE(VPIN_FIXED_PWM) { armFixed(param.asInt()); }

BLYNK_WRITE(VPIN_GO) { if (param.asInt() == 1) testGo(); else testStop(); }

BLYNK_WRITE(VPIN_SEQ_NEXT) {
  if (param.asInt() == 1) armSeqStep((int8_t)(seqIndex < 0 ? 0 : seqIndex + 1));
}

BLYNK_WRITE(VPIN_ANCHOR_TEXT) {
  const char* t = param.asStr();
  if (t && *t) { strncpy(anchorText, t, sizeof(anchorText) - 1); anchorText[sizeof(anchorText) - 1] = 0; }
}

BLYNK_WRITE(VPIN_ANCHOR) { if (param.asInt() == 1) sendAnchor(anchorText); }

// ============================================================================
// BLYNK RECONNECT (base behaviour)
// ============================================================================
static uint32_t lastBlynkAttempt = 0;

static void ensureBlynk() {
  if (Blynk.connected()) return;
  const uint32_t now = millis();
  if (hwtElapsedMs(now, lastBlynkAttempt) < 5000UL) return;
  lastBlynkAttempt = now;
  if (Blynk.connect(2000) && Blynk.connected()) {
    Blynk.virtualWrite(VPIN_DIRECTION, manualDirection);
    Blynk.virtualWrite(VPIN_STATUS_LABEL, SKETCH_NAME " DIAGNOSTIC");
  }
}

// ============================================================================
// BASELINE — for the ANNOTATION ONLY. It never decides what is recorded.
// ============================================================================
static int calibrateChannel(int pin) {
  const uint32_t start = millis();
  long total = 0; int count = 0;
  while (hwtElapsedMs(millis(), start) < CALIBRATION_MS) {
    total += analogRead(pin); count++;
    delay(2);
  }
  return count ? (int)(total / count) : analogRead(pin);
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(300);
  analogReadResolution(12);

  // Motor first, at rest, before anything else can go wrong.
  ledcAttach(MOTOR_PWM_PIN, PWM_FREQUENCY, PWM_RESOLUTION);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  digitalWrite(MOTOR_DIR_PIN, DIRECTION_FORWARD);
  ledcWrite(MOTOR_PWM_PIN, 0);
  rampCurrent = 0; rampTarget = 0;

  analogSetPinAttenuation(HALL_PIN_A, ADC_11db);
  pinMode(HALL_PIN_A, INPUT);
#if HWT_SECOND_CHANNEL
  analogSetPinAttenuation(HALL_PIN_B, ADC_11db);
  pinMode(HALL_PIN_B, INPUT);
#endif

  Serial.println("[HWT] annotation baseline — keep magnets away for 2 s...");
  baselineA = calibrateChannel(HALL_PIN_A);
  setAnnThresholds(baselineA, annNorthEnterA, annNorthExitA, annSouthEnterA, annSouthExitA);
#if HWT_SECOND_CHANNEL
  baselineB = calibrateChannel(HALL_PIN_B);
  setAnnThresholds(baselineB, annNorthEnterB, annNorthExitB, annSouthEnterB, annSouthExitB);
#endif

  sessionId = esp_random();
  cap.begin(LOCO_ID, sessionId, HWT_PERIOD_US);
  cmdQueue = xQueueCreate(8, sizeof(HwtCmd));
  auxQueue = xQueueCreate(8, sizeof(HwtAux));

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("[HWT] Connecting WiFi");
  const uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && hwtElapsedMs(millis(), t0) < 15000UL) {
    delay(300); Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[HWT] WiFi OK. IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("[HWT] WiFi timeout — sampling runs anyway; batches will be dropped and counted.");
  }

  hostIpValid = hostIp.fromString(HWT_HOST_DEFAULT);
  udp.begin(HWT_LOCAL_PORT);

  Blynk.config(BLYNK_AUTH_TOKEN);
  const bool ok = Blynk.connect(3000);
  if (Blynk.connected()) {
    Blynk.virtualWrite(VPIN_THROTTLE, 0);
    Blynk.virtualWrite(VPIN_DIRECTION, manualDirection);
    Blynk.virtualWrite(VPIN_STATUS_LABEL, SKETCH_NAME " DIAGNOSTIC");
  }

  // Sampler ABOVE the network task, both off the Blynk core.
  xTaskCreatePinnedToCore(samplerTask, "hwtSample", 4096, nullptr, 3, &samplerHandle, 0);
  xTaskCreatePinnedToCore(networkTask, "hwtNet",    6144, nullptr, 1, nullptr,        0);

  Serial.println("============================================================");
  Serial.println(SKETCH_NAME);
  Serial.println("DIAGNOSTIC ONLY — NO NAVIGATION AUTHORITY");
  Serial.println("INVESTIGATORY / UNAPPROVED — not operational firmware");
  Serial.printf ("Loco     : %s  id=%lu   session=%08lX\n",
                 LOCO_NAME, (unsigned long)LOCO_ID, (unsigned long)sessionId);
  Serial.printf ("Hall A   : GPIO %d (ADC1)  baseline=%d\n", HALL_PIN_A, baselineA);
#if HWT_SECOND_CHANNEL
  Serial.printf ("Hall B   : GPIO %d (ADC1)  baseline=%d  [UNVERIFIED WIRING]\n",
                 HALL_PIN_B, baselineB);
#else
  Serial.println("Hall B   : disabled at compile time (HWT_SECOND_CHANNEL 0)");
#endif
  Serial.printf ("Sampling : %lu us slots, raw, unaveraged, both channels per slot\n",
                 (unsigned long)HWT_PERIOD_US);
  Serial.printf ("Stream   : UDP -> %s:%u   commands in on :%u\n",
                 HWT_HOST_DEFAULT, HWT_HOST_PORT, HWT_LOCAL_PORT);
  Serial.printf ("Blynk    : %s\n", ok ? "connected" : "not connected (will retry)");
  Serial.println("Driving  : Blynk throttle, or FIXED <pwm> + GO. Nothing moves at boot.");
  Serial.println("Direction: F/R/N refused unless PWM is at zero -- STOP/E-STOP first.");
  Serial.println("Brake    : NOT IMPLEMENTED -- use STOP or E-STOP to stop the locomotive.");
  Serial.println("Anchors  : ANCHOR <text> — the only ground truth in the record.");
  Serial.println("Recorder : tools/hwt_receiver.py");
  Serial.println("============================================================");
}

// ============================================================================
// LOOP — control side, core 1. Acquisition does not live here and cannot be
// delayed by anything in it.
// ============================================================================
void loop() {
  if (Blynk.connected()) Blynk.run();
  else                   ensureBlynk();

  serviceCommands();
  serviceRamp();

  static uint32_t lastStatusMs = 0;
  if (hwtElapsedMs(millis(), lastStatusMs) >= STATUS_PERIOD_MS) {
    lastStatusMs = millis();
    sendStatus();
  }
}

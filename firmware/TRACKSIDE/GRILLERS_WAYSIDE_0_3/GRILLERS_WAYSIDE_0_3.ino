/*
  GRILLERS_WAYSIDE_0_3
  NGR trackside locomotive observer
  ESP32 Dev Module

  Hall sensors:
    GPIO 32 = GRILLERS_CW
    GPIO 35 = GRILLERS_CCW

  OBSERVATION ONLY:
    - no locomotive commands
    - no NAV correction
    - no control authority

  Identity magnet signatures:
    NN = OTTO
    SS = TOBY
    NS = HANS
    SN = FRANZ

  This first build intentionally does not consume locomotive ESP-NOW position
  broadcasts yet.  Its purpose is to establish the independent physical
  observation channel first.

  Hall acquisition follows the current NGR practice:
    - 12-bit ESP32 ADC
    - median of 5 ADC reads
    - 1 kHz sampling
    - two consecutive samples outside threshold before opening an event

  IMPORTANT:
    HALL_THRESHOLD_COUNTS is deliberately easy to change after the first
    stationary/pass tests.  Start at 70 counts, matching the current NGR
    detector convention, then tune only from observed data if necessary.

  0.2 changes (from the 0.1 bench capture, 2026-09-24):
    - release timer is cleared when an event opens; 0.1 kept the previous
      close time, so the first sub-threshold sample closed the event at once
      (1 ms events, one magnet split into several events)
    - hysteresis: open at HALL_THRESHOLD_COUNTS, close below
      HALL_RELEASE_COUNTS
    - baseline adapts only when the reading is near baseline, and 16x slower;
      0.1 chased the magnet's flanks and then read the return as the opposite
      pole (phantom S -> false FRANZ)
    - [RAW] line every RAW_REPORT_MS with both sensors' raw and baseline, so
      a silent sensor can be told from a dead one on the bench

  0.3 changes (from the 0.2 bench run, 2026-09-24):
    - periodic [RAW] off by default; output only when a threshold is crossed
    - [CAL] flags a baseline outside the plausible quiet range
    - an event open longer than MAX_EVENT_MS is a fault, not a magnet
      (locos do not stop over these sensors): it is reported as [FAULT],
      not paired, and the baseline is reseeded from the current reading.
      0.2 calibrated CCW at 486 against a quiet reading near 1920 and held
      one event open forever, because the baseline is frozen during events.
*/

#include <Arduino.h>

#define SKETCH_NAME "GRILLERS_WAYSIDE_0_3"

static constexpr uint8_t HALL_CW_PIN  = 32;
static constexpr uint8_t HALL_CCW_PIN = 35;

static constexpr uint16_t SAMPLE_MS = 1;
static constexpr int16_t  HALL_THRESHOLD_COUNTS = 70;
static constexpr int16_t  HALL_RELEASE_COUNTS   = 35;

// A passage contains two identity magnets.  These limits are intentionally
// broad for the first observational build.  They prevent one old event from
// being paired indefinitely with an unrelated later event.
static constexpr uint32_t ID_PAIR_MIN_MS = 40;
static constexpr uint32_t ID_PAIR_MAX_MS = 3000;

// After a magnetic excursion returns near baseline, wait briefly before
// allowing another opening.  This is detector hygiene only; it has no NAV role.
static constexpr uint32_t EVENT_RELEASE_MS = 20;

// Baseline learns only while the sensor is quiet: within BASELINE_QUIET_COUNTS
// of baseline and not inside an event.  1/4096 per 1 ms sample (~4 s).
static constexpr uint8_t BASELINE_SHIFT = 12;
static constexpr int16_t BASELINE_QUIET_COUNTS = 20;

// Bench diagnostic: raw + baseline for both sensors.  0 disables.
static constexpr uint32_t RAW_REPORT_MS = 0;

// Quiet-field reading must fall in this range at calibration.
static constexpr int16_t BASELINE_PLAUSIBLE_MIN = 1200;
static constexpr int16_t BASELINE_PLAUSIBLE_MAX = 2600;

// No real passage keeps a magnet over the sensor this long.
static constexpr uint32_t MAX_EVENT_MS = 3000;

enum Polarity : uint8_t { SOUTH = 0, NORTH = 1 };

struct HallEvent {
  bool valid = false;
  Polarity polarity = NORTH;
  uint32_t openedMs = 0;
  uint32_t closedMs = 0;
  int16_t rawAtOpen = 0;
  int16_t baselineAtOpen = 0;
  int16_t peakSigned = 0;
};

struct SensorState {
  const char* name;
  uint8_t pin;

  int32_t baseline256 = 0;
  bool baselineReady = false;

  bool active = false;
  uint8_t consecutive = 0;
  uint32_t quietSinceMs = 0;

  HallEvent current;

  HallEvent firstIdentity;
  bool waitingSecond = false;
};

struct PassageObservation {
  bool valid = false;
  const char* sensor = nullptr;
  const char* loco = nullptr;
  Polarity p1 = NORTH;
  Polarity p2 = NORTH;
  uint32_t firstMs = 0;
  uint32_t secondMs = 0;
};

static SensorState cw  {"GRILLERS_CW",  HALL_CW_PIN};
static SensorState ccw {"GRILLERS_CCW", HALL_CCW_PIN};

static PassageObservation lastCW;
static PassageObservation lastCCW;

static int16_t median5(uint8_t pin) {
  int v[5];
  for (int i = 0; i < 5; ++i) v[i] = analogRead(pin);

  for (int i = 1; i < 5; ++i) {
    int x = v[i];
    int j = i - 1;
    while (j >= 0 && v[j] > x) {
      v[j + 1] = v[j];
      --j;
    }
    v[j + 1] = x;
  }
  return (int16_t)v[2];
}

static const char* polarityName(Polarity p) {
  return p == NORTH ? "N" : "S";
}

static const char* identify(Polarity first, Polarity second) {
  if (first == NORTH && second == NORTH) return "OTTO";
  if (first == SOUTH && second == SOUTH) return "TOBY";
  if (first == NORTH && second == SOUTH) return "HANS";
  return "FRANZ"; // SOUTH -> NORTH
}

static void calibrateSensor(SensorState& s) {
  int64_t total = 0;
  const int samples = 1000;

  for (int i = 0; i < samples; ++i) {
    total += median5(s.pin);
    delay(1);
  }

  int16_t b = (int16_t)(total / samples);
  s.baseline256 = ((int32_t)b) << 8;
  s.baselineReady = true;
  s.quietSinceMs = millis();

  Serial.printf("[CAL] %-13s GPIO=%u baseline=%d\n",
                s.name, s.pin, b);
  if (b < BASELINE_PLAUSIBLE_MIN || b > BASELINE_PLAUSIBLE_MAX) {
    Serial.printf("[FAULT] sensor=%s calibration baseline=%d outside %d..%d "
                  "(magnet nearby, or sensor/wiring)\n",
                  s.name, b, BASELINE_PLAUSIBLE_MIN, BASELINE_PLAUSIBLE_MAX);
  }
}

static void printEvent(const SensorState& s, const HallEvent& e) {
  Serial.printf(
    "[HALL] sensor=%s polarity=%s open_ms=%lu close_ms=%lu "
    "raw=%d baseline=%d peak_signed=%d\n",
    s.name,
    polarityName(e.polarity),
    (unsigned long)e.openedMs,
    (unsigned long)e.closedMs,
    e.rawAtOpen,
    e.baselineAtOpen,
    e.peakSigned
  );
}

static void reportIdentity(SensorState& s,
                           const HallEvent& a,
                           const HallEvent& b) {
  const uint32_t gap = b.openedMs - a.openedMs;
  const char* loco = identify(a.polarity, b.polarity);

  PassageObservation obs;
  obs.valid = true;
  obs.sensor = s.name;
  obs.loco = loco;
  obs.p1 = a.polarity;
  obs.p2 = b.polarity;
  obs.firstMs = a.openedMs;
  obs.secondMs = b.openedMs;

  if (&s == &cw) lastCW = obs;
  else           lastCCW = obs;

  Serial.printf(
    "[IDENTITY] sensor=%s signature=%s%s loco=%s pair_gap_ms=%lu\n",
    s.name,
    polarityName(a.polarity),
    polarityName(b.polarity),
    loco,
    (unsigned long)gap
  );

  // Direction comes from WHICH station-side sensor saw the same locomotive
  // first.  Magnet order is identity evidence, not direction evidence.
  if (lastCW.valid && lastCCW.valid &&
      strcmp(lastCW.loco, lastCCW.loco) == 0) {

    const uint32_t tCW  = lastCW.secondMs;
    const uint32_t tCCW = lastCCW.secondMs;

    if (tCW < tCCW) {
      Serial.printf(
        "[PASSAGE] station=GRILLERS loco=%s first=GRILLERS_CW "
        "second=GRILLERS_CCW travel=CW_SIDE_TO_CCW_SIDE\n",
        loco
      );
    } else if (tCCW < tCW) {
      Serial.printf(
        "[PASSAGE] station=GRILLERS loco=%s first=GRILLERS_CCW "
        "second=GRILLERS_CW travel=CCW_SIDE_TO_CW_SIDE\n",
        loco
      );
    }

    // Consume the pair so it cannot be reused for another passage.
    lastCW.valid = false;
    lastCCW.valid = false;
  }
}

static void acceptCompletedEvent(SensorState& s, const HallEvent& e) {
  printEvent(s, e);

  if (!s.waitingSecond) {
    s.firstIdentity = e;
    s.waitingSecond = true;
    return;
  }

  uint32_t gap = e.openedMs - s.firstIdentity.openedMs;

  if (gap < ID_PAIR_MIN_MS) {
    Serial.printf("[PAIR] sensor=%s ignored=TOO_CLOSE gap_ms=%lu\n",
                  s.name, (unsigned long)gap);
    return;
  }

  if (gap > ID_PAIR_MAX_MS) {
    Serial.printf("[PAIR] sensor=%s first_expired gap_ms=%lu; reseed\n",
                  s.name, (unsigned long)gap);
    s.firstIdentity = e;
    return;
  }

  reportIdentity(s, s.firstIdentity, e);
  s.waitingSecond = false;
}

static void sampleSensor(SensorState& s, uint32_t now) {
  int16_t raw = median5(s.pin);

  if (!s.baselineReady) {
    s.baseline256 = ((int32_t)raw) << 8;
    s.baselineReady = true;
  }

  int16_t baseline = (int16_t)(s.baseline256 >> 8);
  int16_t signedExcursion = raw - baseline;
  int16_t magnitude = abs(signedExcursion);

  if (!s.active) {
    if (magnitude >= HALL_THRESHOLD_COUNTS) {
      if (++s.consecutive >= 2) {
        s.active = true;
        s.consecutive = 0;

        s.current = HallEvent{};
        s.current.valid = true;
        s.current.openedMs = now;
        s.current.rawAtOpen = raw;
        s.current.baselineAtOpen = baseline;
        s.current.peakSigned = signedExcursion;
        s.quietSinceMs = 0;

        // Current NGR convention: positive signed excursion = N.
        // If the bench test shows the physical sensor orientation is opposite,
        // reverse this ONE assignment, not the identity table.
        s.current.polarity =
          (signedExcursion >= 0) ? NORTH : SOUTH;

        Serial.printf(
          "[OPEN] sensor=%s polarity=%s raw=%d baseline=%d excursion=%d\n",
          s.name,
          polarityName(s.current.polarity),
          raw, baseline, signedExcursion
        );
      }
    } else {
      s.consecutive = 0;

      // Quiet-field baseline adaptation.
      if (magnitude < BASELINE_QUIET_COUNTS) {
        s.baseline256 +=
          ((((int32_t)raw << 8) - s.baseline256) >> BASELINE_SHIFT);
      }
    }
    return;
  }

  // Event is open: retain strongest signed excursion.
  if (abs(signedExcursion) > abs(s.current.peakSigned))
    s.current.peakSigned = signedExcursion;

  if (now - s.current.openedMs > MAX_EVENT_MS) {
    Serial.printf("[FAULT] sensor=%s event open %lu ms raw=%d baseline=%d; "
                  "discarded, baseline reseeded to raw\n",
                  s.name, (unsigned long)(now - s.current.openedMs),
                  raw, baseline);
    s.baseline256 = ((int32_t)raw) << 8;
    s.active = false;
    s.consecutive = 0;
    s.quietSinceMs = now;
    s.current = HallEvent{};
    return;
  }

  // Close only after returning inside the release band.
  if (magnitude < HALL_RELEASE_COUNTS) {
    if (s.quietSinceMs == 0) s.quietSinceMs = now;

    if ((now - s.quietSinceMs) >= EVENT_RELEASE_MS) {
      s.current.closedMs = now;
      acceptCompletedEvent(s, s.current);

      s.active = false;
      s.quietSinceMs = now;
      s.current = HallEvent{};
    }
  } else {
    s.quietSinceMs = 0;
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("================================================");
  Serial.printf(" NGR %s\n", SKETCH_NAME);
  Serial.println(" Station: GRILLERS");
  Serial.println(" MODE: OBSERVATION ONLY");
  Serial.printf(" CW Hall : GPIO %u\n", HALL_CW_PIN);
  Serial.printf(" CCW Hall: GPIO %u\n", HALL_CCW_PIN);
  Serial.printf(" Open %d / release %d counts\n",
                HALL_THRESHOLD_COUNTS, HALL_RELEASE_COUNTS);
  Serial.println(" Identity: NN=OTTO SS=TOBY NS=HANS SN=FRANZ");
  Serial.println("================================================");

  analogReadResolution(12);
  pinMode(HALL_CW_PIN, INPUT);
  pinMode(HALL_CCW_PIN, INPUT);

  Serial.println("[CAL] Keep both Hall sensors clear of magnets.");
  calibrateSensor(cw);
  calibrateSensor(ccw);

  Serial.println("[READY] Grillers wayside observer running.");
}

void loop() {
  static uint32_t nextSample = 0;
  uint32_t now = millis();

  if ((int32_t)(now - nextSample) >= 0) {
    nextSample = now + SAMPLE_MS;
    sampleSensor(cw, now);
    sampleSensor(ccw, now);
  }

  static uint32_t nextRaw = 0;
  if (RAW_REPORT_MS && (int32_t)(now - nextRaw) >= 0) {
    nextRaw = now + RAW_REPORT_MS;
    Serial.printf("[RAW] CW raw=%d base=%d active=%d | CCW raw=%d base=%d active=%d\n",
                  median5(cw.pin), (int)(cw.baseline256 >> 8), cw.active,
                  median5(ccw.pin), (int)(ccw.baseline256 >> 8), ccw.active);
  }

  // Expire an orphaned first identity magnet cleanly.
  if (cw.waitingSecond &&
      millis() - cw.firstIdentity.openedMs > ID_PAIR_MAX_MS) {
    Serial.println("[PAIR] sensor=GRILLERS_CW expired");
    cw.waitingSecond = false;
  }

  if (ccw.waitingSecond &&
      millis() - ccw.firstIdentity.openedMs > ID_PAIR_MAX_MS) {
    Serial.println("[PAIR] sensor=GRILLERS_CCW expired");
    ccw.waitingSecond = false;
  }
}

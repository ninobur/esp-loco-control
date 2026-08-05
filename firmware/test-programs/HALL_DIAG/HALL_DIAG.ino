/*
 * ============================================================================
 * HALL_DIAG  —  NGR Hall sensor bench diagnostic
 * ============================================================================
 * Standalone. No WiFi, no MQTT, no navigation. Serial output only.
 * Its one job: show every magnet hit with PEAK STRENGTH and POLARITY, live,
 * so a misreading sensor is visible magnet-by-magnet.
 *
 * Use: flash to the locomotive's ESP32, open Serial Monitor at 115200,
 * roll the loco slowly over the markers by hand. Each marker prints one line:
 *
 *     MARKER #7   NORTH   peak= 182   (baseline 1741, delta +182)
 *     MARKER #8   SOUTH   peak= 168   (baseline 1741, delta -168)
 *
 * What healthy looks like: consistent peaks (chain-v3 Otto ran 126-162),
 * polarity matching the known NGR DNA sequence, baseline steady.
 * What a fault looks like: peaks near the deadband (~<50), polarity flipping
 * randomly, baseline wandering, or NO print at all when a magnet passes.
 *
 * PIN: HALL_PIN is 33 (ADC1_CH5) on BOTH locomotives — the same wiring on each.
 * It is NOT in LL_LocoConfig: it is `#define HALL_PIN 33` in QUORUM.ino, and
 * docs/CLAUDE.md records the Hall sensor on GPIO 33.
 *
 * (This sketch originally shipped with 34, which is the IR wheel sensor's pin,
 * not the Hall's. Reading 34 here would sample an unconnected input and print
 * "baseline out of range — sensor may be disconnected" — the exact false
 * negative this tool exists to rule out. Corrected on operator confirmation.)
 * ============================================================================
 */
#include <Arduino.h>

const int  HALL_PIN        = 33;    // Hall sensor, both locos. 34 is the IR sensor.
const int  DEADBAND_COUNTS = 25;    // ignore noise below this delta (SOLONAV uses 25)
const int  ENTRY_MARGIN    = 13;    // hysteresis, matches SOLONAV entry margin
const int  BASELINE_SAMPLES = 128;  // median-of-128 baseline, like the nav firmware
const unsigned long PRINT_MIN_GAP_MS = 50;  // debounce between distinct markers

int  baseline = 0;
int  markerCount = 0;
bool inMarker = false;
int  peakDelta = 0;        // signed: +north-ish / -south-ish (sign = polarity)
unsigned long lastMarkerMs = 0;

int readADC() { return analogRead(HALL_PIN); }

void computeBaseline() {
  // Median of BASELINE_SAMPLES taken at rest — matches the firmware's method
  // so the numbers here are comparable to loopstat 'baseline'.
  static int buf[BASELINE_SAMPLES];
  for (int i = 0; i < BASELINE_SAMPLES; i++) { buf[i] = readADC(); delay(2); }
  for (int i = 1; i < BASELINE_SAMPLES; i++) {
    int k = buf[i], j = i - 1;
    while (j >= 0 && buf[j] > k) { buf[j+1] = buf[j]; j--; }
    buf[j+1] = k;
  }
  baseline = buf[BASELINE_SAMPLES/2];
}

void setup() {
  Serial.begin(115200);
  delay(500);
  analogReadResolution(12);
  pinMode(HALL_PIN, INPUT);
  Serial.println("\n=== HALL_DIAG — NGR Hall sensor bench diagnostic ===");
  Serial.printf("Pin %d, deadband %d, entry margin %d\n", HALL_PIN, DEADBAND_COUNTS, ENTRY_MARGIN);
  Serial.println("Establishing baseline — keep sensor AWAY from magnets for 3s...");
  computeBaseline();
  Serial.printf("Baseline = %d\n", baseline);
  if (baseline < 100 || baseline > 4000)
    Serial.println(">>> WARNING: baseline out of range — sensor may be disconnected (reads ~0) or miswired.");
  Serial.println("Ready. Roll the loco slowly over the markers.\n");
}

void loop() {
  int raw = readADC();
  int delta = raw - baseline;               // signed
  int mag   = abs(delta);

  // Rolling debug so you can SEE the sensor is alive even between markers.
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 1000) {
    lastDebug = millis();
    Serial.printf("   [idle] raw=%d  baseline=%d  delta=%+d\n", raw, baseline, delta);
  }

  if (!inMarker) {
    // Entering a marker: delta exceeds deadband + entry margin
    if (mag > DEADBAND_COUNTS + ENTRY_MARGIN) {
      inMarker = true;
      peakDelta = delta;
    }
  } else {
    // Inside a marker: track the peak (largest magnitude, keep its sign)
    if (mag > abs(peakDelta)) peakDelta = delta;

    // Leaving the marker: delta falls back below deadband
    if (mag < DEADBAND_COUNTS) {
      inMarker = false;
      unsigned long now = millis();
      if (now - lastMarkerMs > PRINT_MIN_GAP_MS) {
        lastMarkerMs = now;
        markerCount++;
        const char* pol = (peakDelta >= 0) ? "NORTH" : "SOUTH";
        Serial.printf("MARKER #%d   %s   peak=%4d   (baseline %d, delta %+d)\n",
                      markerCount, pol, abs(peakDelta), baseline, peakDelta);
      }
      peakDelta = 0;
    }
  }
  delay(2);   // ~500 Hz sampling, plenty for hand-rolling
}

/*
 * NAVI_HYPOTHESIS_SHADOW
 *
 * Serial-driven, non-motor shadow sketch for the bounded hypothesis mechanism.
 * It cannot command a locomotive. Feed it declarations and Hall observations;
 * it prints every surviving route interpretation as JSON.
 *
 * Commands (one line each):
 *   DECLARE <mm> <dir> <now_ms>       dir is 1 (CW) or -1 (CCW)
 *   OBS <ms> <open> <window> <valid> <moving>
 *                                     polarities are N or S; booleans 0/1
 *   VMAX <mm_per_second>              hard upper bound; 0 disables it
 *
 * This is a replay/shadow experiment, not field-accepted navigation firmware.
 */
#include <Arduino.h>

#include "HypothesisNavigator.h"

using namespace navi_hypothesis;

static HypothesisNavigator navigator;
static char lineBuffer[128];
static uint8_t lineLength = 0;

static uint8_t parsePole(char value) { return value == 'N' || value == 'n'; }

static void printState(Result result) {
  Serial.printf("{\"result\":\"%s\",\"certain\":%u,\"count\":%u,\"hypotheses\":[",
                resultName(result), navigator.positionCertain() ? 1U : 0U,
                static_cast<unsigned>(navigator.count()));
  for (uint8_t i = 0; i < navigator.count(); ++i) {
    const Hypothesis& hypothesis = navigator.hypothesis(i);
    if (i) Serial.print(',');
    Serial.printf("{\"mm\":%u,\"last_real_ms\":%lu,\"faults\":%u,\"causes\":%u}",
                  static_cast<unsigned>(hypothesis.mm),
                  static_cast<unsigned long>(hypothesis.lastRealMs),
                  static_cast<unsigned>(hypothesis.faults),
                  static_cast<unsigned>(hypothesis.causes));
  }
  Serial.println("]}");
}

static void handleLine(char* line) {
  unsigned mm = 0;
  int direction = 0;
  unsigned long now = 0;
  unsigned vmax = 0;
  char opening = 0, window = 0;
  unsigned valid = 0, moving = 0;

  if (sscanf(line, "DECLARE %u %d %lu", &mm, &direction, &now) == 3) {
    navigator.declare(static_cast<uint8_t>(mm), static_cast<int8_t>(direction), now);
    printState(navigator.result());
    return;
  }
  if (sscanf(line, "OBS %lu %c %c %u %u", &now, &opening, &window,
             &valid, &moving) == 5) {
    Observation observation;
    observation.atMs = static_cast<uint32_t>(now);
    observation.openingPolarity = parsePole(opening);
    observation.windowPolarity = parsePole(window);
    observation.windowValid = valid != 0;
    observation.motionPermitsAdvance = moving != 0;
    printState(navigator.observe(observation));
    return;
  }
  if (sscanf(line, "VMAX %u", &vmax) == 1) {
    navigator.setPhysicalVmax(vmax);
    Serial.printf("{\"vmax_mm_s\":%u}\n", vmax);
    return;
  }
  Serial.println("{\"error\":\"expected DECLARE, OBS, or VMAX\"}");
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("{\"sketch\":\"NAVI_HYPOTHESIS_SHADOW\",\"field_accepted\":false,\"motor_authority\":false}");
}

void loop() {
  while (Serial.available()) {
    const char value = static_cast<char>(Serial.read());
    if (value == '\r') continue;
    if (value == '\n') {
      lineBuffer[lineLength] = 0;
      if (lineLength) handleLine(lineBuffer);
      lineLength = 0;
    } else if (lineLength + 1 < sizeof(lineBuffer)) {
      lineBuffer[lineLength++] = value;
    } else {
      lineLength = 0;
      Serial.println("{\"error\":\"line too long\"}");
    }
  }
}

#pragma once

#include <Arduino.h>

struct PwmSpeedEntry {
  uint8_t pwm;
  float pKph;
};

// Active locomotive for this experimental sketch: Otto (9950011).
// The boot banner must name Otto before any field use. Change one include
// below to build for Toby; do not reuse Otto's measured profile.
#include "LL_LocoConfig_9950011.h"   // Otto   <-- ACTIVE
//#include "LL_LocoConfig_9950012.h"     // Toby

#ifndef LOCO_ID
#error "No locomotive profile selected in LocoConfig.h."
#endif

// A locomotive may not inherit another's measured thresholds by accident.
// NAVI_RECOGNIZER_MEASURED_ON is stamped into the profile by whoever did the
// measuring; if it is missing, or names a different locomotive, the build
// stops here rather than flying Toby's numbers on Otto's sensor.
#ifndef NAVI_RECOGNIZER_MEASURED_ON
#error "This profile has no measured NAVI_ONE recognizer block. Run the survey on THIS locomotive and record the values in its profile; do not copy another's."
#endif
#if NAVI_RECOGNIZER_MEASURED_ON != LOCO_ID
#error "The NAVI_ONE recognizer thresholds in this profile were measured on a DIFFERENT locomotive."
#endif

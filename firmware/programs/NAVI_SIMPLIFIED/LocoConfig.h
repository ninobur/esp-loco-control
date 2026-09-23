#pragma once

#include <Arduino.h>

struct PwmSpeedEntry {
  uint8_t pwm;
  float pKph;
};

// Otto by default; define NAVI_BUILD_TOBY for Toby's measured profile.
#ifdef NAVI_BUILD_TOBY
#include "LL_LocoConfig_9950012.h"
#else
#include "LL_LocoConfig_9950011.h"
#endif

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

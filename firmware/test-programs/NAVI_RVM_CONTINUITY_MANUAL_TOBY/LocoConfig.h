#pragma once
// NAVI_RVM_CONTINUITY_MANUAL_TOBY: Toby (9950012). Identical to NAVI_RVM_CONTINUITY_MANUAL
// except for the locomotive: this build uses Toby's installed hardware profile
// (LL_LocoConfig_9950012.h, copied unchanged from firmware/config). Of it the sketch uses:
// LOCO_NAME, LOCO_ID, MOTOR_PWM_PIN (4), MOTOR_DIR_PIN (2), PWM_FREQUENCY, PWM_RESOLUTION,
// SAFE_DIRECTION_CHANGE_PWM and the battery thresholds. Hall GPIO 33 and I2C 21/22 are
// Toby's, as in his NAVI_COHERENCE 0.6 firmware.
//
// SHAPE MODEL AND SIGN. RvmShapeModel.h was built from OTTO's Hall (2026-10-15); this build
// is the field test of that model on Toby (David, 2026-10-19). The polarity sign is the same
// as Otto's: N = ADC above the baseline. That is Toby's own convention in NAVI_COHERENCE 0.6
// (NAVI_ONE X22 ExcursionDetector: departure = raw - baseline, departure >= 0 -> N), which
// matched the map 926/926 in Toby's 20Q3 run 5. The legacy HALL_POLARITY_INVERTED flag in the
// profiles is read by nothing. The continuity constants (50 / 24 / 16) were measured on Otto.
#include "LL_LocoConfig_9950012.h"
#if LOCO_ID != 9950012
#error "NAVI_RVM_CONTINUITY_MANUAL_TOBY is for Toby (9950012)."
#endif

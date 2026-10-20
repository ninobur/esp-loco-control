#pragma once
// NAVI_RVM_SHAPE_MANUAL: Otto only. The shape model (RvmShapeModel.h) was built
// from Otto's Hall sensor on 2026-10-15 and the polarity sign (ROUTE 1 = ADC above
// baseline) was verified on Otto. It is not a Toby model.
// LL_LocoConfig_9950011.h is Otto's installed hardware profile, copied unchanged
// from NAVI_RVM (d18fe67). Of it this sketch uses: LOCO_NAME, LOCO_ID,
// MOTOR_PWM_PIN, MOTOR_DIR_PIN, PWM_FREQUENCY, PWM_RESOLUTION,
// SAFE_DIRECTION_CHANGE_PWM and the battery thresholds. Its legacy navigator
// macros (HALL_*, NAVI_*, IR_*, BLYNK_*) are profile data with no reader here.
#include "LL_LocoConfig_9950011.h"
#if LOCO_ID != 9950011
#error "NAVI_RVM_SHAPE_MANUAL's shape model and polarity sign are for Otto (9950011)."
#endif

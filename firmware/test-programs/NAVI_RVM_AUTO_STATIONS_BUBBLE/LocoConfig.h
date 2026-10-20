#pragma once
// NAVI_RVM_AUTO_STATIONS_BUBBLE: one sketch, two locomotives. Select at compile
// time with -DNAVI_LOCO=9950012 for Toby; the default is Otto (9950011).
//
// Of each installed hardware profile (copied unchanged) this sketch uses:
// LOCO_NAME, LOCO_ID, MOTOR_PWM_PIN, MOTOR_DIR_PIN, PWM_FREQUENCY,
// PWM_RESOLUTION, SAFE_DIRECTION_CHANGE_PWM and the battery thresholds. Hall
// GPIO 33 and I2C 21/22 are the same on both.
//
// Shape model and polarity sign: RvmShapeModel.h was built from Otto's Hall
// (2026-10-15). Both locomotives use N = ADC above the baseline (Toby's own
// convention; matched the map 926/926 in his 20Q3 run 5).
//
// CONTINUITY. Otto: on (field-proven, Runs 9-12). Toby: OFF. His Hall level
// steps ~+23 ADC while running steadily at PWM 90 (Run 12, block 5400) and
// moves ~52 ADC with the motor; the Hall-only continuity rule cannot tell that
// from a stall on a shoulder and withheld his moving baseline (18 claims, 2
// CONTRADICTIONs). Replay of the same run with continuity off and the 70-count
// gate: 203 consecutive correct claims, 0 CONTRADICTIONs. Cost: a stop with
// Toby's sensor over a magnet may break that magnet's recognition.
#ifndef NAVI_LOCO
#define NAVI_LOCO 9950011
#endif

#if NAVI_LOCO == 9950011
#include "LL_LocoConfig_9950011.h"
#define RVM_CONTINUITY_ENABLED 1
#elif NAVI_LOCO == 9950012
#include "LL_LocoConfig_9950012.h"
#define RVM_CONTINUITY_ENABLED 0
#else
#error "NAVI_LOCO must be 9950011 (Otto) or 9950012 (Toby)"
#endif

#if LOCO_ID != NAVI_LOCO
#error "hardware profile does not match NAVI_LOCO"
#endif

#pragma once

#include <Arduino.h>

struct PwmSpeedEntry {
  uint8_t pwm;
  float pKph;
};

// ============================================================================
// Active profile selector. Exactly ONE include may be active below.
//
// TARGET: Toby (9950012).
//
// Verify after flashing: the boot serial line must read
//     [BOOT] QUORUM_1_13 — 9950012
// If it names the other locomotive, the wrong profile was compiled in and the
// image must be rebuilt — an ID mismatch puts two locomotives on the same MQTT
// topics. 2026-08-12: this file's comment said TARGET Toby while the active
// include was Otto's, which is exactly the trap the boot check exists to catch.
// ============================================================================
//#include "LocoConfig_2095111.h"      // Hans
//#include "LL_LocoConfig_9950011.h"   // Otto
#include "LL_LocoConfig_9950012.h"     // Toby  <-- ACTIVE

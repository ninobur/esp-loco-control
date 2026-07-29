#pragma once

#include <Arduino.h>

struct PwmSpeedEntry {
  uint8_t pwm;
  float pKph;
};

// ============================================================================
// Active profile selector for LL_Auto_r22.ino
// July17 2026: canonical selector for the r22 adaptive-baseline flash.
// TARGET: Toby (9950012). Exactly ONE include may be active below.
// Verify after flashing: boot serial must read "LL_Auto_r22 - 9950012 booting".
// ============================================================================
//#include "LocoConfig_2095111.h"      // Hans
//#include "LL_LocoConfig_9950011.h"   // Otto
#include "LL_LocoConfig_9950011.h"     

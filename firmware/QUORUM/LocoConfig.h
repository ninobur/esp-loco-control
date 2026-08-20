#pragma once

#include <Arduino.h>

struct PwmSpeedEntry {
  uint8_t pwm;
  float pKph;
};

// ============================================================================
// Active profile selector. Exactly ONE include may be active below.
//
// TARGET: Otto (9950011).
//
// Verify after flashing: the boot serial line must read
//     [BOOT] QUORUM_1_16R_IR_TEST_A — 9950011
// If it names the other locomotive, the wrong profile was compiled in and the
// image must be rebuilt — an ID mismatch puts two locomotives on the same MQTT
// topics. 2026-08-12: this file's comment said TARGET Toby while the active
// include was Otto's, which is exactly the trap the boot check exists to catch.
//
// 2026-08-18: Otto's include appeared TWICE in this list, once above the active
// line and once below it. Uncommenting either would have compiled Otto's
// profile while the other stayed commented and looked untouched — the same trap
// wearing a different hat. Reduced to one line per locomotive.
//
// 2026-08-19: checking the compiled identity by running `strings` over
// firmware/QUORUM/build/ is NOT a verification. arduino-cli builds to a temp
// directory unless given --build-path, so that folder is a stale Arduino IDE
// artifact and reported the WRONG locomotive while the selector was correct.
// Verify with `--build-path <dir>` and read that dir, or read the boot banner.
// ============================================================================
//#include "LocoConfig_2095111.h"      // Hans
//#include "LL_LocoConfig_9950012.h"   // Toby
#include "LL_LocoConfig_9950011.h"     // Otto  <-- ACTIVE

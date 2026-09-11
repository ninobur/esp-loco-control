#pragma once

#include <Arduino.h>

struct PwmSpeedEntry {
  uint8_t pwm;
  float pKph;
};

// ============================================================================
// ACTIVE PROFILE SELECTOR — exactly ONE include below may be uncommented.
//
// TARGET: Otto (9950011).
//
// VERIFY AFTER FLASHING. The boot serial line must read
//
//     [BOOT] NAVI_ONE_1_0X14_FIELDTEST "Operator-ruling corrective" — 9950011
//     [BOOT] CORRECTIVE FIELD-TEST BUILD — not field-accepted NAVI_ONE 1.0.
//     [BOOT] Morphology is diagnostic only; pause/resume/stitch authority removed.
//
// If it names the other locomotive, the wrong profile was compiled in and the
// image must be rebuilt: an ID mismatch puts two locomotives on the same MQTT
// topics.
//
// IF THESE LINES ARE ABSENT you are looking at a different image than the
// one this file describes. The FIELDTEST suffix is not decoration: this build
// removes decision 0070's expired paused/resumed/stitch experiment and restores
// the operator's 500 ms re-read guard. Morphology has no navigation authority.
//
// ----------------------------------------------------------------------------
// THIS FILE'S OWN HISTORY, WHICH IT KEEPS REPEATING
// ----------------------------------------------------------------------------
// 2026-08-12: the comment said TARGET Toby while the active include was Otto's.
// 2026-08-18: Otto's include appeared TWICE, once above the active line and
//   once below it — uncommenting either compiled Otto while the other stayed
//   commented and looked untouched. Reduced to one line per locomotive.
// 2026-08-19: checking the compiled identity by running `strings` over
//   firmware/QUORUM/build/ is NOT a verification. arduino-cli builds to a temp
//   directory unless given --build-path, so that folder is a stale Arduino IDE
//   artifact and reported the WRONG locomotive while the selector was correct.
//   Verify with `--build-path <dir>` and read that dir, or read the boot banner.
// 2026-08-30: found by review to have re-grown BOTH earlier traps at once —
//   the header said TARGET Otto while Toby's include was active, and Toby's
//   line appeared twice. The boot-verify text still named QUORUM_1_16R_IR_TEST_A
//   and 9950011, so following this file's own procedure verbatim would have
//   failed every correct NAVI_ONE build. Hans's line pointed at a file that is
//   not in this directory, and there was no line for Otto at all, though his
//   profile sits here — so selecting Otto meant typing a new line, which is
//   how the typos start.
// 2026-09-10: commit 7781951 selected Otto while the header still said TARGET
//   Toby and still named X11/Toby as the expected image. X14 corrected the
//   target but initially wrote the friendly name "Otto" where the firmware
//   actually prints the numeric LOCO_NAME, 9950011. The verification text now
//   quotes the compiled serial output exactly.
//
// ONE LINE PER LOCOMOTIVE. EVERY LINE POINTS AT A FILE THAT EXISTS.
// ============================================================================
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

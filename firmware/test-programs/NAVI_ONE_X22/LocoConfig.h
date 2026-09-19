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
//     [BOOT] NAVI_ONE_1_0X22_LOCKED_BASELINE_FIELDTEST "locked per-interval reference; leashed; age published" — 9950011
//     [BOOT] X22 LOCKED-BASELINE FIELD TEST — not field-accepted NAVI_ONE 1.0.
//     [BOOT] The reference is LOCKED. It is re-established once per marker interval.
//     [BOOT] There is no continuous adaptation and no measured-rest estimator.
//     [BOOT] RISK: the predicted failure is a STALE lock, not a migrating one.
//
// If it names the other locomotive, the wrong profile was compiled in and the
// image must be rebuilt: an ID mismatch puts two locomotives on the same MQTT
// topics.
//
// IF THESE LINES ARE ABSENT you are looking at a different image than the
// one this file describes. The FIELDTEST suffix is not decoration: this build
// DELETES the X19 adaptive-rest architecture that X20 and X21 were flown with,
// and replaces it with a reference that is locked between marker intervals.
// Morphology still has no navigation authority, and the 645 ms guard stands.
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
// 2026-09-10: X15 adds the operator-approved post-stop successor exception;
//   the active locomotive remains Otto and no profile constants changed.
// 2026-09-12: departure diagnostic retains X15 behavior and adds telemetry
//   only. The active locomotive remains Otto and no profile constants changed.
// 2026-09-12: X16 experimentally raises the shared completed-passage floor to
//   82 ms and publishes each floor rejection on diag/acquisition. This is not
//   field-accepted production policy; the active locomotive remains Otto.
// 2026-09-14: X17 preserves X16 and holds the startup baseline authoritative
//   for the run. The former adaptive median is published as shadow telemetry
//   only and has no effect on acquisition or navigation.
// 2026-09-14: X18 preserves X17 acquisition/navigation and gives the shadow
//   median bounded authority only after a complete SET LOCATION-anchored lap:
//   one value per MM, route median, maximum two counts in either direction.
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

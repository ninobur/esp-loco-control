#pragma once
// ============================================================================
// LL_LocoConfig_9950012.h
// Profile: Toby (9950012) - permanent production profile
// Hall thresholds measured with NGR Hall Probe diagnostic, 2026-06-26.
// July17 2026: canonical copy for LL_Auto_r22 (adaptive baseline).
//   Values verified current against Toby's own boot serial this date
//   (deadband=25 entryMargin=13 minPeak=35 dominance=120). No value changes.
// ============================================================================

#include "credentials.h"   // WIFI_SSID, WIFI_PASS, BLYNK_AUTH_TOKEN_* (git-ignored)

// Identity
#define LOCO_NAME "9950012"
#define LOCO_ID 9950012UL

// ---------------------------------------------------------------------------
// IR Test A (docs/QUORUM_1_16R_IR_TEST_A_FIRMWARE_SPEC.md §2). Toby carried the
// sensor car for Gate 1 and the 2026-08-18 first live run; the car then moved
// to Otto, whose low-speed phantom markers are what the layer must discriminate.
//
// DISABLED here rather than deleted. The sender registers exactly one peer, so
// only one locomotive can be paired at a time — leaving Toby enabled while Otto
// holds the pairing would mean a locomotive listening for a sender that never
// addresses it. With IR_TEST_A_ENABLED absent, every IR path compiles to inert
// stubs (QUORUM.ino:370). The bench-paired MAC is kept in the comment below so
// handing the car back is a one-line change, not a re-pairing session:
//
//   IR Test Car STA MAC, bench-paired 2026-08-18: EC:E3:34:78:A2:60
//
// ALL-ZERO IS A SAFE PLACEHOLDER: reception stays disabled and is reported; it
// never falls back to accepting any sender.
// ---------------------------------------------------------------------------
// Re-enable by uncommenting BOTH lines — keep the #ifndef guards, which let
// tests/harness.cpp's own definitions win (see the note in Otto's profile; an
// unconditional #define here silently fails 17 IR tests with state NO_SENSOR).
// #ifndef IR_TEST_A_ENABLED
// #define IR_TEST_A_ENABLED 1
// #endif
// #ifndef IR_SENSOR_MAC_BYTES
// #define IR_SENSOR_MAC_BYTES {0xEC,0xE3,0x34,0x78,0xA2,0x60}
// #endif
#define HALL_POLARITY_INVERTED false

// Blynk
#define BLYNK_TEMPLATE_ID "TMPL2xjo1-Ltx"
#define BLYNK_TEMPLATE_NAME "Loco Driver I"
#define BLYNK_AUTH_TOKEN BLYNK_AUTH_TOKEN_9950012

// Blynk virtual pins
#define VPIN_THROTTLE V1
#define VPIN_DIRECTION V0
#define VPIN_BRAKE V3
#define VPIN_ESTOP V2
#define VPIN_LOWVOLT_LED V4
#define VPIN_VOLTAGE_DISPLAY V5
#define VPIN_CURRENT_DISPLAY V6

// Motor control pins
#define MOTOR_PWM_PIN 4
#define MOTOR_DIR_PIN 2

#define PWM_CHANNEL 0
#define PWM_FREQUENCY 20000
#define PWM_RESOLUTION 8

// CTO3 consist extent — decision 0030/0033, spec docs/CTO3/BUBBLE_V1_SPEC.md.
// Occupied track relative to the Hall sensor, in MARKERS (mile markers, the
// control frame — never millimetres). A property of the CONSIST: change these
// when cars are added or removed. The producer applies them to its own
// published bounds; consumers receive occupied track, not a sensor point.
#define CONSIST_EXTENT_FRONT_MARKERS 2
#define CONSIST_EXTENT_REAR_MARKERS  4

// Direction values
#define DIRECTION_REVERSE 0
#define DIRECTION_NEUTRAL 1
#define DIRECTION_FORWARD 2
#define SAFE_DIRECTION_CHANGE_PWM 15

// Speed constants
#define NORMAL_PWM 110

// Ramp behavior
#define RAMP_UP_DELAY_MS   150UL
#define RAMP_DOWN_DELAY_MS 100UL

// Voltage thresholds
#define DISCONNECTED_VOLTAGE_THRESHOLD 12.5f
#define THROTTLE_LIMIT_VOLTAGE 13.5f
#define SHUTDOWN_VOLTAGE 13.25f
#define RECOVERY_VOLTAGE 14.0f
#define MIN_THROTTLE_PWM 60
#define VOLTAGE_COUNTER_LIMIT 5
#define VOLTAGE_REPORT_INTERVAL_MS 120000UL
#define VOLTAGE_DISPLAY_INTERVAL_MS 60000UL

// Hall sensor profile — measured with NGR Hall Probe diagnostic 2026-06-26.
// Noise: mean 4.2 counts, observed maximum 19.
// Weakest real magnet signal: 39 counts.
// Enter at baseline +/- 38 (deadband 25 + entry margin 13); exit at +/- 25.
// July17: as of LL_Auto_r22 these thresholds ride an adaptive baseline
// (slew-limited +/-1 count per 500 ms) rather than the boot-time value.
#define HALL_DEADBAND_COUNTS       25
#define HALL_ENTRY_MARGIN_COUNTS   13
#define HALL_MIN_PEAK_DELTA        35
#define HALL_DOMINANCE_PERCENT     120U

// ---------------------------------------------------------------------------
// NAVI_ONE recognizer — MEASURED ON TOBY, and on Toby only.
//
// Every one of these sits at the midpoint of a measured gap between the real
// and the false population in the 2026-08-28 PWM-90 circuit survey (351
// waveforms; see docs/research/20260828_WHAT_THE_HALL_SENSOR_SEES.md). None of
// them was chosen. They are not constants of the railway and they are not
// portable: Otto enters at 70 counts against Toby's 38, a materially different
// sensor environment, and nothing about these numbers has been measured there.
//
// NAVI_RECOGNIZER_MEASURED_ON is checked against LOCO_ID at compile time, so a
// profile that has not had this work done cannot silently inherit Toby's.
// ---------------------------------------------------------------------------
// IR PRESENCE IS DECLARED, NOT PROBED.
//
// 0.1 inferred it from the peak-to-peak span of GPIO 34 over a 4 s boot
// window, which is backwards: a fitted sensor sitting still over uniform
// ballast has LOW variance and was called NOT FITTED, permanently, for the
// whole session -- while a floating pin, which is what the probe was written
// to catch, swings freely and reads as present. The operator's principle
// applies to instruments as much as to position: the declaration is truth.
//
// 0 = not fitted. Pin 34 is not touched at all, which is also what keeps the
//     floating-input crosstalk off the Hall line (see NAVI_ONE.ino).
// 1 = fitted. Sampled at 100 Hz, observing only: no vote, no verdict, no path
//     to navMm.
//
// 2026-08-30: 0. Toby's IR unit is being proved off the car in its own thread
// before it is mounted.
#define IR_FITTED 0

#define NAVI_RECOGNIZER_MEASURED_ON  9950012UL
#define NAVI_GUARD_MS                200U     // close-to-open, decision 0057
#define NAVI_AMPLITUDE_FLOOR         0.34f    // of the trailing median accepted peak
#define NAVI_RESIDUAL_CEILING        0.13f    // normalised RMS of the Gaussian fit
#define NAVI_BOOTSTRAP_GAIN          190U     // until eight peaks are in hand
#define NAVI_AUTO_CRUISE_PWM         90       // the surveyed regime

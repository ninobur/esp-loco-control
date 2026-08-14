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

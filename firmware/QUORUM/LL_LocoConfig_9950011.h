#pragma once
// ============================================================================
// LL_LocoConfig_9950011.h
// Profile: Otto (9950011)
// ============================================================================

#include "credentials.h"   // WIFI_SSID, WIFI_PASS, BLYNK_AUTH_TOKEN_* (git-ignored)

// Identity
#define LOCO_NAME "9950011"
#define LOCO_ID 9950011UL
#define HALL_POLARITY_INVERTED true

// Blynk
#define BLYNK_TEMPLATE_ID "TMPL2xjo1-Ltx"
#define BLYNK_TEMPLATE_NAME "Loco Driver I"
#define BLYNK_AUTH_TOKEN BLYNK_AUTH_TOKEN_9950011

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

// ---------------------------------------------------------------------------
// Hall thresholds. Previously undefined, so Otto ran on the sketch fallbacks
// of 50/80/80 — an entry threshold of 130 counts.
//
// Measured 2026-07-28, five CW laps, 769 markers: peak minimum was exactly
// 130 and the fifth percentile 137. The distribution was truncated at the
// threshold, which is what a threshold cutting into the signal looks like.
// 4% of markers were being missed and nothing showed it.
//
// These are Toby's values, which run at 0.5% error with zero spurious events.
// Entry drops from 130 to 38 — well below the observed floor, well above the
// few counts of baseline noise.
// ---------------------------------------------------------------------------
#define HALL_DEADBAND_COUNTS       25
#define HALL_ENTRY_MARGIN_COUNTS   13
#define HALL_MIN_PEAK_DELTA        35

// ---------------------------------------------------------------------------
// CTO3 Station Stop v1 (docs/CTO3/station-stop-v1/README.md, CTO3_SPEC §12
// step 3): Otto only, Arches only. With this define, the station arming loop
// considers ONLY the named station; all others are passed at section cruise
// with no state change. Remove the define to restore all-stations arming.
// Toby's profile deliberately has no equivalent — his behaviour is unchanged
// and the v1 restriction cannot activate for him.
// ---------------------------------------------------------------------------
#define MISSION_ONLY_STATION "Arches"

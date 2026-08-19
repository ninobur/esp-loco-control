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

// ---------------------------------------------------------------------------
// IR Test A (docs/QUORUM_1_16R_IR_TEST_A_FIRMWARE_SPEC.md §2). Otto carries the
// sensor car as of 2026-08-18, taking it over from Toby: the phantom markers
// this layer exists to discriminate are Otto's. His 2026-08-17 night session
// logged 21 LOW_PWM-gated acceptances, ~half carrying the same weak signature
// (peak 38-49) that gets QUARANTINED at working PWM — accepted unvalidated only
// because navLadder's velocity model switches off below GATE_LOW_PWM_FLOOR.
// IR measures speed in exactly that regime.
//
// The sender registers ONE peer, so this is a hand-off, not an addition: while
// Otto is paired, Toby's profile must keep IR_TEST_A_ENABLED absent.
//
// The MAC is the Test Car ESP32's STA MAC — fill in at bench pairing (spec
// §11.3 step 1; the sender prints it at boot). ALL-ZERO IS A SAFE PLACEHOLDER:
// reception stays disabled and is reported; it never falls back to accepting
// any sender.
// ---------------------------------------------------------------------------
// #ifndef-guarded so an EARLIER definition wins. tests/harness.cpp defines both
// of these before including QUORUM.ino, so that the receiver paths are testable
// "regardless of the compiled profile" (harness.cpp:52). That was only ever true
// while Otto's profile had no IR block: an unconditional #define here is seen
// AFTER the harness's, silently overrides it, and every fixture packet is then
// rejected as bad-source — 17 IR tests fail with state NO_SENSOR and no hint of
// why. Nothing in a real build defines these first, so the guard changes no
// flashed behaviour.
#ifndef IR_TEST_A_ENABLED
#define IR_TEST_A_ENABLED 1
#endif
#ifndef IR_SENSOR_MAC_BYTES
#define IR_SENSOR_MAC_BYTES {0xEC,0xE3,0x34,0x78,0xA2,0x60}   // IR Test Car, bench-paired 2026-08-18
#endif

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
// CTO3 Station Stop: the v1 Arches-only filter (MISSION_ONLY_STATION,
// 2026-08-08) was REMOVED here by operator direction 2026-08-08 after the
// inaugural Arches cycle validated the phase chain — all four stations now
// arm with their existing R21 profiles. The stationEnabled() mechanism
// remains in the sketch for future missions.
// ---------------------------------------------------------------------------

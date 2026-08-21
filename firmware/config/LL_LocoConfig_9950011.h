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
// ---------------------------------------------------------------------------
// 2026-08-20, PHANTOM GATE. The operator realigned Otto's Hall sensor this
// afternoon and his mean peak rose 146.7 -> 176.4 (Toby sits at ~193). That fix
// exposed the real defect: with genuine markers now reading 104-295, a
// population of SPURIOUS events at peak 40-91 and duration 40-58 ms became
// unmistakable. They were always there; before the realignment they overlapped
// the genuine distribution and could not be separated.
//
// Measured, 596 markers after the realignment:
//     genuine reads   585, peak 104 minimum, duration 116 ms minimum
//     spurious reads   11, peak  91 maximum, duration  40-58 ms (9 of them)
// A clean gap from 91 to 104, with nothing in between.
//
// These phantoms cost a NO_QUORUM at 14:30:51. Reads of peak 40, 51, 49 and 43
// at mm 16, 19, 20, 21 were admitted as markers, advancing navMm ahead of the
// physical locomotive; the polarity sequence desynchronised, the quorum opened
// and never resolved.
//
// The entry threshold is baseline +/- (DEADBAND + ENTRY_MARGIN) = 25 + 13 = 38
// counts, and EVENT_FLOOR_MS is 40 ms. The phantoms cleared BOTH by a hair.
// Raising the entry margin to 65 puts entry at 90 counts: above every observed
// phantom, and 14 counts below the weakest genuine read.
//
// NOTE for whoever reads this next: HALL_MIN_PEAK_DELTA below gates NOTHING.
// It appears only in the boot telemetry string. So does HALL_DOMINANCE_PERCENT
// in Toby's profile. Changing either has no effect on detection; the entry
// threshold is the only amplitude gate there is.
//
// RISK, stated plainly: a genuine marker whose peak falls below 90 counts is
// now MISSED, and a missed marker drifts position - the failure that bit Otto
// in July when he ran on the 50/80/80 fallbacks. The 14-count margin rests on
// ONE session at one temperature (92 F). If Otto starts missing markers, this
// is the first thing to revisit.
// ---------------------------------------------------------------------------
//
// 2026-08-20, SAME EVENING — 65 was too aggressive and is reduced to 45
// (entry 70). The 90-count gate ran eight minutes with 208 markers and ZERO
// disagreements, the cleanest stretch Otto produced all day, and then his
// position fell FOUR markers behind the railway around mm 100-125: quorum
// adopted offset +4 at 16:11:12 and gave up at mm 123. The gate was rejecting
// genuine markers in a stretch that runs weaker than the 596-marker sample
// used to set it (whose minimum was 104).
//
// 70 keeps the phantom cluster out (peaks 40-66) and restores 34 counts of
// headroom under the weakest genuine read measured.
//
// AND A CORRECTION TO THE WATCH SIGNAL RECORDED IN 0040: steps of 2 in the
// marker sequence do NOT reveal missed markers. A rejected event never reaches
// telemetry, so the next accepted marker still reads mm+1 and the step stays
// 1 while the position silently lags. 241 of 247 steps were 1 while four
// markers went missing. The real signature is the quorum adopting POSITIVE
// offsets, and dt_conserve_ratio drifting toward 2.
#define HALL_ENTRY_MARGIN_COUNTS   45
#define HALL_MIN_PEAK_DELTA        35

// ---------------------------------------------------------------------------
// 2026-08-21 TRIAL: quarantine physical floor 350 -> 500 ms, Otto only.
// Operator direction. Rationale and bound recorded at Q_FLOOR_MS in QUORUM.ino:
// shortest map interval 280 mm, fleet maximum measured 400 mm/s = 700 ms, so
// 500 ms implies 560 mm/s and keeps a 1.4x margin. Toby stays at 350 as the
// control.
//
// What this buys: the quarantine's three criteria (weak, too soon, opposite
// polarity) currently only vote below 0.40 x median interval ~= 400 ms, while
// the absolute floor already rejects below 350 ms. Raising the floor covers
// the 350-500 ms band outright.
//
// What it risks: dt is measured from the last RECEIVED event, phantoms
// included, so a genuine magnet arriving within 500 ms of a phantom is
// quarantined - and quarantine's ambiguous default is PHANTOM (2026-08-20:
// 3 committed against 60 discarded). A wrongly quarantined marker becomes a
// missed marker and a position lag. Watch for quorum adopting POSITIVE offsets.
// ---------------------------------------------------------------------------
#define Q_FLOOR_MS_OVERRIDE 500

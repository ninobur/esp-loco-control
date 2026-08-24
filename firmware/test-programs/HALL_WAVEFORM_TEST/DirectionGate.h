// ============================================================================
// DirectionGate.h — HALL_WAVEFORM_TEST direction-change safety gate
//
// INVESTIGATORY / UNAPPROVED. Diagnostic instrument only.
//
// Pure C++ (no Arduino, no globals, no I/O), like HallCapture.h — so the
// decision itself is host-testable, not just grep-checked in the .ino.
//
// CODEX safety review, 2026-08-24, corrected 2026-08-24: matches established
// operator behavior — Blynk has refused every direction selection while the
// locomotive is moving, FORWARD, REVERSE and NEUTRAL alike, for about a
// year; the Flask console's own omission of NEUTRAL never changed that.
//
// The rule: a direction request is accepted ONLY when the motor is fully at
// rest — no PWM applied (rampCurrent == 0) and nothing commanded
// (rampTarget == 0). A refused request changes nothing: not the software
// direction, not the physical pin, not PWM, and it does not itself begin
// stopping the locomotive. The operator must STOP or E-STOP first, then
// reissue the direction request.
//
// Both call sites (BLYNK_WRITE(VPIN_DIRECTION) and the DIR command) call
// this SAME function, so they cannot diverge from each other by construction.
// ============================================================================
#pragma once

// Result of a direction request, computed purely so every field can be
// asserted directly in a test — no mocking of Serial, Blynk or digitalWrite.
struct DirectionOutcome {
  bool applied;        // true: the caller should set manualDirection (and the
                        // pin, if writePin) to the requested direction.
                        // false: the caller must change NOTHING.
  bool writePin;        // valid only when applied. NEUTRAL is a software-only
                        // state and never writes the physical pin, refused or not.
  int  echoDirection;   // what a UI control (Blynk) should show: the
                        // requested direction if applied, otherwise the
                        // direction that was already in effect — a refusal
                        // must never leave the UI showing an unapplied request.
};

static inline DirectionOutcome decideDirectionRequest(
    int requestedDirection, int rampCurrent, int rampTarget,
    int currentDirection, int neutralValue) {
  DirectionOutcome o;
  o.applied = (rampCurrent == 0 && rampTarget == 0);
  if (o.applied) {
    o.writePin      = (requestedDirection != neutralValue);
    o.echoDirection = requestedDirection;
  } else {
    o.writePin      = false;
    o.echoDirection = currentDirection;
  }
  return o;
}

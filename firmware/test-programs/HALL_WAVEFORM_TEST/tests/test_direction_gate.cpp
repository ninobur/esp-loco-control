// ============================================================================
// test_direction_gate.cpp — host tests for DirectionGate.h
//
// INVESTIGATORY / UNAPPROVED.
//
// Build and run:  ./run_tests.sh
//
// Exercises the REAL decision function decideDirectionRequest() (the exact
// code both BLYNK_WRITE(VPIN_DIRECTION) and the DIR command call) against
// every property the CODEX safety review required:
//   - FORWARD/REVERSE/NEUTRAL are each refused while moving;
//   - a refusal changes nothing (echoes the direction already in effect,
//     never writes the pin);
//   - all three choices work while fully at rest;
//   - the pin is written for FORWARD/REVERSE and never for NEUTRAL, whether
//     the request is fresh or a no-op reselection.
// The one property this file cannot exercise directly -- that a refusal
// never actually assigns manualDirection or calls digitalWrite in the .ino
// -- is because both call sites gate those statements behind `if (!o.applied)
// return;` before reaching them; that structure is checked by
// test_no_control_authority.py, which greps the .ino itself.
// ============================================================================
#include "../DirectionGate.h"

#include <initializer_list>
#include <stdio.h>

// Mirrors LL_LocoConfig's DIRECTION_* values (0 REV, 1 NEUTRAL, 2 FWD),
// already the convention this repo uses (see HwtDir in HallCapture.h).
static const int REV = 0, NEU = 1, FWD = 2;

static int failures = 0;
static int checks   = 0;

static void ck(bool cond, const char* what) {
  checks++;
  if (!cond) { failures++; printf("  FAIL  %s\n", what); }
}

// ---------------------------------------------------------------------------
// 1. Refused while moving -- FORWARD, REVERSE, NEUTRAL, each on its own.
// ---------------------------------------------------------------------------
static void testRefusedWhileMoving() {
  printf("refused while moving (FORWARD, REVERSE, NEUTRAL)\n");
  const int movingCases[][2] = {
    {50, 50},   // steady cruise
    {50, 0},    // decelerating, not yet at zero
    {0, 50},    // just commanded, ramp hasn't started climbing yet
    {1, 0},     // one count of PWM still applied
    {0, 1},     // one count still commanded
  };
  const int requests[] = {FWD, REV, NEU};
  const char* names[]  = {"FORWARD", "REVERSE", "NEUTRAL"};

  for (auto& mc : movingCases) {
    for (int r = 0; r < 3; r++) {
      DirectionOutcome o = decideDirectionRequest(requests[r], mc[0], mc[1], FWD, NEU);
      char msg[128];
      snprintf(msg, sizeof(msg), "%s refused at rampCurrent=%d rampTarget=%d",
              names[r], mc[0], mc[1]);
      ck(!o.applied, msg);
    }
  }
}

// ---------------------------------------------------------------------------
// 2. A refused request changes nothing: pin never written, and the echo
//    value is exactly the direction already in effect -- never the request.
// ---------------------------------------------------------------------------
static void testRefusalChangesNothing() {
  printf("refusal changes nothing (no pin write, echo is the accepted direction)\n");
  const int current = REV;
  for (int req : {FWD, REV, NEU}) {
    DirectionOutcome o = decideDirectionRequest(req, 10, 0, current, NEU);
    ck(!o.applied, "still refused while rampCurrent nonzero");
    ck(!o.writePin, "a refused request never asks for a pin write");
    ck(o.echoDirection == current,
       "a refused request echoes the direction already in effect, not the request");
  }
  // Requesting the SAME direction the motor is already moving in is refused
  // exactly as any other request -- moving is moving, regardless of whether
  // the request would have been a no-op.
  DirectionOutcome same = decideDirectionRequest(FWD, 40, 40, FWD, NEU);
  ck(!same.applied, "even a same-direction reselection is refused while moving");
  ck(same.echoDirection == FWD, "echo matches the direction already in effect");
}

// ---------------------------------------------------------------------------
// 3. All three choices work while fully at rest.
// ---------------------------------------------------------------------------
static void testAllThreeWorkWhileStopped() {
  printf("all three choices accepted while fully stopped\n");
  DirectionOutcome f = decideDirectionRequest(FWD, 0, 0, NEU, NEU);
  ck(f.applied, "FORWARD accepted at rest");
  ck(f.writePin, "FORWARD writes the pin");
  ck(f.echoDirection == FWD, "FORWARD echoes the requested direction");

  DirectionOutcome r = decideDirectionRequest(REV, 0, 0, FWD, NEU);
  ck(r.applied, "REVERSE accepted at rest");
  ck(r.writePin, "REVERSE writes the pin");
  ck(r.echoDirection == REV, "REVERSE echoes the requested direction");

  DirectionOutcome n = decideDirectionRequest(NEU, 0, 0, FWD, NEU);
  ck(n.applied, "NEUTRAL accepted at rest");
  ck(!n.writePin, "NEUTRAL never writes the pin, even when accepted");
  ck(n.echoDirection == NEU, "NEUTRAL echoes the requested direction");
}

// ---------------------------------------------------------------------------
// 4. The pin is written for FORWARD/REVERSE and only for FORWARD/REVERSE --
//    never for NEUTRAL, in any accepted case.
// ---------------------------------------------------------------------------
static void testPinWriteExactlyMatchesNonNeutral() {
  printf("pin write correlates exactly with non-NEUTRAL, when accepted\n");
  for (int req : {FWD, REV, NEU}) {
    DirectionOutcome o = decideDirectionRequest(req, 0, 0, NEU, NEU);
    ck(o.applied, "accepted at rest, any requested direction");
    ck(o.writePin == (req != NEU), "writePin is true iff the request is not NEUTRAL");
  }
}

// ---------------------------------------------------------------------------
// 5. The boundary itself: exactly at rest is the only accepting state.
// ---------------------------------------------------------------------------
static void testExactBoundary() {
  printf("exact zero/zero boundary\n");
  ck(decideDirectionRequest(FWD, 0, 0, NEU, NEU).applied,
     "0/0 is accepted");
  ck(!decideDirectionRequest(FWD, 1, 0, NEU, NEU).applied,
     "1/0 is refused -- current alone nonzero is still moving");
  ck(!decideDirectionRequest(FWD, 0, 1, NEU, NEU).applied,
     "0/1 is refused -- a pending target alone is still refused");
}

int main() {
  printf("HALL_WAVEFORM_TEST — direction gate tests (investigatory)\n\n");
  testRefusedWhileMoving();
  testRefusalChangesNothing();
  testAllThreeWorkWhileStopped();
  testPinWriteExactlyMatchesNonNeutral();
  testExactBoundary();
  printf("\n%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}

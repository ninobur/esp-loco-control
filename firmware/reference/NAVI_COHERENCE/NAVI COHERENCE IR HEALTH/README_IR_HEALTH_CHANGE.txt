NAVI_COHERENCE — IR HEALTH FIRST CHANGE
2026-09-22

Scope
-----
This package changes only the interpretation of IR instrument health in
MovementEvidence. It does not change Navigator position logic, Hall detection,
baseline acquisition, station logic, AUTO, RouteMap, or the 10-magnet
correction mechanism.

Files
-----
IrHealth.h
  New small health-classification layer.

MovementEvidence.h
  Replacement for NAVI_COHERENCE_0_5_AUTO_ENABLED/MovementEvidence.h.
  It includes IrHealth.h and stops treating TRACKING as the sole definition of
  a healthy IR instrument.

Key rule
--------
IR is a measurement instrument. Its data are used whenever the instrument is
available. There is no earn-trust period in this change.

AVAILABLE:
  TRACKING
  SIGNAL_STALE

UNAVAILABLE / initializing:
  PRIMING
  REACQUIRING
  INADEQUATE_CONTRAST
  SATURATION
  SAMPLE_GAP
  missing/stale transport

Why SIGNAL_STALE is AVAILABLE
-----------------------------
The detector sets SIGNAL_STALE after 2.5 seconds without a completed wheel
pulse. A locomotive intentionally stopped at a station will naturally do this.
No pulses is meaningful zero movement; it is not evidence that the optical
instrument failed.

What remains conservative
-------------------------
If measurement-integrity counters change between two endpoints
(unreliableSamples, saturatedSamples, sampleGaps, openAborts, inferred pulse
counters), that interval remains INTERRUPTED. This patch does not guess how
much distance was lost inside a demonstrably interrupted measurement interval.

Not yet changed
---------------
The IR detector itself still changes opticalReason as written today. This
package changes NAVI-side interpretation only.

No notification behavior is added here yet. The new MovementSource::health()
and available() accessors provide the state needed for the sketch to publish
IR AVAILABLE/UNAVAILABLE transitions in the next small change.

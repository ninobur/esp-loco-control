# NAVI_IR 0.3 — operator design ruling

Recorded 2026-09-20, in response to the NAVI_IR 0.1 field failure, the 0.2
fix, and a proposed IR-seeded LOST-recovery mechanism. Operator ruling,
verbatim:

That resolves it. I would encode NAVI_IR 0.3 around these rules:

1. NAVI remains the primary navigator. After declaration it advances through
   the known MM pattern, using Hall polarity plus the existing timing/physical
   gates. We are not replacing NAVI with IR.
2. IR acquires its own confidence. The first correctly identified Hall MM
   becomes the IR odometer/map anchor. For the next 10 accepted MMs, IR
   compares actual wheel travel with mapped MM-to-MM distance. Ten consecutive
   consistent comparisons establish `IR_CONFIDENT`.
3. IR becomes a continuous reality gate. Once anchored—even during
   acquisition—IR contributes to rejecting Hall signals that occur too soon
   to be the next mapped magnet. For 0.3, use both the existing timing gate
   and IR travel gate. A Hall waveform that cannot physically be the next MM
   should not advance NAVI merely because its polarity looks right.
4. IR maintains an independent map position estimate. Once confident, it
   continually knows approximately where the Hall sensor should be relative
   to the mapped magnets. It doesn't independently declare a magnet crossing;
   it predicts where the next one should occur.
5. Uncertainty invokes backup navigation, not surrender. If NAVI loses
   confidence, it retains the last confirmed MM and asks IR/map:

   ```
   Last confirmed MM: MM116
   IR travel since anchor: 1,247 mm
   IR predicts next landmark: MM120
   Predicted arrival: 14:32:18.4
   ```

   NAVI then effectively says: "I am uncertain. IR predicts the next
   observation should be MM120 at approximately this time. Test that
   hypothesis."
6. Hall confirms the reconnect. If the predicted magnet arrives in the
   appropriate distance/time window and its Hall identity agrees, NAVI can
   reconnect immediately. No second 10-magnet qualification is required. If
   it doesn't agree, keep reasoning; don't guess and don't invoke ONE STRIKE.
7. IR-invalid remains UNKNOWN. Bad optical quality, reboot, broken
   continuity, stale evidence, etc. removes IR's gating/recovery authority
   for that interval. It never becomes "zero movement."

This also answers the issue Claude raised. His IR-seeded reseed is useful,
but it should be the recovery portion of a larger continuous mechanism. IR
shouldn't suddenly wake up only after `LOST`; by then it should already have
been maintaining its independent relationship between wheel travel and the
map.

The resulting division of labor is quite clean:

- Hall: "I see a magnetic event with these characteristics."
- Timing + IR gates: "Could this possibly be the next mapped magnet?"
- NAVI: "Given the map and history, I accept/reject this event and maintain
  position."
- IR backup: "If you lose confidence, I have maintained independent travel
  evidence from our last confirmed landmark. Here is where you should look
  next."

I have enough now to implement 0.3 without another architectural question.

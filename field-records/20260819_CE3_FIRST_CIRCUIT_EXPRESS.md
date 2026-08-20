# 2026-08-19 — Circuit Express: first field validation, and four things it found

**Session:** two locomotives, CCW, AUTO. Otto (9950011) and Toby (9950012),
both on `ca27f1c` (`QUORUM_1_16R_IR_TEST_A`). Four CE cycles pressed, four
completed. Decision 0038; console button live since v1.9.5 with nothing
subscribed until tonight.

Toby also received, in one flash, decision 0037's inversion dissolution, the
1.16Ra radio instrumentation, and the IR odometry coverage fields — he had been
several revisions behind.

---

## 1. Verdict: CE works, on both locomotives, repeatedly

Every element of the specification was demonstrated on real track:

| | |
|---|---|
| assignment from existing roles | leader→EXPRESS, follower→LOCAL, every time |
| severance | symmetric, both locomotives, same second |
| **no immediate re-pair** | P1-A confirmed fixed in the field |
| express cruise / local cruise | PWM 110 / 75 as specified |
| express dwell | **4.997 s** measured |
| rotating skip | every third station met, no exceptions |
| ending | on the re-latched role, never on a distance |
| role swap | correct at all four closures |

**The rotation, proven end-to-end in run 3** — nine stations, three skips,
three *different* platforms omitted while all four were served:

| # | station | action |
|---|---|---|
| 1 | Arches | served |
| 2 | Grillers | served |
| 3 | **Patio** | **skipped** |
| 4 | Bamboo | served |
| 5 | Arches | served |
| 6 | **Grillers** | **skipped** |
| 7 | Patio | served |
| 8 | Bamboo | served |
| 9 | **Arches** | **skipped** |

Roles alternated across runs — Otto express, then Toby, then Otto, then Toby —
so the mission code was exercised on both locomotives.

**The architectural bet paid off.** No CE code watches for the express catching
the local. Layer 5's traffic protection decelerated it and Q1/Q2 re-derived the
roles, exactly as `PHYSICAL_ENVELOPE_NOTE.txt:13` predicted. Closures at
22:09:16, 22:18:08 and 22:35:06 all fell out of geometry.

**Counter reset is specified behaviour**, confirmed by the operator: the
"every third" count starts at the CE press, so the express serves two and skips
the third from wherever it happens to be. Which platform gets skipped therefore
depends on where CE was pressed, and in practice that varies — run 1 skipped
Bamboo first, run 3 skipped Patio.

---

## 2. The STALE fleet stops: diagnosed, and the first analysis was wrong

**14 fleet stops** (Toby 8, Otto 6). Same recurring failure as 2026-08-15 and
2026-08-16, undiagnosed until tonight because the channel instrumentation was
never flashed. It is now.

**Not a channel problem.** Both locomotives on `ch=11`, `chg=0` — no channel
change has ever occurred — and `txf=0` throughout. The field record's standing
hypothesis, *"a locomotive healthy on MQTT while its partner starves is a
channel problem"*, is contradicted by its own instrumentation. Both starve,
symmetrically.

**Packet loss, measured over both full CE runs**, status-against-status
(`ctoTxAttempts` vs peer `ctoRxAccepted`, both status-only):

| | received | lost |
|---|---|---|
| Otto → Toby | 83.2% | 16.8% |
| Toby → Otto | 87.0–87.7% | 12.3–13.0% |

Reproduced independently across run 1 (525 s) and run 3 (505 s), ~1000 packets
each.

**A correction, recorded because the error nearly set a threshold.** A first
analysis in session reported 30–38% loss. That was wrong. It was not the
`rx`-versus-`txd` confusion proposed in review — `ctoTxAttempts++` sits inside
`ctoTxStatus()` alone and `ctoTxEcho()` never touches it, so `tx` is
status-only and the pairing was correct. The error was **sampling**: a
20-second window off four heartbeats published ~5 s apart, with the two
locomotives' samples offset by ~2 s. At forty packets the quantisation
dominates; at a thousand the figure stabilises immediately.

**Average loss does not explain the stops, and must not be used to fix them.**
At 13–17% mean loss, six consecutive misses under independent loss would be
vanishingly rare, yet fourteen occurred. They cluster in time (ten transitions
inside ~21 s around 22:15) and in space (Otto mm 101→91 while Toby mm 168→157).
That is correlated positional RF dropout, not Bernoulli noise. Durations ran
0.002–4.04 s, median ~1.25 s.

`CTO_PEER_STALE_MS` is 3000 ms, inherited from r12 and never measured. At 2 Hz
it tolerates exactly six consecutive losses.

**Do not change it from an average.** A longer timeout chosen from mean loss
would conceal the burst structure rather than measure it. Instrument actual
inter-arrival gaps and consecutive-miss run lengths first.

---

## 3. Skipped platforms are run at full cruise

Confirmed by telemetry: Toby held PWM 110 and 357–392 mm/s straight through
Grillers without a flicker. The skip at [QUORUM.ino:2897](../firmware/QUORUM/QUORUM.ino)
`continue`s before the station arms, so the entire approach ladder —
`approachTargetForOffset`, zone hold, all of it — is bypassed.

**This was not a considered decision.** It fell out of the implementation and
0038 records it neither way. It is not evidence of a safety defect: the zone
speeds were tuned for controlled stops, not documented as platform speed
restrictions. Review recommends retaining full-speed run-through unless
appearance, platform convention or a measured track constraint argues
otherwise. **Either way it must be recorded in 0038 explicitly.**

---

## 4. Defect: a mission change is applied over 700 ms regardless of size

Reported by the operator watching the express accelerate implausibly fast, and
confirmed:

```
22:47:50  pwm=38
22:47:51  pwm=54     ← CE assigned
22:47:52  pwm=110    ← +56 PWM inside one second
```

The alert publishes `actualPwm`, so this is physically applied, not a display
artefact. `servicePwmRamp()` moves ±1 per `pwmStepMs`, so that is 56 discrete
steps at ~12 ms each — against the ~140 ms/step it had been using one second
earlier.

**Cause:** [QUORUM.ino:2914](../firmware/QUORUM/QUORUM.ino) —
`if(ctoDesiredPwm != want) requestPwmOver(want, APPROACH_RAMP_MS)` with
`APPROACH_RAMP_MS = 700`. `requestPwmOver` derives its step rate as
`durationMs / delta`, so **any** cruise change is compressed into 700 ms
whatever its magnitude. For an ordinary correction of a few PWM that is gentle;
for a 56-PWM mission change it is violent.

This is not CE-specific — it is the continuous cruise-correction path — but CE
is the first thing to make a large enough change for it to show.

---

## 5. Defect: `SKIPPED` cannot name the station it skipped

`SKIPPED` reports `station:"NONE"`. It is published before `stIndex` is
assigned, and `stationPublish()` derives the name solely from `stIndex`, so
`NONE` is the only value it can emit. Which platform was skipped had to be
inferred from position all evening.

Operationally harmless; it weakens the evidence for the very behaviour the
event exists to record. The fix is to pass the known station index to the skip
publication rather than temporarily moving the station machine's active index.

---

## 6. Other observations

- **Otto's disagreements are geographically concentrated** at mm 49–70 (mm 59
  four times; 58, 56, 51, 50 three each), with strong peaks (130–190)
  throughout. Same stretch each occurrence. Worth a look at that track section.
- **Three quorum self-resolutions, all successful** — 6 s, 2 s, and one longer.
  No `NO_QUORUM` all session, against two `HARD_BOUND` failures on 2026-08-16.
- **Speed varies 22% with position at constant PWM.** Otto at PWM 110 measured
  307–375 mm/s depending only on where he was. Compared like-for-like across
  runs 1 and 3 the mean difference was **−1 mm/s over 14 shared positions** —
  the locomotive is entirely repeatable, the *track* is not. This is the
  clearest argument yet for decision 0014's SPEED_HOLD.
- **A withdrawn concern, recorded so it is not raised again.** The pKPH figures
  in `SPEED_CONTROL_DISCUSSION.txt` (clean 50–69, cascade 78–82) predate the
  Hall firmware fix; the locomotives have since run at PWM 200 with no
  degradation of magnet sensing. Those numbers were cited twice in session as a
  live limit after the operator had already corrected them once. They are
  historical evidence about a superseded detector. Tonight supplied no evidence
  for restoring them.

---

## 7. State at end of session

**Flashed and verified by boot banner:** both locomotives on `ca27f1c`.

**Committed, not deployed** (`86e3539`): the CTO row shows EXPRESS/LOCAL
instead of "unpaired" while a mission runs — operator request, since
"unpaired" is true of the pairing and useless about what the train is doing.
Needs both a flash and a Pi deploy; the console field does nothing until the
firmware sends it.

**A verification failure worth not repeating.** Checking the compiled identity
with `strings firmware/QUORUM/build/...` is not a check: `arduino-cli` builds
to a temp directory unless given `--build-path`, so that folder is a stale
Arduino IDE artefact. It reported the wrong locomotive while the selector was
correct — indistinguishable from the wrong-profile bug the boot banner exists
to catch. Noted in `LocoConfig.h`.

**Open, in review's recommended order:**

1. Instrument inter-arrival gaps and consecutive-miss runs; only then
   reconsider `CTO_PEER_STALE_MS`.
2. Decide and record the skipped-platform speed policy in 0038.
3. Give `SKIPPED` the station name.
4. Decide whether a mission change should ramp gently (§4).
5. Otto's mm 49–70 disagreement cluster — physical.

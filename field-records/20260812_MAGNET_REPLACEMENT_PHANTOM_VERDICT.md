# Phantom Hall events — resolved. Field verdict, 2026-08-12

**Locomotive:** Otto (9950011), `QUORUM_1_13`, unchanged from the 2026-08-11 beta.
No firmware was modified, rebuilt or flashed for this test. The only variable is
the track.

**Capture:** `field-records/logs/20260812_otto_ccw_ramped_mm102_reseated.log`
(one 190.9 min capture containing two sessions — see "Segmenting the capture").

**Verdict: CONFIRMED. The phantom was stacked double magnets, and it is gone.**

---

## 1. The physical change

The operator replaced the magnets at **mm 99–102** and **mm 61–63** with disk
magnets glued to the tops of the crossties, in place of bar magnets buried in
ballast.

On extraction, **mm 102 was found to be a stacked double bar magnet** — two bars
face to face, like poles aligned, south upward, magnetically clamped and
indistinguishable from a single magnet without digging. It was one of the
remedies applied days earlier when that marker read weak.

This is the cause. Markers in ballasted sections sit further from the sensor,
read weak, and were "fixed" by adding a second magnet. The doubled magnet
produces a stronger and spatially broader field whose return flux, beyond the
magnet edge, still clears the ±38 enter threshold — opening a second event of
opposite polarity after `EVENT_EXIT_HOLD_MS` has closed the first.

**The weak-magnet remedy created the phantom.**

## 2. Result: the phantom is eliminated

Both sessions are in one capture, on one locomotive, on one firmware image. The
only difference is the magnets.

| | before replacement | after replacement |
|---|---|---|
| markers captured | 235 | 214 |
| **weak events (peak < 80)** | **12 (5.1%)** | **0 (0.0%)** |
| minimum peak observed | **39** | **113** |
| route-wide peak median | 149 | 149 |
| `PHANTOM_REJECTED` events | 4 | **0** |

Weak events before replacement occurred at mm 68, 69, 82, 86, 93, 96, 101 (×2),
102 (×2), 105, 112. After replacement **every marker 0–170 was crossed and
captured at least once, and every one of those sites read clean**:

| mm | 68 | 69 | 82 | 86 | 93 | 96 | 101 | 102 | 105 | 112 |
|---|---|---|---|---|---|---|---|---|---|---|
| peak after | 147 | 143 | 168 | 163 | 171 | 164 | **218** | **222** | 146 | 148 |

mm 101 and 102 produced a phantom on **9 of 9** ramped CCW crossings during the
2026-08-11 beta. They now produce a single clean read each.

### Amplitude is not the mechanism — doubling is

The replaced sites now read **stronger** than before (peak median at those
markers 150 → 222, against a route median of 149) and produce **no** weak
companion events. A strong single magnet does not phantom; a doubled one does.

This retires the absolute-peak-threshold framing of the superseded low-PWM
proposal, and it retires "the phantom sites are the strongest markers" as a
causal statement. The phantom sites were the strongest markers *because* they
were doubled, not the other way round.

## 3. Second finding: mm 99 is installed inverted

The CCW run produced one disagreement per lap, five laps. There is exactly one
polarity mismatch in the entire post-replacement session:

```
+149.76min  mm=99  obs=N  DNA=S  MISMATCH  peak=279  ms=197  dt=1208  pwm=90  gate=ACTIVE
```

Peak 279, 197 ms wide, conservation gate ACTIVE and passing. This is a strong,
correctly-detected magnet presenting the wrong face. The other six replacements
verify correct:

| mm | 102 | 101 | 100 | **99** | 63 | 62 | 61 |
|---|---|---|---|---|---|---|---|
| observed | S | S | N | **N** | N | S | N |
| DNA | S | S | N | **S** | N | S | N |
| | ok | ok | ok | **inverted** | ok | ok | ok |

QUORUM absorbed it on every lap — one corrupted bit in a 12-marker window does
not unseat the leader. **Action: flip the disk at mm 99.**

## 4. Third finding: the CW leg terminated in NO_QUORUM, and it is a different fault

After five CCW laps the operator reversed to CW. The leg ended in NO_QUORUM near
mm 120. The terminal snapshot survived:

```json
{"e":"NO_QUORUM","mm":132,"dir":"CW","since":12,
 "sc":[5,5,7,3,8,8],"ld":3,"ru":4,"mg":0,"ev":12,
 "adv":null,"advw":12,"advr":5,"advn":12,
 "ring":[["N",121],["N",122],["N",123],["N",124],["N",125],["N",126],
         ["S",127],["N",128],["S",129],["S",130],["N",131],["N",132]]}
```

- **HARD_BOUND.** Ring filled to `QUORUM_MAX` 12 without quorum.
- Offsets **+3 and +4 tied at 8/12**, margin 0 against `QUORUM_MARGIN` 2.
- **The advisory behaved exactly as designed** (decision 0023): full ring
  (`advn` 12 ≥ `advw` 12), no unique exact window, so it published `null` rather
  than a guess. The audit fields make the silence distinguishable from a short
  ring. This is the first field exercise of the 1.13 advisory and it passed.

The ring carries **six consecutive N at claimed mm 121–126**. The map has no
six-N run in that region; the ring scores only 5/12 at offset 0. The nearest such
run in `NGR_DNA1` is indices 107–113, which would place the locomotive at offset
**−14** — far outside `QUORUM_OFFSETS` — and even that scores only 8/12.

**The true offset was outside the fence.** Same class as the 2026-08-10 incident
C recorded in `docs/QUORUM_PRIOR_AWARE_ADJUDICATION_DESIGN_NOTE.md`. This is not
the phantom, it is not fixed by the magnet work, and it is not addressed by
anything in decision 0024.

**Root cause not established.** See the next section for why.

## 5. The capture is incomplete — CORRECTED 2026-08-12: the Mac slept

`agree` reached 1047 and `disagree` 28 — about **1075 markers processed, 214
captured**. The entire CW leg between mm 5 and the failure near mm 120 is
missing.

> **This section originally read:** *"`capture.sh` logged zero RECONNECT lines,
> so the subscriber never lost the broker. The messages were never delivered.
> The broker runs on the Pi whose SD card went read-only on 2026-08-11 and has
> not been replaced. A degraded broker drops QoS 0 publishes silently."* and
> concluded *"The SD card must be replaced before the next CW run."*
>
> **That was wrong, and it was wrong in a way worth recording.** The reasoning
> inverted the meaning of its own strongest fact: zero RECONNECT lines was read
> as proof the subscriber was healthy, when it was in fact proof the detector
> could not fire. See decision 0026.

The measured cause: the loss falls inside **three total-silence windows** —
1117 s, 626 s, 374 s — with no message from any publisher on any topic. Across
all three the locomotive's `mqtt_attempts` stayed at **1** and `uptime_ms`
advanced exactly with wall time, so it never reconnected and never rebooted; its
`agree` counter advanced 511, 244 and 80 into the holes, which is 835 of the
~860 missing. Each window ends with a burst of **retained topics only** — the
resubscribe signature. `pmset -g log` matches every boundary to a sleep/wake
transition within seconds: `Idle Sleep` 22:32:55 against last data 22:32:56,
`DarkWake` 22:51:32 against resumption 22:51:33, and so on.

**The MacBook was on battery and idle-slept three times.** `mosquitto_sub` 2.1.2
reconnects internally via `mosquitto_loop_forever`, so it never exited and
`capture.sh`'s reconnect logging — which assumed it would — never ran.

Consequences for this record:

- Sections 2 and 3 rest on markers that **did** arrive, including full route
  coverage 0–170, and stand unchanged.
- Section 4 rests on the terminal snapshot alone. It establishes *what* failed,
  not *where the divergence began*.
- ~~The SD card must be replaced before the next CW run.~~ **The SD card is
  exonerated for this loss and is not a blocker for re-running CW.** It should
  still be replaced — it did cost evidence on 2026-08-11 — but it would not have
  saved this session, and capture no longer runs on the Pi.
- `capture.sh` now holds the host awake and writes `# STALL` lines. Decision 0026.

## 6. Segmenting the capture

One `capture.sh` process spanned both sessions. Segment on the nav reset at
+143.99 min (`dir` UNSET → CCW, `nav` UNSET → NORMAL, `agree`/`disagree` reset).

| span | session |
|---|---|
| +0.0 … +10.0 min | before replacement — retain as the control |
| +144.0 … +190.9 min | after replacement — the test |

The file name says `mm102_reseated`; reseating was the *earlier* remedy and did
not fix anything. The pre-replacement portion still shows two weak events each at
mm 101 and 102.

## 7. State at end of session

Otto is stopped in `NO_QUORUM`, `auto=1`, `pwm=0`, last confirmed mm 120,
dead-reckoned mm 132. Recovery requires a DECLARE at its true physical position,
as at +206.3 min in the earlier session.

## 8. Actions

1. **Flip the disk at mm 99.** Confirmed and isolated.
2. **Recover Otto** with a DECLARE at its actual position.
3. **Replace the Pi SD card** before the next CW run.
4. **Re-run CW with a verified capture.** Root-causing the CW divergence needs
   the marker trail, which this session did not produce.
5. **No firmware change.** See decision 0025.

## References

- `docs/QUORUM_PHANTOM_HYPOTHESES.md` — hypotheses, now resolved
- `docs/decisions/0025-*` — the phantom is a maintenance artefact
- `docs/decisions/0024-*` — containment analysis, now not to be implemented
- `docs/decisions/0023-*` — the advisory, exercised successfully here
- `field-records/20260811_QUORUM_1_13_beta_verdict.md` — the 9/9 baseline

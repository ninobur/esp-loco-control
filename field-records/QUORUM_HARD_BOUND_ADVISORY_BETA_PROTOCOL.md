# HARD_BOUND advisory — controlled beta test protocol

**Build under test:** commit `723f0b4`, sketch `QUORUM_1_12C`
**Scope:** the diagnostic HARD_BOUND advisory and the replay harness. Nothing else.
**Not in this build:** the low-PWM phantom fix (documented only), any fence or
adoption change.
**Operational change: none.** Navigation, stopping, the station machine and
recovery are byte-for-byte unchanged; proven by `verify_inert.py` against both
`70cab5b` and `160eff5`.

---

## 0. Before flashing

1. **Preserve the running image.** The advisory does not change behaviour, but
   rollback should not depend on that being true.
   ```bash
   esptool.py --port <PORT> read_flash 0x0 0x400000 otto_pre_723f0b4.bin
   ```
2. **Record the commit** in the run log: `723f0b4`.
3. **Note a gap in build identity.** `SKETCH_NAME` is still `QUORUM_1_12C` —
   unchanged, because changing it was outside the approved scope. A log
   therefore cannot be attributed to this build by sketch name alone. Test 1
   below closes that gap in ten seconds, and should be run first.
4. **Clear the retained ghost.** `ngr/loco/9950011/mm/no_quorum` is retained.
   A stale snapshot from 2026-08-10 will otherwise appear the moment anything
   subscribes and will be mistaken for a fresh result. Confirm it is cleared by
   a declaration before starting.

---

## 1. Build-identity stamp, and the null case (do first, 10 seconds, stationary)

Publish to `ngr/loco/9950011/cmd/force_lost`:

```
NOQUORUM
```

**Expect** on `ngr/loco/9950011/mm/no_quorum`:

```json
{"e":"NO_QUORUM", ... ,"adv":null,"advw":12,"advr":5,"advn":<0-12>, "ring":[...]}
```

- The presence of `adv`/`advw`/`advr`/`advn` **is** the proof the loco is
  running `723f0b4`. No other build emits them.
- `adv` **must** be `null`. This is `FORCED_BY_FIXTURE`, not `HARD_BOUND`, and
  the advisory is scoped to HARD_BOUND alone.
- Then declare position to clear the state.

A non-null `adv` here is a **blocking defect** — stop the test.

---

## 2. Normal running unchanged (passive, whole session)

Run the ordinary schedule. Watch for:

- no change to station stops, dwell, ramps, or cruise;
- `state/nav` AGREE/DISAGREE, `QUORUM_OPEN`/`TIED`/`ADOPTED`/`CLOSED` behaving
  as before;
- `mm/no_quorum` published **only** at a genuine terminal event.

---

## 3. Provoked positive case — the advisory actually advising

A natural HARD_BOUND cannot be scheduled, and when one occurs the advisory only
speaks if the true offset is within ±5. On 2026-08-10 that was 1 incident in 3.
This provokes the positive case deliberately.

**Method.** Place the locomotive physically **at a known landmark**, declare a
position **3 or 4 markers AHEAD** of where it actually is, then drive CW. The
odometer is now ahead of truth by more than the fence's −1 limit, so no
candidate can be correct and HARD_BOUND is the right outcome — while the
evidence ring still matches the true position exactly.

**Run this in MANUAL.** In MANUAL the navigator observes and publishes but never
writes to the motor (§0.2 bicameral rule), so a deliberately wrong position
cannot drive the train anywhere unexpected. In AUTO the station machine would
arm against the wrong position and could stop up to ~1.2 m off a platform.

Predicted outcomes, from the replay harness against this exact build:

| place the loco at | declare | drive CW | stops reporting | **must advise** |
|---|---|---|---|---|
| Grillers (63) | `66` | ~16 markers, 4.8 m | mm 82 | **79** |
| Bamboo (157) | `161` | ~16 markers, 4.8 m | mm 6 | **2** |
| Arches (107) | `111` | ~16 markers, 4.8 m | mm 127 | **123** |
| Southpoint (0) | `4` | ~16 markers, 4.8 m | mm 20 | **16** |
| Patio (15) | `18` | ~18 markers, 5.4 m | mm 36 | **33** |
| Eastpoint (140) | `143` | ~24 markers, 7.2 m | mm 167 | **164** |

In every case the advised marker **is the locomotive's true physical position**
— walk to it and check. Marker counts assume ~300 mm spacing and no missed
reads; allow a few extra markers.

Across 1 197 simulated provocations (7 offsets × 171 start positions) this
produced **zero wrong non-null advisories**. Declaring only 1 ahead produces no
stop at all, correctly: offset −1 is inside the fence and QUORUM recovers
unaided. Declaring 6 or more ahead goes silent, correctly: beyond the ±5 search.

Declare the true position afterwards to recover.

---

## 4. Natural HARD_BOUND (opportunistic)

If one occurs, capture the whole `mm/no_quorum` payload. Either result is a
pass:

- `adv:<marker>` — walk to that marker and confirm the locomotive is there.
- `adv:null` with `advn:12` — no exact unique window within ±5. Expected when
  the true offset exceeds 5 (incident A was +8) or readings are corrupted
  (incident B). Note the physical position anyway; it is the evidence that
  decides whether widening the search radius is worth proposing.

---

## 5. Capture

- `ngr/loco/9950011/#` for the whole session — the retained
  `mm/no_quorum` payload is the object under test.
- `mm/marker` (this carries `peak`, `ms`, `pwm`, `timing_gate`),
  `state/nav`, `state/loopstat`, `state/station`, `state/throttle`.
- Boot serial, for `[BOOT] QUORUM_1_12C`.

`state/throttle` matters more than it looks: it is the only source for
reconstructing `pwmCommandedAtDetect`, without which the replay harness cannot
verify a future capture.

---

## 6. Pass / fail

**Pass**

- Test 1 gives `adv:null` and the `adv*` fields are present.
- Normal running is indistinguishable from before.
- Every provoked case advises the true marker.
- Any natural HARD_BOUND either advises the true marker or advises nothing.
- Operator declaration remains the only recovery. `nav_ready 0` persists until
  a declaration.

**Blocking defect — stop the test**

- Any non-null `adv` that is **not** the locomotive's true position.
- Any non-null `adv` on a reason other than `HARD_BOUND`.
- Any change to stopping, station or recovery behaviour.
- The locomotive resuming without a declaration.

---

## 7. Known-unfixed, and the opportunity in this session

The low-PWM phantom defect is **live in this build**. Station departures can
still inject phantom marker events at pwm < 40, which is what displaced the
odometer by 2 on 2026-08-10 and led — via a wrong +3 adoption — to a 5-marker
error. If a wrong adoption or an unexplained displacement appears in this
session, that is the known defect, not a regression from the advisory.

That makes this session the natural place to gather the evidence the fix is
waiting on. It costs nothing extra: run the ordinary Bamboo and Grillers
departures and keep the log. `docs/QUORUM_LOW_PWM_PHANTOM_DESIGN_PROPOSAL.md`
§7 asks for 20–30 logged departures to turn a 7-point inference into a
distribution. Do not avoid the stations — they are the measurement.

A slow pass through **mm 66–82** at pwm ≤ 90 is the other outstanding
measurement: every pwm ≥ 110 reading in the 2026-08-10 run falls in that
stretch, so speed and position are confounded and that capture cannot say
whether the 14.9% misread rate there is a marker fault or a detection-speed
limit.

---

## 8. After the run

Replay the new capture through the harness:

```bash
python3 firmware/QUORUM/tests/extract_fixture.py --capture <new.log>
python3 firmware/QUORUM/tests/run_suite.py
```

The fidelity check will state how faithfully the session replays. Anything the
firmware did that the harness cannot reproduce is itself a finding.

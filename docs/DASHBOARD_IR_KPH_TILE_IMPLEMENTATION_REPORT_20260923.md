# Dashboard — IR KPH tile — implementation report

**Historical report, superseded for conversion and Toby's source attribution
by decision 0099 and NAVI_IR_SPEED_TELEMETRY_20260923.md.** The old 0.162
examples below document the prior display, not the current definition. Toby's
bare `telem/speed` is Hall-derived, never independent IR speed. The new IR
tile uses additive `telem/ir` speed fields; no bare-number IR fallback remains.

**Describes commit d3daf91 (`server/ngr_app_v1_11_2.py`, loco page only).**
Firmware untouched. No MQTT topic or payload format changed.

---

## What was asked

The per-loco page's MM / KPH / PWM tile row (`num-row`) should show a
fourth value: IR-measured speed, converted through the **same** mm/s ->
km/h scaling the KPH tile already uses, not raw mm/s.

## What changed

Added a fourth `num-cell`, **IR KPH**, computed client-side in
`pollState()` from `speed_view.ir_mmps` (the same `telem/speed` /
`quorum-speed-view/1` payload `irRow()` already parses for the fleet
console):

```js
irPkph = Math.round(sv.ir_mmps * PKPH_PER_MM_S)   // '--' if ir_valid is false
```

`PKPH_PER_MM_S` is the server's existing `3.6 * 45.0 / 1000.0` constant
(the one `est_mm_s` goes through to make the KPH tile), now injected into
the page's JS via `render_template_string(..., pkph_per_mm_s=PKPH_PER_MM_S)`
so the two tiles share one source of truth instead of a second hand-copied
constant that could drift from the first.

`speed_view` was added to `AGE_FIELDS` so the new tile ages and greys the
same way MM/KPH/PWM do, off the real age of the last `telem/speed` message.

`.num-row` went from a 3-column to a 4-column grid; `.big-num` dropped
46px -> 38px and `.num-lbl` 19px -> 15px (letter-spacing 2px -> 1px) so
four tiles fit a phone width without wrapping. MM/KPH/PWM keep their
existing colors; IR KPH is a new light purple (`#c9a0ff`), unused
elsewhere on the page.

## Anomaly found, not fixed silently

`speed_view` already carries a **firmware-computed** `ir_pkph` field —
but the firmware's scaling constant is `PKPH_PER_MMPS = 1.0f/5.37325f`
(~0.1861), not the dashboard's `3.6*45/1000` (0.162). Same name, same
intent ("mm/s at the sensor -> prototype km/h"), ~15% different answer.
This tile deliberately uses the dashboard's own constant (recompute from
`ir_mmps`, ignore the firmware's `ir_pkph`) because that is what was
asked for ("same conversion as KPH") and because the KPH tile the
operator reads every run is anchored to the dashboard constant, not the
firmware one. The firmware's `ir_pkph` remains computed, transmitted, and
now permanently unused by any consumer in this repo. Left as found —
worth a look at some point to decide which constant is actually right,
or whether they are answering two different questions on purpose.

## Verification

No live locomotive/broker in this environment, so verified by seeding
`loco_state` directly (bypassing MQTT) and running the Flask app locally:

- `render_template_string(LOCO_HTML, ...)` renders clean — confirms the
  new `{{ pkph_per_mm_s }}` Jinja substitution doesn't collide with any
  literal `{`/`}` elsewhere on the page.
- Seeded `mm=013, pkph=15.0, pwm=60, speed_view.ir_mmps=130.0 (ir_valid=1)`
  at 375x812 (mobile): all four tiles render on one line, no wrap —
  `013 / 15 / 60 / 21` (130 * 0.162 = 21.06, rounds to 21) — with correct
  fresh (colored) and stale (grey, "Ns ago") styling as the seed ages past
  5 s.
- Seeded `ir_valid=0`: IR KPH tile reads `—` (em dash), grey, while
  MM/KPH/PWM stay fresh/colored — confirms an invalid IR reading doesn't
  get rendered as a fabricated number, matching `irRow()`'s "no signal"
  convention on the fleet console.
- `python3 -m py_compile` clean before and after.

Not field-tested against a real IR sensor packet stream — the payload
shape (`ir_valid`/`ir_mmps` field names and semantics) was taken from
`irRow()` and the firmware's own `quorum-speed-view/1` publisher
(`NAVI_CL2.ino`, `QUORUM.ino`, `NAVI_2.ino`), not observed live.

## Note on this commit's scope

`server/ngr_app_v1_11_2.py` had substantial unrelated uncommitted work
already sitting in the working tree when this job started (NAVI
ruling/evidence display, `telem/ir` link heartbeat, HALL BASELINE panel —
apparently mid-flight from an earlier session, per CLAUDE.md's "own
commits, do not combine"). This commit contains **only** the IR KPH tile;
the pre-existing changes were left staged in the working tree exactly as
found, untouched and still uncommitted.

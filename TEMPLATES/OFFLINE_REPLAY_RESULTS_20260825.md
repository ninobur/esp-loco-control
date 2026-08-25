# TEMPLATES offline replay — 2026-08-25

**Tool:** `tools/templates_replay.py`  
**Tests:** `tools/test_templates_replay.py`  
**Status:** reproducible investigatory evidence; no firmware authority.

## Candidate under test

- artifact if duration < 80 ms or peak < 60 counts;
- merge a later credible excursion opening within 350 ms;
- fixed 280 ms arrival placeholder (effectively subsumed by the merge window);
- expected polarity checked before advance;
- forward omission hypotheses 0..4;
- perfect unique explanation after at least six credible observations;
- safe stop after twelve unresolved credible observations.

The overlapping 280/350 ms placeholders reveal that the event-summary model
does not yet express the physical-arrival boundary cleanly. They are recorded,
not defended.

## Full-capture result

| Capture/session | Replay events | Genuine proxy survived | Deliberate omissions of genuine proxy | False coordinate insertions (phantom proxy) | Recoveries | Safe stops |
|---|---:|---:|---:|---:|---:|---:|
| Toby CCW `D7651658` | 1,202 | 1,197 / 1,197 | 0 | 0 / 1 | 0 | 0 |
| Otto CCW `A3201FFF` | 505 | 379 / 379 | 0 | 0 / 58 | 0 | 1 |
| Otto CW pre-reboot `A3201FFF` | 38 | 1 / 1 | 0 | 0 / 14 | 0 | 1 |
| Otto CW restarted `77943FAD` | 997 | 898 / 898 | 0 | **14 / 56** | 1 | 1 |
| Otto change-1 `3E6A88F1` | 187 | 34 / 34 | 0 | 0 / 59 | 0 | 1 |
| Otto change-1 `103B1969` | 88 | 10 / 10 | 0 | 0 / 17 | 0 | 1 |

The restarted Otto CW recovery adopted one omitted-marker hypothesis after six
observations and 5.034 seconds, then later stopped safely. Without independent
physical anchors that adoption cannot be classified as correct; it is not
claimed as a success.

Artifacts are structurally excluded from the recovery set. Proxy
observation/recovery contamination is emitted by the tool for audit. Absolute
position drift and incorrect adoption remain unmeasurable without independent
anchors.

## Synthetic result

Five deterministic tests pass:

- a 43 ms / 39 count spike has no standing;
- an expected broad passage advances exactly once;
- one deliberate omission recovers from a uniquely matching credible sequence;
- an unresolvable credible stream stops safely and cannot advance afterward;
- an opposite-polarity companion lobe within the merge window is not counted
  as a second marker.

## Reproduction

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/test_templates_replay.py -v
PYTHONDONTWRITEBYTECODE=1 python3 tools/templates_replay.py \
  /private/tmp/toby_ccw_20260824.qtcap \
  /private/tmp/otto_ccw_20260824.qtcap \
  /private/tmp/otto_cw_20260824.qtcap \
  /private/tmp/otto_change1_20260825.qtcap --json
```

## Verdict

**Rejected as a firmware candidate.** It passes Toby protection and synthetic
omission recovery, but it does not meet the governing Otto acceptance
criterion under the conservative phantom proxy. The next iteration must use
waveform features rather than only closed-event duration and peak, and must add
independent physical truth anchors before reporting absolute drift or recovery
correctness.

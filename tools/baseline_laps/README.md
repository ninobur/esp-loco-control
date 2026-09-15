# baseline_laps — Hall baseline mining and whole-lap controller replay

Reads NGR run logs, cuts them into sessions and laps the way the navigator
anchors position, and replays a proposed whole-lap baseline controller over the
result.

**This is analysis, not firmware.** Nothing here changes behaviour, and the
rolling one-second median is treated as measurement only — under X17 it has no
navigation authority and these scripts grant it none.

Reports:
[methodology and dataset](../../docs/NAVI_BASELINE_LAP_DATASET_20260914.md) ·
[controller replay and candidate sweep](../../docs/NAVI_BASELINE_CONTROLLER_REPLAY_20260914.md)

## Scripts

| script | what |
|---|---|
| `extract_baseline_laps.py` | logs → `sessions.csv`, `origins.csv`, `laps.csv`, `markers.csv`, `observations.csv` |
| `validate_baseline_laps.py` | structural checks on the extraction + reconciliation against figures reported by hand |
| `replay_baseline_controller.py` | replays controller candidates; candidate/lap/safety CSVs |
| `validate_replay.py` | unit tests of the controller model — origin, direction change, reboot, new `SET LOCATION`, incomplete laps, rounding, the escape rule |

## Regenerate

```bash
python3 tools/baseline_laps/extract_baseline_laps.py --out tools/baseline_laps/out --compare
python3 tools/baseline_laps/validate_baseline_laps.py --out tools/baseline_laps/out
python3 tools/baseline_laps/validate_replay.py        --out tools/baseline_laps/out
python3 tools/baseline_laps/replay_baseline_controller.py \
    --out tools/baseline_laps/out --normal-cap 0,1,2,3,4 --inlap-only \
    --setloc-resets-baseline yes --detail "cap2/noescape,cap2/P2/T10/X8/newest"
```

`--logs` defaults to `~/ngr-telemetry/pi/NGR/telemetry/runs` (the archive the
nightly `tools/fetch_pi_telemetry.sh` fills — never the working tree, which git
can rewrite under a running capture). `--loco` defaults to 9950011, `--date` to
20260914 and accepts `all`.

## Lap definition

`ROUTE_N = 171`. **The lap origin is the location supplied by `SET LOCATION`,
and it holds for the rest of the boot session.** A complete lap is 171
firmware-accepted advances ending on that origin marker.

- A **direction change** does not move the origin. It discards the lap in
  progress; counting resumes toward the same origin, starting at the next
  arrival there.
- A new **`SET LOCATION`** opens a new origin and resets controller state.
- A **reboot** resets everything; a session is one boot.
- Rejections, refusals and withdrawals republish `adv` unchanged and do not
  advance the lap.

The firmware's `adv` counter also resets at a direction change, so it is **not**
a valid anchor — it is used only to establish that an advance was accepted. The
old `adv`-epoch cut and the MM000-wrap cut are emitted under `--compare` for
reconciliation and nothing else.

## Controller parameters

`--normal-cap` (0 reproduces X17 as shipped) · `--persistence` (0 disables the
escape rule) · `--threshold` · `--exceptional-cap` (`inf` allowed) · `--target
newest|median` · `--estimator` · `--rounding estimate|correction_nearest|correction_trunc`
· `--setloc-resets-baseline yes|no` · `--inlap-only` · `--detail` · `--prefix`.
All accept comma-separated lists and sweep the product.

## Gotchas found the hard way

- Deduplicate `uptime_ms` **per session**, not per file: `uptime_ms = 1000`
  recurs at every boot.
- A marker that did not advance republishes the same `adv`. Treating that as a
  reset shatters a session into dozens of one-marker epochs.
- The `online` LWT publishes ~11 ms before `bootid`. Letting an unused topic
  extend a session stretches it across the reboot.
- Below `baseline_adapt_pwm` the shadow median is frozen and republished. One
  stationary tail in this dataset echoes a single sample 2,440 times.
- PWM does not detect a locomotive sitting **on** a magnet: MM094 at 10:36:08
  reads +185 counts at PWM 44. Not passing a marker for 3 s does.
- `peak` is published against `entryBaseline_`, not against the resting level.
  Recover the true amplitude before judging any passage.
- Round half **away from zero**. Python's `round()` is banker's rounding, and
  this dataset is full of exact .5 lap estimates.

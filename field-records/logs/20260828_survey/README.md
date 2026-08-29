# 2026-08-28 — the evidence behind the day's conclusions

Toby (9950012). Curated off the Pi; the originals live in
`~/NGR/telemetry/all_20260828.log` on 192.168.68.142 and in the nightly pull at
`~/ngr-telemetry/`.

| file | what it is |
|---|---|
| `toby_1_13WX_markers.log.gz` | every `mm/marker` record from the 1.13W and 1.13X sessions |
| `toby_1_13WX_waveforms.log.gz` | every `mm/wave` capture, both builds |
| `toby_1_13X_survey_waveforms.log.gz` | the environment survey alone, with `rej` dispositions |
| `toby_calibration_CW.log.gz` | the manual CW calibration run, PWM 90 |
| `toby_calibration_CCW.log.gz` | the manual CCW calibration run, PWM 90 |
| `magnet_types.json` | the operator's walking survey: 101 disk, 70 bar, by marker |

## What rests on which file

**The environment is empty** — `toby_1_13X_survey_waveforms.log.gz`. 345 captures
over a full circuit: 192 magnets, 153 others, and every one of the 153 within
0.35 s of a magnet by publish time, 14–64 ms by device clock. Zero isolated.

**The re-reads** — `toby_1_13WX_markers.log.gz`. 21 same-mm pairs in twelve laps;
the 14 that arrive under 400 ms carry 38–44 counts against 140+ and the opposite
pole 13 times out of 13.

**The conservation test refuses real magnets** — same file. 42 rejections, 29 on
full intervals of 1116–2410 ms.

**Per-direction calibration** — the two calibration runs. 171/171 in each, and
duration runs slower CW than CCW at 26 of 26 consecutive markers across
MM060–085.

**Magnet type from shape, 99.4%** — `magnet_types.json` against the waveform
files. 166 of 167 correct, the miss being MM012 by nine ten-thousandths, found
physically off-centre on concrete.

## Reading a waveform record

```json
{"t":83595,"mm":58,"pol":"S","pk":216,"dur":161,"pwm":91,
 "n":185,"sc":3,"pre":12,"tr":0,"clip":0,"rej":0,"drop":0,"d":"<base64>"}
```

`d` is `n` int8 samples of `raw - baseline`, multiply by `sc`, offset 128.
`pre` samples precede the excursion opening. `rej`: 0 admitted, 1 refused by the
40 ms floor, 2 sub-threshold and never an event. `clip` must be 0. `tr` marks a
truncated capture — those may be judged on jitter, amplitude and duration but
never on shape, because a cropped arch reads as flat-topped.

Analysis tools are on `claude/quorum-hall-waveform-diagnostic-plutez` under
`tools/navi/`.

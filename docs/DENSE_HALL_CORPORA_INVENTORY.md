# Where the fully-preserved Hall waveforms are

Answer to "do we have examples of fully preserved waveforms on the Pi?" — yes,
two corpora, both from late August, both continuous 1 kHz with no event framing
and no selection. Neither was used in the September analyses, which were bounded
by the sparse dump policy.

## The three sources, by density

| | what it is | Otto samples | continuous time |
| --- | --- | --- | --- |
| **QUORUM TRACE** `~/NGR/qt_logs/*.qtcap` | every hallTask tick **plus the detector's own decisions** | 6,446,880 | **~1 h 47 m** |
| **HALL_WAVEFORM_TEST** `~/NGR/hwt_logs/*.hwt` | every ADC conversion, dual channel, nothing interpreted | 2,345,579 | **~39 m** |
| fault-preserved dumps (used so far) | `diag/waveform`, framed windows only | 436 records | ~6.5 min, 90 s of it in AUTO |

## 1. QUORUM TRACE — the richer of the two

2026-08-24/25. Five `.qtcap` files, 158 MB. Otto (9950011) in four of them,
Toby (9950012) adds a further 4.29 M samples.

`QtSample` is 16 bytes **per tick**: `raw` (QUORUM's own *averaged* ADC read,
so comparable to NAVI's median-of-five rather than a single conversion),
`baseline`, running N/S event peaks, `pwmActual`, `pwmCommanded`, direction,
e-stop, and a `late` flag whenever the tick gap exceeded 1.25× nominal.

Interleaved in the same stream, `QtDecision` records — Otto's totals:

```
  EVENT_OPENED          3,226
  EVENT_FLOOR_REJECT    1,371
  EVENT_CLOSED          2,550
  ACCEPT_EVENT          2,350
  AGREE / DISAGREE      1,359 / 79
  QUORUM_EVENT            257   (includes PHANTOM_REJECTED)
```

**This is the thing the September work lacked.** Per-sample PWM separates
moving from stationary exactly, and the decision records supply per-event
verdicts — against an impostor denominator of *three* in the September AUTO
corpus.

## 2. HALL_WAVEFORM_TEST — rawer, with operator anchors

2026-08-24. Six `.hwt` files, 24 MB, streamed over UDP by a recorder that
"never reorders, never repairs, never drops and never interprets".

```
otto_040-041_CCW_run.hwt                   928,025 samples   928 s
hwt_20260824_093903.hwt                    522,526           523 s
otto_CCW_PWM90.hwt                         290,810           291 s   3 anchors
otto_066-067_CCW_grillers_start-stop.hwt   239,626           240 s   2 anchors
otto_040-041_CW_PWM40.hwt                  226,375           226 s   5 anchors
hwt_20260824_093642.hwt                    138,217           138 s
```

Gaps are 283–795 samples per file (0.1–0.35%) and are **counted in the header**,
so they are known rather than silent. Zero queue drops anywhere.

`otto_066-067_CCW_grillers_start-stop.hwt` is a **station start-stop** — the
dwell condition, recorded continuously. `otto_040-041_CW_PWM40` carries operator
anchors including `GO_PWM_40_REV`, `ESTOP_ON`, `ESTOP_OFF`.

Two channels: GPIO 33 (the installed sensor) and GPIO 35 (a second channel).

## 3. Where the tooling is

**Not on `main` and not in the working tree.** The whole apparatus lives on
branch `claude/quorum-hall-waveform-diagnostic-plutez`:

```
firmware/test-programs/HALL_WAVEFORM_TEST/   the sketch, HallCapture.h,
                                             CAPTURE_FORMAT.md, tests
firmware/QUORUM/QuorumTrace.h                the QT wire format
tools/hwt_format.py  hwt_decode.py  hwt_plot.py  hwt_receiver.py
tools/hwt_excursions.py  hwt_gate_replay.py  hwt_adc_delta_diagnostics.py
tools/hwt_continuity_population_report.py
tools/hwt_gate_replay_continuity_comparison.py
```

`~/NGR/hwt_format.py`, `hwt_receiver.py` and `~/NGR/qt_tools/` are on the Pi;
`hwt_decode.py` is **not** on the Pi, only on that branch.

## 4. What to be careful about

- **Both corpora are 2026-08-24/25.** That is before the September track work at
  Arches, before the Grillers stop-offset changes, and before the whole X13–X20
  lineage. Marker geography and stopping places have moved since.
- **Different front ends.** HWT takes a *single* `analogRead` per sample; NAVI
  takes a median of five (and decision 0073's transient population is single
  conversions). QUORUM TRACE's `raw` is already averaged. So HWT carries a
  transient population NAVI never sees — recoverable by filtering in software,
  but not to be compared raw.
- **Different opening criteria.** QUORUM and HWT frame events their own way, not
  by X19's ≥70-for-2-samples. This is an advantage, not a problem: with the
  continuous stream the X19 rule can be applied in software and **every**
  opening seen, including ones firmware accepted silently and never dumped.
- **No map-confirmed navigation ground truth** of the kind September's
  `mm/marker ADVANCED` rulings gave. Ground truth has to come from the anchors,
  the QT decision records, and run-by-run geography.
- HWT's own README says the sketch was "never flashed". The captures are dated
  after it; the README is stale, not the data.

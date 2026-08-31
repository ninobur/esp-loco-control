# NAVI_ONE 0.3 — Field finding 04

## Waveform capture is field-verified; the Pi runlog destroyed the first dump

**Date:** 2026-08-31
**Locomotive:** Toby (9950012), NAVI_ONE 0.3, boot at ~10:16, no reflash between runs
**Sketch:** `agent/toby-1-13-flash` @ `84b1730` — the six-passage window of decision 0063
**Status:** Observed. Not a decision. Nothing here has been ratified.

---

## What was tested

The operator inserted a magnet of the opposite pole immediately before MM050 and
drove Toby CW under AUTO, to force a `WrongMagnet` strike and exercise the
`diag/waveform` dump path end to end for the first time in the field.

The test was run twice. The first run produced an unusable capture, for a reason
that had nothing to do with the firmware.

---

## Run 1 — 10:20:14 — dump published, record destroyed

Six messages arrived on `ngr/loco/9950012/diag/waveform` within 3 ms. The
firmware did its job.

They were logged through `server/ngr_runlog.py:285`:

```python
payload = msg.payload.decode("utf-8", "replace")
```

The dump is a packed binary struct followed by raw `int16` samples. Every byte
that is not valid UTF-8 became U+FFFD. The signature is visible in the log: slot
0's `sampleCount` field reads `efbfbd00`, i.e. its low byte was replaced. Counts
were 9, 100, 93, 92, 94, 94 replacements across the six payloads.

**The mapping is many-to-one, so this is irreversible.** The messages are
non-retained, so there was no second copy. The data is gone.

This was predicted before the run and the prediction was not acted on in time.
The recommended mitigation at the time — a binary-safe `mosquitto_sub -F '%x'`
subscriber running alongside the test — was not in place.

### What survived, and what it said

The first eight header bytes are all in the ASCII range and came through intact
in all six slots:

- every slot: `outcome=0` (`MAGNET`), `isMagnet=1`, `shapeTested=1`
- every slot: `slotTotal=6`, `chunkTotal=1`

So even from the wreckage: the recognizer *accepted* all six passages including
the inserted magnet, and the navigator is what rejected it. The strike came from
identity, not from recognition. Run 2 confirmed this directly.

---

## The logger fix

CODEX changed `on_message` to wrap waveform payloads before any decode:

```python
if topic.endswith("/diag/waveform"):
    payload = json.dumps({
        "encoding": "base64",
        "byte_length": len(msg.payload),
        "payload_b64": base64.b64encode(msg.payload).decode("ascii"),
    }, separators=(",", ":"))
```

Reviewed on the Pi and correct on the points that matter:

- encodes **raw `msg.payload`**, before the utf-8 decode and before newline
  normalisation
- gated on the topic suffix, so ordinary telemetry is untouched
- base64's alphabet contains no `\t`, `\r` or `\n`, so the one-line
  normalisation cannot damage it
- carries `byte_length` so a decoder can verify the round trip
- written to both the daily `all_` log and the per-run log

One regression was checked for and ruled out: waveform payloads are now valid
JSON, where before they were not, so `data` is a dict for them. Every
`isinstance(data, dict)` branch downstream is gated behind `is_marker`,
`is_loopstat` or `is_bootid`, none of which are true for `diag/waveform`. The
parsed dict is inert.

CODEX reports byte-for-byte round-trip tests passed and that both logs hold
identical binary payloads.

---

## Run 2 — 10:28:23 — six slots, full fidelity

Decoded with `tools/waveform_b64_to_csv.py`. 944 samples total, no decimation.

Slot 0 is the most recently pushed passage. Marker identities are from the
`mm/marker` stream over 10:28:18–10:28:23.

| slot | marker | outcome | polarity | peak | ampRatio | residual | samples |
|-----:|--------|---------|----------|-----:|---------:|---------:|--------:|
| 0 | **inserted** | MAGNET | 1 (N) | 123 | 0.5642 | 0.1023 | 175 |
| 1 | MM049 | MAGNET | 1 (N) | 239 | 1.0963 | 0.0632 | 162 |
| 2 | MM048 | MAGNET | 1 (N) | 207 | 1.0895 | 0.0766 | 153 |
| 3 | MM047 | MAGNET | 0 (S) | 218 | 1.1474 | 0.0730 | 153 |
| 4 | MM046 | MAGNET | 0 (S) | 239 | 1.2579 | 0.0720 | 152 |
| 5 | MM045 | MAGNET | 0 (S) | 221 | 1.1632 | 0.0736 | 149 |

Passages are evenly spaced at ~1.07 s, so the loco held a steady speed
throughout the window.

The strike: at 10:28:23.801 the inserted passage read **N** where MM050 expected
**S**. Toby latched `STRUCK` without advancing past MM049.

### The polarity pattern is the track, not an anomaly

Both dumps showed polarity `1,1,1,0,0,0` newest-to-oldest, which looked like it
might need explaining. It does not. The `mm/marker` stream gives the surveyed
orientations directly — MM045, MM046 and MM047 are S; MM048 and MM049 are N —
and the inserted magnet read N. `polarity 0 = S, 1 = N`. The pattern reproduced
across both runs because both runs crossed the same markers. Closed.

---

## What the data supports

1. **The dump path works.** Trigger, window depth, ordering, encoding, transport
   and decode are all confirmed against real field data. Each passage fit in one
   message; chunking was published as `chunkTotal=1` and therefore has *not*
   been exercised in the field.
2. **The design worked exactly as decision 0058 intended.** The recognizer
   judged the inserted magnet a well-formed magnet — correctly, it is one — and
   the navigator rejected it on identity. Recognition and position stayed
   separate.
3. **The five surveyed markers sit squarely inside the calibration.**
   `MagnetRecognizer.h` records real magnets at residual 0.0473–0.0805; MM045–049
   came in at 0.0632–0.0766.

## What the data does NOT support

The inserted magnet returned residual 0.1023 at amplitude ratio 0.5642, against
0.063–0.077 at 1.09–1.26 for the surveyed five. It is tempting to read this as
*low amplitude inflates the Gaussian residual*, which would be a tidy
explanation for findings 02 (MM110, residual 0.1422) and 03 (MM146, 0.1811).

**That inference is not available from this data**, for two reasons:

- It rests on a single low-amplitude point. Across the five surveyed markers,
  amplitude ranges 1.09→1.26 with no monotonic relationship to residual.
- It is confounded. The inserted magnet is a physically different magnet — 175
  samples against 149–162, visibly broader and flatter-topped — so its shape may
  depart from Gaussian for reasons that have nothing to do with amplitude.
  Findings 02 and 03 were rejections of *surveyed* markers.

Notably, 0.1023 lands in the empty gap between the two calibrated populations
(real 0.0473–0.0805, non-primary 0.1948–1.1660, ceiling 0.13) — which is where a
genuine magnet that is not one of ours ought to land.

The open question from findings 02 and 03 therefore remains open. The way to
close it is the operator's stated plan: capture a naturally occurring shape
rejection with the now-working dump path, **without touching recognition
thresholds**.

---

## Artefacts

- `tools/waveform_b64_to_csv.py` — decoder; `--self-test` verifies the 40-byte
  packed header against `WaveformDump.h` and refuses truncated chunks
- `~/ngr-telemetry/waveforms/` — the six decoded CSVs and the extracted log
  lines, kept outside the working tree per the rule in
  `tools/fetch_pi_telemetry.sh` that git must not be able to rewrite captures

## Carried forward

- Chunking (`chunkTotal > 1`) is still bench-only.
- The pre-fix corrupted payloads remain in `all_20260831.log` at 10:20:14. The
  decoder refuses them by design rather than reconstructing something plausible.

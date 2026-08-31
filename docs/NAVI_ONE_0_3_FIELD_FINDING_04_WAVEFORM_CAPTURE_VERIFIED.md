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

## The conclusion

**Waveforms are now recorded in detail when a disagreement happens, so that the
cause of a change in the residual can be discerned.**

That is the whole of it. Everything the published scalars can tell us about a
residual excursion, they had already told us, and it was not enough.

### Why the scalars were never going to be enough

Every MM110 pass across the session of findings 02/03, pulled from the retained
log:

| time | peak | ratio | resid | gap_ms | gain |
|------|-----:|------:|------:|-------:|-----:|
| 22:27:20 | 176 | 0.838 | 0.0800 | 1125 | 210 |
| 22:30:42 | 180 | 0.865 | 0.0825 | 1138 | 208 |
| 22:34:04 | 177 | 0.839 | 0.0788 | 1126 | 211 |
| 22:37:27 | 175 | 0.841 | 0.0868 | 1138 | 208 |
| **22:40:50 (rejected)** | **179** | **0.856** | **0.1422** | 1164 | 209 |

Peak, ratio, gap and gain on the rejected pass all sit inside the range of the
four clean passes immediately before it. Only the residual moves, to nearly
double the highest clean value. Nothing else about the crossing was different in
any dimension NAVI_ONE publishes.

This also settles a question left open in the first draft of this record. The
inserted magnet in today's test had both low amplitude (0.5642) and an elevated
residual (0.1023), which invited reading residual as amplitude-driven. The MM110
table refutes that for the case that matters: ratio 0.856 is unremarkable while
the residual nearly doubles. Amplitude is not the mechanism there.

Residual is the normalised RMS error of a best-amplitude Gaussian fit to the
arc above 20% of peak. It rises for asymmetry, a shoulder or double bump, a
flattened or truncated top, a skewed arc from speed change during the crossing,
edge noise inside the threshold window, or any brief electrical transient. Each
of those has a distinct sample-by-sample signature and none of them is
distinguishable from the others in a scalar. The waveform is the only thing that
separates them, which is why the instrument exists.

## Artefacts

- `tools/waveform_b64_to_csv.py` — decoder; `--self-test` verifies the 40-byte
  packed header against `WaveformDump.h` and refuses truncated chunks
- `~/ngr-telemetry/waveforms/` — the six decoded CSVs and the extracted log
  lines, kept outside the working tree per the rule in
  `tools/fetch_pi_telemetry.sh` that git must not be able to rewrite captures

## Carried forward

- Chunking (`chunkTotal > 1`) is field-proven as of 10:37:16, when a 368-sample
  passage (capacity is 332 per message) split across two chunks and reassembled
  correctly. Every passage in the 10:28 dump had fit one message.
- The pre-fix corrupted payloads remain in `all_20260831.log` at 10:20:14. The
  decoder refuses them by design rather than reconstructing something plausible.
- The cause of the MM110 and MM146 residual excursions remains unknown, and the
  thresholds remain untouched.
- A shape rejection lags one marker and the lag is exposed by the next polarity
  mismatch, which strikes, stops and dumps — so the event is captured. Note only
  that `WaveformWindow<6>` retains the strike passage plus five older ones, and
  `ROUTE_POLARITY` has same-polarity runs of 6 (MM36–41) and 7 (MM107–113).

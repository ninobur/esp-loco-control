# NAVI_TRACER_0_2 — Stage B, scoped evaluation: MM045→MM044

**Status: offline research result. Not a decision record. No firmware
changed, flashed, or substituted. Scope limited to the single evaluation the
operator authorized — the MM045→MM044 encounter only. Stopping here per
instruction, before any further Stage B expansion.**

Date: 2026-10-22. Locomotive: Otto (9950011). Run 2, boot `B4F02DFBE4416E6C`,
2026-10-22. Governing decisions: see
`docs/navi/NAVI_TRACER_0_2_STAGE_A_ARCHITECTURE_20261022.md` §2 (operator
decisions 1–7).

## 1. How the recording was obtained

Read-only SSH access to the Pi (`david@192.168.68.142`), confirmed working.
Source file: `/home/david/NGR/navi_sync/navi_sync_20261015_100403.nsr`
(13,440,694,403 bytes; despite its Oct-15 name, the Pi's recorder has kept
appending to this one file continuously, including through the Oct 22
session). No file was modified, moved, or deleted; the extraction script
opened it `"rb"` only.

Method: a small Python script (using the Pi's own deployed
`navi_sync_format.py`, SHA-256-identical to this repository's copy at
`CURRENT/tools/navi_sync_format.py`) binary-searches the capture by `recv_us`
(the Pi's wall-clock microseconds at datagram receipt, monotonic) to locate
the time window, then streams sequentially through it extracting
`REC_HALL_NATIVE` records for loco 9950011. Run on the Pi; only small result
files were copied back (max 46 MB), never the full 13 GB capture. Temporary
files created on the Pi (`/tmp/extract_mm044_window.py`,
`/tmp/mm044_tight.json`, `/tmp/mm044_window.json`) were deleted after use.

The extraction was run twice: a first 42 s pass (210,000 native samples,
bad_frames=0) to locate the event by scanning for 70-ADC excursions, then a
tight check against the exact recorded boundaries (below). No samples were
inferred, interpolated, or reconstructed; every number in §3–§6 is a direct
read of recorded native ADC values, binned by `RvmTracing.h`'s own exact
20 ms rule (plain mean of native reads per block, NaN on <50 reads or a
serial-jump-touched block — none occurred in this window: bad_frames=0,
serial contiguous).

## 2. Item 2 — the recorded boundaries

Found directly in the Pi's MQTT telemetry log,
`/home/david/NGR/telemetry/runs/9950011_20261022_153206.log` (covers
15:32:06–15:33:59, matching this event):

```
2026-10-22T15:33:25.303  state/nav  mm:45 target:44 event:CLOSE_AND_ADVANCE
                         baseline_established:0 departure:4.30
2026-10-22T15:33:25.952  state/nav  mm:45 target:44 event:OPEN_CANDIDATE
                         baseline_established:1 baseline_level:2098.31
                         baseline_slope:213.7989 baseline_noise:16.20
                         departure:90.65
```

These are the recorded closure and opening events — the same ones the
engineering failure report's replay reconstructed (its slope +213.80 ADC/s,
level 2115.41 ADC, noise 16.20 match this log's 213.7989/16.20 almost
exactly; the small level difference is the replay's own fit at a slightly
later sample than the log's instantaneous report). Mapped to the native
recording's on-device clock via the Pi's `recv_us`: close at device
`t_us≈169751224`, opening at device `t_us≈170403582` — a 0.652 s window,
consistent with the committed 0.76 s figure within replay/telemetry
rounding.

## 3. Item 3/4 — median over the recorded closure→opening window, PWM>0, unfiltered

| | value |
|---|---:|
| Window | device t_us [169751224, 170403582), span 0.652 s |
| Samples (20 ms blocks, PWM>0, unfiltered) | n = 32 |
| PWM in window | 35–37 |
| Minimum Hall | 1992.39 |
| Maximum Hall | 2072.63 |
| **Median** | **2056.78** |
| Mean | 2047.57 |
| Std | 23.66 |

Every single block in this window is above 1992 — **the window contains not
one sample of genuine background.** An independently-established local
background (the quiet stretch immediately before this window, device t_us
[169008540, 169308540), n=15) reads median 1901.34, min 1900.02, max 1903.71
— consistent with the engineering failure report's "approximately 1900 ADC"
and with this same recording's own overall-window median (1903.49) computed
separately over a wider 42 s span.

**Median − background = +155.44 ADC** — more than double the 70-ADC
departure criterion itself. The median, as decided (closure→opening,
unfiltered, PWM-gated), is not a background measurement here. It is a value
from inside the rising flank of MM044's own approach.

## 4. Item 5 — recognition outcome against this median (not just an error number)

Scanning forward from the window using this median (2056.78) as the live
reference and the unchanged 70-ADC/2-consecutive-bin criterion:

- The criterion **fires at device t_us=170528540**, hall=1975.04,
  **departure = −81.74 (negative sign)** — 125 ms *after* the old
  recognizer's own (differently-broken) OPEN_CANDIDATE declaration.
- This is the *falling* side of MM044's own waveform, descending back
  toward true background, crossing below the contaminated median. **The
  decided design opens a candidate with the wrong sign here, for a
  different mechanical reason than NAVI_TRACER_0_1 did (that one
  extrapolated a line; this one measured a median that sat inside the
  excursion), but the practical failure is the same**: the reference itself
  is wrong, so the sign of "departure" is decided relative to the wrong
  point.
- RTB check (6 consecutive bins within `max(35, 3×23.66)=70.97` ADC of
  2056.78): **never reached** in the scanned data. The signal does return to
  real background (~1900–1907, confirmed directly in the raw samples through
  device t_us≈170828540 and beyond), but 1900–1907 is ~150–157 ADC from the
  median reference — nowhere near the 70.97 ADC quiet band. A candidate
  opened against this reference would be stuck exactly the way MM044 was
  stuck under NAVI_TRACER_0_1 (telemetry: `SUSPENDED`, `candidate_open:1`,
  `departure:212.41`, unresolved for the 17+ seconds of logged telemetry
  that follow).

**Verdict for item 5: the median, as decided, does not support the 70-ADC×2
criterion on this real encounter. It fails the same way the frozen line did
— wrong-signed opening, no path to RTB — by a different route.**

## 5. Item 6 — line-versus-curve (ΔBIC), evaluated separately, non-gating

Per decision 5, ΔBIC was evaluated on its own, not used to gate anything
here or in §4:

| Window | n | ΔBIC |
|---|---:|---:|
| Full candidate span (open→would-be-RTB-or-+2s) | 94 | 168.19 |
| Background start → the (wrong-signed) opening found in §4 | 62 | 391.94 |

Both strongly favor a curve over a line — consistent with this being a
genuine, clean, single magnetic encounter in the raw signal (which it is:
the physical waveform itself is unremarkable, rises smoothly from ~1900 to
a peak ~2073 and descends smoothly back toward ~1900–1907). ΔBIC correctly
confirms "this is a real magnet," which was never in question. **It
confirms nothing about whether the baseline reference is correct** — that
is a separate question, and on this evidence it isn't ΔBIC's question to
answer, per decision 5.

## 6. Item 7 — does the specified architecture succeed or fail here, and why

**It fails on this encounter, as specified.**

The cause is not the median statistic's own breakdown behavior (as
revision 1's §3.2 reasoning worried about in the abstract) — the median
never got a chance to break down, because **it was never given any
background samples to work with in the first place.** The recorded
closure→opening window (0.652 s) is, in its entirety, the rising flank of
the next magnet. That is a direct, physical fact about this specific
encounter (confirmed in raw samples, §3's table), not a statistical
artifact of which central-tendency measure was chosen: a fitted line would
have the same problem for the same reason — there is no genuinely ordinary
sample anywhere inside this window to fit or to average.

Why the window is like this: directly inspecting the raw signal (not just
the two declared boundary events), the Hall reading was already
measurably rising — ~1903 at device t_us≈169308540, climbing smoothly from
there — about 0.44 s *before* the recorded closure event (169751224) ever
fired. The genuinely flat, true-background stretch in this recording
(device t_us≈169108540–169308540, the 0.2 s used as §3's independent
background check) ended *before* MM045's own closure was declared, not
between closure and opening. Put plainly: by the time anything declared
"MM045 is closed," the real signal had already been climbing toward MM044
for nearly half a second. Whether that lag belongs to MM045's own closure
test being late, or to the real magnetic spacing simply being this tight
at this PWM, is not settled by this evaluation — determining that would
require tracing MM045's own candidate lifecycle, which is outside what was
authorized for this pass.

This does not resolve whether the architecture as decided is viable in
general — one encounter is one encounter, and MM152/MM031/MM063 were
explicitly out of scope for this pass. It does establish, with real data,
that the closure→opening window is not guaranteed to contain any
background sample at all, for at least this one real, recorded,
PWM-60-ish encounter on this locomotive.

## 7. What this evaluation did not do

- No change to firmware, no flashing, no architectural substitution.
- No admission filter, timer, or new threshold was added or tested — none
  proposed.
- MM152, MM031, MM063 were not evaluated — out of this pass's authorized
  scope.
- MM045's own candidate lifecycle (why its closure declared when it did)
  was not traced — would require extending scope beyond this evaluation.
- No conclusion is drawn here about what to do next; that is the
  operator's decision per the "stop after this evaluation" instruction.

Stopping here, as instructed, before any further Stage B expansion.

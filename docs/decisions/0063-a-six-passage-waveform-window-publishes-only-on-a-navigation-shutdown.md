# 0063 — A six-passage waveform window publishes only on a navigation shutdown

Status: Proposed (2026-08-31)

## Decision

NAVI_ONE keeps the raw, oriented Hall samples of the last six completed
passages in a small ring on the Hall task (`WaveformWindow.h`). It publishes
that window, chunked so nothing is truncated, only when `withdraw()` runs —
i.e. only on a navigation-caused AUTO shutdown (`WrongMagnet` or
`Contradicted`). Manual auto-off, dispatcher release, e-stop, and low voltage
do not trigger it.

Depth is 6, matching the worst-case missed-magnet bound measured in decision
0059. Fidelity is exact: the window stores what `HallCapture` actually handed
the recognizer — its already-decimated samples — and the wire format
(`WaveformDump.h`) chunks a passage across as many MQTT messages as it needs
rather than ever cropping or resampling it to fit one.

## Context

Two field events on 2026-08-30/31 (physical MM110 and MM147/145, both on
Toby, NAVI_ONE_0_3) showed the same mechanism: a real, well-formed magnet
passage — normal peak, normal amplitude ratio, normal polarity — failed only
the Gaussian shape test (`WRONG_SHAPE`), which is not an error the firmware
surfaces beyond a `notmag` counter tick. That produced a silent one-marker
lag, resolved markers later as a `WrongMagnet` strike whose warning pointed at
the wrong location entirely. Reconstructing both events from the retained
MQTT log (`ngr-runlog` on the Pi, already running, `docs/reviews` and two
field-finding docs) explained *that* it happened and roughly *where*, but not
*why* the shape residual spiked — `HallCapture`'s own passage buffer is
reused by the very next passage, so the actual waveform was gone within
about a second of the rejection, long before anyone downstream could ask for
it.

The operator's framing: we don't need every waveform, only the one that
shuts the process down — and a short trailing window, kept anyway,
retroactively includes whatever came just before it, without needing to know
in advance which passage will turn out to matter.

## Alternatives considered

- **Publish shape data on every passage, always.** Rejected: continuous
  waveform traffic for a diagnostic that is only useful after a rare event is
  needless load on the broker and the Pi's logger for no benefit the rest of
  the time.
- **Trigger on `WRONG_SHAPE`/`NotAMagnet` itself, not on the eventual
  strike.** Considered and set aside for now: most `NotAMagnet` outcomes are
  presumably ordinary weak rebounds, not the interesting case, and dumping on
  every one of those would be far noisier than dumping on the rare shutdown
  it may lead to. The operator's ruling was explicit: trigger on the event
  that shuts AUTO down.
- **Resample each waveform to fit one MQTT message.** Rejected by the
  operator's ruling: "do not discard information the firmware processes to
  compute the value." The chunked wire format exists specifically so fidelity
  never trades against message size.
- **A shallower window (3–4 passages).** Considered, matching the operator's
  first framing of the idea; widened to 6 to cover the measured worst-case
  lag from decision 0059 rather than the typical case observed so far (both
  events resolved within 2–3 markers).

## Consequences

- ~6.3 KB of additional static RAM (six ring-buffered passages at up to 512
  int16 samples each) and ~7 KB of additional flash. Confirmed by a clean
  compile against `esp32:esp32:esp32` 3.3.11 (Toby's actual core): 966,451
  bytes flash (73%), 58,132 bytes globals (17%) — comfortable headroom on
  both.
- A new topic, `ngr/loco/9950012/diag/waveform`, not retained: it is a
  one-shot diagnostic burst meaningful only immediately after the shutdown
  that triggered it, not a state a later boot should inherit.
- A future review of any navigation-caused strike now has the option of
  asking for its own waveform, if the operator or an analyst chooses to parse
  it — the format is documented and host-tested in
  `firmware/test-programs/NAVI_ONE/WaveformDump.h`, but nothing on the Pi or
  the console decodes it yet. That is future work, not part of this decision.
- This is a genuine capability addition to a build that had already cleared
  four gates. A fifth gate (`gate_waveform.cpp`, 16 checks) now covers the
  window's depth/eviction behaviour, exact sample preservation, and the
  chunk/reassembly round-trip, including an oversized passage that does not
  fit one message. All five gates and a full Arduino compile pass as of this
  writing. **Not flashed, not field-validated.** Per standing practice nothing
  is flashed without the operator's explicit say-so, separate from this
  record.

## References

- `firmware/test-programs/NAVI_ONE/WaveformWindow.h`
- `firmware/test-programs/NAVI_ONE/WaveformDump.h`
- `firmware/test-programs/NAVI_ONE/tests/gate_waveform.cpp`
- `docs/NAVI_ONE_0_3_FIELD_FINDING_02_SHAPE_REJECTION_LAG_STOP.md` (MM110)
- `docs/NAVI_ONE_0_3_FIELD_FINDING_03_SECOND_SHAPE_REJECTION_MM146.md` (MM147/145, second occurrence)
- decisions 0058, 0059 (the strike latch; the six-marker bound this window's depth matches)

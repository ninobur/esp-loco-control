# NAVI_ONE 0.3 waveforms — Toby, 2026-08-31

Passages recovered from the `ngr/loco/9950012/diag/waveform` dumps of
decision 0063, over four navigation stops: 10:20:14, 10:28:23, 10:37:16
and 11:01:15 (plus 10:46:22).

The 10:20:14 dump is **not** here. It was logged before the Pi's base64 fix,
through a utf-8 `errors="replace"` decode that mapped every non-UTF-8 byte to
U+FFFD. That is many-to-one and the messages were non-retained, so those six
passages are unrecoverable. See finding 04.

## captured_passages.tsv

22 distinct passages, signed baseline-relative counts, orientation undone —
what the sensor actually saw. `want_pol`/`want_peak` are the correct values:
identical to the firmware's for every passage but the two entry-impulse
mis-latches of findings 05 and 06, where the firmware reported the pole and
amplitude of a single artifact sample.

Consumed by gate 6 (`tests/gate_polarity.cpp`). Regenerate the per-passage
CSVs from a runlog with `tools/waveform_b64_to_csv.py`.

Two passages appear in two dumps each — `103716` re-dumped `102823`'s two
newest slots, still in the ring from the earlier stop. They are deduplicated
here by `openedAtMs`.

#!/usr/bin/env python3
"""test_excursions.py — host tests for hwt_excursions.py.

INVESTIGATORY / UNAPPROVED.

Builds small synthetic captures with full control over every sample's raw
value, dt_us and PWM, and checks the excursion collector against exactly
the behaviours the tool promises: narrow spikes are kept, broad positive
and negative excursions are measured with the right sign, a rolling
baseline does not swallow a real transient in slow drift, missing samples
are never fabricated, integrated flux uses each sample's own measured
interval, and a wrapped sample-sequence counter does not break timing.
Run via ./run_tests.sh.
"""

import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", "tools"))
sys.path.insert(0, TOOLS)

import hwt_format as F              # noqa: E402
import hwt_excursions as E          # noqa: E402

failures = 0
checks = 0


def ck(cond, what):
    global failures, checks
    checks += 1
    if not cond:
        failures += 1
        print("  FAIL  %s" % what)


def ckEq(got, want, what):
    global failures, checks
    checks += 1
    if got != want:
        failures += 1
        print("  FAIL  %s (got %r, want %r)" % (what, got, want))


def ckClose(got, want, what, tol=1e-6):
    global failures, checks
    checks += 1
    if abs(got - want) > tol:
        failures += 1
        print("  FAIL  %s (got %r, want %r +/- %r)" % (what, got, want, tol))


def build_batch(specs, *, batch_seq, first_sample_seq, t0_us, session=0xBEEF):
    """specs: list of (raw, dt_us, pwm_actual, pwm_commanded)."""
    payload = b"".join(F.pack_sample(raw, dt_us=dt_us, pwm_actual=pwm_a,
                                     pwm_commanded=pwm_c)
                       for raw, dt_us, pwm_a, pwm_c in specs)
    return F.build_record(F.REC_SAMPLES, payload, session_id=session,
                          batch_seq=batch_seq, first_sample_seq=first_sample_seq,
                          t0_us=t0_us, n_samples=len(specs))


def write_capture(records):
    fd, path = tempfile.mkstemp(suffix=".hwt")
    with os.fdopen(fd, "wb") as fh:
        F.write_capture_header(fh, 0)
        for r in records:
            F.write_frame(fh, 0, r)
    return path


def find(path, **kw):
    """Defaults to --baseline-mode moving: these are the ORIGINAL tests,
    written to exercise the centered rolling baseline's own edge behaviour
    (mean bleed, window-vs-drift tradeoffs). See find_frozen() below for
    the corrected pre-event mode."""
    defaults = dict(entry_threshold=30.0, exit_threshold=15.0,
                    baseline_mode="moving",
                    baseline_window=501, baseline_method="mean",
                    gap_margin_s=0.05)
    defaults.update(kw)
    return E.find_excursions(path, **defaults)


def find_frozen(path, **kw):
    defaults = dict(entry_threshold=30.0, exit_threshold=15.0,
                    baseline_mode="frozen",
                    pre_window=200, baseline_method="mean",
                    gap_margin_s=0.05)
    defaults.update(kw)
    return E.find_excursions(path, **defaults)


def test_tall_narrow_spike_survives():
    print("a single-sample spike is kept, not filtered")
    specs = [(1900, 1000, 0, 0)] * 200
    specs[100] = (2150, 1000, 0, 0)          # +250 above baseline, one sample
    path = write_capture([build_batch(specs, batch_seq=1, first_sample_seq=0, t0_us=0)])
    exc = find(path)
    os.unlink(path)

    ckEq(len(exc), 1, "exactly one excursion found")
    e = exc[0]
    ckEq(e["n_samples"], 1, "the spike is one sample wide")
    ckEq(e["duration_ms"], "0.000", "a one-sample excursion has zero duration")
    ck(float(e["max_abs_flux"]) > 200, "the spike's magnitude is preserved")
    ckEq(e["incomplete"], 0, "a clean single-sample spike is not incomplete")


def test_broad_positive_and_negative_excursions_measured():
    print("broad positive and negative excursions, both measured correctly")
    specs = [(1900, 1000, 70, 90)] * 300
    for i in range(50, 100):
        specs[i] = (2050, 1000, 70, 90)      # +150 plateau, 50 samples
    for i in range(200, 250):
        specs[i] = (1750, 1000, 70, 90)      # -150 plateau, 50 samples
    path = write_capture([build_batch(specs, batch_seq=1, first_sample_seq=0, t0_us=0)])
    exc = find(path)
    os.unlink(path)

    ckEq(len(exc), 2, "both plateaus are found as separate excursions")
    pos, neg = exc[0], exc[1]
    ck(float(pos["max_pos_flux"]) > 100, "the positive plateau has positive flux")
    ckEq(float(pos["max_neg_flux"]), 0.0, "a purely positive excursion has zero negative flux")
    ck(float(neg["max_neg_flux"]) < -100, "the negative plateau has negative flux")
    ckEq(float(neg["max_pos_flux"]), 0.0, "a purely negative excursion has zero positive flux")
    ck(48 <= pos["n_samples"] <= 50, "the positive excursion's width matches the plateau")
    ck(48 <= neg["n_samples"] <= 50, "the negative excursion's width matches the plateau")


def test_baseline_drift_does_not_hide_excursion():
    print("slow baseline drift does not swallow a real transient")
    n = 2000
    specs = []
    for i in range(n):
        drift = 1900 + i * (100.0 / n)       # 100-count drift over the whole run
        specs.append((int(round(drift)), 1000, 0, 0))
    specs[1000] = (int(round(1900 + 1000 * (100.0 / n))) + 200, 1000, 0, 0)
    path = write_capture([build_batch(specs, batch_seq=1, first_sample_seq=0, t0_us=0)])
    exc = find(path, baseline_window=101)    # narrow enough to track the drift closely
    os.unlink(path)

    ck(len(exc) >= 1, "the transient survives the drift")
    hit = [e for e in exc if e["start_sample"] <= 1000 <= e["end_sample"]]
    ck(len(hit) == 1, "the transient is found at the injected sample")
    if hit:
        ck(float(hit[0]["max_abs_flux"]) > 150,
           "the transient's magnitude is not eaten by the local baseline")


def test_missing_samples_never_interpolated():
    print("a transport gap inside an excursion is never bridged")
    lead_in = [(1900, 1000, 0, 0)] * 50
    onset = [(2000, 1000, 0, 0)] * 30                # samples 50..79
    resume = [(2000, 1000, 0, 0)] * 30               # samples 100..129 (after the hole)
    tail = [(1900, 1000, 0, 0)] * 50                 # samples 130..179
    b1 = build_batch(lead_in + onset, batch_seq=1, first_sample_seq=0, t0_us=0)
    # batch_seq 2, samples 80..99, is never written -- transport loss
    b3 = build_batch(resume + tail, batch_seq=3, first_sample_seq=100, t0_us=100000)
    path = write_capture([b1, b3])
    # The mean baseline (whole-array here: window 501 > 160 samples) is
    # dragged up by the 60 elevated samples to 1937.5, so the 100 flat
    # samples read as a small negative deviation (-37.5) too -- a mean
    # filter's edges always bleed a little. Choose thresholds that clear
    # that bleed (entry 50, exit 40 both clear 37.5) so the excursion opens
    # and closes exactly at the true step, not at the array's edges.
    exc = find(path, entry_threshold=50.0, exit_threshold=40.0)
    os.unlink(path)

    ckEq(len(exc), 1, "one excursion spans the gap")
    e = exc[0]
    ckEq(e["start_sample"], 50, "excursion opens where the signal first rises")
    ckEq(e["end_sample"], 129, "excursion closes where the signal last falls")
    ckEq(e["nominal_span_samples"], 80, "the nominal span includes the hole")
    ckEq(e["n_samples"], 60, "only the 60 samples that actually arrived are counted")
    ck(e["n_samples"] < e["nominal_span_samples"],
       "missing samples are never fabricated to fill the span")
    ckEq(e["incomplete"], 1, "an excursion crossing a transport gap is marked incomplete")
    ck(e["gaps_within_count"] >= 1, "the gap is recorded against this excursion")


def test_integrated_flux_uses_actual_intervals():
    print("integrated flux uses each sample's own measured interval")
    specs = [(1900, 1000, 0, 0)] * 20
    dts = [500, 1000, 1500, 2000]
    for k, dt in enumerate(dts):
        specs[10 + k] = (2000, dt, 0, 0)
    path = write_capture([build_batch(specs, batch_seq=1, first_sample_seq=0, t0_us=0)])
    # exit_threshold must clear the flat samples' own deviation from this
    # mean baseline (20 counts -- the four elevated samples pull the
    # whole-array mean up), or the excursion never closes and swallows
    # neighbouring flat samples too.
    exc = find(path, entry_threshold=50.0, exit_threshold=30.0)
    os.unlink(path)

    ckEq(len(exc), 1, "one excursion found")
    e = exc[0]
    # baseline = mean of 20 samples = (16*1900 + 4*2000)/20 = 1920.0 exactly;
    # dev on the four elevated samples = 80 exactly; integrated flux =
    # 80 * sum(dt_ms) = 80 * (0.5+1.0+1.5+2.0) = 80 * 5.0 = 400.0
    ckClose(float(e["integrated_abs_flux_count_ms"]), 400.0,
           "integrated flux matches the hand-computed value using real dt", tol=0.5)
    # A version that assumed a fixed 1 ms grid would compute 80 * 4 = 320.0 instead.
    ck(abs(float(e["integrated_abs_flux_count_ms"]) - 320.0) > 10,
       "the result is NOT what a fixed-1ms-grid assumption would produce")


def test_sequence_wraparound_does_not_break_timing():
    print("a wrapped sample-sequence counter does not break timing")
    specs = [(1900, 1000, 0, 0)] * 10
    specs[7] = (2200, 1000, 0, 0)             # spike after the wrap point
    b = build_batch(specs, batch_seq=1, first_sample_seq=0xFFFFFFFA, t0_us=0)
    path = write_capture([b])
    exc = find(path)
    os.unlink(path)

    ckEq(len(exc), 1, "the decoder and collector survive a wrapped sequence counter")
    e = exc[0]
    ckClose(float(e["t_max_abs_flux_s"]), 0.007,
           "the peak's timestamp is unaffected by sample_seq wraparound", tol=1e-6)
    ck(0 <= e["start_sample"] <= 0xFFFFFFFF, "start_sample is a valid 32-bit value")
    ck(0 <= e["end_sample"] <= 0xFFFFFFFF, "end_sample is a valid 32-bit value")


def test_frozen_baseline_does_not_absorb_broad_excursion():
    """The core claim of --baseline-mode frozen, checked directly against
    --baseline-mode moving on the identical synthetic signal: an 800-flat /
    400-elevated(+200) / 800-flat run. The moving 501-sample centered mean
    both crushes the measured peak (its own local window is dragged toward
    the plateau) AND smears the reported boundary far past the true step,
    because the window's own half-width leaks into every sample within
    250 samples of the edge. The frozen baseline, computed once from 200
    quiet samples before the step and never updated by the plateau's own
    samples, recovers the exact boundary and the true amplitude."""
    print("frozen baseline does not absorb a broad excursion; moving baseline does")
    lead_in = [(1900, 1000, 0, 0)] * 800
    plateau = [(2100, 1000, 0, 0)] * 400        # true +200 above baseline
    tail = [(1900, 1000, 0, 0)] * 800
    specs = lead_in + plateau + tail
    path = write_capture([build_batch(specs, batch_seq=1, first_sample_seq=0, t0_us=0)])

    moving = find(path, baseline_window=501)
    frozen = find_frozen(path, pre_window=200)
    os.unlink(path)

    hit_moving = [e for e in moving if e["start_sample"] <= 900 <= e["end_sample"]]
    hit_frozen = [e for e in frozen if e["start_sample"] <= 900 <= e["end_sample"]]
    ckEq(len(hit_moving), 1, "moving baseline reports one (smeared) excursion here")
    ckEq(len(hit_frozen), 1, "frozen baseline reports one (exact) excursion here")

    moving_peak = float(hit_moving[0]["max_abs_flux"])
    frozen_peak = float(hit_frozen[0]["max_abs_flux"])
    ck(90 < moving_peak < 110,
       "moving baseline crushes the true 200-count step to about half (measured ~99.8)")
    ck(frozen_peak > 195,
       "frozen baseline recovers close to the true injected amplitude of 200")
    ck(frozen_peak > 1.8 * moving_peak,
       "frozen measures at least ~1.8x what moving measures for this same event")

    me, fe = hit_moving[0], hit_frozen[0]
    moving_width = me["end_sample"] - me["start_sample"] + 1
    ck(moving_width > 1.5 * 400,
       "moving baseline's reported width is smeared well past the true 400-sample plateau")
    ckEq(fe["start_sample"], 800, "frozen excursion opens exactly where the plateau starts")
    ckEq(fe["end_sample"], 1199, "frozen excursion closes exactly where the plateau ends")
    ckEq(fe["n_samples"], 400, "frozen excursion's width matches the true plateau exactly")
    ckEq(fe["baseline_mode"], "frozen", "the row records which baseline mode produced it")
    ckEq(me["baseline_mode"], "moving", "the row records which baseline mode produced it")
    ckEq(int(fe["baseline_n_quiet"]), 200,
        "the frozen baseline used a full pre_window of quiet history")


def test_frozen_baseline_is_constant_across_the_whole_excursion():
    print("frozen baseline never changes within one excursion, however long")
    specs = [(1900, 1000, 0, 0)] * 300 + [(2050, 1000, 0, 0)] * 500
    path = write_capture([build_batch(specs, batch_seq=1, first_sample_seq=0, t0_us=0)])
    result = E.analyze_captures(path, entry_threshold=30.0, exit_threshold=15.0,
                                baseline_mode="frozen", pre_window=200,
                                baseline_method="mean")
    os.unlink(path)

    exc = result["excursions"]
    ckEq(len(exc), 1, "one excursion found")
    e = exc[0]
    ctx = result["sessions"][e["session"]]
    baseline = ctx["baseline"]
    span = range(e["start_sample"], e["end_sample"] + 1)
    values = {baseline[i] for i in span}
    ckEq(len(values), 1, "the baseline array holds exactly one value across the whole excursion")
    ckClose(next(iter(values)), 1900.0, "that value is the true pre-event baseline", tol=1e-6)


def test_frozen_baseline_uses_only_quiet_history_between_close_excursions():
    """Two plateaus separated by a quiet gap shorter than pre_window. The
    quiet window is a persistent deque (it is not reset at each excursion
    boundary), so the second excursion's baseline legitimately mixes OLD
    quiet history from before the first excursion with the NEW quiet gap
    samples -- both are genuinely quiet. What must never happen is the
    first excursion's own ELEVATED samples leaking in. The gap's raw value
    (1920) is deliberately distinct from both the lead-in (1900) and the
    plateaus (2000/2050), so contamination from the first excursion would
    be arithmetically visible: 30 contaminated samples at 2000 instead of
    1920 would pull the mixed mean to 1915.0, not the 1903.0 computed from
    genuinely-quiet values alone."""
    print("frozen baseline between close excursions is never padded with excursion samples")
    lead_in = [(1900, 1000, 0, 0)] * 300          # fills the 200-sample quiet window
    first = [(2000, 1000, 0, 0)] * 50             # first excursion, elevated
    gap = [(1920, 1000, 0, 0)] * 30               # true quiet gap, shorter than pre_window
    second = [(2050, 1000, 0, 0)] * 50            # second excursion, elevated
    specs = lead_in + first + gap + second
    path = write_capture([build_batch(specs, batch_seq=1, first_sample_seq=0, t0_us=0)])
    exc = find_frozen(path, pre_window=200, entry_threshold=50.0, exit_threshold=30.0)
    os.unlink(path)

    ckEq(len(exc), 2, "two excursions found, separated by the quiet gap")
    first_exc, second_exc = exc
    ckClose(float(first_exc["baseline_at_excursion"]), 1900.0,
           "the first excursion's baseline is the pure lead-in value", tol=1e-6)
    ckEq(int(second_exc["baseline_n_quiet"]), 200,
        "the second excursion's baseline used a full window: old lead-in "
        "quiet samples plus the new quiet gap -- both legitimately quiet")
    ckClose(float(second_exc["baseline_at_excursion"]), 1903.0,
           "the mix (170 old @1900 + 30 new @1920) is exactly 1903.0 -- proving "
           "the first excursion's elevated samples (which would give 1915.0 "
           "if they had leaked in) never entered the quiet window", tol=1e-6)


def test_low_pwm_dwell_flagged_and_separately_reportable():
    print("a low-PWM dwell window is flagged on overlapping excursions, not on others")
    lead_in = [(1900, 1000, 0, 90)] * 100
    dwell = [(1900, 1000, 0, 90)] * 1200          # 1.2s at PWM 0 -- a dwell
    spike_in_dwell = [(2200, 1000, 0, 90)]        # one spike WHILE stopped
    moving = [(1900, 1000, 70, 90)] * 200
    spike_outside = [(2200, 1000, 70, 90)]        # one spike while moving
    tail = [(1900, 1000, 70, 90)] * 100
    specs = lead_in + dwell + spike_in_dwell + moving + spike_outside + tail
    path = write_capture([build_batch(specs, batch_seq=1, first_sample_seq=0, t0_us=0)])
    exc = find_frozen(path, dwell_pwm_max=5.0, dwell_min_ms=1000.0)
    os.unlink(path)

    in_dwell_start = 100 + 1200
    outside_start = 100 + 1200 + 1 + 200
    hit_in = [e for e in exc if e["start_sample"] == in_dwell_start]
    hit_out = [e for e in exc if e["start_sample"] == outside_start]
    ckEq(len(hit_in), 1, "the spike during the dwell is found")
    ckEq(len(hit_out), 1, "the spike outside the dwell is found")
    ckEq(hit_in[0]["in_low_pwm_dwell"], 1, "the in-dwell spike is flagged")
    ckEq(hit_out[0]["in_low_pwm_dwell"], 0, "the moving-PWM spike is not flagged")


def test_compare_baselines_produces_alt_columns():
    print("--compare-baselines measures the same excursion against both modes")
    lead_in = [(1900, 1000, 0, 0)] * 800
    plateau = [(2100, 1000, 0, 0)] * 400
    tail = [(1900, 1000, 0, 0)] * 800
    specs = lead_in + plateau + tail
    path = write_capture([build_batch(specs, batch_seq=1, first_sample_seq=0, t0_us=0)])
    exc = find_frozen(path, pre_window=200, compare_baselines=True)
    os.unlink(path)

    hit = [e for e in exc if e["start_sample"] <= 900 <= e["end_sample"]]
    ckEq(len(hit), 1, "one excursion found")
    e = hit[0]
    ckEq(e["alt_baseline_mode"], "moving", "the alt mode is the OTHER mode from the primary")
    ck(e["alt_max_abs_flux"] != "", "the alt peak column is populated")
    ck(float(e["alt_max_abs_flux"]) < float(e["max_abs_flux"]),
       "the alt (moving) measurement of this same broad event is smaller, "
       "exactly matching the direct moving-vs-frozen comparison above")


def main():
    print("HALL_WAVEFORM_TEST — excursion analysis tests (investigatory)\n")
    test_tall_narrow_spike_survives()
    test_broad_positive_and_negative_excursions_measured()
    test_baseline_drift_does_not_hide_excursion()
    test_missing_samples_never_interpolated()
    test_integrated_flux_uses_actual_intervals()
    test_sequence_wraparound_does_not_break_timing()
    test_frozen_baseline_does_not_absorb_broad_excursion()
    test_frozen_baseline_is_constant_across_the_whole_excursion()
    test_frozen_baseline_uses_only_quiet_history_between_close_excursions()
    test_low_pwm_dwell_flagged_and_separately_reportable()
    test_compare_baselines_produces_alt_columns()
    print("\n%d checks, %d failures" % (checks, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

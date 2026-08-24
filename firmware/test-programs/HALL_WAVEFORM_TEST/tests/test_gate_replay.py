#!/usr/bin/env python3
"""test_gate_replay.py — host tests for tools/hwt_gate_replay.py.

INVESTIGATORY / UNAPPROVED.

Builds small synthetic captures against a small synthetic QuorumMap (NOT
the real 171-marker map, so these tests do not depend on the firmware's
current map contents) and checks the gate pipeline's decisions directly:
narrow spikes are rejected regardless of polarity, broad plausible curves
are accepted exactly when timing and expected polarity both hold, opposite-
polarity-too-soon is diagnosed specifically as a probable return (not just
a generic timing failure), incomplete excursions never advance position,
a flipped polarity convention visibly flips the outcome instead of quietly
adapting, and predicted position advances by exactly one marker per
ACCEPT_EXPECTED_MARKER and never otherwise.

Synthetic map: 6 markers, alternating polarity (N,S,N,S,N,S), 300 mm apart,
direction CW unless stated. vel_model_slope=1.0, vel_model_intercept=0.0,
so velocity_mm_s(pwm) == pwm and the max-credible-speed bound (evaluated at
PWM 255) is exactly 255 mm/s -- giving a minimum physical time to the next
marker of 300/255 = 1.176470... s, used throughout via qmap.

Run via ./run_tests.sh.
"""

import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", "tools"))
sys.path.insert(0, TOOLS)

import hwt_format as F               # noqa: E402
import hwt_gate_replay as G          # noqa: E402
from quorum_map import QuorumMap     # noqa: E402

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


def make_qmap():
    return QuorumMap.from_data(dna=[1, 0, 1, 0, 1, 0], spacing_mm=[300] * 6,
                               vel_model_slope=1.0, vel_model_intercept=0.0)


MIN_TIME_S = 300.0 / 255.0   # spacing / max_credible_speed_mm_s(), this map
LEAD = int(MIN_TIME_S * 1000) + 500   # a lead-in comfortably over MIN_TIME_S,
                                       # in ms/samples (dt=1ms): use this BEFORE
                                       # any event that a test wants judged
                                       # against a plausible (not "too soon")
                                       # elapsed time from a t=0.0 manifest seed


def make_manifest(*, capture, direction="CW", start_mm=0, start_time_s=0.0,
                  positive_deviation_is="N"):
    return G.RunManifest.from_data(
        capture=capture, direction=direction, start_mm=start_mm,
        start_time_s=start_time_s, positive_deviation_is=positive_deviation_is)


def build_batch(specs, *, batch_seq=1, first_sample_seq=0, t0_us=0, session=0xC0FFEE):
    """specs: list of (raw, dt_us, pwm_actual, pwm_commanded)."""
    payload = b"".join(F.pack_sample(raw, dt_us=dt_us, pwm_actual=pa, pwm_commanded=pc)
                       for raw, dt_us, pa, pc in specs)
    return F.build_record(F.REC_SAMPLES, payload, session_id=session, batch_seq=batch_seq,
                          first_sample_seq=first_sample_seq, t0_us=t0_us, n_samples=len(specs))


def write_capture(records):
    fd, path = tempfile.mkstemp(suffix=".hwt")
    with os.fdopen(fd, "wb") as fh:
        F.write_capture_header(fh, 0)
        for r in records:
            F.write_frame(fh, 0, r)
    return path


def write_capture_from_specs(specs, chunk=5000):
    """One UDP datagram is capped at 65535 bytes (FRAME_FMT's length field is
    a uint16) -- at 10 bytes/sample that is ~6553 samples per record. Split
    a long specs list into consecutive batches, exactly as the real firmware
    would across multiple datagrams, so tests can build arbitrarily long
    synthetic runs without hand-tracking this limit at every call site."""
    records = []
    seq = 0
    t0 = 0
    for i in range(0, len(specs), chunk):
        part = specs[i:i + chunk]
        records.append(build_batch(part, batch_seq=len(records) + 1,
                                   first_sample_seq=seq, t0_us=t0))
        seq += len(part)
        t0 += sum(s[1] for s in part)
    return write_capture(records)


PWM = 70   # comfortably above GATE_LOW_PWM_FLOOR-equivalents; not used by this
          # prototype's gates directly, but kept realistic on every sample


def flat(n, level=1900, pwm=PWM):
    return [(level, 1000, pwm, pwm)] * n


def plateau(n, delta, level=1900, pwm=PWM):
    """A flat-topped excursion: constant deviation, continuity_ratio == 0.0
    (no internal sign changes) regardless of width -- see
    hwt_gate_replay.continuity_ratio()'s docstring for why a flat top reads
    as maximally smooth by this measure."""
    return [(level + delta, 1000, pwm, pwm)] * n


def run(path, manifest, qmap, **gate_kwargs):
    events, _ctx = G.build_acquisition_events(path, manifest)
    defaults = dict(min_duration_ms=40.0, min_abs_flux=300.0)
    defaults.update(gate_kwargs)
    events, streaks = G.replay(events, qmap, manifest, **defaults)
    return events, streaks


def test_narrow_spike_expected_polarity_rejected_without_advancing():
    print("narrow spike, expected polarity -- rejected, no advancement")
    qmap = make_qmap()
    specs = flat(300) + [(1900 + 200, 1000, PWM, PWM)] + flat(300)   # 1-sample spike, N-polarity
    path = write_capture_from_specs(specs)
    manifest = make_manifest(capture=path, start_mm=0, start_time_s=0.0)
    events, _ = run(path, manifest, qmap)
    os.unlink(path)

    ck(len(events) >= 1, "at least one candidate event found")
    e = events[0]
    ckEq(e["disp_final"], "REJECT_SPIKE", "a 1-sample event fails the morphology bar")
    ckEq(e["disp_predicted_mm_before"], e["disp_predicted_mm_after"],
        "predicted position is unchanged by a rejected event")


def test_narrow_spike_wrong_polarity_rejected_without_advancing():
    print("narrow spike, wrong polarity -- rejected, no advancement")
    qmap = make_qmap()
    specs = flat(300) + [(1900 - 200, 1000, PWM, PWM)] + flat(300)   # 1-sample spike, S-polarity
    path = write_capture_from_specs(specs)
    manifest = make_manifest(capture=path, start_mm=0, start_time_s=0.0)
    events, _ = run(path, manifest, qmap)
    os.unlink(path)

    e = events[0]
    ckEq(e["disp_final"], "REJECT_SPIKE",
        "morphology gate rejects it BEFORE polarity is ever compared -- same "
        "disposition regardless of which polarity the spike happened to have")
    ckEq(e["disp_predicted_mm_before"], e["disp_predicted_mm_after"], "no advancement")


def test_broad_expected_polarity_plausible_time_accepted_once():
    print("broad expected-polarity curve at a plausible time -- accepted once")
    qmap = make_qmap()
    specs = flat(LEAD) + plateau(100, +150) + flat(LEAD)
    path = write_capture_from_specs(specs)
    # start_mm=0 (N); expected next marker (CW) is mm=1 (S). The event itself
    # must be N to match mm=0's own polarity... no: expected polarity is for
    # the NEXT marker. mm=0 dna=1(N), mm=1 dna=0(S). A positive plateau reads
    # N here, which would NOT match mm=1's expected S. Use start_mm=1 (S) so
    # the expected next marker (mm=2, N) matches this positive/N plateau.
    manifest = make_manifest(capture=path, start_mm=1, start_time_s=0.0)
    events, _ = run(path, manifest, qmap)
    os.unlink(path)

    ckEq(len(events), 1, "one candidate event")
    e = events[0]
    ckEq(e["disp_final"], "ACCEPT_EXPECTED_MARKER", "plausible, timely, expected polarity -> accepted")
    ckEq(int(e["disp_predicted_mm_before"]), 1, "predicted position starts at the declared start")
    ckEq(int(e["disp_predicted_mm_after"]), 2, "predicted position advances by exactly one marker, CW")


def test_broad_wrong_polarity_plausible_time_rejected_no_advance():
    print("broad WRONG-polarity curve at a plausible time -- rejected, no advancement")
    qmap = make_qmap()
    specs = flat(LEAD) + plateau(100, -150) + flat(LEAD)   # S-polarity
    path = write_capture_from_specs(specs)
    # start_mm=1 (S); expected next (mm=2) is N. This event is S -- wrong.
    manifest = make_manifest(capture=path, start_mm=1, start_time_s=0.0)
    events, _ = run(path, manifest, qmap)
    os.unlink(path)

    e = events[0]
    ckEq(e["disp_final"], "REJECT_WRONG_EXPECTED_POLARITY",
        "plausible and timely, but polarity contradicts the expected next marker -- \"No Way\"")
    ckEq(e["disp_predicted_mm_before"], e["disp_predicted_mm_after"],
        "a wrong-polarity event never advances the predicted position")


def test_opposite_polarity_impossibly_soon_rejected_as_probable_return():
    print("opposite-polarity response impossibly soon after acceptance -- probable return")
    qmap = make_qmap()
    too_soon_gap = 300   # 0.3s, well under MIN_TIME_S (~1.176s)
    specs = (flat(LEAD) + plateau(100, +150) + flat(too_soon_gap)   # accepted: N
            + plateau(100, -150) + flat(300))                        # too soon, opposite: S
    path = write_capture_from_specs(specs)
    manifest = make_manifest(capture=path, start_mm=1, start_time_s=0.0)   # mm1(S)->expect mm2(N)
    events, _ = run(path, manifest, qmap)
    os.unlink(path)

    ckEq(len(events), 2, "two candidate events")
    ckEq(events[0]["disp_final"], "ACCEPT_EXPECTED_MARKER", "the first, well-timed N event is accepted")
    ckEq(events[1]["disp_final"], "REJECT_PROBABLE_RETURN",
        "opposite polarity to the just-accepted marker, arriving before the minimum physical "
        "travel time -- diagnosed as a same-magnet return, not a generic timing failure")
    ckEq(events[1]["disp_predicted_mm_before"], events[1]["disp_predicted_mm_after"],
        "a probable-return event never advances the predicted position")


def test_legitimate_opposite_polarity_next_marker_plausible_time_accepted():
    print("legitimate opposite-polarity next marker at a plausible time -- accepted")
    qmap = make_qmap()
    plausible_gap = int(MIN_TIME_S * 1000) + 500
    specs = (flat(LEAD) + plateau(100, +150) + flat(LEAD)      # accepted: N (mm1->mm2)
            + plateau(100, -150) + flat(plausible_gap))              # plausible, opposite: S (mm2->mm3)
    path = write_capture_from_specs(specs)
    manifest = make_manifest(capture=path, start_mm=1, start_time_s=0.0)
    events, _ = run(path, manifest, qmap)
    os.unlink(path)

    ckEq(len(events), 2, "two candidate events")
    ckEq(events[0]["disp_final"], "ACCEPT_EXPECTED_MARKER", "first event accepted")
    ckEq(events[1]["disp_final"], "ACCEPT_EXPECTED_MARKER",
        "second event: opposite polarity from the first is EXPECTED here (mm2 is N, mm3 is S) "
        "and arrives with plenty of physical time -- accepted")
    ckEq(int(events[1]["disp_predicted_mm_after"]), 3, "position now at mm3")


def test_same_polarity_impossibly_soon_rejected_by_physical_timing():
    print("same-polarity response impossibly soon -- rejected by physical timing, not 'return'")
    qmap = make_qmap()
    too_soon_gap = 300
    specs = (flat(LEAD) + plateau(100, +150) + flat(too_soon_gap)   # accepted: N
            + plateau(100, +150) + flat(300))                        # too soon, SAME polarity: N
    path = write_capture_from_specs(specs)
    manifest = make_manifest(capture=path, start_mm=1, start_time_s=0.0)
    events, _ = run(path, manifest, qmap)
    os.unlink(path)

    ckEq(events[0]["disp_final"], "ACCEPT_EXPECTED_MARKER", "first event accepted")
    ckEq(events[1]["disp_final"], "REJECT_PHYSICALLY_TOO_SOON",
        "same polarity as the previous accepted marker, so this is NOT a return pattern -- "
        "the plain physical-timing rejection applies instead")
    ckEq(events[1]["disp_predicted_mm_before"], events[1]["disp_predicted_mm_after"], "no advancement")


def test_slow_genuine_broad_response_not_rejected_merely_for_width():
    print("a slow (wide) genuine broad response is not rejected merely for being wide")
    qmap = make_qmap()
    specs = flat(LEAD) + plateau(2000, +150) + flat(LEAD)   # 2 SECONDS wide
    path = write_capture_from_specs(specs)
    manifest = make_manifest(capture=path, start_mm=1, start_time_s=0.0)
    events, _ = run(path, manifest, qmap)
    os.unlink(path)

    e = events[0]
    ck(float(e["phys_duration_ms"]) > 1900, "sanity: this excursion really is wide (>1.9s)")
    ckEq(e["disp_final"], "ACCEPT_EXPECTED_MARKER",
        "width alone does not fail the morphology gate when flux and continuity are fine")


def test_incomplete_response_excluded_and_does_not_advance():
    print("an incomplete (gapped) response is rejected and never advances position")
    qmap = make_qmap()
    lead_in = flat(300)
    onset = plateau(50, +150)          # samples 300..349, opens
    resume = plateau(50, +150)         # samples 400..449 (after a real transport gap)
    tail_gap = int(MIN_TIME_S * 1000) + 500
    tail = flat(tail_gap)
    b1 = build_batch(lead_in + onset, batch_seq=1, first_sample_seq=0, t0_us=0)
    # batch_seq 2, samples 350..399, never arrives -- genuine transport loss
    b2 = build_batch(resume + tail, batch_seq=3, first_sample_seq=400, t0_us=400000)
    path = write_capture([b1, b2])
    manifest = make_manifest(capture=path, start_mm=1, start_time_s=0.0)
    events, _ = run(path, manifest, qmap)
    os.unlink(path)

    ckEq(len(events), 1, "one excursion spans the gap (frozen detector, same as before)")
    e = events[0]
    ckEq(int(e["phys_incomplete"]), 1, "the excursion is correctly flagged incomplete")
    ckEq(e["disp_final"], "REJECT_INCOMPLETE", "an incomplete response is excluded, not accepted")
    ckEq(e["disp_predicted_mm_before"], e["disp_predicted_mm_after"], "no advancement")


def test_wrong_polarity_convention_fails_visibly():
    print("a wrong polarity-orientation convention fails visibly, not silently")
    qmap = make_qmap()
    specs = flat(LEAD) + plateau(100, +150) + flat(LEAD)   # positive deviation
    path = write_capture_from_specs(specs)

    manifest_right = make_manifest(capture=path, start_mm=1, start_time_s=0.0,
                                   positive_deviation_is="N")
    manifest_wrong = make_manifest(capture=path, start_mm=1, start_time_s=0.0,
                                   positive_deviation_is="S")   # flipped convention
    events_right, _ = run(path, manifest_right, qmap)
    events_wrong, _ = run(path, manifest_wrong, qmap)
    os.unlink(path)

    ckEq(events_right[0]["disp_final"], "ACCEPT_EXPECTED_MARKER",
        "with the correct convention, this event is accepted")
    ckEq(events_wrong[0]["disp_final"], "REJECT_WRONG_EXPECTED_POLARITY",
        "with the convention flipped, the SAME raw data visibly disagrees with the map instead "
        "of silently being re-interpreted to agree")
    ckEq(events_right[0]["det_open_polarity"] == events_wrong[0]["det_open_polarity"], False,
        "the two conventions assign genuinely different polarities to the identical measurement")


def test_repeated_rejected_events_never_advance_position():
    print("repeated rejected events never enter the ring or alter predicted position")
    qmap = make_qmap()
    specs = flat(200)
    for _ in range(5):
        specs += [(1900 + 200, 1000, PWM, PWM)] + flat(200)   # 5 separate 1-sample spikes
    path = write_capture_from_specs(specs)
    manifest = make_manifest(capture=path, start_mm=1, start_time_s=0.0)
    events, _ = run(path, manifest, qmap)
    os.unlink(path)

    ckEq(len(events), 5, "five candidate spikes")
    for e in events:
        ckEq(e["disp_final"], "REJECT_SPIKE", "each one individually rejected")
        ckEq(int(e["disp_predicted_mm_before"]), 1, "predicted position never moves off the start")
        ckEq(int(e["disp_predicted_mm_after"]), 1, "predicted position never moves off the start")


def test_position_advances_exactly_once_per_accepted_marker():
    print("predicted position advances exactly once per ACCEPT_EXPECTED_MARKER")
    qmap = make_qmap()
    # three consecutive genuine, alternating-polarity, well-timed markers,
    # each preceded by an interspersed narrow spike (rejected -- must not
    # itself count as an advance, and must not eat into the NEXT plateau's
    # elapsed-time budget: the spike's own morphology rejection happens
    # before timing is even checked, but the flat(LEAD) that follows it
    # still has to fully separate this iteration's plateau from the
    # PREVIOUS iteration's accepted marker for the timing gate to pass)
    specs = flat(LEAD)   # clears MIN_TIME_S for the very first plateau, from the t=0 seed
    deltas = [+150, -150, +150]   # N, S, N -- matches mm1->2->3->4 (S,N,S,N; mm1=S)
    for d in deltas:
        specs += [(1900 + (200 if d > 0 else -200), 1000, PWM, PWM)]   # interspersed spike, either polarity
        specs += flat(LEAD)
        specs += plateau(100, d)
        specs += flat(50)   # small buffer; no gate depends on this tail
    path = write_capture_from_specs(specs)
    manifest = make_manifest(capture=path, start_mm=1, start_time_s=0.0)   # mm1=S
    events, _ = run(path, manifest, qmap)
    os.unlink(path)

    accepted = [e for e in events if e["disp_final"] == "ACCEPT_EXPECTED_MARKER"]
    ckEq(len(accepted), 3, "all three genuine markers accepted")
    ckEq(int(events[-1]["disp_predicted_mm_after"]), 4,
        "position advanced by exactly 3 (one per accept): mm1 -> mm4")
    for e in events:
        if e["disp_final"] != "ACCEPT_EXPECTED_MARKER":
            ckEq(e["disp_predicted_mm_before"], e["disp_predicted_mm_after"],
                "a non-accepted event (the interspersed spikes) never advances position")


def jagged_plateau(n, high, low, block=5, level=1900, pwm=PWM):
    """A broad excursion that is NOT flat-topped: alternates between `high`
    and `low` (both above the entry threshold) every `block` samples, so
    its continuity_ratio is well above zero at any reasonable dead zone --
    unlike plateau(), which is always exactly 0.0. Used only to prove that
    disposition does not depend on this value; the corrected pipeline does
    not use continuity_ratio for anything else."""
    out = []
    hi = True
    while len(out) < n:
        out += [(level + (high if hi else low), 1000, pwm, pwm)] * min(block, n - len(out))
        hi = not hi
    return out


def test_continuity_settings_cannot_change_disposition():
    print("changing continuity settings (dead zone, or omitting the legacy gate "
         "entirely) cannot change any disposition -- continuity is diagnostic-only")
    qmap = make_qmap()
    specs = flat(LEAD) + jagged_plateau(150, 200, 150, block=5) + flat(LEAD)
    path = write_capture_from_specs(specs)
    manifest = make_manifest(capture=path, start_mm=1, start_time_s=0.0)

    events_by_dead_zone = {}
    for dz in (0.0, 5.0, 20.0, 40.0, 80.0):
        evs, _ctx = G.build_acquisition_events(path, manifest, continuity_dead_zone=dz)
        evs, _streaks = G.replay(evs, qmap, manifest, min_duration_ms=40.0, min_abs_flux=300.0)
        events_by_dead_zone[dz] = evs
    os.unlink(path)

    ratios = {dz: evs[0]["det_continuity_ratio"] for dz, evs in events_by_dead_zone.items()}
    ck(len(set(ratios.values())) > 1,
      "sanity: the dead zone DOES change the diagnostic det_continuity_ratio value "
      "(ratios seen: %s) -- so the disposition check below is not vacuous" % ratios)

    dispositions = {dz: evs[0]["disp_final"] for dz, evs in events_by_dead_zone.items()}
    first = next(iter(dispositions.values()))
    for dz, disp in dispositions.items():
        ckEq(disp, first,
            "disposition must be identical across every continuity_dead_zone setting "
            "(dead_zone=%s gave %s, expected %s)" % (dz, disp, first))
    ckEq(first, "ACCEPT_EXPECTED_MARKER",
        "sanity: this jagged-but-otherwise-plausible, well-timed, expected-polarity "
        "curve IS accepted -- a high continuity_ratio does not cause a rejection "
        "under the corrected pipeline")

    # And explicitly: the legacy comparison path, when NOT invoked (the default,
    # matching every call above and every production code path), behaves
    # identically regardless of how jagged the curve is -- confirmed above. Here,
    # confirm the inverse holds too: the legacy path, when explicitly invoked with
    # a strict threshold, DOES reject this same jagged curve -- proving the
    # diagnostic field is real, non-trivial data, and that its absence from the
    # default pipeline is a deliberate omission, not an accident of it never
    # having anything to reject. (path was already unlinked above; rebuild fresh.)
    path2 = write_capture_from_specs(specs)
    evs2, _ctx = G.build_acquisition_events(path2, manifest, continuity_dead_zone=0.0)
    evs2, _streaks = G.replay(evs2, qmap, manifest, min_duration_ms=40.0, min_abs_flux=300.0,
                              legacy_continuity_max_ratio=0.5)
    os.unlink(path2)
    ckEq(evs2[0]["disp_final"], "REJECT_SPIKE",
        "the WITHDRAWN legacy gate, only when explicitly re-enabled for comparison, "
        "still rejects this jagged curve -- confirming det_continuity_ratio is "
        "genuine, non-trivial data and its removal from the default pipeline was a "
        "deliberate correction, not a no-op")


def main():
    print("HALL_WAVEFORM_TEST — gate-replay tests (investigatory)\n")
    test_narrow_spike_expected_polarity_rejected_without_advancing()
    test_narrow_spike_wrong_polarity_rejected_without_advancing()
    test_broad_expected_polarity_plausible_time_accepted_once()
    test_broad_wrong_polarity_plausible_time_rejected_no_advance()
    test_opposite_polarity_impossibly_soon_rejected_as_probable_return()
    test_legitimate_opposite_polarity_next_marker_plausible_time_accepted()
    test_same_polarity_impossibly_soon_rejected_by_physical_timing()
    test_slow_genuine_broad_response_not_rejected_merely_for_width()
    test_incomplete_response_excluded_and_does_not_advance()
    test_wrong_polarity_convention_fails_visibly()
    test_repeated_rejected_events_never_advance_position()
    test_position_advances_exactly_once_per_accepted_marker()
    test_continuity_settings_cannot_change_disposition()
    print("\n%d checks, %d failures" % (checks, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

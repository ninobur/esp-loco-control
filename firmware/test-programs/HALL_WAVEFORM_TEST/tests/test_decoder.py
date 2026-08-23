#!/usr/bin/env python3
"""test_decoder.py — host tests for the HALL_WAVEFORM_TEST receiver/decoder.

INVESTIGATORY / UNAPPROVED.

Builds synthetic capture files (transport loss, duplication, reordering,
corruption, declared missed slots, a reboot mid-file) and checks that the
decoder puts the surviving samples in order and names every hole for what it
was. Run via ./run_tests.sh.
"""

import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", "tools"))
sys.path.insert(0, TOOLS)

import hwt_format as F          # noqa: E402
import hwt_decode as D          # noqa: E402

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


N = 125


def batch(seq, first, t0, *, session=0xAAAA, missed_before=0, drops=0,
          base=1900, two_channel=False):
    payload = b""
    for i in range(N):
        payload += F.pack_sample(base + i, (4095 - (base + i)) if two_channel else 0,
                                 dt_us=1000, pwm_actual=70, pwm_commanded=90,
                                 ch1_present=two_channel)
    return F.build_record(F.REC_SAMPLES, payload, session_id=session,
                          batch_seq=seq, first_sample_seq=first, t0_us=t0,
                          n_samples=N, missed_before=missed_before,
                          cum_queue_drops=drops)


def anchor(seq, sample_seq, text, *, session=0xAAAA, t_us=0):
    import struct
    p = struct.pack(F.ANCHOR_FMT, 1, sample_seq, t_us, 2, 70, 90,
                    len(text), text.encode())
    return F.build_record(F.REC_ANCHOR, p, session_id=session, batch_seq=seq,
                          first_sample_seq=sample_seq)


def write_capture(records):
    fd, path = tempfile.mkstemp(suffix=".hwt")
    with os.fdopen(fd, "wb") as fh:
        F.write_capture_header(fh, 0)
        for r in records:
            F.write_frame(fh, 0, r)
    return path


def samples_of(rows):
    return [r for r in rows if r["row_type"] == "SAMPLE"]


def kinds(rows, kind):
    return [r for r in rows if r["row_type"] == kind]


def test_clean_run():
    print("clean run decodes in order")
    recs = [batch(i + 1, i * N, i * N * 1000) for i in range(4)]
    path = write_capture(recs)
    rows, rep = D.decode(path)
    os.unlink(path)

    ss = samples_of(rows)
    ckEq(len(ss), 4 * N, "every sample decoded")
    ckEq(rep["samples"], 4 * N, "report counts every sample")
    seqs = [int(r["phys_sample_seq"]) for r in ss]
    ckEq(seqs, list(range(4 * N)), "samples are in sequence order")
    ts = [float(r["phys_t_s"]) for r in ss]
    ck(all(b > a for a, b in zip(ts, ts[1:])), "timestamps increase monotonically")
    ckEq(rep["transport_lost_batches"], 0, "no transport loss reported")
    ckEq(len(kinds(rows, "GAP")), 0, "no gap rows on a clean run")


def test_out_of_order_and_duplicate_arrival():
    print("reordered and duplicated datagrams")
    recs = [batch(i + 1, i * N, i * N * 1000) for i in range(4)]
    shuffled = [recs[2], recs[0], recs[3], recs[0], recs[1]]   # late, duplicated
    path = write_capture(shuffled)
    rows, rep = D.decode(path)
    os.unlink(path)

    seqs = [int(r["phys_sample_seq"]) for r in samples_of(rows)]
    ckEq(seqs, list(range(4 * N)), "arrival order does not affect output order")
    ckEq(rep["duplicate_batches"], 1, "the duplicate is counted")
    ckEq(rep["samples"], 4 * N, "the duplicate is not counted twice")


def test_transport_loss():
    print("transport loss is named and bounded")
    recs = [batch(i + 1, i * N, i * N * 1000) for i in range(5)]
    del recs[2]                       # batch 3 never arrives
    path = write_capture(recs)
    rows, rep = D.decode(path)
    os.unlink(path)

    ckEq(rep["transport_gaps"], 1, "one transport gap found")
    ckEq(rep["transport_lost_batches"], 1, "one batch lost")
    ckEq(rep["samples"], 4 * N, "surviving samples all decoded")
    gaps = kinds(rows, "GAP")
    ckEq(len(gaps), 1, "a GAP row marks the hole")
    ck("samples 250..374 absent" in gaps[0]["info"],
       "the GAP row names the exact missing sample range")
    seqs = [int(r["phys_sample_seq"]) for r in samples_of(rows)]
    ck(250 not in seqs and 374 not in seqs, "lost samples are not invented")
    ck(249 in seqs and 375 in seqs, "samples either side survive")


def test_declared_missed_slots():
    print("slots the locomotive never acquired")
    recs = [batch(1, 0, 0),
            batch(2, N + 40, (N + 40) * 1000, missed_before=40)]
    path = write_capture(recs)
    rows, rep = D.decode(path)
    os.unlink(path)

    ckEq(rep["declared_missed_slots"], 40, "declared missed slots are reported")
    m = kinds(rows, "MISSED")
    ckEq(len(m), 1, "a MISSED row marks the stall")
    ck("never acquired" in m[0]["info"], "MISSED says the samples do not exist")
    ck(rep["transport_gaps"] == 0,
       "a sampler stall is NOT reported as transport loss")


def test_corrupt_record():
    print("corrupted datagram is rejected, not decoded")
    good = batch(1, 0, 0)
    bad = bytearray(batch(2, N, N * 1000))
    bad[F.HDR_LEN + 7] ^= 0xFF          # flip a bit in the payload
    path = write_capture([good, bytes(bad)])
    rows, rep = D.decode(path)
    os.unlink(path)

    ckEq(rep["bad_records"], 1, "the corrupted record is counted")
    ckEq(rep["samples"], N, "its samples are not admitted to the record")
    ck(any("CRC" in b for b in rep["bad_detail"]), "the reason given is the CRC")
    ckEq(len(kinds(rows, "BAD")), 1, "a BAD row appears in the CSV")


def test_anchors_are_placed_and_kept_separate():
    print("operator anchors")
    recs = [batch(1, 0, 0),
            anchor(2, 60, "patio-marker-15"),
            batch(3, N, N * 1000),
            anchor(4, 180, "second-pass")]
    path = write_capture(recs)
    rows, rep = D.decode(path)
    os.unlink(path)

    ckEq(rep["anchors"], 2, "both anchors decoded")
    a = kinds(rows, "ANCHOR")
    ckEq([r["op_text"] for r in a], ["patio-marker-15", "second-pass"],
         "anchor text is preserved, in order")
    ck(all(r["phys_ch0_raw"] == "" for r in a),
       "an anchor carries no physical measurement of its own")
    # An anchor is emitted immediately BEFORE the sample it names, so the
    # waveform after the line is what the operator was looking at.
    idx = [i for i, r in enumerate(rows) if r["row_type"] == "ANCHOR"][0]
    before = [r for r in rows[:idx] if r["row_type"] == "SAMPLE"]
    after = [r for r in rows[idx:] if r["row_type"] == "SAMPLE"]
    ckEq(int(before[-1]["phys_sample_seq"]), 59, "anchor follows sample 59")
    ckEq(int(after[0]["phys_sample_seq"]), 60, "anchor precedes the sample it names")


def test_session_boundary():
    print("reboot mid-file")
    recs = [batch(1, 0, 0, session=0x1111),
            batch(2, N, N * 1000, session=0x1111),
            batch(1, 0, 0, session=0x2222)]
    path = write_capture(recs)
    rows, rep = D.decode(path)
    os.unlink(path)

    ckEq(rep["sessions"], 2, "both boot sessions seen")
    s = kinds(rows, "SESSION")
    ckEq(len(s), 1, "a SESSION row separates them")
    ck("do not join" in s[0]["info"], "the SESSION row says not to join across it")
    ckEq(rep["transport_gaps"], 0,
         "a reboot is not miscounted as transport loss")


def test_dual_channel_alignment():
    print("dual-channel alignment through the decoder")
    recs = [batch(1, 0, 0, two_channel=True)]
    path = write_capture(recs)
    rows, _rep = D.decode(path)
    os.unlink(path)
    ss = samples_of(rows)
    ck(all(r["phys_ch1_raw"] != "" for r in ss), "channel B present in every row")
    ck(all(int(r["phys_ch0_raw"]) + int(r["phys_ch1_raw"]) == 4095 for r in ss),
       "both channels stay paired, same row, same timestamp")

    # Single-sensor build (the default): B must be blank, not zero.
    recs = [batch(1, 0, 0, two_channel=False)]
    path = write_capture(recs)
    rows, _rep = D.decode(path)
    os.unlink(path)
    ss = samples_of(rows)
    ck(all(r["phys_ch1_raw"] == "" for r in ss),
       "an absent channel is blank, never a fabricated zero reading")
    ck(all(r["ch1_present"] == 0 for r in ss), "absence is stated explicitly")


def test_plot_loader_breaks_the_trace():
    """The plotter must not draw a straight line across samples that do not
    exist — a drawn-through gap is a fabricated waveform."""
    print("plot loader leaves holes open")
    import csv as _csv
    import hwt_plot as P

    recs = [batch(i + 1, i * N, i * N * 1000) for i in range(4)]
    del recs[1]
    path = write_capture(recs)
    rows, _rep = D.decode(path)
    os.unlink(path)

    fd, csv_path = tempfile.mkstemp(suffix=".csv")
    with os.fdopen(fd, "w", newline="") as fh:
        w = _csv.DictWriter(fh, fieldnames=D.COLUMNS)
        w.writeheader()
        w.writerows(rows)
    samples, anchors, breaks = P.load(csv_path)
    os.unlink(csv_path)

    ckEq(sum(1 for x in samples if x is None), 1,
         "one break marker where the batch was lost")
    ckEq(sum(1 for x in samples if x), 3 * N, "every surviving sample is plottable")
    ckEq([b[0] for b in breaks], ["GAP"], "the break is named GAP")
    idx = samples.index(None)
    ck(samples[idx - 1] is not None and samples[idx + 1] is not None,
       "the break sits between the two surviving runs")


def test_truncated_file():
    print("truncated capture file")
    recs = [batch(1, 0, 0), batch(2, N, N * 1000)]
    path = write_capture(recs)
    with open(path, "rb") as fh:
        data = fh.read()
    with open(path, "wb") as fh:
        fh.write(data[:-400])        # power loss mid-write
    rows, rep = D.decode(path)
    os.unlink(path)
    ckEq(rep["samples"], N, "the intact record still decodes")
    ck(len(samples_of(rows)) == N, "the torn tail contributes nothing")


def main():
    print("HALL_WAVEFORM_TEST — decoder tests (investigatory)\n")
    test_clean_run()
    test_out_of_order_and_duplicate_arrival()
    test_transport_loss()
    test_declared_missed_slots()
    test_corrupt_record()
    test_anchors_are_placed_and_kept_separate()
    test_session_boundary()
    test_dual_channel_alignment()
    test_plot_loader_breaks_the_trace()
    test_truncated_file()
    print("\n%d checks, %d failures" % (checks, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

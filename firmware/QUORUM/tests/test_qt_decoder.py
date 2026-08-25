#!/usr/bin/env python3
"""test_qt_decoder.py — host tests for tools/qt_format.py and
tools/qt_decode.py.

INVESTIGATORY / UNAPPROVED.

Same coverage areas as HALL_WAVEFORM_TEST/tests/test_decoder.py, adapted to
QUORUM TRACE's four record classes and per-stream (not shared) batchSeq
spaces: reordered and duplicated datagrams, transport loss with the exact
missing range, corruption, a reboot mid-file, and a truncated file.
"""

import os
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "tools"))
import qt_format as F        # noqa: E402
import qt_decode as D        # noqa: E402

failures = 0
checks = 0


def ck(cond, what):
    global failures, checks
    checks += 1
    if not cond:
        failures += 1
        print("  FAIL  %s" % what)


def ckEq(got, want, what):
    ck(got == want, "%s (got %r, want %r)" % (what, got, want))


def write_capture(records):
    """records: list of raw sealed datagrams. Returns a path."""
    fh = tempfile.NamedTemporaryFile(suffix=".qtcap", delete=False)
    F.write_capture_header(fh, 1_700_000_000_000_000)
    for i, data in enumerate(records):
        F.write_frame(fh, 1_700_000_000_000_000 + i * 1000, data)
    fh.close()
    return fh.name


def sample_record(batch_seq, first_seq, t0_ms, n, loco=9950012, session=1):
    payload = b"".join(F.pack_sample(1000 + i, 500, dt_ms=1) for i in range(n))
    return F.build_record(F.REC_SAMPLE, payload, loco_id=loco, session_id=session,
                          batch_seq=batch_seq, first_sample_seq=first_seq,
                          t0_ms=t0_ms, n_items=n)


def decision_record(batch_seq, t_ms, kind=F.QTD_ACCEPT_EVENT, mm_after=5,
                    loco=9950012, session=1, obs_pol=1, exp_pol=1):
    payload = struct.pack(
        F.DECISION_FMT, t_ms, kind, 0, 0, 0, mm_after, 1, 1,
        obs_pol, exp_pol, 0, 0, F.OFFSET_NA, F.OFFSET_NA, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0.0, 0, 0, 90, 90, 1, 0)
    return F.build_record(F.REC_DECISION, payload, loco_id=loco, session_id=session,
                          batch_seq=batch_seq, t0_ms=t_ms)


def status_record(batch_seq, t_ms, loco=9950012, session=1):
    payload = struct.pack(F.STATUS_FMT, t_ms, 0, 0, 0, 0, 0, 0, 100000, 0, 25, 13, 3, 2, 12, 6)
    return F.build_record(F.REC_STATUS, payload, loco_id=loco, session_id=session,
                          batch_seq=batch_seq, t0_ms=t_ms)


# ---------------------------------------------------------------------------
# 1. Basic decode: samples, a decision and a status round-trip correctly.
# ---------------------------------------------------------------------------
def test_basic_decode():
    print("basic decode: sample/decision/status round-trip")
    recs = [sample_record(1, 0, 1000, 5), decision_record(1, 1005, mm_after=7),
            status_record(1, 2000)]
    path = write_capture(recs)
    rows, rep = D.decode(path)
    os.unlink(path)
    ckEq(rep["samples"], 5, "five samples decoded")
    ckEq(rep["decisions"], 1, "one decision decoded")
    ckEq(rep["status"], 1, "one status decoded")
    ckEq(rep["transport_gaps"], 0, "no gaps on a clean capture")
    sample_rows = [r for r in rows if r["row_type"] == "SAMPLE"]
    ckEq(len(sample_rows), 5, "five SAMPLE rows in the CSV")
    ckEq(int(sample_rows[0]["phys_sample_seq"]), 0, "first sample_seq is 0")
    dec_rows = [r for r in rows if r["row_type"] == "DECISION"]
    ckEq(dec_rows[0]["dec_nav_mm_after"], 7, "decision carries navMmAfter")


# ---------------------------------------------------------------------------
# 2. Reordering and duplication.
# ---------------------------------------------------------------------------
def test_reorder_and_duplicate():
    print("reordered and duplicated datagrams")
    a = sample_record(1, 0, 1000, 3)
    b = sample_record(2, 3, 1003, 3)
    # Arrival order: b, a, a (duplicate), b (duplicate) -- decoder must not care.
    path = write_capture([b, a, a, b])
    rows, rep = D.decode(path)
    os.unlink(path)
    ckEq(rep["duplicate_records"], 2, "two duplicates detected and kept once")
    ckEq(rep["samples"], 6, "all six samples present despite arrival disorder")
    sample_rows = [r for r in rows if r["row_type"] == "SAMPLE"]
    seqs = [int(r["phys_sample_seq"]) for r in sample_rows]
    ckEq(seqs, sorted(seqs), "samples are in sequence order regardless of arrival order")


# ---------------------------------------------------------------------------
# 3. Transport loss — exact missing range, per stream independently.
# ---------------------------------------------------------------------------
def test_transport_loss_per_stream():
    print("transport loss: exact range, independent per record-type stream")
    recs = [sample_record(1, 0, 1000, 3), sample_record(2, 3, 1003, 3),
            sample_record(3, 6, 1006, 3),   # batch 2 will be dropped "in transport"
            decision_record(1, 1010), decision_record(2, 1020)]  # decisions: no loss
    wire = [recs[0], recs[2], recs[3], recs[4]]   # drop sample batch 2
    path = write_capture(wire)
    rows, rep = D.decode(path)
    os.unlink(path)
    ckEq(rep["transport_gaps"], 1, "exactly one gap (the dropped SAMPLE batch)")
    ckEq(rep["transport_lost_records"], 1, "exactly one record lost")
    ckEq(rep["samples"], 6, "the surviving 6 of 9 samples decoded correctly")
    ckEq(rep["decisions"], 2, "the decision stream is untouched by the sample-stream loss")
    gap_rows = [r for r in rows if r["row_type"] == "GAP"]
    ckEq(len(gap_rows), 1, "one GAP row emitted")
    ck("batchSeq 2..2" in gap_rows[0]["info"], "the GAP row names the exact lost batchSeq")


# ---------------------------------------------------------------------------
# 4. Corruption — CRC failure is reported, not silently accepted.
# ---------------------------------------------------------------------------
def test_corruption_detected():
    print("corruption: a flipped bit fails CRC and is reported, not decoded")
    good = sample_record(1, 0, 1000, 2)
    corrupt = bytearray(sample_record(2, 2, 1002, 2))
    corrupt[F.HDR_LEN] ^= 0xFF   # flip a bit in the payload
    path = write_capture([good, bytes(corrupt)])
    rows, rep = D.decode(path)
    os.unlink(path)
    ckEq(rep["bad_records"], 1, "the corrupted datagram is flagged bad")
    ckEq(rep["samples"], 2, "only the intact batch's samples are decoded")
    bad_rows = [r for r in rows if r["row_type"] == "BAD"]
    ckEq(len(bad_rows), 1, "one BAD row emitted")
    ck("CRC" in bad_rows[0]["info"], "the BAD row names the CRC failure")


# ---------------------------------------------------------------------------
# 5. Reboot mid-file — a new session_id must not be joined to the old one.
# ---------------------------------------------------------------------------
def test_reboot_mid_file():
    print("a reboot mid-file starts a new, unjoined session")
    a = sample_record(1, 0, 1000, 2, session=111)
    b = sample_record(1, 0, 500, 2, session=222)   # new session, its OWN seq 0
    path = write_capture([a, b])
    rows, rep = D.decode(path)
    os.unlink(path)
    ckEq(rep["sessions"], 2, "two sessions detected")
    session_rows = [r for r in rows if r["row_type"] == "SESSION"]
    ckEq(len(session_rows), 1, "one SESSION boundary row (none before the first session)")
    # Both sessions' first sample is sample_seq 0 -- if they were wrongly
    # joined, the decoder would see a sequence collision or a bogus "gap".
    ckEq(rep["transport_gaps"], 0, "no false gap is reported across the session boundary")


# ---------------------------------------------------------------------------
# 6. Truncated capture — the reader stops cleanly, does not crash or lie.
# ---------------------------------------------------------------------------
def test_truncated_capture():
    print("a truncated file is tolerated and reported, not crashed on")
    full = write_capture([sample_record(1, 0, 1000, 3), sample_record(2, 3, 1003, 3)])
    with open(full, "rb") as fh:
        data = fh.read()
    os.unlink(full)
    truncated_path = full + ".trunc"
    with open(truncated_path, "wb") as fh:
        fh.write(data[:len(data) - 5])   # chop off the tail of the last frame
    rows, rep = D.decode(truncated_path)
    os.unlink(truncated_path)
    ckEq(rep["samples"], 3, "the intact first batch is still decoded")
    ck(rep["bad_records"] == 0, "a truncated tail is silently stopped at, not reported as a bad record "
      "(matches read_capture()'s documented behaviour, mirroring hwt_format.py)")


# ---------------------------------------------------------------------------
# 7. Wire-format stability: the decoder parses these sizes by hand.
# ---------------------------------------------------------------------------
def test_wire_format_sizes():
    print("wire format sizes match QuorumTrace.h's C structs")
    ckEq(F.HDR_LEN, 32, "header is 32 bytes")
    ckEq(F.SAMPLE_LEN, 14, "sample record is 14 bytes")
    ckEq(F.DECISION_LEN, 40, "decision record is 40 bytes")
    ckEq(F.STATUS_LEN, 44, "status record is 44 bytes")
    ckEq(F.ANCHOR_LEN, 56, "anchor record is 56 bytes")
    import zlib
    ckEq(zlib.crc32(b"123456789") & 0xFFFFFFFF, 0xCBF43926,
        "sanity: Python zlib.crc32 matches the well-known CRC-32 check value "
        "the firmware's table-based implementation is built against")


def main():
    print("QUORUM TRACE — decoder host tests (investigatory)\n")
    test_basic_decode()
    test_reorder_and_duplicate()
    test_transport_loss_per_stream()
    test_corruption_detected()
    test_reboot_mid_file()
    test_truncated_capture()
    test_wire_format_sizes()
    print("\n%d checks, %d failures" % (checks, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

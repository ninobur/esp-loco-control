#!/usr/bin/env python3
"""test_xhr_decoder.py — the X18 Hall recorder's transport, damaged on purpose.

OBSERVATION TOOLING. Diagnostic only.

Two halves:

  PART 1  reads the capture written by the firmware gate
          (firmware/test-programs/NAVI_ONE_X18_RECORDER/tests/gate_recorder.cpp),
          which emits REAL datagrams from the firmware's own structs. This is
          the only check that the C++ and the Python agree about the wire --
          everything else here is Python talking to itself.

  PART 2  synthesises streams and damages them: dropped datagrams, truncation,
          bit flips, bad magic, a foreign format, reordering, duplicates, a
          micros() wrap, a millis() wrap, and a locomotive that reboots in the
          middle of the capture.

WHAT IS BEING TESTED IS THAT DAMAGE IS REPORTED. A decoder that quietly
recovered from a corrupt datagram would fail these tests, because a capture
that reads clean when it is not is worth less than no capture at all.

    python3 tools/tests/test_xhr_decoder.py [path/to/gate_recorder.xhr]
"""

import os
import struct
import subprocess
import sys
import tempfile
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import xhr_format as F      # noqa: E402
import xhr_decode as D      # noqa: E402

failures = []
checks = [0]


def ok(cond, what):
    checks[0] += 1
    if not cond:
        failures.append(what)
        print("  FAIL  %s" % what)


def eq(got, want, what):
    checks[0] += 1
    if got != want:
        failures.append(what)
        print("  FAIL  %s: got %r, want %r" % (what, got, want))


# ---------------------------------------------------------------------------
# A datagram builder that seals the same way the firmware does. It uses
# zlib.crc32, and the firmware uses a nibble table; PART 1 is what proves the
# two agree, so this builder is never the authority on the format.
# ---------------------------------------------------------------------------
def seal(rec_type, n_items, session, batch_seq, first_seq, t0_ms, t0_us,
         payload, baseline=1948, ring_drops=0, nav_mm=42, nav_dir=1,
         st_phase=0, ctx=0x01, loco=9950011, version=F.FORMAT_VERSION,
         magic=F.MAGIC):
    hdr = struct.pack(F.HDR_FMT, magic, version, rec_type, n_items, loco,
                      session, batch_seq, first_seq, t0_ms, t0_us, baseline,
                      ring_drops, nav_mm, nav_dir, st_phase, ctx, 0)
    crc = zlib.crc32(hdr + payload) & 0xFFFFFFFF
    return hdr[:F.HDR_LEN - 4] + struct.pack("<I", crc) + payload


def samples(n, raw=1950, dt=1000, flags=F.F_DIR_FWD, pwm=90):
    out = b""
    for i in range(n):
        d = 0 if i == 0 else dt
        f = flags | (F.F_LATE if d > 1250 else 0)
        out += struct.pack(F.SAMPLE_FMT, d, raw, pwm, pwm, f, 0)
    return out


# Keyword arguments belong to one of two places: the payload generator or the
# header sealer. Split them explicitly rather than forwarding blindly, so a
# typo in a test is a TypeError and not a silently ignored parameter.
_SAMPLE_KW = ("raw", "dt", "flags", "pwm")


def sample_batch(session, seq, t0_ms=None, t0_us=None, n=100, **kw):
    t0_ms = seq * 100 if t0_ms is None else t0_ms
    t0_us = seq * 100000 if t0_us is None else t0_us
    skw = {k: v for k, v in kw.items() if k in _SAMPLE_KW}
    hkw = {k: v for k, v in kw.items() if k not in _SAMPLE_KW}
    return seal(F.REC_SAMPLES, n, session, seq, (seq - 1) * 100, t0_ms, t0_us,
                samples(n, **skw), **hkw)


def write_capture(path, datagrams):
    with open(path, "wb") as fh:
        F.write_capture_header(fh, 1700000000000000)
        for i, d in enumerate(datagrams):
            F.write_frame(fh, 1700000000000000 + i * 100000, d)


def decode(path, session=None):
    return D.decode(path, session)


# ===========================================================================
def part1(gate_capture):
    print("PART 1 — the firmware's own bytes, read by the real decoder")
    if not os.path.exists(gate_capture):
        print("  SKIPPED: %s not present (build and run gate_recorder first)" % gate_capture)
        return False
    audits, bad, frames = decode(gate_capture)
    eq(len(bad), 0, "the firmware gate's capture has no unreadable datagram")
    eq(len(audits), 3, "the gate's capture holds three sessions")
    a = list(audits.values())[0]
    eq(a.samples, 12000, "12,000 samples decoded from the firmware's batches")
    eq(a.loco_id, 9950011, "loco id survives the wire")
    eq(len(a.missing(F.REC_SAMPLES)), 0, "no batch missing from a clean run")
    eq(len(a.rulings), 1, "the ruling datagram decoded")
    r = a.rulings[0]
    eq(r["ruling"], "ADVANCED", "the ruling's verdict survives the wire")
    eq(r["peak"], 173, "the ruling's peak survives the wire")
    eq(r["polarity"], "N", "the ruling's polarity survives the wire")
    eq(r["nav_mm_before"], 41, "navMm before")
    eq(r["nav_mm_after"], 42, "navMm after")
    ok(abs(r["amplitude_ratio"] - 1.02) < 1e-5, "the float survives the wire")
    eq(len(a.status), 1, "the status datagram decoded")
    st = a.status[0]
    eq(st["measured_hz"], 1000.0, "measured rate survives the wire")
    eq(st["hall_stack_free_bytes"], 2100, "hall stack free, in BYTES")
    eq(st["rssi"], -58, "a negative rssi survives as a signed value")
    # The synthetic magnet the gate wrote must come back out intact.
    peak = 0
    with open(gate_capture, "rb") as _f:
        pass
    for _recv, data in F.iter_capture(gate_capture):
        try:
            hdr, payload = F.parse_record(data)
        except F.BadRecord:
            continue
        if hdr.rec_type == F.REC_SAMPLES and hdr.session_id == a.session_id:
            for s in F.iter_samples(hdr, payload):
                peak = max(peak, s["raw"])
    eq(peak, 2120, "the synthetic 170-count magnet arrives undistorted")
    # The wrap session: an 8 ms gap measured ACROSS the micros() rollover, and
    # a 200 ms stall that saturates rather than wrapping.
    wrap = [x for x in audits.values() if x.session_id == 0x05E5510D][0]
    dts = sorted(g[1] for g in wrap.tick_gaps)
    eq(dts, [8000, 65535], "the firmware measured across the micros() wrap "
                           "and saturated the long stall")
    ok(any(g[2] for g in wrap.tick_gaps), "the saturated gap is marked saturated")
    return True


def part2():
    print("")
    print("PART 2 — synthetic damage")
    tmp = tempfile.mkdtemp(prefix="xhr_test_")
    S = 0xDEADBEEF

    # -- a clean stream is clean --------------------------------------------
    p = os.path.join(tmp, "clean.xhr")
    write_capture(p, [sample_batch(S, i) for i in range(1, 21)])
    a, bad, _ = decode(p)
    au = a[S]
    eq(len(bad), 0, "clean: nothing unreadable")
    eq(au.samples, 2000, "clean: every sample present")
    eq(len(au.missing(F.REC_SAMPLES)), 0, "clean: nothing missing")
    eq(au.duplicates[F.REC_SAMPLES], 0, "clean: no duplicates")
    eq(au.reordered[F.REC_SAMPLES], 0, "clean: nothing reordered")
    eq(len(au.tick_gaps), 0, "clean: no tick gaps")

    # -- GAPS: datagrams 5, 6 and 12 never arrive ---------------------------
    p = os.path.join(tmp, "gaps.xhr")
    write_capture(p, [sample_batch(S, i) for i in range(1, 21)
                      if i not in (5, 6, 12)])
    au = decode(p)[0][S]
    eq(au.missing(F.REC_SAMPLES), [5, 6, 12], "gaps: named exactly")
    eq(au.samples, 1700, "gaps: only the samples that arrived are counted")
    # and the CSV must mark the discontinuity rather than bridge it
    csv = os.path.join(tmp, "gaps.csv")
    D.write_csv(p, csv, None)
    rows = open(csv).read().strip().split("\n")[1:]
    gapped = [r for r in rows if int(r.split(",")[5]) > 0]
    eq(len(gapped), 2, "gaps: two discontinuities marked in the CSV")
    eq(int(gapped[0].split(",")[5]), 200, "gaps: the 5-6 hole is 200 samples wide")
    eq(int(gapped[1].split(",")[5]), 100, "gaps: the 12 hole is 100 samples wide")

    # -- ON-BOARD vs TRANSPORT loss -----------------------------------------
    # The locomotive's own ring-drop counter moves across the gap, so the loss
    # is attributable without guessing.
    p = os.path.join(tmp, "onboard.xhr")
    ds = [sample_batch(S, i, ring_drops=0) for i in range(1, 6)]
    ds += [sample_batch(S, i, ring_drops=3) for i in range(9, 13)]
    write_capture(p, ds)
    au = decode(p)[0][S]
    eq(au.missing(F.REC_SAMPLES), [6, 7, 8], "on-board: the gap is seen")
    ok(au.ring_drop_steps >= 1, "on-board: the drop counter moved across the gap")
    p = os.path.join(tmp, "transport.xhr")
    ds = [sample_batch(S, i, ring_drops=0) for i in range(1, 6)]
    ds += [sample_batch(S, i, ring_drops=0) for i in range(9, 13)]
    write_capture(p, ds)
    au = decode(p)[0][S]
    eq(au.missing(F.REC_SAMPLES), [6, 7, 8], "transport: the gap is seen")
    eq(au.ring_drop_steps, 0, "transport: the drop counter did NOT move")

    # -- DUPLICATES ----------------------------------------------------------
    p = os.path.join(tmp, "dup.xhr")
    ds = [sample_batch(S, i) for i in range(1, 11)]
    ds.insert(5, ds[4])
    ds.append(ds[9])
    write_capture(p, ds)
    au = decode(p)[0][S]
    eq(au.duplicates[F.REC_SAMPLES], 2, "duplicates: both counted")
    eq(au.samples, 1000, "duplicates: counted once, not twice")

    # -- REORDER -------------------------------------------------------------
    p = os.path.join(tmp, "reorder.xhr")
    ds = [sample_batch(S, i) for i in range(1, 11)]
    ds[3], ds[6] = ds[6], ds[3]
    write_capture(p, ds)
    au = decode(p)[0][S]
    # Swapping two datagrams puts THREE below the high-water mark: 7 arrives
    # early, so 5, 6 and 4 all land behind it. Three is the honest count of
    # datagrams that arrived out of order, not a count of swaps.
    eq(au.reordered[F.REC_SAMPLES], 3, "reorder: every late arrival is reported")
    eq(len(au.missing(F.REC_SAMPLES)), 0, "reorder: nothing is actually missing")
    eq(au.samples, 1000, "reorder: every sample still decoded")

    # -- CORRUPTION ----------------------------------------------------------
    good = sample_batch(S, 1)
    # one flipped bit in the payload
    flipped = bytearray(good); flipped[F.HDR_LEN + 40] ^= 0x01
    # one flipped bit in the header
    hflip = bytearray(good); hflip[20] ^= 0x80
    # truncated in flight
    trunc = good[:400]
    # header claims more samples than the payload holds
    liar = seal(F.REC_SAMPLES, 100, S, 2, 100, 200, 200000, samples(50))
    # not our format at all
    foreign = b"HWT1" + good[4:]
    # a version we do not know
    newver = seal(F.REC_SAMPLES, 100, S, 3, 200, 300, 300000, samples(100), version=9)
    # garbage
    junk = b"\x00" * 60
    p = os.path.join(tmp, "corrupt.xhr")
    write_capture(p, [good, bytes(flipped), bytes(hflip), trunc, liar,
                      foreign, newver, junk])
    a, bad, frames = decode(p)
    eq(frames, 8, "corruption: every frame is still on disk, verbatim")
    eq(len(bad), 7, "corruption: seven datagrams refused")
    why = " | ".join(b[3] for b in bad)
    ok("CRC mismatch" in why, "corruption: a payload bit flip is caught by CRC")
    ok(why.count("CRC mismatch") >= 2, "corruption: a header bit flip is caught too")
    ok("payload" in why or "short datagram" in why, "corruption: truncation is caught")
    ok("HWT1" in why, "corruption: a foreign format is named, not misread")
    ok("version" in why, "corruption: an unknown version is refused")
    eq(a[S].samples, 100, "corruption: only the one good datagram is decoded")

    # -- TIMESTAMP WRAP ------------------------------------------------------
    # micros() rolls over inside a batch. Reconstruction is by measured dt, so
    # the wrap must be invisible in the deltas and must not create a tick gap.
    p = os.path.join(tmp, "wrap.xhr")
    write_capture(p, [sample_batch(S, 1, t0_us=0xFFFFFF00)])
    au = decode(p)[0][S]
    eq(len(au.tick_gaps), 0, "micros wrap: no false tick gap")
    eq(au.samples, 100, "micros wrap: all samples decoded")
    got = [s["t_us"] for _r, d in F.iter_capture(p)
           for s in F.iter_samples(*F.parse_record(d))]
    ok(got[1] < got[0], "micros wrap: reconstructed time wraps, as the clock does")
    eq(((got[99] - got[0]) & 0xFFFFFFFF), 99000, "micros wrap: 99 ms across the rollover")
    # millis() wrap in the batch header, 49.7 days in. Reported, not repaired.
    p = os.path.join(tmp, "mswrap.xhr")
    write_capture(p, [seal(F.REC_SAMPLES, 100, S, 1, 0, 0xFFFFFF9C, 0, samples(100)),
                      seal(F.REC_SAMPLES, 100, S, 2, 100, 0x00000000, 100000, samples(100))])
    a2 = decode(p)[0][S]
    eq(a2.samples, 200, "millis wrap: both batches decoded")

    # -- A REAL TICK GAP -----------------------------------------------------
    p = os.path.join(tmp, "stall.xhr")
    pay = bytearray(samples(100))
    struct.pack_into(F.SAMPLE_FMT, pay, 50 * F.SAMPLE_LEN,
                     65535, 1950, 90, 90, F.F_DIR_FWD | F.F_LATE, 0)
    write_capture(p, [seal(F.REC_SAMPLES, 100, S, 1, 0, 0, 0, bytes(pay))])
    au = decode(p)[0][S]
    eq(len(au.tick_gaps), 1, "stall: one tick gap found")
    eq(au.tick_gaps[0][1], 65535, "stall: reported at its saturated value")
    ok(au.tick_gaps[0][2], "stall: marked saturated, not treated as 65 ms exactly")
    eq(au.late, 1, "stall: the locomotive's own late flag is counted")

    # -- MIXED SESSIONS / REBOOT --------------------------------------------
    # Both sessions restart batchSeq and sampleSeq at 1/0. Joining them would
    # invent a continuous trace that never existed.
    p = os.path.join(tmp, "reboot.xhr")
    A, B = 0x11111111, 0x22222222
    ds = [sample_batch(A, i) for i in range(1, 6)]
    ds += [sample_batch(B, i) for i in range(1, 4)]
    write_capture(p, ds)
    aud = decode(p)[0]
    eq(len(aud), 2, "reboot: two sessions kept apart")
    eq(aud[A].samples, 500, "reboot: session A's samples")
    eq(aud[B].samples, 300, "reboot: session B's samples")
    eq(len(aud[A].missing(F.REC_SAMPLES)), 0, "reboot: A is complete on its own")
    eq(len(aud[B].missing(F.REC_SAMPLES)), 0, "reboot: B is complete on its own")
    # and --session isolates one
    only = decode(p, B)[0]
    eq(len(only), 1, "reboot: --session selects exactly one")
    eq(only[B].samples, 300, "reboot: and decodes only its samples")
    # the CSV numbers sessions separately and never bridges the reboot
    csv = os.path.join(tmp, "reboot.csv")
    D.write_csv(p, csv, None)
    rows = [r.split(",") for r in open(csv).read().strip().split("\n")[1:]]
    ok(all(int(r[5]) == 0 for r in rows), "reboot: no false gap across the reboot")
    eq(len(set(r[0] for r in rows)), 2, "reboot: the CSV keeps both session ids")

    # -- A TRUNCATED CAPTURE FILE (the Pi lost power mid-write) -------------
    p2 = os.path.join(tmp, "halfwritten.xhr")
    raw = open(os.path.join(tmp, "clean.xhr"), "rb").read()
    open(p2, "wb").write(raw[:len(raw) - 300])
    au = decode(p2)[0][S]
    eq(au.samples, 1900, "half-written: the last whole frame is the last one read")
    eq(len(au.missing(F.REC_SAMPLES)), 0, "half-written: no invented gap")

    # -- MIXED RECORD TYPES SHARE NO COUNTER --------------------------------
    p = os.path.join(tmp, "mixed.xhr")
    rul = seal(F.REC_RULING, 1, S, 1, 500, 500, 500000,
               struct.pack(F.RULING_FMT, 500, 360, 500, 499, 173, 140, 1860,
                           1948, 1948, 1951, 1.02, 1, 1, 0, 1, 41, 42, 1, 0, 90, 0))
    stat = seal(F.REC_STATUS, 1, S, 1, F.SAMPLE_SEQ_NA, 1000, 1000000,
                struct.pack(F.STATUS_FMT, 1000, 1000, 1000, 0, 0, 0, 1000, 260,
                            180000, 2100, 5200, 10000, 1, -58, 1, 1, 0))
    write_capture(p, [sample_batch(S, 1), rul, stat, sample_batch(S, 2)])
    au = decode(p)[0][S]
    eq(len(au.missing(F.REC_SAMPLES)), 0, "mixed: samples complete")
    eq(len(au.missing(F.REC_RULING)), 0, "mixed: rulings complete")
    eq(len(au.missing(F.REC_STATUS)), 0, "mixed: status complete")
    eq(len(au.rulings), 1, "mixed: the ruling decoded")
    eq(au.rulings[0]["sample_seq"], 499, "mixed: the ruling's join key survives")


def main():
    gate = sys.argv[1] if len(sys.argv) > 1 else "/tmp/gate_recorder.xhr"
    print("X18 Hall recorder — transport and decoder")
    part1(gate)
    part2()
    print("")
    print("%s  %d checks, %d failures" %
          ("FAILED" if failures else "PASSED", checks[0], len(failures)))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""qt_format.py — QUORUM TRACE wire and capture-file format.

INVESTIGATORY / UNAPPROVED. Diagnostic tooling only.

Single source of truth for the binary layout, shared by qt_receiver.py,
qt_decode.py and the tests. It must stay in step with
firmware/QUORUM/QuorumTrace.h.

Nothing here interprets a decision. It unpacks bytes and reports damage --
same discipline as tools/hwt_format.py, which this deliberately mirrors.
"""

import struct
import zlib

# --- wire (one UDP datagram = one record) -----------------------------------
MAGIC = b"QTR1"
FORMAT_VERSION = 1

HDR_FMT = "<4sBBHIIIIII"
HDR_LEN = struct.calcsize(HDR_FMT)          # 32: magic4+ver1+recType1+nItems2+
                                            # locoId4+sessionId4+batchSeq4+
                                            # firstSampleSeq4+t0Ms4+crc32(4)

SAMPLE_FMT = "<HhhhhBBBB"
SAMPLE_LEN = struct.calcsize(SAMPLE_FMT)    # 14

DECISION_FMT = "<I" + "B" * 11 + "b" * 3 + "b" * 6 + "HH" + "f" + "HH" + "B" * 4
DECISION_LEN = struct.calcsize(DECISION_FMT)  # 40

STATUS_FMT = "<" + "I" * 9 + "HH" + "B" * 4
STATUS_LEN = struct.calcsize(STATUS_FMT)    # 44

ANCHOR_FMT = "<IIIBBBB40s"
ANCHOR_LEN = struct.calcsize(ANCHOR_FMT)    # 56

REC_SAMPLE, REC_DECISION, REC_ANCHOR, REC_STATUS = 1, 2, 3, 4
REC_NAME = {REC_SAMPLE: "SAMPLE", REC_DECISION: "DECISION",
           REC_ANCHOR: "ANCHOR", REC_STATUS: "STATUS"}

# QtSample.flags bit layout (QuorumTrace.h)
SAMPLE_FLAG_ACTIVE = 0x01
SAMPLE_FLAG_POLE = 0x02
SAMPLE_DIR_SHIFT = 2
SAMPLE_DIR_MASK = 0x03
SAMPLE_FLAG_ESTOP = 0x10
SAMPLE_FLAG_LATE = 0x20
DIR_NAME = {0: "UNSET", 1: "CW", 2: "CCW", 3: "?"}   # qtEncodeDir()'s mapping

# QtDecision.kind (QtDecisionKind)
QTD_EVENT_OPENED, QTD_EVENT_FLOOR_REJECT, QTD_EVENT_CLOSED = 1, 2, 3
QTD_NAV_ON_MARKER_ENTRY, QTD_TIMING_GATE_RESULT, QTD_ACCEPT_EVENT = 4, 5, 6
QTD_COMPARISON_AGREE, QTD_COMPARISON_DISAGREE, QTD_QUORUM_EVENT = 7, 8, 9
KIND_NAME = {
    QTD_EVENT_OPENED: "EVENT_OPENED", QTD_EVENT_FLOOR_REJECT: "EVENT_FLOOR_REJECT",
    QTD_EVENT_CLOSED: "EVENT_CLOSED", QTD_NAV_ON_MARKER_ENTRY: "NAV_ON_MARKER_ENTRY",
    QTD_TIMING_GATE_RESULT: "TIMING_GATE_RESULT", QTD_ACCEPT_EVENT: "ACCEPT_EVENT",
    QTD_COMPARISON_AGREE: "AGREE", QTD_COMPARISON_DISAGREE: "DISAGREE",
    QTD_QUORUM_EVENT: "QUORUM_EVENT",
}
# QtDecision.quorumEvent (QtQuorumEvent) -- meaningful only when kind == QTD_QUORUM_EVENT
QUORUM_EVENT_NAME = {
    0: "OTHER", 1: "QUORUM_OPEN", 2: "QUORUM_TIED", 3: "QUORUM_ADOPTED",
    4: "QUORUM_REOPENED", 5: "QUORUM_CLOSED", 6: "NO_QUORUM",
    7: "PHANTOM_REJECTED", 8: "FIXTURE_REJECTED", 9: "FORCED_OFFSET",
}
# QtDecision.timingGate (QtTimingGate)
GATE_NAME = {
    0: "NO_POSITION", 1: "NO_DIR", 2: "LOW_PWM", 3: "RAMP",
    4: "NO_PREV", 5: "ACTIVE_PHANTOM", 6: "ACTIVE_ACCEPTED",
}
NAV_STATE_NAME = {0: "UNSET", 1: "NORMAL", 2: "EVALUATING", 3: "NO_QUORUM"}
POLARITY_NA = 0xFF
OFFSET_NA = -128

# --- capture file (what the receiver writes; identical shape to HWT's) ------
FILE_MAGIC = b"QTRACE01"   # exactly 8 bytes -- must match FILE_HDR_FMT's "8s"
FILE_HDR_FMT = "<8sQ"
FILE_HDR_LEN = struct.calcsize(FILE_HDR_FMT)
FRAME_FMT = "<QH"
FRAME_LEN = struct.calcsize(FRAME_FMT)


class Header:
    __slots__ = ("version", "rec_type", "n_items", "loco_id", "session_id",
                 "batch_seq", "first_sample_seq", "t0_ms", "crc32")

    def __init__(self, fields):
        (_magic, self.version, self.rec_type, self.n_items, self.loco_id,
         self.session_id, self.batch_seq, self.first_sample_seq, self.t0_ms,
         self.crc32) = fields


class BadRecord(Exception):
    """A datagram that cannot be trusted. Always reported, never guessed at."""


def crc_ok(data: bytes) -> bool:
    """CRC-32 over header (crc field zeroed) + payload, matching QuorumTrace.h."""
    if len(data) < HDR_LEN:
        return False
    stored = struct.unpack_from("<I", data, HDR_LEN - 4)[0]
    blanked = data[:HDR_LEN - 4] + b"\x00\x00\x00\x00" + data[HDR_LEN:]
    return zlib.crc32(blanked) & 0xFFFFFFFF == stored


_PAYLOAD_LEN = {REC_SAMPLE: SAMPLE_LEN, REC_DECISION: DECISION_LEN,
                REC_ANCHOR: ANCHOR_LEN, REC_STATUS: STATUS_LEN}


def parse_record(data: bytes):
    """Return (Header, payload_bytes). Raises BadRecord on anything doubtful."""
    if len(data) < HDR_LEN:
        raise BadRecord("short datagram (%d bytes)" % len(data))
    fields = struct.unpack_from(HDR_FMT, data, 0)
    if fields[0] != MAGIC:
        raise BadRecord("bad magic %r" % (fields[0],))
    if fields[1] != FORMAT_VERSION:
        raise BadRecord("unknown format version %d" % fields[1])
    hdr = Header(fields)
    payload = data[HDR_LEN:]
    if hdr.rec_type == REC_SAMPLE:
        if len(payload) != hdr.n_items * SAMPLE_LEN:
            raise BadRecord("payload %d bytes, header claims %d samples"
                            % (len(payload), hdr.n_items))
    elif hdr.rec_type in _PAYLOAD_LEN:
        want = _PAYLOAD_LEN[hdr.rec_type]
        if len(payload) != want:
            raise BadRecord("%s payload %d bytes, want %d"
                            % (REC_NAME.get(hdr.rec_type, "?"), len(payload), want))
    else:
        raise BadRecord("unknown record type %d" % hdr.rec_type)
    if not crc_ok(data):
        raise BadRecord("CRC mismatch")
    return hdr, payload


def iter_samples(hdr: Header, payload: bytes):
    """Yield one dict per sample. dtUs is named for wire parity with HWT1 but
    is actually MILLISECONDS -- QuorumTrace.h has no per-sample microsecond
    timer; see QtSample's own doc comment."""
    t_ms = hdr.t0_ms
    for i in range(hdr.n_items):
        dt_ms, raw, baseline, peak_n, peak_s, pwm_act, pwm_cmd, flags, _pad = \
            struct.unpack_from(SAMPLE_FMT, payload, i * SAMPLE_LEN)
        if i:
            t_ms += dt_ms
        yield {
            "sample_seq": (hdr.first_sample_seq + i) & 0xFFFFFFFF,
            "t_ms": t_ms,
            "dt_ms": dt_ms,
            "raw": raw,
            "baseline": baseline,
            "peak_n": peak_n,
            "peak_s": peak_s,
            "pwm_actual": pwm_act,
            "pwm_commanded": pwm_cmd,
            "event_active": bool(flags & SAMPLE_FLAG_ACTIVE),
            "event_pole": "N" if (flags & SAMPLE_FLAG_POLE) else "S",
            "dir": DIR_NAME.get((flags >> SAMPLE_DIR_SHIFT) & SAMPLE_DIR_MASK, "?"),
            "estop": int(bool(flags & SAMPLE_FLAG_ESTOP)),
            "late": int(bool(flags & SAMPLE_FLAG_LATE)),
        }


def _na(v, sentinel):
    return None if v == sentinel else v


def parse_decision(payload: bytes) -> dict:
    if len(payload) < DECISION_LEN:
        raise BadRecord("short decision payload")
    f = struct.unpack_from(DECISION_FMT, payload, 0)
    (t_ms, kind, quorum_event, timing_gate, mm_before, mm_after,
     state_before, state_after, obs_pol, exp_pol, miss_streak, eval_count,
     leader_off, runner_off, margin,
     s0, s1, s2, s3, s4, s5,
     dt, dt_expected, ratio, peak, duration_ms,
     pwm_act, pwm_cmd, ring_inserted, _pad) = f
    return {
        "t_ms": t_ms, "kind": KIND_NAME.get(kind, "?%d" % kind),
        "quorum_event": QUORUM_EVENT_NAME.get(quorum_event, "?%d" % quorum_event) if kind == QTD_QUORUM_EVENT else "",
        "timing_gate": GATE_NAME.get(timing_gate, "") if kind in (QTD_NAV_ON_MARKER_ENTRY, QTD_TIMING_GATE_RESULT) else "",
        "nav_mm_before": mm_before, "nav_mm_after": mm_after,
        "nav_state_before": NAV_STATE_NAME.get(state_before, "?"),
        "nav_state_after": NAV_STATE_NAME.get(state_after, "?"),
        "observed_polarity": _na(obs_pol, POLARITY_NA),
        "expected_polarity": _na(exp_pol, POLARITY_NA),
        "miss_streak": miss_streak, "eval_count": eval_count,
        "leader_offset": _na(leader_off, OFFSET_NA),
        "runner_up_offset": _na(runner_off, OFFSET_NA),
        "quorum_margin": margin,
        "scores": [s0, s1, s2, s3, s4, s5],
        "dt": dt, "dt_expected_ms": dt_expected, "dt_conserve_ratio": ratio,
        "event_peak": peak, "event_duration_ms": duration_ms,
        "pwm_actual": pwm_act, "pwm_commanded": pwm_cmd,
        "ring_inserted": bool(ring_inserted),
    }


def parse_status(payload: bytes) -> dict:
    if len(payload) < STATUS_LEN:
        raise BadRecord("short status payload")
    f = struct.unpack_from(STATUS_FMT, payload, 0)
    keys = ("uptime_ms", "sample_seq", "decision_seq", "cum_sample_ring_drops",
            "cum_decision_ring_drops", "cum_hall_queue_drops", "cum_floor_rejects",
            "free_heap", "udp_send_failures", "hall_deadband_counts",
            "hall_entry_margin_counts", "quorum_trigger", "quorum_margin",
            "quorum_max", "quorum_candidates")
    return dict(zip(keys, f))


def parse_anchor(payload: bytes) -> dict:
    if len(payload) < ANCHOR_LEN:
        raise BadRecord("short anchor payload")
    (aid, sseq, t_ms, d, pwm_act, pwm_cmd, tlen, text) = struct.unpack_from(
        ANCHOR_FMT, payload, 0)
    return {
        "anchor_id": aid, "sample_seq": sseq, "t_ms": t_ms,
        "dir": DIR_NAME.get(d, "?"), "pwm_actual": pwm_act,
        "pwm_commanded": pwm_cmd,
        "text": text[:tlen].decode("utf-8", "replace"),
    }


def build_record(rec_type, payload=b"", *, loco_id=9950012, session_id=1,
                 batch_seq=1, first_sample_seq=0, t0_ms=0, n_items=0):
    """Assemble a sealed record. Used by the tests to make synthetic
    captures; the firmware is the only thing that makes real ones."""
    hdr = struct.pack(HDR_FMT, MAGIC, FORMAT_VERSION, rec_type, n_items,
                      loco_id, session_id, batch_seq, first_sample_seq, t0_ms, 0)
    crc = zlib.crc32(hdr + payload) & 0xFFFFFFFF
    return hdr[:-4] + struct.pack("<I", crc) + payload


def pack_sample(raw, baseline, *, dt_ms=1, peak_n=0, peak_s=0, pwm_actual=0,
                pwm_commanded=0, active=False, pole="N", direction="UNSET",
                estop=False, late=False):
    flags = 0
    if active: flags |= SAMPLE_FLAG_ACTIVE
    if pole == "N": flags |= SAMPLE_FLAG_POLE
    inv_dir = {v: k for k, v in DIR_NAME.items()}
    flags |= (inv_dir.get(direction, 0) & SAMPLE_DIR_MASK) << SAMPLE_DIR_SHIFT
    if estop: flags |= SAMPLE_FLAG_ESTOP
    if late: flags |= SAMPLE_FLAG_LATE
    return struct.pack(SAMPLE_FMT, min(dt_ms, 0xFFFF), raw, baseline,
                       peak_n, peak_s, pwm_actual, pwm_commanded, flags, 0)


def write_capture_header(fh, start_unix_us: int):
    fh.write(struct.pack(FILE_HDR_FMT, FILE_MAGIC, start_unix_us))


def write_frame(fh, recv_unix_us: int, data: bytes):
    fh.write(struct.pack(FRAME_FMT, recv_unix_us, len(data)))
    fh.write(data)


def read_capture(path):
    """Yield (recv_unix_us, datagram_bytes) in arrival order."""
    with open(path, "rb") as fh:
        head = fh.read(FILE_HDR_LEN)
        if len(head) < FILE_HDR_LEN or struct.unpack(FILE_HDR_FMT, head)[0] != FILE_MAGIC:
            raise BadRecord("%s is not a QUORUM TRACE capture file" % path)
        while True:
            frame = fh.read(FRAME_LEN)
            if len(frame) < FRAME_LEN:
                return
            recv_us, length = struct.unpack(FRAME_FMT, frame)
            data = fh.read(length)
            if len(data) < length:
                return          # truncated tail: stop, and let the caller say so
            yield recv_us, data

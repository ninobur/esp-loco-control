#!/usr/bin/env python3
"""xhr_format.py — X18 Hall recorder wire and capture-file format.

OBSERVATION TOOLING. Diagnostic only; nothing here has any authority over the
railway, and nothing here interprets the waveform. It unpacks bytes and
reports damage.

Single source of truth for the binary layout, shared by xhr_receiver.py,
xhr_decode.py and the tests. It must stay in step with
firmware/programs/NAVI_ONE/variants/NAVI_ONE_X18_RECORDER/HallRecorder.h, and
tests/gate_recorder.cpp exists so that "must" is checked by machine: that gate
emits real datagrams from the firmware's own structs and this module parses
them, so a layout drift fails a test instead of quietly corrupting a capture.

It refuses HWT1 and QTR1 magic by name. Those formats record different things
from different builds and are not interchangeable with this one.
"""

import struct
import zlib

# --- wire (one UDP datagram = one record) -----------------------------------
MAGIC = b"XHR1"
FORMAT_VERSION = 1

HDR_FMT = "<4sBBHIIIIIIhHBbBBI"
HDR_LEN = struct.calcsize(HDR_FMT)              # 44

SAMPLE_FMT = "<HhBBBB"
SAMPLE_LEN = struct.calcsize(SAMPLE_FMT)        # 8

RULING_FMT = "<IIIIHHHhhhfBBBBBBbBBB2x"
RULING_LEN = struct.calcsize(RULING_FMT)        # 44

STATUS_FMT = "<IIIIIIIIIIIHHbBBB"
STATUS_LEN = struct.calcsize(STATUS_FMT)        # 52

REC_SAMPLES, REC_RULING, REC_STATUS = 1, 2, 3
REC_NAME = {REC_SAMPLES: "SAMPLES", REC_RULING: "RULING", REC_STATUS: "STATUS"}

# Formats this tooling must never silently accept. They are real formats from
# real builds in this repo; reading one as XHR1 would produce numbers.
FOREIGN_MAGIC = {
    b"HWT1": "HALL_WAVEFORM_TEST (decode with tools/hwt_decode.py)",
    b"QTR1": "QUORUM trace (decode with the QUORUM trace tooling)",
}

SAMPLE_SEQ_NA = 0xFFFFFFFF
MM_NA = 0xFF
POLARITY_NA = 0xFF

F_DIR_FWD, F_ESTOP, F_AUTO = 0x01, 0x02, 0x04
F_MAY_ADAPT, F_PASSAGE, F_LATE, F_LOW_VOLT = 0x08, 0x10, 0x20, 0x40

CTX_POS_KNOWN, CTX_AUTO, CTX_ESTOP, CTX_MQTT_UP = 0x01, 0x02, 0x04, 0x08

ST_PHASE = {0: "IDLE", 1: "APPROACH", 2: "ZONE", 3: "ZERO_RAMP",
            4: "DWELL", 5: "DEPART"}
RULING_NAME = {0: "NONE", 1: "ADVANCED", 2: "WRONG_MAGNET", 3: "CONTRADICTED",
               4: "NOT_A_MAGNET", 5: "NO_POSITION", 6: "STALE_EPOCH"}

DT_SATURATED = 65535     # "at least this long", never a wrap

# --- capture file (what the receiver writes) --------------------------------
# 24-byte file header, then per datagram: recv wall-clock micros, length, bytes.
# The receiver appends and never rewrites, so a capture interrupted by power
# loss is readable up to its last whole frame.
FILE_MAGIC = b"XHRCAP01"
FILE_HDR_FMT = "<8sQQ"                          # magic, started_us, reserved
FILE_HDR_LEN = struct.calcsize(FILE_HDR_FMT)    # 24
FRAME_FMT = "<QH"
FRAME_LEN = struct.calcsize(FRAME_FMT)


class Header(object):
    __slots__ = ("version", "rec_type", "n_items", "loco_id", "session_id",
                 "batch_seq", "first_sample_seq", "t0_ms", "t0_us",
                 "baseline", "ring_drops_lo", "nav_mm", "nav_dir",
                 "st_phase", "ctx_flags", "crc32")

    def __init__(self, fields):
        (_magic, self.version, self.rec_type, self.n_items, self.loco_id,
         self.session_id, self.batch_seq, self.first_sample_seq, self.t0_ms,
         self.t0_us, self.baseline, self.ring_drops_lo, self.nav_mm,
         self.nav_dir, self.st_phase, self.ctx_flags, self.crc32) = fields

    @property
    def phase_name(self):
        return ST_PHASE.get(self.st_phase, "?%d" % self.st_phase)


class BadRecord(Exception):
    """A datagram that cannot be trusted. Always reported, never guessed at."""


def crc_ok(data):
    """CRC-32 over the header with the crc field zeroed, then the payload."""
    if len(data) < HDR_LEN:
        return False
    stored = struct.unpack_from("<I", data, HDR_LEN - 4)[0]
    blanked = data[:HDR_LEN - 4] + b"\x00\x00\x00\x00" + data[HDR_LEN:]
    return zlib.crc32(blanked) & 0xFFFFFFFF == stored


def parse_record(data):
    """Return (Header, payload). Raises BadRecord on anything doubtful.

    Length is checked against what the header CLAIMS before the CRC, so a
    truncated datagram is reported as truncated rather than as corruption.
    """
    if len(data) < HDR_LEN:
        raise BadRecord("short datagram (%d bytes, header is %d)" % (len(data), HDR_LEN))
    fields = struct.unpack_from(HDR_FMT, data, 0)
    magic = fields[0]
    if magic != MAGIC:
        if magic in FOREIGN_MAGIC:
            raise BadRecord("%s record, not XHR1 — %s"
                            % (magic.decode("ascii"), FOREIGN_MAGIC[magic]))
        raise BadRecord("bad magic %r" % (magic,))
    if fields[1] != FORMAT_VERSION:
        raise BadRecord("unknown format version %d" % fields[1])
    hdr = Header(fields)
    payload = data[HDR_LEN:]
    want = {REC_SAMPLES: hdr.n_items * SAMPLE_LEN,
            REC_RULING: RULING_LEN,
            REC_STATUS: STATUS_LEN}.get(hdr.rec_type)
    if want is None:
        raise BadRecord("unknown record type %d" % hdr.rec_type)
    if len(payload) != want:
        raise BadRecord("payload %d bytes, header claims %d (%s, n=%d)"
                        % (len(payload), want, REC_NAME.get(hdr.rec_type, "?"),
                           hdr.n_items))
    if not crc_ok(data):
        raise BadRecord("CRC mismatch")
    return hdr, payload


def iter_samples(hdr, payload):
    """Yield one dict per sample.

    Firmware time is reconstructed from each sample's MEASURED dt, not from an
    assumed 1 kHz grid: t0 is exact and every step after it is what the
    locomotive actually measured. A saturated dt is passed through with its own
    flag rather than being smoothed away -- the gap is the evidence.
    """
    t_us = hdr.t0_us
    for i in range(hdr.n_items):
        dt_us, raw, pwm_act, pwm_cmd, flags, _pad = struct.unpack_from(
            SAMPLE_FMT, payload, i * SAMPLE_LEN)
        if i:
            t_us = (t_us + dt_us) & 0xFFFFFFFF
        yield {
            "sample_seq": (hdr.first_sample_seq + i) & 0xFFFFFFFF,
            "t_us": t_us,
            "dt_us": dt_us,
            "dt_saturated": dt_us == DT_SATURATED,
            "raw": raw,
            "pwm_actual": pwm_act,
            "pwm_commanded": pwm_cmd,
            "dir": "FWD" if flags & F_DIR_FWD else "REV",
            "estop": bool(flags & F_ESTOP),
            "auto": bool(flags & F_AUTO),
            "may_adapt": bool(flags & F_MAY_ADAPT),
            "passage_open": bool(flags & F_PASSAGE),
            "late": bool(flags & F_LATE),
            "low_voltage": bool(flags & F_LOW_VOLT),
            "flags": flags,
        }


def parse_ruling(payload):
    f = struct.unpack(RULING_FMT, payload)
    return {
        "t_ms": f[0], "opened_ms": f[1], "closed_ms": f[2], "sample_seq": f[3],
        "peak": f[4], "duration_ms": f[5], "gap_ms": f[6],
        "entry_baseline": f[7], "close_baseline": f[8], "shadow_baseline": f[9],
        "amplitude_ratio": f[10],
        "polarity": ("N" if f[11] == 1 else "S") if f[11] != POLARITY_NA else "?",
        "ruling": RULING_NAME.get(f[12], "?%d" % f[12]), "ruling_code": f[12],
        "outcome": f[13], "is_magnet": bool(f[14]),
        "nav_mm_before": f[15], "nav_mm_after": f[16], "nav_dir": f[17],
        "st_phase": ST_PHASE.get(f[18], "?%d" % f[18]),
        "pwm_actual": f[19], "post_stop_successor": bool(f[20]),
    }


def parse_status(payload):
    f = struct.unpack(STATUS_FMT, payload)
    return {
        "t_ms": f[0], "uptime_ms": f[1], "cum_samples": f[2],
        "cum_sample_ring_drops": f[3], "cum_ruling_ring_drops": f[4],
        "cum_udp_failures": f[5], "max_gap_us": f[6], "max_tick_body_us": f[7],
        "free_heap": f[8],
        # BYTES. uxTaskGetStackHighWaterMark returns bytes on ESP-IDF.
        "hall_stack_free_bytes": f[9], "net_stack_free_bytes": f[10],
        "measured_hz": f[11] / 10.0, "ring_high_water": f[12],
        "rssi": f[13], "mqtt": bool(f[14]), "wifi": bool(f[15]),
    }


# --- capture-file helpers ---------------------------------------------------
def write_capture_header(fh, started_us):
    fh.write(struct.pack(FILE_HDR_FMT, FILE_MAGIC, started_us, 0))


def write_frame(fh, recv_us, data):
    """Append one datagram exactly as it arrived, with its arrival time."""
    fh.write(struct.pack(FRAME_FMT, recv_us, len(data)))
    fh.write(data)


def iter_capture(path):
    """Yield (recv_us, datagram_bytes) from a capture file.

    A trailing partial frame -- the receiver was killed mid-write, or the Pi
    lost power -- ends iteration quietly at the last whole frame. It is not an
    error and it is not repaired; the frames before it are exactly what
    arrived.
    """
    with open(path, "rb") as fh:
        head = fh.read(FILE_HDR_LEN)
        if len(head) < FILE_HDR_LEN:
            raise BadRecord("capture file is shorter than its header")
        magic, _started, _res = struct.unpack(FILE_HDR_FMT, head)
        if magic != FILE_MAGIC:
            raise BadRecord("not an XHR capture file (magic %r)" % (magic,))
        while True:
            fh_bytes = fh.read(FRAME_LEN)
            if len(fh_bytes) < FRAME_LEN:
                return
            recv_us, length = struct.unpack(FRAME_FMT, fh_bytes)
            data = fh.read(length)
            if len(data) < length:
                return
            yield recv_us, data

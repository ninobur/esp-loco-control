#!/usr/bin/env python3
"""hwt_format.py — HALL_WAVEFORM_TEST wire and capture-file format.

INVESTIGATORY / UNAPPROVED. Diagnostic tooling only.

Single source of truth for the binary layout, shared by hwt_receiver.py,
hwt_decode.py, hwt_plot.py and the tests. It must stay in step with
firmware/test-programs/HALL_WAVEFORM_TEST/HallCapture.h — see CAPTURE_FORMAT.md.

Nothing here interprets the waveform. It unpacks bytes and reports damage.
"""

import struct
import zlib

# --- wire (one UDP datagram = one record) -----------------------------------
MAGIC = b"HWT1"
FORMAT_VERSION = 1

HDR_FMT = "<4sBBHIIIIQIIIII"
HDR_LEN = struct.calcsize(HDR_FMT)          # 52
SAMPLE_FMT = "<HHHBBBB"
SAMPLE_LEN = struct.calcsize(SAMPLE_FMT)    # 10

REC_SAMPLES, REC_ANCHOR, REC_STATUS = 1, 2, 3
REC_NAME = {REC_SAMPLES: "SAMPLES", REC_ANCHOR: "ANCHOR", REC_STATUS: "STATUS"}

ANCHOR_FMT = "<IIQBBBB40s"
ANCHOR_LEN = struct.calcsize(ANCHOR_FMT)    # 60
STATUS_FMT = "<IIIIIIIIIIHHHBBBBBBB"
STATUS_LEN = struct.calcsize(STATUS_FMT)    # 53

ANN_NAME = {0: "", 1: "N", 2: "S", 3: "?"}
DIR_NAME = {0: "REV", 1: "NEUTRAL", 2: "FWD", 3: "?"}

CH_RAW_MASK = 0x0FFF
CH_ANN_SHIFT = 12
CH_PRESENT = 0x4000

CTX_DIR_MASK = 0x03
CTX_ESTOP = 0x04
CTX_FIXED = 0x08
CTX_SEQRUN = 0x10
CTX_LATE = 0x20

# --- capture file (what the receiver writes) --------------------------------
# 16-byte file header, then per datagram: recv-wall-clock micros, length, bytes.
FILE_MAGIC = b"HWTCAP01"
FILE_HDR_FMT = "<8sQ"
FILE_HDR_LEN = struct.calcsize(FILE_HDR_FMT)
FRAME_FMT = "<QH"
FRAME_LEN = struct.calcsize(FRAME_FMT)


class Header:
    __slots__ = ("version", "rec_type", "n_samples", "loco_id", "session_id",
                 "batch_seq", "first_sample_seq", "t0_us", "missed_before",
                 "cum_missed", "cum_queue_drops", "max_gap_us", "crc32")

    def __init__(self, fields):
        (_magic, self.version, self.rec_type, self.n_samples, self.loco_id,
         self.session_id, self.batch_seq, self.first_sample_seq, self.t0_us,
         self.missed_before, self.cum_missed, self.cum_queue_drops,
         self.max_gap_us, self.crc32) = fields


class BadRecord(Exception):
    """A datagram that cannot be trusted. Always reported, never guessed at."""


def crc_ok(data: bytes) -> bool:
    """CRC-32 over header (crc field zeroed) + payload, matching the firmware."""
    if len(data) < HDR_LEN:
        return False
    stored = struct.unpack_from("<I", data, HDR_LEN - 4)[0]
    blanked = data[:HDR_LEN - 4] + b"\x00\x00\x00\x00" + data[HDR_LEN:]
    return zlib.crc32(blanked) & 0xFFFFFFFF == stored


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
    if hdr.rec_type == REC_SAMPLES and len(payload) != hdr.n_samples * SAMPLE_LEN:
        raise BadRecord("payload %d bytes, header claims %d samples"
                        % (len(payload), hdr.n_samples))
    if not crc_ok(data):
        raise BadRecord("CRC mismatch")
    return hdr, payload


def iter_samples(hdr: Header, payload: bytes):
    """Yield one dict per sample. Physical measurement and annotation stay in
    separate keys — never merged, never reconciled here."""
    # Firmware time is reconstructed from the MEASURED dt of each sample, not
    # from an assumed 1 kHz grid: t0 is exact, and every step after it is what
    # the locomotive actually measured.
    t_us = hdr.t0_us
    for i in range(hdr.n_samples):
        ch0, ch1, dt_us, pwm_act, pwm_cmd, ctx, _pad = struct.unpack_from(
            SAMPLE_FMT, payload, i * SAMPLE_LEN)
        if i:
            t_us += dt_us
        yield {
            "sample_seq": (hdr.first_sample_seq + i) & 0xFFFFFFFF,
            "t_us": t_us,
            "ch0_raw": ch0 & CH_RAW_MASK,
            "ch0_present": bool(ch0 & CH_PRESENT),
            "ch0_ann": ANN_NAME[(ch0 >> CH_ANN_SHIFT) & 0x03],
            "ch1_raw": ch1 & CH_RAW_MASK,
            "ch1_present": bool(ch1 & CH_PRESENT),
            "ch1_ann": ANN_NAME[(ch1 >> CH_ANN_SHIFT) & 0x03],
            "dt_us": dt_us,
            "pwm_actual": pwm_act,
            "pwm_commanded": pwm_cmd,
            "dir": DIR_NAME[ctx & CTX_DIR_MASK],
            "estop": int(bool(ctx & CTX_ESTOP)),
            "fixed_mode": int(bool(ctx & CTX_FIXED)),
            "seq_running": int(bool(ctx & CTX_SEQRUN)),
            "late": int(bool(ctx & CTX_LATE)),
        }


def parse_anchor(payload: bytes) -> dict:
    if len(payload) < ANCHOR_LEN:
        raise BadRecord("short anchor payload")
    (aid, sseq, t_us, d, pwm_act, pwm_cmd, tlen, text) = struct.unpack_from(
        ANCHOR_FMT, payload, 0)
    return {
        "anchor_id": aid, "sample_seq": sseq, "t_us": t_us,
        "dir": DIR_NAME.get(d, "?"), "pwm_actual": pwm_act,
        "pwm_commanded": pwm_cmd,
        "text": text[:tlen].decode("utf-8", "replace"),
    }


def parse_status(payload: bytes) -> dict:
    if len(payload) < STATUS_LEN:
        raise BadRecord("short status payload")
    f = struct.unpack_from(STATUS_FMT, payload, 0)
    keys = ("uptime_ms", "sample_seq", "cum_missed", "cum_queue_drops",
            "cum_batches", "max_gap_us", "measured_millihz", "free_heap",
            "udp_send_failures", "max_slot_us", "queue_high_water",
            "baseline_a", "baseline_b", "channels", "dir", "pwm_actual",
            "pwm_commanded", "estop", "fixed_mode", "seq_running")
    d = dict(zip(keys, f))
    d["dir"] = DIR_NAME.get(d["dir"], "?")
    d["measured_hz"] = d["measured_millihz"] / 1000.0
    return d


def build_record(rec_type, payload=b"", *, loco_id=9950011, session_id=1,
                 batch_seq=1, first_sample_seq=0, t0_us=0, n_samples=0,
                 missed_before=0, cum_missed=0, cum_queue_drops=0,
                 max_gap_us=0):
    """Assemble a sealed record. Used by the tests to make synthetic captures;
    the firmware is the only thing that makes real ones."""
    hdr = struct.pack(HDR_FMT, MAGIC, FORMAT_VERSION, rec_type, n_samples,
                      loco_id, session_id, batch_seq, first_sample_seq, t0_us,
                      missed_before, cum_missed, cum_queue_drops, max_gap_us, 0)
    crc = zlib.crc32(hdr + payload) & 0xFFFFFFFF
    return hdr[:-4] + struct.pack("<I", crc) + payload


def pack_sample(ch0_raw, ch1_raw=0, *, dt_us=1000, pwm_actual=0,
                pwm_commanded=0, ctx=2, ch0_ann=0, ch1_ann=0,
                ch0_present=True, ch1_present=False):
    def ch(raw, ann, present):
        w = (raw & CH_RAW_MASK) | ((ann & 0x03) << CH_ANN_SHIFT)
        return w | (CH_PRESENT if present else 0)
    return struct.pack(SAMPLE_FMT, ch(ch0_raw, ch0_ann, ch0_present),
                       ch(ch1_raw, ch1_ann, ch1_present),
                       min(dt_us, 0xFFFF), pwm_actual, pwm_commanded, ctx, 0)


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
            raise BadRecord("%s is not an HWT capture file" % path)
        while True:
            frame = fh.read(FRAME_LEN)
            if len(frame) < FRAME_LEN:
                return
            recv_us, length = struct.unpack(FRAME_FMT, frame)
            data = fh.read(length)
            if len(data) < length:
                return          # truncated tail: stop, and let the caller say so
            yield recv_us, data

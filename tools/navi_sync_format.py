#!/usr/bin/env python3
"""NSR1 wire and capture-file format for native Hall + accepted IR evidence.

This module only validates and unpacks bytes. It never repairs, interpolates,
or turns either stream into a navigation decision.
"""

import struct
import zlib

MAGIC = b"NSR1"
FORMAT_VERSION = 1
HDR_FMT = "<4sBBHIIIIIQQIBbBBI"
HDR_LEN = struct.calcsize(HDR_FMT)
HALL_FMT = "<H5HhBBBB"
HALL_LEN = struct.calcsize(HALL_FMT)
IR_FMT = "<Q6sBBBBBBbBB110s"
IR_LEN = struct.calcsize(IR_FMT)
STATUS_FMT = "<IQIIIIIIIIHHBBBB"
STATUS_LEN = struct.calcsize(STATUS_FMT)
NAVI_FMT = "<QQQIIIIhhBBBbBBB"
NAVI_LEN = struct.calcsize(NAVI_FMT)
NATIVE_HALL_FMT = "<IQhBB"
NATIVE_HALL_LEN = struct.calcsize(NATIVE_HALL_FMT)

REC_HALL, REC_IR, REC_STATUS = 1, 2, 3
REC_NAVI, REC_HALL_NATIVE = 4, 5
REC_NAME = {REC_HALL: "HALL", REC_IR: "IR", REC_STATUS: "STATUS",
            REC_NAVI: "NAVI", REC_HALL_NATIVE: "HALL_NATIVE"}
SEQ_NA = 0xFFFFFFFF
MM_NA = 0xFF

HALL_F_DIR_FWD, HALL_F_ESTOP, HALL_F_AUTO = 0x01, 0x02, 0x04
HALL_F_NAV_KNOWN, HALL_F_HOLD, HALL_F_LOW_VOLT = 0x08, 0x10, 0x20
HALL_F_LATE = 0x40
CTX_NAV_KNOWN, CTX_AUTO_ENROLLED, CTX_AUTO_RUNNING = 0x01, 0x02, 0x04
CTX_ESTOP, CTX_LOW_VOLT, CTX_HOLD = 0x08, 0x10, 0x20

FILE_MAGIC = b"NSRCAP01"
FILE_HDR_FMT = "<8sQQ"
FILE_HDR_LEN = struct.calcsize(FILE_HDR_FMT)
FRAME_FMT = "<QH"
FRAME_LEN = struct.calcsize(FRAME_FMT)


class BadRecord(Exception):
    """A datagram cannot be trusted as NSR1 evidence."""


class Header:
    __slots__ = ("version", "rec_type", "n_items", "loco_id", "session_id",
                 "batch_seq", "first_item_seq", "t0_ms", "t0_us",
                 "loco_boot_id", "ring_drops", "nav_mm", "nav_dir",
                 "station_phase", "ctx_flags", "crc32")

    def __init__(self, fields):
        (_magic, self.version, self.rec_type, self.n_items, self.loco_id,
         self.session_id, self.batch_seq, self.first_item_seq, self.t0_ms,
         self.t0_us, self.loco_boot_id, self.ring_drops, self.nav_mm,
         self.nav_dir, self.station_phase, self.ctx_flags, self.crc32) = fields


def crc_ok(data):
    if len(data) < HDR_LEN:
        return False
    blanked = data[:HDR_LEN - 4] + b"\x00\x00\x00\x00" + data[HDR_LEN:]
    stored = struct.unpack_from("<I", data, HDR_LEN - 4)[0]
    return (zlib.crc32(blanked) & 0xFFFFFFFF) == stored


def parse_record(data):
    if len(data) < HDR_LEN:
        raise BadRecord("short datagram (%d bytes; header is %d)" % (len(data), HDR_LEN))
    fields = struct.unpack_from(HDR_FMT, data, 0)
    if fields[0] != MAGIC:
        raise BadRecord("bad magic %r" % (fields[0],))
    if fields[1] != FORMAT_VERSION:
        raise BadRecord("unknown NSR1 version %d" % fields[1])
    header = Header(fields)
    payload = data[HDR_LEN:]
    if header.rec_type == REC_HALL:
        if not 1 <= header.n_items <= 48:
            raise BadRecord("invalid Hall item count %d" % header.n_items)
        want = header.n_items * HALL_LEN
    elif header.rec_type in (REC_IR, REC_STATUS):
        if header.n_items != 1:
            raise BadRecord("%s item count is %d, expected 1" %
                            (REC_NAME[header.rec_type], header.n_items))
        want = IR_LEN if header.rec_type == REC_IR else STATUS_LEN
    elif header.rec_type == REC_NAVI:
        if header.n_items != 1:
            raise BadRecord("NAVI item count is %d, expected 1" % header.n_items)
        want = NAVI_LEN
    elif header.rec_type == REC_HALL_NATIVE:
        if not 1 <= header.n_items <= 48:
            raise BadRecord("invalid native Hall item count %d" % header.n_items)
        want = header.n_items * NATIVE_HALL_LEN
    else:
        raise BadRecord("unknown record type %d" % header.rec_type)
    if len(payload) != want:
        raise BadRecord("payload is %d bytes, expected %d" % (len(payload), want))
    if not crc_ok(data):
        raise BadRecord("CRC32 mismatch")
    return header, payload


def iter_hall(header, payload):
    for i in range(header.n_items):
        f = struct.unpack_from(HALL_FMT, payload, i * HALL_LEN)
        yield {
            "sample_seq": (header.first_item_seq + i) & 0xFFFFFFFF,
            "t_us": header.t0_us if i == 0 else None,
            "dt_us": f[0], "raw": list(f[1:6]), "median": f[6],
            "pwm_actual": f[7], "pwm_commanded": f[8], "flags": f[9],
        }


def parse_ir(payload):
    f = struct.unpack(IR_FMT, payload)
    return {
        "rx_us": f[0], "source_mac": ":".join("%02X" % x for x in f[1]),
        "accept_kind": f[2], "health_fault": f[3], "readiness": f[4],
        "pwm_actual": f[5], "pwm_commanded": f[6], "nav_mm": f[7],
        "nav_dir": f[8], "station_phase": f[9], "ctx_flags": f[10],
        "wire": f[11],
    }


def iter_native_hall(header, payload):
    for i in range(header.n_items):
        serial, t_us, raw, pwm, direction = struct.unpack_from(
            NATIVE_HALL_FMT, payload, i * NATIVE_HALL_LEN)
        yield {"serial": serial, "t_us": t_us, "raw": raw,
               "pwm": pwm, "direction": direction}


def parse_navi(payload):
    f = struct.unpack(NAVI_FMT, payload)
    keys = ("t_us", "ir_um", "opening_ir_um", "hall_serial",
            "opening_serial", "hall_queue_drops", "ir_queue_drops",
            "median5", "reference", "kind", "mm", "target",
            "direction", "degraded", "position_reliable", "spatial_phase")
    return dict(zip(keys, f))


def parse_status(payload):
    f = struct.unpack(STATUS_FMT, payload)
    return {
        "t_ms": f[0], "t_us": f[1], "hall_samples": f[2],
        "hall_ring_drops": f[3], "ir_accepted": f[4], "ir_ring_drops": f[5],
        "ir_input_queue_drops": f[6], "udp_failures": f[7],
        "datagrams_sent": f[8], "max_hall_gap_us": f[9],
        "hall_high_water": f[10], "ir_high_water": f[11],
        "wifi_connected": f[12], "mqtt_connected": f[13],
    }


def write_capture_header(fh, started_us):
    fh.write(struct.pack(FILE_HDR_FMT, FILE_MAGIC, started_us, 0))


def write_frame(fh, recv_us, data):
    fh.write(struct.pack(FRAME_FMT, recv_us, len(data)))
    fh.write(data)


def iter_capture(path):
    with open(path, "rb") as fh:
        head = fh.read(FILE_HDR_LEN)
        if len(head) < FILE_HDR_LEN:
            raise BadRecord("capture is shorter than its header")
        magic, _started, _reserved = struct.unpack(FILE_HDR_FMT, head)
        if magic != FILE_MAGIC:
            raise BadRecord("not an NSR capture (%r)" % (magic,))
        while True:
            frame = fh.read(FRAME_LEN)
            if len(frame) < FRAME_LEN:
                return
            recv_us, length = struct.unpack(FRAME_FMT, frame)
            data = fh.read(length)
            if len(data) < length:
                return
            yield recv_us, data

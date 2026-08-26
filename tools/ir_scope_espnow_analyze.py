#!/usr/bin/env python3
"""Analyze IR_SCOPE_ESPNOW daylight test logs for false-motion vs contrast-loss."""
import struct
import sys

# Packet struct (packed), TX -> RX, type=1 sampler batch:
# magic H, version B, type B, sid I, batchSeq I, firstSample I,
# count H, missedBefore H, runMin h, runMax h, thrHigh h, thrLow h,
# lateTotal I, missedTotal I, queueDrops I, sendErrors I, pulses I, latch I, contrastLoss I,
# sample[96] H, crc H
HDR_FMT = "<HBBIII HH hhhh IIIIIII"
HDR_FMT = "<HBB III HH hhhh IIIIIII"  # 2+1+1+4+4+4+2+2+2+2+2+2+4*7 = 56 bytes
HDR = struct.Struct(HDR_FMT)
assert HDR.size == 56, HDR.size
SAMPLES = struct.Struct("<96H")
CRC = struct.Struct("<H")
PKT_SIZE = HDR.size + SAMPLES.size + CRC.size
assert PKT_SIZE == 250, PKT_SIZE

FLAG_INPULSE = 0x1000
FLAG_RISE = 0x2000
FLAG_FALL = 0x4000
FLAG_CONTRAST = 0x8000
RAW_MASK = 0x0FFF

def crc16(data: bytes) -> int:
    c = 0xFFFF
    for byte in data:
        c ^= byte << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) & 0xFFFF if (c & 0x8000) else (c << 1) & 0xFFFF
    return c

def parse_file(path):
    rows = []
    first_pi_t = None
    with open(path, "r", errors="replace") as f:
        for line in f:
            parts = line.split(None, 2)
            if len(parts) < 3:
                continue
            try:
                pi_t = float(parts[0])
            except ValueError:
                continue
            if parts[1] != "RX":
                continue
            rest = parts[2]
            fields = rest.split()
            # <millis> <rssi> <len> <crc16> <hex>
            if len(fields) < 5:
                continue
            if first_pi_t is None:
                first_pi_t = pi_t
            millis_s, rssi_s, len_s, crc_s, hexpayload = fields[:5]
            try:
                rssi = int(rssi_s)
                plen = int(len_s)
                hex_crc = int(crc_s, 16)
                data = bytes.fromhex(hexpayload.strip())
            except ValueError:
                continue
            if len(data) != plen or plen != PKT_SIZE:
                continue
            if crc16(data) != hex_crc:
                continue  # transport-corrupted line, skip
            magic, version, ptype, sid, batchSeq, firstSample, count, missedBefore, \
                runMin, runMax, thrHigh, thrLow, lateTotal, missedTotal, queueDrops, \
                sendErrors, pulses, latch, contrastLoss = HDR.unpack(data[:56])
            if magic != 0x4952 or ptype != 1:
                continue
            samples = SAMPLES.unpack(data[56:56+192])
            rows.append(dict(
                pi_t=pi_t, elapsed=pi_t - first_pi_t, rssi=rssi, sid=sid,
                batchSeq=batchSeq, firstSample=firstSample, count=count,
                missedBefore=missedBefore, runMin=runMin, runMax=runMax,
                thrHigh=thrHigh, thrLow=thrLow, lateTotal=lateTotal,
                missedTotal=missedTotal, queueDrops=queueDrops, sendErrors=sendErrors,
                pulses=pulses, latch=latch, contrastLoss=contrastLoss, samples=samples,
            ))
    return rows

def analyze(path, label):
    rows = parse_file(path)
    if not rows:
        print(f"=== {label} ===\nNo valid packets parsed from {path}\n")
        return
    rows.sort(key=lambda r: r["firstSample"])
    n = len(rows)
    dur = rows[-1]["pi_t"] - rows[0]["pi_t"]
    pulses0, pulses1 = rows[0]["pulses"], rows[-1]["pulses"]
    latch0, latch1 = rows[0]["latch"], rows[-1]["latch"]
    closs0, closs1 = rows[0]["contrastLoss"], rows[-1]["contrastLoss"]
    spans = [r["runMax"] - r["runMin"] for r in rows]
    total_rise = total_fall = total_inpulse_samples = 0
    rise_times = []
    for r in rows:
        for i, s in enumerate(r["samples"]):
            if s & FLAG_RISE:
                total_rise += 1
                rise_times.append(r["elapsed"])
            if s & FLAG_FALL:
                total_fall += 1
            if s & FLAG_INPULSE:
                total_inpulse_samples += 1
    print(f"=== {label} ===")
    print(f"file: {path}")
    print(f"packets parsed: {n}, span {rows[0]['batchSeq']}..{rows[-1]['batchSeq']} "
          f"(gap-free would be {rows[-1]['batchSeq']-rows[0]['batchSeq']+1})")
    print(f"duration: {dur:.1f}s, rssi min/med/max: "
          f"{min(r['rssi'] for r in rows)}/"
          f"{sorted(r['rssi'] for r in rows)[n//2]}/"
          f"{max(r['rssi'] for r in rows)} dBm")
    print(f"cumulative pulses: {pulses0} -> {pulses1}  (delta {pulses1-pulses0})")
    print(f"cumulative latch discards: {latch0} -> {latch1}  (delta {latch1-latch0})")
    print(f"cumulative contrast-loss: {closs0} -> {closs1}  (delta {closs1-closs0})")
    print(f"missedTotal end: {rows[-1]['missedTotal']}, queueDrops end: {rows[-1]['queueDrops']}, "
          f"sendErrors end: {rows[-1]['sendErrors']}")
    print(f"span (runMax-runMin): min {min(spans)}, median {sorted(spans)[len(spans)//2]}, max {max(spans)}")
    print(f"in-batch rise flags: {total_rise}, fall flags: {total_fall}, inPulse-flagged samples: {total_inpulse_samples}")
    if rise_times:
        print(f"rise-flag elapsed-time range: {min(rise_times):.1f}s .. {max(rise_times):.1f}s")
    print()
    return dict(rows=rows, pulses_delta=pulses1-pulses0, total_rise=total_rise,
                total_fall=total_fall, spans=spans)

DAYLIGHT_SESSION_FILES = [
    ("20260826_ir_daylight_01_stationary_shade.log", "Phase 1: stationary, shade"),
    ("20260826_ir_daylight_02_stationary_sun.log", "Phase 2: stationary, sun"),
    ("20260826_ir_daylight_03_rolling_shade.log", "Phase 3: rolling, shade"),
    ("20260826_ir_daylight_04_rolling_sun.log", "Phase 4: rolling, sun"),
    ("20260826_ir_daylight_05_rolling_boundary.log", "Phase 5: rolling, shade/sun boundary"),
]

if __name__ == "__main__":
    import os
    if len(sys.argv) > 1:
        for path in sys.argv[1:]:
            analyze(path, os.path.basename(path))
    else:
        d = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "field-records", "logs")
        for fname, label in DAYLIGHT_SESSION_FILES:
            analyze(os.path.join(d, fname), label)

#!/usr/bin/env python3
"""Extract CRC-checked IR TX 1.6 R2 radio captures into a replay fixture.

Reads a Pi recorder log (optionally .gz) and writes plain text consumed by
tools/test_ir_r2_bench_replay.cpp:

  B <firstSample> <batchSeq> <runMin> <runMax> <thrHigh> <thrLow> <missedTotal>
    <pulses> <latch> <contrastLoss> <96 x sampleword>
  M <sequence> <capturedUs> <rises> <completed> <openAborts> <gaps>
    <saturated> <unreliable> <reason> <span>

Only type-1 raw batches with the given SID and type-5 movement snapshots
with the given boot id are kept. Transport CRC and each packet's internal
CRC are checked. Duplicate batches (same firstSample) are
dropped after checking they are byte-identical. Nothing is interpolated:
sample indices absent from the output were never received.
"""
import gzip
import struct
import sys

RAW_HDR = struct.Struct("<HBB III HH hhhh IIIIIII")
RAW = struct.Struct("<96H")
MOVE = struct.Struct("<HBBI QQQQ QQQ QQQ II Q BBHH")
assert RAW_HDR.size == 56 and MOVE.size == 110


def crc16(data):
    c = 0xFFFF
    for byte in data:
        c ^= byte << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c


def main(path, out, sid_hex, boot_hex):
    sid, boot = int(sid_hex, 16), int(boot_hex, 16)
    opener = gzip.open if path.endswith(".gz") else open
    batches, moves, rejected = {}, {}, 0
    with opener(path, "rt", errors="replace") as f:
        for line in f:
            fields = line.split()
            if len(fields) < 7 or fields[1] != "RX":
                continue
            try:
                length, wire_crc = int(fields[4]), int(fields[5], 16)
                data = bytes.fromhex(fields[6])
            except ValueError:
                rejected += 1
                continue
            if len(data) != length or crc16(data) != wire_crc:
                rejected += 1
                continue
            if length == 250:
                h = RAW_HDR.unpack(data[:56])
                if h[0] != 0x4952 or h[2] != 1 or h[3] != sid or h[6] != 96:
                    continue
                if crc16(data[:-2]) != int.from_bytes(data[-2:], "little"):
                    rejected += 1
                    continue
                if h[5] in batches:
                    assert batches[h[5]] == data, f"conflicting batch {h[5]}"
                batches[h[5]] = data
            elif length == 110:
                m = MOVE.unpack(data)
                if m[0] != 0x4952 or m[2] != 5 or m[4] != boot:
                    continue
                if m[-1] != crc16(data[:108]):
                    rejected += 1
                    continue
                moves[m[3]] = m
    with open(out, "w") as o:
        for first in sorted(batches):
            data = batches[first]
            h = RAW_HDR.unpack(data[:56])
            s = RAW.unpack(data[56:248])
            # firstSample batchSeq runMin runMax thrHigh thrLow missedTotal pulses latch closs
            o.write("B %d %d %d %d %d %d %d %d %d %d %s\n" % (
                h[5], h[4], h[8], h[9], h[10], h[11], h[13], h[16], h[17],
                h[18], " ".join(map(str, s))))
        for seq in sorted(moves):
            m = moves[seq]
            # seq capturedUs rises completed openAborts gaps sat unreliable reason span
            o.write("M %d %d %d %d %d %d %d %d %d %d\n" % (
                m[3], m[5], m[6], m[7], m[13], m[12], m[11], m[10], m[17], m[19]))
    print(f"{path}: {len(batches)} raw batches, {len(moves)} movement "
          f"snapshots, {rejected} CRC/format rejects -> {out}", file=sys.stderr)


if __name__ == "__main__":
    if len(sys.argv) != 5:
        sys.exit("usage: ir_r2_capture_extract.py LOG OUT SID BOOT")
    main(*sys.argv[1:])

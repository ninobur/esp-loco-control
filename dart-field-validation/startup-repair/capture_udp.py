"""Temporary receive-only NSR1 check; no service changes or device commands."""
import collections
import json
import socket
import struct
import sys
import time
import zlib

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(("0.0.0.0", 47620))
s.settimeout(1)
seconds = float(sys.argv[1]) if len(sys.argv) > 1 else 120
end = time.monotonic() + seconds
counts = collections.Counter()
max_drops = collections.Counter()
bad_crc = 0
boots = set()
gaps = []
missing = 0
previous = None
nonzero_pwm = 0
while time.monotonic() < end:
    try:
        data, source = s.recvfrom(4096)
    except socket.timeout:
        continue
    if len(data) < 56 or data[:4] != b"NSR1" or struct.unpack_from("<I", data, 8)[0] != 9950011:
        continue
    kind = data[5]
    counts[kind] += 1
    valid = zlib.crc32(data[:52] + b"\0" * 4 + data[56:]) == struct.unpack_from("<I", data, 52)[0]
    bad_crc += not valid
    boot = struct.unpack_from("<Q", data, 36)[0]
    boots.add(f"{boot:016X}")
    max_drops[kind] = max(max_drops[kind], struct.unpack_from("<I", data, 44)[0])
    if counts[kind] <= 4:
        print(json.dumps({"sample_type": kind, "source": source[0], "valid_crc": valid,
                          "wire_hex": data.hex()}), flush=True)
    if kind == 5 and valid:
        for i in range(struct.unpack_from("<H", data, 6)[0]):
            serial, stamp, raw, pwm, direction = struct.unpack_from("<IQhBB", data, 56 + i * 16)
            nonzero_pwm += pwm != 0
            if previous and previous[0] == boot:
                if serial == previous[1] + 1:
                    gaps.append(stamp - previous[2])
                else:
                    missing += max(0, serial - previous[1] - 1)
            previous = (boot, serial, stamp)
s.close()
gaps.sort()
print(json.dumps({"summary": True, "seconds": seconds, "packets_by_type": dict(counts),
                  "crc_failures": bad_crc, "boot_ids": sorted(boots), "max_ring_drops": dict(max_drops),
                  "native_missing_serials": missing, "native_nonzero_pwm": nonzero_pwm,
                  "native_gap_count": len(gaps), "gap_p50_us": gaps[len(gaps)//2] if gaps else None,
                  "gap_p99_us": gaps[int(len(gaps)*.99)] if gaps else None,
                  "gap_max_us": max(gaps) if gaps else None}), flush=True)

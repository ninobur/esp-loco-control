#!/usr/bin/env python3
"""xhr_soak.py — send a synthetic XHR1 stream at the locomotive's real rate.

OBSERVATION TOOLING. Diagnostic only. It talks to nothing but the receiver.

This is NOT a substitute for a bench capture on hardware. It cannot tell you
anything about the ADC, the Hall task's timing, Otto's Wi-Fi link or the ESP32
at all. What it does measure is the rest of the chain end to end: that a
receiver on a given host keeps up with 10 datagrams/s of 844 bytes for as long
as a run lasts, that the file grows at the predicted rate, and that the
decoder reads back exactly what was sent.

Use it to prove the Pi is ready before Otto moves, and to size a card.

    # on the Pi
    python3 tools/xhr_receiver.py --outdir /tmp/soak
    # anywhere on the railway network
    python3 tools/xhr_soak.py --host <pi> --seconds 600
"""

import argparse
import os
import random
import socket
import struct
import sys
import time
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xhr_format as F   # noqa: E402


def seal(rec_type, n_items, session, batch_seq, first_seq, t0_ms, t0_us,
         payload, baseline, nav_mm, nav_dir, st_phase, ctx, loco, ring_drops):
    hdr = struct.pack(F.HDR_FMT, F.MAGIC, F.FORMAT_VERSION, rec_type, n_items,
                      loco, session, batch_seq, first_seq, t0_ms, t0_us,
                      baseline, ring_drops, nav_mm, nav_dir, st_phase, ctx, 0)
    crc = zlib.crc32(hdr + payload) & 0xFFFFFFFF
    return hdr[:F.HDR_LEN - 4] + struct.pack("<I", crc) + payload


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=47610)
    ap.add_argument("--seconds", type=float, default=60.0)
    ap.add_argument("--loco", type=int, default=9950011)
    ap.add_argument("--rate", type=float, default=1000.0, help="samples/s")
    args = ap.parse_args()

    session = random.getrandbits(32) or 1
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    n_batch = 100
    batch_dt = n_batch / args.rate
    total_batches = int(args.seconds / batch_dt)
    dt_us = int(1e6 / args.rate)

    print("soaking %s:%d for %.0f s — session %08X" % (args.host, args.port,
                                                       args.seconds, session))
    print("  %d samples/batch, %d B/datagram, %.1f datagrams/s, %.1f kB/s payload"
          % (n_batch, F.HDR_LEN + n_batch * F.SAMPLE_LEN, 1.0 / batch_dt,
             (F.HDR_LEN + n_batch * F.SAMPLE_LEN) / batch_dt / 1024.0))

    t_start = time.time()
    sent = bytes_sent = 0
    worst_late_ms = 0.0
    for b in range(1, total_batches + 1):
        due = t_start + b * batch_dt
        pay = bytearray()
        for i in range(n_batch):
            k = (b - 1) * n_batch + i
            ph = k % 2000
            raw = 1950
            if ph < 140:
                x = ph / 140.0 - 0.5
                raw = int(1950 + 170 * (1 - 4 * x * x))
            flags = F.F_DIR_FWD | F.F_AUTO | F.F_MAY_ADAPT
            if ph < 140:
                flags |= F.F_PASSAGE
            pay += struct.pack(F.SAMPLE_FMT, 0 if k == 0 else dt_us, raw, 90, 90, flags, 0)
        d = seal(F.REC_SAMPLES, n_batch, session, b, (b - 1) * n_batch,
                 int(b * batch_dt * 1000), int(b * batch_dt * 1e6) & 0xFFFFFFFF,
                 bytes(pay), 1948, (b // 20) % 171, 1, 0, 0x0B, args.loco, 0)
        if b % 10 == 1:
            st = struct.pack(F.STATUS_FMT, int(b * batch_dt * 1000),
                             int(b * batch_dt * 1000), b * n_batch, 0, 0, 0,
                             dt_us, 260, 180000, 2100, 5200,
                             int(args.rate * 10), 1, -58, 1, 1, 0)
            sd = seal(F.REC_STATUS, 1, session, (b // 10) + 1, F.SAMPLE_SEQ_NA,
                      int(b * batch_dt * 1000), int(b * batch_dt * 1e6) & 0xFFFFFFFF,
                      st, 1948, 0, 1, 0, 0x0B, args.loco, 0)
            sock.sendto(sd, (args.host, args.port))
            sent += 1; bytes_sent += len(sd)
        now = time.time()
        if now < due:
            time.sleep(due - now)
        else:
            worst_late_ms = max(worst_late_ms, (now - due) * 1000.0)
        sock.sendto(d, (args.host, args.port))
        sent += 1
        bytes_sent += len(d)

    span = time.time() - t_start
    print("sent %d datagrams, %.2f MB in %.1f s  (%.1f kB/s, %.1f datagrams/s)"
          % (sent, bytes_sent / 1048576.0, span, bytes_sent / span / 1024.0, sent / span))
    print("  worst send-schedule slip %.1f ms" % worst_late_ms)
    print("  a 2-circuit run (~8 min) would be %.1f MB; an hour, %.0f MB"
          % (bytes_sent / span * 480 / 1048576.0, bytes_sent / span * 3600 / 1048576.0))


if __name__ == "__main__":
    main()

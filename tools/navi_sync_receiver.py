#!/usr/bin/env python3
"""Listen-only Pi receiver for the NSR1 native Hall + accepted IR stream.

It writes every UDP datagram before attempting to decode it.  Bad datagrams,
sequence gaps, on-locomotive ring drops, and receiver-side loss are reported
but never repaired or discarded from the binary capture.

Example on the Pi:

    python3 /home/david/NGR/navi_sync_receiver.py \
        --outdir /home/david/NGR/navi_sync

The resulting ``navi_sync_*.nsr`` file is the evidence.  Decode it later with
``tools/navi_sync_decode.py`` or ``tools/navi_sync_format.py``.
"""

import argparse
import os
import socket
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import navi_sync_format as F  # noqa: E402


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=47620)
    ap.add_argument("--outdir", default=".")
    ap.add_argument("--name", help="capture name; default navi_sync_<time>.nsr")
    ap.add_argument("--flush-secs", type=float, default=5.0)
    ap.add_argument("--min-free-mb", type=float, default=200.0)
    args = ap.parse_args()

    os.makedirs(args.outdir, exist_ok=True)
    name = args.name or "navi_sync_%s.nsr" % time.strftime("%Y%m%d_%H%M%S")
    path = os.path.join(args.outdir, name)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4 << 20)
    sock.bind(("", args.port))
    sock.settimeout(0.5)
    print("recording %s (udp/%d, listen-only)" % (path, args.port))
    print("Hall: legacy grouped and EWO per-ADC records; IR: type-5 snapshots")
    print("ctrl-c to stop")

    fh = open(path, "wb")
    F.write_capture_header(fh, int(time.time() * 1e6))
    last_flush = time.time()
    last_seq = {}
    last_drops = {}
    stats = {"datagrams": 0, "bytes": 0, "hall_items": 0, "native_hall": 0,
             "ir": 0, "navi": 0, "status": 0, "bad": 0, "gaps": 0, "lost": 0, "onboard": 0,
             "sessions": set()}
    try:
        while True:
            try:
                data, _addr = sock.recvfrom(2048)
            except socket.timeout:
                if time.time() - last_flush >= args.flush_secs:
                    fh.flush(); last_flush = time.time()
                continue

            recv_us = int(time.time() * 1e6)
            F.write_frame(fh, recv_us, data)
            stats["datagrams"] += 1; stats["bytes"] += len(data)
            try:
                hdr, payload = F.parse_record(data)
            except F.BadRecord as exc:
                stats["bad"] += 1
                print("BAD datagram kept (%d bytes): %s" % (len(data), exc))
                continue

            stats["sessions"].add((hdr.loco_id, hdr.session_id))
            key = (hdr.session_id, hdr.rec_type)
            previous = last_seq.get(key)
            if previous is not None and hdr.batch_seq > previous + 1:
                missing = hdr.batch_seq - previous - 1
                stats["gaps"] += 1; stats["lost"] += missing
                old_drop = last_drops.get(hdr.session_id)
                if old_drop is not None and hdr.ring_drops != old_drop:
                    stats["onboard"] += missing
                    origin = "on locomotive"
                else:
                    origin = "in transit/receiver"
                print("GAP %s %d..%d (%d missing; %s)" %
                      (F.REC_NAME[hdr.rec_type], previous + 1,
                       hdr.batch_seq - 1, missing, origin))
            if previous is None or hdr.batch_seq > previous:
                last_seq[key] = hdr.batch_seq
            last_drops[hdr.session_id] = hdr.ring_drops
            if hdr.rec_type == F.REC_HALL:
                stats["hall_items"] += hdr.n_items
            elif hdr.rec_type == F.REC_HALL_NATIVE:
                stats["native_hall"] += hdr.n_items
            elif hdr.rec_type == F.REC_IR:
                stats["ir"] += 1
            elif hdr.rec_type == F.REC_NAVI:
                stats["navi"] += 1
            elif hdr.rec_type == F.REC_STATUS:
                stats["status"] += 1
                status = F.parse_status(payload)
                print("status Hall=%d IR=%d drops=%d/%d udp_fail=%d gap=%dus" %
                      (status["hall_samples"], status["ir_accepted"],
                       status["hall_ring_drops"], status["ir_ring_drops"],
                       status["udp_failures"], status["max_hall_gap_us"]))
            if stats["datagrams"] % 100 == 0:
                print("%d datagrams, %d native Hall samples, %d IR snapshots, %d NAVI decisions, %d bad" %
                      (stats["datagrams"], stats["native_hall"], stats["ir"], stats["navi"], stats["bad"]))
            if time.time() - last_flush >= args.flush_secs:
                fh.flush(); last_flush = time.time()
    except KeyboardInterrupt:
        pass
    finally:
        fh.flush(); fh.close(); sock.close()
    print("stopped %s: %d datagrams, %d native Hall samples, %d IR snapshots, %d NAVI decisions, %d bad" %
          (path, stats["datagrams"], stats["native_hall"], stats["ir"], stats["navi"], stats["bad"]))
    print("gaps=%d missing=%d attributed_on_loco=%d sessions=%d" %
          (stats["gaps"], stats["lost"], stats["onboard"], len(stats["sessions"])))


if __name__ == "__main__":
    main()

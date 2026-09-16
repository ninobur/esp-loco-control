#!/usr/bin/env python3
"""xhr_receiver.py — Pi-side recorder for the X18 continuous Hall trace.

OBSERVATION TOOLING. Diagnostic only.

Listens for the locomotive's UDP stream and writes every datagram to a capture
file, byte for byte, with its arrival wall-clock time. It never reorders,
never repairs, never fills a gap and never interprets: decoding is
xhr_decode.py's job, so a bug in analysis can never damage the evidence.

The console commentary while it runs is exactly that -- commentary. It is
computed AFTER the bytes are already on disk, and it is allowed to be wrong
about a damaged datagram without the datagram itself being altered or dropped.

LISTEN-ONLY. Unlike hwt_receiver.py, this one has no command path back to the
locomotive: the X18 recorder binds no port and parses nothing inbound, so
there is nothing to send to. Otto is driven from the console and MQTT exactly
as it always is, and this program cannot influence the railway at all.

Run it on the Pi BEFORE Otto moves:

    python3 tools/xhr_receiver.py --outdir ~/NGR/hall_records

Stop it with ctrl-c, then decode:

    python3 tools/xhr_decode.py ~/NGR/hall_records/<file>.xhr --report
"""

import argparse
import os
import shutil
import socket
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xhr_format as F   # noqa: E402


def human(n):
    for unit in ("B", "kB", "MB", "GB"):
        if n < 1024 or unit == "GB":
            return "%.1f %s" % (n, unit)
        n /= 1024.0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=47610, help="stream port (default 47610)")
    ap.add_argument("--outdir", default=".", help="where capture files are written")
    ap.add_argument("--name", help="capture file name (default xhr_<timestamp>.xhr)")
    ap.add_argument("--flush-secs", type=float, default=5.0,
                    help="how often the file is flushed to the OS (default 5)")
    ap.add_argument("--min-free-mb", type=float, default=200.0,
                    help="stop recording rather than fill the card (default 200 MB)")
    ap.add_argument("--quiet", action="store_true", help="no periodic console line")
    args = ap.parse_args()

    os.makedirs(args.outdir, exist_ok=True)
    name = args.name or ("xhr_%s.xhr" % time.strftime("%Y%m%d_%H%M%S"))
    path = os.path.join(args.outdir, name)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # A generous kernel receive buffer is the cheapest insurance there is: the
    # locomotive cannot retransmit, so anything the socket drops is simply
    # gone. 4 MB is ~8 minutes of this stream.
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4 << 20)
    got = sock.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)
    sock.bind(("", args.port))
    sock.settimeout(0.5)

    print("recording %s  (udp/%d, rcvbuf %s)" % (path, args.port, human(got)))
    print("listen-only: this program cannot command the locomotive")
    print("ctrl-c to stop")

    stats = {"datagrams": 0, "bytes": 0, "samples": 0, "rulings": 0,
             "status": 0, "bad": 0, "batch_gaps": 0, "lost_batches": 0,
             "loco_drops": 0, "sessions": 0}
    last_seq = {}            # (session, rectype) -> last batch_seq
    last_drops = {}          # session -> last ring_drops_lo
    seen_sessions = set()
    last_print = last_flush = time.time()
    first_recv = None

    fh = open(path, "wb")
    F.write_capture_header(fh, int(time.time() * 1e6))
    try:
        while True:
            try:
                data, _addr = sock.recvfrom(2048)
            except socket.timeout:
                now = time.time()
                if now - last_flush >= args.flush_secs:
                    last_flush = now
                    fh.flush()
                continue

            # WRITE FIRST. Everything below this line is commentary on a
            # recording that has already been made durable.
            recv_us = int(time.time() * 1e6)
            F.write_frame(fh, recv_us, data)
            stats["datagrams"] += 1
            stats["bytes"] += len(data)
            if first_recv is None:
                first_recv = recv_us

            try:
                hdr, payload = F.parse_record(data)
            except F.BadRecord as e:
                stats["bad"] += 1
                print("  BAD DATAGRAM (%d bytes, kept verbatim): %s" % (len(data), e))
                continue

            if hdr.session_id not in seen_sessions:
                seen_sessions.add(hdr.session_id)
                stats["sessions"] = len(seen_sessions)
                if len(seen_sessions) > 1:
                    print("  ** NEW SESSION %08X — the locomotive rebooted. Records "
                          "either side of this line belong to different runs and "
                          "must never be joined." % hdr.session_id)
                else:
                    print("  session %08X, loco %d" % (hdr.session_id, hdr.loco_id))

            key = (hdr.session_id, hdr.rec_type)
            prev = last_seq.get(key)
            if prev is not None and hdr.batch_seq > prev + 1:
                missing = hdr.batch_seq - prev - 1
                stats["batch_gaps"] += 1
                stats["lost_batches"] += missing
                # A gap WITH a rise in the locomotive's own ring-drop counter
                # was dropped on board; a gap WITHOUT one was lost in transit.
                # The receiver never has to guess, so it never does.
                pd = last_drops.get(hdr.session_id)
                onboard = (pd is not None and hdr.ring_drops_lo != pd)
                if onboard:
                    stats["loco_drops"] += missing
                print("  GAP %s seq %d..%d (%d lost, %s)"
                      % (F.REC_NAME.get(hdr.rec_type, "?"), prev + 1,
                         hdr.batch_seq - 1, missing,
                         "dropped on the locomotive" if onboard else "lost in transit"))
            if prev is None or hdr.batch_seq > prev:
                last_seq[key] = hdr.batch_seq
            last_drops[hdr.session_id] = hdr.ring_drops_lo

            if hdr.rec_type == F.REC_SAMPLES:
                stats["samples"] += hdr.n_items
            elif hdr.rec_type == F.REC_RULING:
                stats["rulings"] += 1
                r = F.parse_ruling(payload)
                print("  RULING %-12s mm %s->%s pol %s peak %d dur %d ms  sample %d"
                      % (r["ruling"],
                         "?" if r["nav_mm_before"] == F.MM_NA else r["nav_mm_before"],
                         "?" if r["nav_mm_after"] == F.MM_NA else r["nav_mm_after"],
                         r["polarity"], r["peak"], r["duration_ms"], r["sample_seq"]))
            elif hdr.rec_type == F.REC_STATUS:
                stats["status"] += 1
                s = F.parse_status(payload)
                if not args.quiet:
                    print("  STATUS %.1f Hz  ring %d hi  drops %d/%d  udpfail %d  "
                          "maxgap %d us  tickbody %d us  heap %d  hall stack %d B  "
                          "rssi %d"
                          % (s["measured_hz"], s["ring_high_water"],
                             s["cum_sample_ring_drops"], s["cum_ruling_ring_drops"],
                             s["cum_udp_failures"], s["max_gap_us"],
                             s["max_tick_body_us"], s["free_heap"],
                             s["hall_stack_free_bytes"], s["rssi"]))

            now = time.time()
            if now - last_flush >= args.flush_secs:
                last_flush = now
                fh.flush()
                free = shutil.disk_usage(args.outdir).free
                if free < args.min_free_mb * 1024 * 1024:
                    print("  ** ONLY %s FREE — stopping rather than filling the card"
                          % human(free))
                    break
            if not args.quiet and now - last_print >= 10.0:
                last_print = now
                print("  %d datagrams  %s  %d samples  %d rulings  %d bad  "
                      "%d lost batches"
                      % (stats["datagrams"], human(stats["bytes"]),
                         stats["samples"], stats["rulings"], stats["bad"],
                         stats["lost_batches"]))
    except KeyboardInterrupt:
        pass
    finally:
        fh.flush()
        os.fsync(fh.fileno())
        fh.close()
        sock.close()

    span = (time.time() * 1e6 - first_recv) / 1e6 if first_recv else 0.0
    print("\nwrote %s  (%s)" % (path, human(os.path.getsize(path))))
    print("  %d datagrams, %d samples, %d rulings, %d status"
          % (stats["datagrams"], stats["samples"], stats["rulings"], stats["status"]))
    if span > 0:
        print("  %.1f s wall clock, %.1f datagrams/s, %s/s sustained"
              % (span, stats["datagrams"] / span, human(stats["bytes"] / span)))
    print("  %d unreadable datagrams (kept verbatim on disk)" % stats["bad"])
    print("  %d transport gaps covering %d batches, of which %d were dropped "
          "on the locomotive" % (stats["batch_gaps"], stats["lost_batches"],
                                 stats["loco_drops"]))
    if stats["sessions"] > 1:
        print("  ** %d SESSIONS in this file: the locomotive rebooted mid-capture. "
              "Decode per session; do not join them." % stats["sessions"])
    print("  decode with: python3 tools/xhr_decode.py %s --report" % path)


if __name__ == "__main__":
    main()

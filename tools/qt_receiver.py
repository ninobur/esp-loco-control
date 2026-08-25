#!/usr/bin/env python3
"""qt_receiver.py — host recorder for QUORUM TRACE.

INVESTIGATORY / UNAPPROVED. Diagnostic tooling only.

Listens for the locomotive's UDP trace stream and writes every datagram to a
capture file, byte for byte, with its arrival wall-clock time. It never
reorders, never repairs, never drops and never interprets: decoding is
qt_decode.py's job, so a bug in analysis can never damage the evidence. Same
discipline as tools/hwt_receiver.py, which this deliberately mirrors.

Run it on the Pi (or any host on the railway network):

    python3 tools/qt_receiver.py --outdir ~/NGR/qt_logs

This receiver is listen-only by design. Operator anchors do not go through
it: they are entered via MQTT (ngr/loco/<id>/cmd/trace_anchor -- see
firmware/QUORUM/README_TRACE.md, "Anchor mechanism"), which the locomotive
turns into an ordinary QT_REC_ANCHOR record on THIS SAME UDP stream --
handled below exactly like any other record type, no special-casing needed
here. This tool has no command/publish path of its own, on purpose: the
capture file stays a pure, append-only recording no analysis bug can
corrupt.
"""

import argparse
import os
import socket
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import qt_format as F   # noqa: E402


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=47700, help="stream port (default 47700)")
    ap.add_argument("--outdir", default=".", help="where capture files are written")
    ap.add_argument("--name", help="capture file name (default qt_<timestamp>.qtcap)")
    ap.add_argument("--quiet", action="store_true", help="no periodic console line")
    args = ap.parse_args()

    os.makedirs(args.outdir, exist_ok=True)
    name = args.name or ("qt_%s.qtcap" % time.strftime("%Y%m%d_%H%M%S"))
    path = os.path.join(args.outdir, name)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1 << 20)
    sock.bind(("", args.port))
    sock.settimeout(0.5)

    print("recording %s  (port %d)  ctrl-c to stop" % (path, args.port))

    stats = {"records": 0, "samples": 0, "decisions": 0, "status": 0,
             "bad": 0, "batch_gaps": 0, "lost_batches": 0}
    last_seq = {}   # (session_id, rec_type) -> last batch_seq seen (per-type streams; see qt_format.py)
    last_print = time.time()

    fh = open(path, "wb")
    F.write_capture_header(fh, int(time.time() * 1e6))
    try:
        while True:
            try:
                data, _addr = sock.recvfrom(2048)
            except socket.timeout:
                continue
            # Write FIRST. Parsing is best-effort commentary on top of a
            # recording that has already been made durable.
            F.write_frame(fh, int(time.time() * 1e6), data)
            stats["records"] += 1

            try:
                hdr, payload = F.parse_record(data)
            except F.BadRecord as e:
                stats["bad"] += 1
                print("  BAD RECORD: %s" % e)
                continue

            key = (hdr.session_id, hdr.rec_type)
            prev = last_seq.get(key)
            if prev is not None and hdr.batch_seq > prev + 1:
                stats["batch_gaps"] += 1
                stats["lost_batches"] += hdr.batch_seq - prev - 1
            if prev is None or hdr.batch_seq > prev:
                last_seq[key] = hdr.batch_seq

            if hdr.rec_type == F.REC_SAMPLE:
                stats["samples"] += hdr.n_items
            elif hdr.rec_type == F.REC_DECISION:
                stats["decisions"] += 1
                d = F.parse_decision(payload)
                print("  DECISION %-20s mm %s->%s  %s->%s  obs=%s exp=%s"
                      % (d["kind"], d["nav_mm_before"], d["nav_mm_after"],
                         d["nav_state_before"], d["nav_state_after"],
                         d["observed_polarity"], d["expected_polarity"]))
            elif hdr.rec_type == F.REC_STATUS:
                stats["status"] += 1
                s = F.parse_status(payload)
                if not args.quiet:
                    print("  STATUS samples=%d decisions=%d sample_ring_drops=%d "
                          "decision_ring_drops=%d anchor_ring_drops=%d udp_fail=%d heap=%d"
                          % (s["sample_seq"], s["decision_seq"],
                             s["cum_sample_ring_drops"], s["cum_decision_ring_drops"],
                             s["cum_anchor_ring_drops"],
                             s["udp_send_failures"], s["free_heap"]))
            elif hdr.rec_type == F.REC_ANCHOR:
                a = F.parse_anchor(payload)
                print("  ANCHOR #%d \"%s\"" % (a["anchor_id"], a["text"]))

            now = time.time()
            if not args.quiet and now - last_print >= 5.0:
                last_print = now
                fh.flush()
                print("  %d records  %d samples  %d decisions  %d status  %d bad  "
                      "%d lost batches in transport"
                      % (stats["records"], stats["samples"], stats["decisions"],
                         stats["status"], stats["bad"], stats["lost_batches"]))
    except KeyboardInterrupt:
        pass
    finally:
        fh.close()
        sock.close()

    print("\nwrote %s" % path)
    print("  %d records, %d samples, %d decisions, %d status"
          % (stats["records"], stats["samples"], stats["decisions"], stats["status"]))
    print("  %d unreadable datagrams, %d transport gaps covering %d batches"
          % (stats["bad"], stats["batch_gaps"], stats["lost_batches"]))
    print("  decode with: python3 tools/qt_decode.py %s" % path)


if __name__ == "__main__":
    main()

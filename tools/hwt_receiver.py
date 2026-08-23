#!/usr/bin/env python3
"""hwt_receiver.py — host recorder for HALL_WAVEFORM_TEST.

INVESTIGATORY / UNAPPROVED. Diagnostic tooling only.

Listens for the locomotive's UDP batch stream and writes every datagram to a
capture file, byte for byte, with its arrival wall-clock time. It never
reorders, never repairs, never drops and never interprets: decoding is
hwt_decode.py's job, so a bug in analysis can never damage the evidence.

Run it on the Pi (or any host on the railway network):

    python3 tools/hwt_receiver.py --outdir ~/NGR/hwt_logs

Operator anchors and test-driving commands go the other way, to the
locomotive's command port. Type them at the prompt while it runs:

    ANCHOR patio-marker-15      insert an anchor into the stream
    FIXED 70 / GO / STOP        arm and run a fixed-PWM step
    DIR F | DIR R               set direction
    NEXT                        arm the next sequence step
    ESTOP 1 | ESTOP 0           local E-STOP (the locomotive's own always wins)

--loco is needed for that direction; without it the receiver is listen-only.
"""

import argparse
import os
import socket
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hwt_format as F   # noqa: E402


def command_loop(sock, loco_addr, stop):
    """Forward operator lines to the locomotive. Anchors are the only ground
    truth this experiment has, so this path is kept deliberately dumb."""
    while not stop.is_set():
        try:
            line = input()
        except (EOFError, KeyboardInterrupt):
            stop.set()
            return
        line = line.strip()
        if not line:
            continue
        if line.lower() in ("quit", "exit"):
            stop.set()
            return
        sock.sendto(line.encode("utf-8")[:63], loco_addr)
        print("  -> %s" % line)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=47600, help="stream port (default 47600)")
    ap.add_argument("--loco", help="locomotive IP, to send anchors and commands")
    ap.add_argument("--loco-port", type=int, default=47601)
    ap.add_argument("--outdir", default=".", help="where capture files are written")
    ap.add_argument("--name", help="capture file name (default hwt_<timestamp>.hwt)")
    ap.add_argument("--quiet", action="store_true", help="no periodic console line")
    args = ap.parse_args()

    os.makedirs(args.outdir, exist_ok=True)
    name = args.name or ("hwt_%s.hwt" % time.strftime("%Y%m%d_%H%M%S"))
    path = os.path.join(args.outdir, name)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1 << 20)
    sock.bind(("", args.port))
    sock.settimeout(0.5)

    stop = threading.Event()
    if args.loco:
        t = threading.Thread(target=command_loop, args=(sock, (args.loco, args.loco_port), stop),
                             daemon=True)
        t.start()
        print("commands go to %s:%d — type ANCHOR <text>, FIXED <pwm>, GO, STOP, HELP"
              % (args.loco, args.loco_port))
    else:
        print("listen-only (no --loco): anchors cannot be sent from here")

    print("recording %s  (port %d)  ctrl-c to stop" % (path, args.port))

    stats = {"records": 0, "samples": 0, "bad": 0, "anchors": 0,
             "batch_gaps": 0, "lost_batches": 0}
    last_seq = {}
    last_print = time.time()

    fh = open(path, "wb")
    F.write_capture_header(fh, int(time.time() * 1e6))
    try:
        while not stop.is_set():
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

            prev = last_seq.get(hdr.session_id)
            if prev is not None and hdr.batch_seq > prev + 1:
                stats["batch_gaps"] += 1
                stats["lost_batches"] += hdr.batch_seq - prev - 1
            if prev is None or hdr.batch_seq > prev:
                last_seq[hdr.session_id] = hdr.batch_seq

            if hdr.rec_type == F.REC_SAMPLES:
                stats["samples"] += hdr.n_samples
            elif hdr.rec_type == F.REC_ANCHOR:
                a = F.parse_anchor(payload)
                stats["anchors"] += 1
                print("  ANCHOR #%d \"%s\"  sample=%d dir=%s pwm=%d/%d"
                      % (a["anchor_id"], a["text"], a["sample_seq"], a["dir"],
                         a["pwm_actual"], a["pwm_commanded"]))
            elif hdr.rec_type == F.REC_STATUS:
                s = F.parse_status(payload)
                if not args.quiet:
                    print("  STATUS %.0f Hz measured  ch=%d  missed=%d qdrop=%d "
                          "qhw=%d maxgap=%dus slot=%dus udpfail=%d heap=%d"
                          % (s["measured_hz"], s["channels"], s["cum_missed"],
                             s["cum_queue_drops"], s["queue_high_water"],
                             s["max_gap_us"], s["max_slot_us"],
                             s["udp_send_failures"], s["free_heap"]))

            now = time.time()
            if not args.quiet and now - last_print >= 5.0:
                last_print = now
                fh.flush()
                print("  %d records  %d samples  %d anchors  %d bad  "
                      "%d lost batches in transport"
                      % (stats["records"], stats["samples"], stats["anchors"],
                         stats["bad"], stats["lost_batches"]))
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        fh.close()
        sock.close()

    print("\nwrote %s" % path)
    print("  %d records, %d samples, %d anchors" %
          (stats["records"], stats["samples"], stats["anchors"]))
    print("  %d unreadable datagrams, %d transport gaps covering %d batches"
          % (stats["bad"], stats["batch_gaps"], stats["lost_batches"]))
    print("  decode with: python3 tools/hwt_decode.py %s" % path)


if __name__ == "__main__":
    main()

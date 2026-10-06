#!/usr/bin/env python3
"""Durably record IR_SCOPE_ESPNOW_RX serial lines with Pi arrival time.

The original one-shot interface remains available through ``--out``.  For
unattended service use, ``--outdir`` creates one raw file per local calendar
day and updates a small JSON health file.  Every receiver line is preserved
byte-for-byte after the existing Pi epoch timestamp prefix.
Decoded Type-7 PULSE_TRANSPORT JSON rows are additive; Type-6/7 accounting is
separate per sid/boot and included in health. The radio receiver is unchanged.
"""
import argparse
import datetime
import json
import os
import time

from ir_pulse_transport import TransportTelemetry, record_line


def atomic_json(path, value):
    temporary = path + ".tmp"
    with open(temporary, "w", encoding="utf-8") as stream:
        json.dump(value, stream, sort_keys=True)
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, path)


def daily_path(outdir, epoch):
    stamp = datetime.datetime.fromtimestamp(epoch).strftime("%Y%m%d")
    return os.path.join(outdir, "ir_espnow_raw_%s.log" % stamp)


def main():
    import serial
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=921600)
    output = parser.add_mutually_exclusive_group(required=True)
    output.add_argument("--out", help="one fixed output file (legacy/manual mode)")
    output.add_argument("--outdir", help="daily rotating output directory")
    parser.add_argument(
        "--health",
        help="JSON health path (default: <outdir>/ir_espnow_health.json)",
    )
    args = parser.parse_args()

    if args.outdir:
        os.makedirs(args.outdir, exist_ok=True)
    health_path = args.health
    if args.outdir and not health_path:
        health_path = os.path.join(args.outdir, "ir_espnow_health.json")

    started = time.time()
    line_count = 0
    byte_count = 0
    last_packet_epoch = None
    current_path = None
    output_stream = None
    last_health = 0.0
    telemetry = TransportTelemetry()

    try:
        with serial.Serial(
            args.port, args.baud, timeout=1, dsrdtr=False, rtscts=False
        ) as receiver:
            receiver.dtr = False
            receiver.rts = False

            while True:
                line = receiver.readline()
                now = time.time()  # Pi receipt/read completion, not before blocking.
                wanted_path = args.out or daily_path(args.outdir, now)
                if wanted_path != current_path:
                    if output_stream is not None:
                        output_stream.close()
                    output_stream = open(wanted_path, "ab", buffering=0)
                    current_path = wanted_path

                if line:
                    byte_count += record_line(output_stream, line, now, telemetry)
                    line_count += 1
                    last_packet_epoch = now

                if health_path and now - last_health >= 5.0:
                    atomic_json(
                        health_path,
                        {
                            "baud": args.baud,
                            "bytes": byte_count,
                            "current_file": current_path,
                            "last_packet_epoch": last_packet_epoch,
                            "lines": line_count,
                            "pid": os.getpid(),
                            "port": args.port,
                            "started_epoch": started,
                            "updated_epoch": now,
                            "pulse_transport": telemetry.health(),
                        },
                    )
                    last_health = now
    finally:
        if output_stream is not None:
            output_stream.close()


if __name__ == "__main__":
    main()

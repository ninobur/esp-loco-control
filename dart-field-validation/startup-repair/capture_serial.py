"""Bounded, receive-only USB capture; optional hardware reset, never commands."""
import argparse
import time
from pathlib import Path
import serial

p = argparse.ArgumentParser()
p.add_argument("output", type=Path)
p.add_argument("--seconds", type=float, default=45)
p.add_argument("--reset", action="store_true")
args = p.parse_args()
s = serial.Serial()
s.port = "/dev/cu.wchusbserial10"
s.baudrate = 115200
s.timeout = 0.2
s.dtr = False
s.rts = False
s.open()
if args.reset:
    s.rts = True
    time.sleep(0.1)
    s.rts = False
end = time.monotonic() + args.seconds
with args.output.open("wb") as out:
    while time.monotonic() < end:
        chunk = s.read(min(8192, max(1, s.in_waiting)))
        if chunk:
            out.write(chunk)
            out.flush()
s.close()

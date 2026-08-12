#!/bin/sh
# NGR telemetry capture — runs on the Mac, writes to the Mac.
#
# Why not on the Pi: on 2026-08-11 the Pi's SD card threw EXT4 I/O errors,
# aborted its journal and remounted read-only mid-session. Every subscriber
# writing to it died at 18:11:51 and ~50 minutes of running was never recorded.
# A capture that dies silently is worse than no capture, because the gap looks
# like evidence of quiet running. The broker itself keeps serving, so the fix is
# to subscribe across the network and keep the file on a disk that works.
#
# Two things this does that the previous arrangement did not:
#   * RECONNECTS. mosquitto_sub exits when the broker drops it; this restarts it
#     and appends, so a blip costs seconds rather than the rest of the session.
#   * SAYS SO. Every restart writes a # RECONNECT line into the capture, so a
#     gap is visible in the evidence instead of being invisible.
#
# Usage:  field-records/tools/capture.sh <name> [host]
#   e.g.  field-records/tools/capture.sh 20260812_otto_ccw_ramped
#
# Verify it is alive with:  tail -f <file>   — or check the size is growing.
# Stop with Ctrl-C.

set -u
NAME="${1:?usage: capture.sh <name> [host]}"
HOST="${2:-192.168.68.142}"
OUT="$(cd "$(dirname "$0")/../logs" && pwd)/${NAME}.log"

printf '# capture %s -> %s\n' "$NAME" "$OUT"
printf '# broker %s ; started %s\n' "$HOST" "$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$OUT"

trap 'printf "# STOPPED %s\n" "$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$OUT"; exit 0' INT TERM

while : ; do
  mosquitto_sub -h "$HOST" -F '%U %t %p' \
      -t 'ngr/loco/9950011/#' \
      -t 'ngr/loco/9950012/#' \
      -t 'ngr/spoke/IR_SPEED_SENSOR/#' \
      -t 'ngr/dispatcher/#' >> "$OUT" 2>/dev/null
  # Only reached when the subscriber exits — broker restart, network blip, or
  # the far end going away. Record it and resume.
  printf '# RECONNECT %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$OUT"
  sleep 2
done

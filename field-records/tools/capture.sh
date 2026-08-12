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
# ---------------------------------------------------------------------------
# 2026-08-12: the first version of this script FAILED IN EXACTLY THE WAY IT WAS
# WRITTEN TO PREVENT, and the three fixes below are the result.
#
# The CCW/CW session of 2026-08-12 recorded 214 of ~1075 markers. The verdict
# blamed the degraded SD card dropping QoS 0 publishes. That was wrong. The
# missing markers fall inside three total-silence windows — 1117 s, 626 s and
# 374 s — and every boundary matches a MacBook sleep/wake transition to within
# seconds ('Idle Sleep' 22:32:55, DarkWake 22:51:32, and so on, on battery).
# The locomotive's own mqtt_attempts counter stayed at 1 across all three, so
# the locomotive never reconnected; the broker never restarted. The Mac slept.
#
#   1. HOLDS SLEEP OFF. caffeinate is held for the life of the capture, and the
#      script says so loudly when running on battery, where a closed lid still
#      sleeps regardless.
#   2. DETECTS ITS OWN SILENCE. The old reconnect logging assumed mosquitto_sub
#      EXITS when the link drops. It does not: 2.x calls mosquitto_loop_forever,
#      which reconnects and resubscribes internally. The process never exits, so
#      no RECONNECT was ever logged and 18 minutes of silence looked like 18
#      minutes of quiet running. A watchdog now writes # STALL / # RESUME lines
#      from the outside, so silence is timestamped in the evidence as it happens.
#      The only fingerprint the old capture left was a burst of retained-state
#      topics on each resubscribe — recognisable only in hindsight.
#   3. SUBSCRIBES TO EVERYTHING. The old topic list missed ngr/marker/# (the
#      SENSORTEST tree that tools/align_markers.py consumes) and ngr/spoke/spoke1.
#      A handful of $SYS counters come along too: they double as a 10 s
#      heartbeat, which is what makes silence unambiguous rather than a guess
#      about whether the railway was simply idle.
# ---------------------------------------------------------------------------
#
# Usage:  field-records/tools/capture.sh <name> [host]
#   e.g.  field-records/tools/capture.sh 20260812_otto_cw_noquorum
#
# Verify it is alive with:  tail -f <file>   — or check the size is growing.
# Stop with Ctrl-C.

set -u

NAME="${1:?usage: capture.sh <name> [host]}"
HOST="${2:-192.168.68.142}"

# Refuse to start alongside another capture. On 2026-08-11 a leftover self-test
# kept running after its output file was deleted — a live process writing to a
# deleted inode, producing nothing while looking healthy. That is the exact
# failure this tool exists to prevent, so it is now refused rather than trusted.
#
# Keyed on the SUBSCRIBER, not on this script's name: the name test matched any
# shell whose command line merely mentioned capture.sh — including the one
# launching it — so it refused to start at all under a wrapper. mosquitto_sub is
# the process that actually writes a capture, so it is the one worth guarding.
OTHER=$(ps ax -o pid=,command= | grep '[m]osquitto_sub' | grep -- "-F" || true)
if [ -n "$OTHER" ]; then
  echo "refusing: a capture subscriber is already running:" >&2
  echo "$OTHER" >&2
  echo "stop it first." >&2
  exit 1
fi
OUT="$(cd "$(dirname "$0")/../logs" && pwd)/${NAME}.log"
STALL_S=45          # $SYS ticks every ~10 s, so 45 s of silence is real silence

printf '# capture %s -> %s\n' "$NAME" "$OUT"
printf '# broker %s ; started %s\n' "$HOST" "$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$OUT"

# --- 1. hold sleep off -----------------------------------------------------
# -i idle sleep, -m disk sleep, -s system sleep; -w ties the assertion to this
# script's lifetime, so it is released the moment the capture stops.
caffeinate -i -m -s -w $$ &
CAFF=$!
printf '# caffeinate held (pid %s)\n' "$CAFF" >> "$OUT"

if pmset -g batt 2>/dev/null | grep -q "Battery Power"; then
  PCT=$(pmset -g batt 2>/dev/null | grep -o '[0-9]*%' | head -1)
  echo "WARNING: on battery ($PCT). caffeinate holds off IDLE sleep, but"      >&2
  echo "         closing the lid still sleeps the machine and the capture"     >&2
  echo "         goes silent. This is what cost the 2026-08-12 CW leg."        >&2
  echo "         Plug in, and leave the lid open."                             >&2
  printf '# WARNING started on battery (%s) — lid must stay open\n' "$PCT" >> "$OUT"
fi

# --- 2. watchdog: timestamp our own silence --------------------------------
# Polls the file from outside the subscriber, so it reports even when
# mosquitto_sub is alive-but-delivering-nothing — the 2026-08-12 failure.
(
  last_size=0; stalled=0
  while : ; do
    sleep 15
    size=$(wc -c < "$OUT" 2>/dev/null | tr -d ' ')
    [ -z "$size" ] && continue
    if [ "$size" = "$last_size" ]; then
      stalled=$((stalled + 15))
      if [ "$stalled" -ge "$STALL_S" ]; then
        printf '# STALL %s no data for %ss\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$stalled" >> "$OUT"
        last_size=$(wc -c < "$OUT" 2>/dev/null | tr -d ' ')
      fi
    else
      [ "$stalled" -ge "$STALL_S" ] && \
        printf '# RESUME %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$OUT"
      stalled=0; last_size=$size
    fi
  done
) &
WATCH=$!

cleanup() {
  printf '# STOPPED %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$OUT"
  kill "$WATCH" "$CAFF" 2>/dev/null
  exit 0
}
trap cleanup INT TERM

# --- 3. subscribe to everything --------------------------------------------
while : ; do
  mosquitto_sub -h "$HOST" -F '%U %t %p' \
      -t 'ngr/#' \
      -t '$SYS/broker/clients/connected' \
      -t '$SYS/broker/messages/dropped' \
      -t '$SYS/broker/uptime' >> "$OUT" 2>/dev/null
  # Only reached if the subscriber exits outright. It normally does NOT — 2.x
  # reconnects internally — so this line is the rare case, and the watchdog
  # above is what catches the common one.
  printf '# RECONNECT %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$OUT"
  sleep 2
done

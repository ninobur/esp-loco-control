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
#   2a. 2026-08-20: DETECTION WAS NOT ENOUGH. The 00:47-01:53 hole of that
#      session was 3,930 s in which NOTHING arrived — not one ngr/# message, not
#      one $SYS tick. The watchdog worked perfectly: it wrote a # STALL line
#      every 15 s and a # RESUME at the end. But the broker and network were
#      fine throughout — other subscribers were querying that same broker live
#      during the hole. The capture's own mosquitto_sub had stopped delivering
#      WITHOUT exiting and WITHOUT reconnecting, which is exactly the condition
#      fix 2 below describes and yet nothing acted on it: an hour of railway
#      went unrecorded while the tool faithfully wrote down that it was
#      recording nothing. A watchdog that only annotates is a smoke alarm with
#      no one home. It now RESTARTS the subscriber, and a shorter keepalive
#      gives the client its own chance to notice first.
#
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
# Keyed on the SUBSCRIBER, and matched on the process NAME rather than on any
# command line. Two earlier versions of this guard searched command lines — first
# for capture.sh, then for mosquitto_sub — and both matched the very shell that
# was launching the capture, because that shell's command line quotes the
# pattern it is searching for. pgrep -x matches the executable's name, which a
# wrapper shell can never satisfy.
OTHER=$(pgrep -x mosquitto_sub || true)
if [ -n "$OTHER" ]; then
  OTHER=$(ps -o pid=,command= -p "$(echo "$OTHER" | tr '\n' ' ')" 2>/dev/null)
  echo "refusing: a capture subscriber is already running:" >&2
  echo "$OTHER" >&2
  echo "stop it first." >&2
  exit 1
fi
OUT="$(cd "$(dirname "$0")/../logs" && pwd)/${NAME}.log"
STALL_S=45          # $SYS ticks every ~10 s, so 45 s of silence is real silence
RESTART_S=90        # ... and 90 s means the subscriber is not coming back on
                    # its own. Kill it; the loop below reconnects from scratch.
                    # Deliberately longer than STALL_S: a stall is worth
                    # recording, a restart is worth doing, and they are not the
                    # same judgement.
PIDFILE="$(mktemp -t ngrcap)"   # where the subscriber publishes its pid

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
  # Silence is measured from the LAST DATA LINE'S OWN TIMESTAMP, never from
  # file growth. 2026-08-20: a growth-based watchdog printed "# RESUME" against
  # an unreachable broker, because the # STALL and # RECONNECT lines it writes
  # are themselves growth — the tool was reading its own noise as traffic. Data
  # lines begin with the broker's %U epoch stamp; comment lines begin with '#'
  # and are skipped, so nothing this script writes can ever look like data.
  stalled=0; since_restart=0; was_stalled=0
  while : ; do
    sleep 15
    last_ts=$(tail -n 2000 "$OUT" 2>/dev/null | grep -v '^#' | tail -n 1 | cut -d' ' -f1 | cut -d. -f1)
    now=$(date +%s)
    case "$last_ts" in
      ''|*[!0-9]*) stalled=$((stalled + 15)) ;;   # no data line in reach: still silent
      *)           stalled=$((now - last_ts)) ;;
    esac
    if [ "$stalled" -ge "$STALL_S" ]; then
      since_restart=$((since_restart + 15))
      if [ "$was_stalled" -eq 0 ] || [ $((stalled % 60)) -lt 15 ]; then
        printf '# STALL %s no data for %ss\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$stalled" >> "$OUT"
      fi
      was_stalled=1
      # Detection is not recovery. Past RESTART_S the subscriber is alive and
      # useless, so kill it and let the loop build a fresh connection. Rate
      # limited to one restart per RESTART_S so a genuinely dead broker gets
      # patient retries rather than a kill storm.
      if [ "$since_restart" -ge "$RESTART_S" ]; then
        SUBPID=$(cat "$PIDFILE" 2>/dev/null)
        if [ -n "$SUBPID" ] && kill -0 "$SUBPID" 2>/dev/null; then
          printf '# RESTART %s killing subscriber pid %s after %ss silent\n' \
                 "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$SUBPID" "$stalled" >> "$OUT"
          kill "$SUBPID" 2>/dev/null
        else
          printf '# RESTART %s no live subscriber to kill after %ss silent\n' \
                 "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$stalled" >> "$OUT"
        fi
        since_restart=0
      fi
    else
      # RESUME only when a real data line is newer than STALL_S — never on our
      # own writes, which is the whole point of timestamping instead of sizing.
      [ "$was_stalled" -eq 1 ] && \
        printf '# RESUME %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$OUT"
      was_stalled=0; since_restart=0
    fi
  done
) &
WATCH=$!

cleanup() {
  printf '# STOPPED %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$OUT"
  SUBPID=$(cat "$PIDFILE" 2>/dev/null)
  [ -n "$SUBPID" ] && kill "$SUBPID" 2>/dev/null
  kill "$WATCH" "$CAFF" 2>/dev/null
  rm -f "$PIDFILE"
  exit 0
}
trap cleanup INT TERM

# --- 3. subscribe to everything --------------------------------------------
while : ; do
  # -k 15: keepalive well under the default 60 s, so the client itself notices
  # a black-holed TCP connection in ~22 s instead of ~90. The 2026-08-20 hole
  # was a connection that never noticed at all.
  mosquitto_sub -h "$HOST" -k 15 -F '%U %t %p' \
      -t 'ngr/#' \
      -t '$SYS/broker/clients/connected' \
      -t '$SYS/broker/messages/dropped' \
      -t '$SYS/broker/uptime' >> "$OUT" 2>/dev/null &
  SUBPID=$!
  echo "$SUBPID" > "$PIDFILE"      # the watchdog needs this to kill it
  wait "$SUBPID"
  # Reached when the subscriber exits on its own OR when the watchdog killed
  # it for going silent. Either way the fix is the same: connect again.
  printf '# RECONNECT %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$OUT"
  sleep 2
done

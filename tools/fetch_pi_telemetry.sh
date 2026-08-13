#!/bin/sh
# Pull NGR telemetry off the Pi to the Mac. Runs nightly from launchd.
#
# WHY THIS EXISTS
# ---------------
# The Pi writes telemetry continuously to its SD card. Two cards failed inside
# 36 hours (2026-08-11 and 2026-08-13), each taking everything not already
# copied off. Anything that lives only on that card is one brownout from gone.
#
# WHY NOT INTO THE REPO
# ---------------------
# field-records/logs/ is git-tracked. On 2026-08-13 a branch switch DELETED the
# active capture file out from under a running capture, which kept writing to a
# deleted inode. A destination that git can rewrite is not a safe destination.
# This writes outside the working tree; curate into field-records/ by hand.
#
# WHY IT SHOUTS WHEN IT FAILS
# ---------------------------
# A backup that fails quietly is worse than none, because the gap looks like
# quiet running. Every run appends an outcome line to fetch.log — OK, or the
# reason it could not. Check it with:  tail ~/ngr-telemetry/fetch.log
#
# Usage:  tools/fetch_pi_telemetry.sh [host]

set -u

DEST="${NGR_TELEM_DEST:-$HOME/ngr-telemetry}"
FETCHLOG="$DEST/fetch.log"
# 3 days of overlap: a couple of missed nights still heal without a full re-pull.
WINDOW_DAYS="${NGR_TELEM_WINDOW:-3}"

mkdir -p "$DEST/pi" || exit 1
stamp() { date -u +%Y-%m-%dT%H:%M:%SZ; }
say()   { printf '%s %s\n' "$(stamp)" "$*" >> "$FETCHLOG"; }

# The Pi answers on .142 normally. The 2026-08-12 rebuild found the wired MAC
# differs from the Wi-Fi radio's, so the router's reservation missed and DHCP
# handed out .73 — and a restored older card will not carry the .142 fix at all.
# Try both rather than assume.
HOST=""
for h in ${1:+$1} 192.168.68.142 192.168.68.73; do
  if ssh -o BatchMode=yes -o ConnectTimeout=8 "$h" true 2>/dev/null; then
    HOST="$h"; break
  fi
done

if [ -z "$HOST" ]; then
  say "FAIL unreachable — tried ${1:+$1 }192.168.68.142 192.168.68.73"
  exit 1
fi

# Record the card and power state alongside the data. On 2026-08-13 the kernel
# logged "Undervoltage detected!" 72 seconds before the card died and nothing
# was listening. If a card fails again, this file says what the rail was doing.
ssh -o BatchMode=yes "$HOST" '
  echo "host=$(hostname) at $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "uptime=$(uptime -p 2>/dev/null)"
  echo "throttled=$(vcgencmd get_throttled 2>/dev/null)"
  echo "card_serial=$(cat /sys/block/mmcblk0/device/serial 2>/dev/null)"
  echo "card_date=$(cat /sys/block/mmcblk0/device/date 2>/dev/null)"
  echo "root_mount=$(mount | grep " / ")"
  echo "runlog=$(systemctl is-active ngr-runlog 2>/dev/null)"
  df -h / 2>/dev/null | tail -1
' > "$DEST/pi/_health_$(date -u +%Y%m%d).txt" 2>/dev/null

# The Pi has no rsync (verified 2026-08-13), so tar over ssh it is. Bounded by
# mtime so this does not re-transfer the whole archive every night.
if ssh -o BatchMode=yes "$HOST" \
      "find NGR/telemetry -type f -mtime -${WINDOW_DAYS} -print0 2>/dev/null \
       | tar czf - --null -T - 2>/dev/null" \
     | tar xzf - -C "$DEST/pi" 2>/dev/null; then
  N=$(find "$DEST/pi" -type f -newermt "-${WINDOW_DAYS} days" 2>/dev/null | wc -l | tr -d ' ')
  SZ=$(du -sh "$DEST/pi" 2>/dev/null | cut -f1)
  say "OK   $HOST — $N file(s) within ${WINDOW_DAYS}d, archive now $SZ"

  # Surface the two states that silently end recording, so a nightly reader of
  # this file learns about them here rather than from a missing run.
  H="$DEST/pi/_health_$(date -u +%Y%m%d).txt"
  grep -q "ro," "$H" 2>/dev/null && say "WARN root filesystem is READ-ONLY — recording is dead"
  grep -q "throttled=0x0$" "$H" 2>/dev/null || say "WARN throttled is not 0x0 — check the supply"
  grep -q "runlog=active" "$H" 2>/dev/null || say "WARN ngr-runlog is not active"
else
  say "FAIL $HOST reachable but the transfer failed"
  exit 1
fi

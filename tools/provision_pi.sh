#!/usr/bin/env bash
#
# Provision a freshly-flashed ngr-pi from the Mac, over SSH.
#
# Written 2026-08-12 after the SD card failed and it turned out nothing in the
# repo described how to rebuild the Pi. Companion to docs/PI_REBUILD_RUNBOOK.md.
#
# Run this from the repo root, AFTER the new card is flashed and the Pi answers
# ssh. It does not touch the card, the bootloader, or the network config — it
# installs packages, copies the app, and installs the systemd unit.
#
# Safe to re-run: every step is idempotent.
#
#   ./tools/provision_pi.sh                     # uses defaults below
#   PI=david@192.168.68.9 ./tools/provision_pi.sh   # different address
#   APP=server/ngr_app_v1_10_9.py ./tools/provision_pi.sh   # different version
#
set -euo pipefail

PI="${PI:-david@192.168.68.142}"
APP="${APP:-server/ngr_app_v1_10_11.py}"

say() { printf '\n\033[1m== %s\033[0m\n' "$1"; }

[ -f "$APP" ] || { echo "No such app file: $APP (run from the repo root)" >&2; exit 1; }
[ -f server/ngr-app.service ] || { echo "Missing server/ngr-app.service" >&2; exit 1; }

say "Checking $PI is reachable"
ssh -o BatchMode=yes -o ConnectTimeout=10 "$PI" 'echo "connected: $(hostname) $(hostname -I)"'

say "Power health — the reason the last card may have died"
# Nonzero means the Pi is browning out. Bit 0 = undervolted now, bit 16 = since
# boot. If this is nonzero, replace the supply before trusting the rebuild.
ssh "$PI" 'vcgencmd get_throttled || echo "(vcgencmd unavailable)"'

say "Installing packages"
# mosquitto-clients is NOT optional: the dashboard shells out to mosquitto_sub
# for the CAL logger.
ssh "$PI" 'sudo apt-get update -qq && sudo apt-get install -y -qq \
    mosquitto mosquitto-clients python3-flask python3-paho-mqtt'

say "Configuring the broker for LAN clients"
# The locomotives and the Mac all connect to this broker over the network, so it
# must listen on more than loopback.
ssh "$PI" 'printf "listener 1883 0.0.0.0\nallow_anonymous true\n" \
    | sudo tee /etc/mosquitto/conf.d/ngr.conf >/dev/null
  sudo systemctl enable --now mosquitto
  sudo systemctl restart mosquitto'

say "Creating directories the app expects"
ssh "$PI" 'mkdir -p /home/david/NGR/logs /home/david/NGR/telemetry/runs'

say "Copying the dashboard ($APP)"
scp -q "$APP" "$PI:/home/david/ngr_app.py.new"
ssh "$PI" 'mv /home/david/ngr_app.py.new /home/david/ngr_app.py'

say "Copying the continuous logger"
# Measured 2026-08-12: ~1.6 KB/s with a train running, so a few tens of GB a
# year at most. Nothing on a 128 GB card. Decision 0028 supersedes 0027, which
# had refused this on write-wear grounds that the measurement did not support.
scp -q server/ngr_runlog.py "$PI:/home/david/NGR/telemetry/ngr_runlog.py"

say "Installing the systemd units"
scp -q server/ngr-app.service "$PI:/tmp/ngr-app.service"
scp -q server/ngr-runlog.service "$PI:/tmp/ngr-runlog.service"
ssh "$PI" 'sudo mv /tmp/ngr-app.service /etc/systemd/system/ngr-app.service
  sudo mv /tmp/ngr-runlog.service /etc/systemd/system/ngr-runlog.service
  sudo systemctl daemon-reload
  sudo systemctl enable ngr-app ngr-runlog
  sudo systemctl restart ngr-app ngr-runlog'

say "Verifying"
sleep 6
ssh "$PI" 'for s in ngr-app ngr-runlog mosquitto; do printf "%-12s %s\n" "$s" "$(systemctl is-active $s)"; done
  echo "--- log growing? ---"
  ls -la /home/david/NGR/telemetry/all_*.log 2>/dev/null || echo "(no log yet — normal if nothing is publishing)"
  df -h / | tail -1'

host="${PI#*@}"
# The app binds 8080, not 80 (ngr_app_v1_10_11.py: app.run(host="0.0.0.0",
# port=8080)). It answers 302 on / and redirects to /console, so follow it.
code=$(curl -sSL -o /dev/null -w '%{http_code}' --max-time 10 "http://${host}:8080/" || echo "no answer")
echo "dashboard http://${host}:8080/console -> ${code}"

say "Retained topics on the rebuilt broker"
# Expect both locomotives' retained bootid if they are powered up. A silent
# result here with locos running means they cannot reach the broker.
ssh "$PI" "timeout 5 mosquitto_sub -h 127.0.0.1 -t 'ngr/#' -v || true" | head -20

say "Done"
cat <<'EOF'
Remaining, by hand:
  - Confirm the address is still 192.168.68.142. The loco firmware has it
    compiled in (firmware/QUORUM/QUORUM.ino:370) and cannot find the broker
    anywhere else.
  - If get_throttled above was nonzero, replace the power supply. That is the
    likely root cause, and a new card will not survive it.
EOF

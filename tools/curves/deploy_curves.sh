#!/usr/bin/env bash
#
# Deploy the magnet-curve database (decision 0072, proposed) to ngr-pi.
#
# WRITTEN 2026-09-02, NOT YET RUN AGAINST THE PI. The Pi is the operator's; he
# runs this, or asks for it to be run. It copies one Python file, installs a
# oneshot service and a five-minute timer, and starts the first pass. Nothing
# here touches the broker, the runlog, the dashboard, or any locomotive.
#
# Safe to re-run: every step is idempotent, and the ingest itself only appends.
#
#   ./tools/curves/deploy_curves.sh                      # defaults below
#   PI=david@192.168.68.73 ./tools/curves/deploy_curves.sh
#
# Also called as a step of tools/provision_pi.sh, so a rebuilt card gets it.
#
set -euo pipefail

PI="${PI:-david@192.168.68.142}"
cd "$(dirname "$0")/../.."      # the repo root, wherever this was invoked from

say() { printf '\n\033[1m== %s\033[0m\n' "$1"; }

for f in tools/curves/decode_curves.py server/ngr-curves.service server/ngr-curves.timer; do
  [ -f "$f" ] || { echo "Missing $f (run from the repo root)" >&2; exit 1; }
done

say "Checking $PI"
ssh -o BatchMode=yes -o ConnectTimeout=10 "$PI" \
  'echo "connected: $(hostname)"; python3 --version; python3 -c "import sqlite3; print(\"sqlite\", sqlite3.sqlite_version)"'

say "Copying the decoder"
ssh "$PI" 'mkdir -p /home/david/NGR/curves'
scp -q tools/curves/decode_curves.py "$PI:/home/david/NGR/curves/decode_curves.py.new"
ssh "$PI" 'mv /home/david/NGR/curves/decode_curves.py.new /home/david/NGR/curves/decode_curves.py'

say "Installing the service and timer"
scp -q server/ngr-curves.service server/ngr-curves.timer "$PI:/tmp/"
ssh "$PI" 'sudo mv /tmp/ngr-curves.service /tmp/ngr-curves.timer /etc/systemd/system/
  sudo systemctl daemon-reload
  sudo systemctl enable --now ngr-curves.timer'

say "First pass — backfills every day on the card; minutes, not seconds"
ssh "$PI" 'sudo systemctl start ngr-curves.service
  systemctl status ngr-curves.service --no-pager 2>/dev/null | tail -4
  python3 /home/david/NGR/curves/decode_curves.py stats /home/david/NGR/curves/curves.sqlite | tail -6
  ls -la /home/david/NGR/curves/
  systemctl list-timers ngr-curves.timer --no-pager | head -2'

say "Done"
cat <<'EOF'
The database is derived data: the all_ logs are the record, and this file is
rebuilt from the Mac mirror in seconds. It is deliberately NOT under
NGR/telemetry, so the nightly pull does not copy it back and forth.

To export for tools/two_sided/ on the Mac, from the mirror:
  python3 tools/curves/decode_curves.py ingest ~/ngr-telemetry/pi/NGR/telemetry /tmp/curves.sqlite
  python3 tools/curves/decode_curves.py export /tmp/curves.sqlite passages.txt
EOF

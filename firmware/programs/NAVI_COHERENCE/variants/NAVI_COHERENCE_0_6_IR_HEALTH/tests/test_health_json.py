#!/usr/bin/env python3
"""Validate actual C++ snprintf output, including valid zero vs unavailable."""
import json
import subprocess
import sys

result = subprocess.run([sys.argv[1]], check=True, text=True, capture_output=True)
rows = [json.loads(line) for line in result.stdout.splitlines()]
assert len(rows) == 13
assert all(row["distance_bounds"] == "UNVALIDATED" and row["health_revision"] == "CAL0_FIX1" for row in rows)
assert rows[0]["health"] == "INADEQUATE_CONTRAST" and rows[0]["accepted"] == 1
assert rows[0]["calibration_id"] == 0 and rows[0]["pulses"] == 119
assert rows[0]["fresh"] == 1 and rows[0]["epoch_active"] == 0
assert rows[1]["health"] == "HEALTHY" and rows[1]["calibration_id"] == 0
payload_count = len(rows)
rows = rows[2:]
assert all(row["shadow"] == 1 and row["ref_basis"] == "NAV05_ACCEPTED" for row in rows)
assert rows[0]["health"] == "NO_SOURCE" and rows[0]["epoch_mm"] is None
assert rows[1]["health"] == "HEALTHY" and rows[1]["readiness"] == "PRIMING"
assert rows[2]["delta_pulses"] == 0 and rows[2]["distance_mm"] == 0
assert rows[2]["ref_valid"] == 1 and rows[2]["epoch"] == 1
assert rows[3]["health"] == "LINK_STALE" and rows[3]["delta_pulses"] is None
assert rows[3]["distance_mm"] is None and rows[3]["epoch_mm"] is None
assert rows[4]["epoch_mm"] == 0 and rows[4]["distance_mm"] is None
assert rows[5]["ref_ms"] == 1100 and rows[5]["ref_mm"] == 42
assert rows[5]["distance_mm"] == 193.04 and rows[5]["ref_alignment_us"] == 0
assert rows[6]["epoch"] == 2 and rows[6]["ref_mm"] == 43
assert rows[7]["epoch"] == 4 and rows[7]["queue_gaps"] == 1
assert rows[8]["health"] == "PACKET_INVALID" and rows[8]["readiness"] == "UNAVAILABLE"
assert rows[9]["epoch"] == 6 and rows[9]["ref_valid"] == 0
assert rows[10]["pulses"] == 2**64 - 1
print(result.stderr.strip())
print(f"PASS {payload_count} real health JSON payloads; maximum {max(map(len, result.stdout.splitlines()))} bytes")

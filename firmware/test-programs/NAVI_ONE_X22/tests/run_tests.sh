#!/usr/bin/env bash
# X22 host tests. Two files, deliberately: the synthetic gates assert the
# properties the detector claims, and the replay checks the implementation
# against the corpus the architecture was argued from. Neither is acquisition
# coverage; both run in under a second.
set -e
cd "$(dirname "$0")"
CXX=${CXX:-g++}
echo "=== payload bounds ==="
python3 check_payload_bounds.py

echo
echo "=== gate_excursion ==="
$CXX -O2 -std=c++17 -Wall -o /tmp/x22_gate gate_excursion.cpp
/tmp/x22_gate
echo
echo "=== replay_x22_20260915 ==="
$CXX -O2 -std=c++17 -Wall -o /tmp/x22_replay replay_x22_20260915.cpp
/tmp/x22_replay fixtures_otto_20260915.txt | tail -14

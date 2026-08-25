#!/usr/bin/env bash
# QUORUM TRACE host tests — INVESTIGATORY / UNAPPROVED.
# Runs only the tests this task added (QuorumTrace.h's ring/batch engine,
# g++; the QUORUM.ino trace source audit and the qt_decode.py host tools,
# python3). The pre-existing fixtures/ directory in this same folder belongs
# to a separate, unrelated effort (no runner for it exists yet) and is left
# untouched.
set -e
cd "$(dirname "$0")"

echo "== trace ring/batch engine (g++) =="
g++ -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter \
    -o /tmp/qt_trace_tests test_quorum_trace.cpp
/tmp/qt_trace_tests

echo
echo "== QUORUM.ino trace source audit (python3) =="
python3 test_quorum_trace_authority.py

echo
echo "== trace host tools: decoder / receiver (python3) =="
python3 test_qt_decoder.py

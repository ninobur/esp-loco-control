# Sweep scripts — the provenance of every fixture start position

The 1.16R review-round fixtures cite sweep counts ("39 of 855 CW straddles
fire", "exactly 4 of 26 polarity-eligible starts expose finding 2", "53
strict CCW hits, none crossing the wrap", "513 out-of-fence runs, no lag-6
false fit"). CODEX's completeness check correctly objected that those
numbers lived only in the session record and could not be re-run. These are
the scripts. Each takes a harness binary path as argv (build one with the
same command run_suite.py uses):

    clang++ -std=c++17 -Wno-format -I shim -I .. -o /tmp/qh ../harness.cpp

| script | what it swept | headline number |
|---|---|---|
| `sweep_rescue.py` | bare same-pole insertion, 855 runs | 0 rescues — a lone insertion adopts −1 before the hard bound; the straddle shape is REQUIRED |
| `sweep_rescue2.py` | miss-then-insertion straddle, CW, 855 runs | 39 rescue hits; start 36 pinned as `syn_adv_rescue_straddle` |
| `sweep_ccw_strict.py` | the straddle in CCW with final state/dir/mm all verified | 53 hits, none crossing the 170/000 wrap; start 50 pinned |
| `sweep_excluded.py` | adopt-correct, break-in-validation, restore shapes | 20 excluded-candidate terminals; (56,4,1,2) pinned |
| `sweep_false_rescue.py` | phantom-pair (−2, outside fence) hunting a lag-6 false fit | 0 exposures in 513 runs |
| `sweep_commit_gate.py` | finding-2 shape vs BOTH binaries (needs the pre-fix harness from 6ebb3b1 as argv 2) | 4 of 26 polarity-eligible starts expose the defect; start 7 pinned |
| `probe_rescue.py` | one straddle in detail + the lag-run map fact | board to eval 12 margin ≤ 1; lag 1–5 max run 6 |
| `probe_excluded.py` | the pinned excluded-candidate run, suffix lengths per candidate | excluded +1 fits all 12; every other candidate ≤ 2 |
| `probe_slow_phantom_family.py` | the SUCCESSOR_SUSPECT chain at start 7 | CONJUNCTION → SUCCESSOR_SUSPECT → CONJUNCTION → SUCCESSOR_FITS_PHANTOM |

The DNA arrays embedded here are copies; `run_suite.py` proves on every run
that the firmware's NGR_DNA1 matches `make_synthetic.py`'s copy, and the
mapfacts verifier additionally confirmed these match the firmware. The
sweeps are evidence generators, not part of the green suite — they are slow
(hundreds of harness invocations) and run on demand.

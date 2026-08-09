# IR_SCOPE synthetic test suite

The generators that produced every replay verification cited in
`docs/IR_DEV_REC/2026-08-09_IR_SCOPE_BUILD.md` and the PR #3 review
rounds. Each writes an IR_SCOPE-format CSV; run `IR_SCOPE_Replay.py`
over the output and compare against the expectations below. These are
synthetic-waveform tests of the replay tool only — they prove replay
semantics, not sensor behaviour.

All scenarios use a 7-spoke wheel model, envelope 1000/3000 (span 2000,
thrHigh 2333, thrLow at 1/3 = 1666), and generate recorded detector
flags with the firmware's own 1/3 semantics (including the 2500 ms
latch discard where noted), so the replay's semantics self-check should
match ~100% in every scenario.

```bash
cd firmware/test-programs/IR_SCOPE/tests
python3 gen_synthetic.py synth.csv          # mixed merging
python3 make_gap_csv.py synth.csv synth_gap.csv   # + transport gap & stall
python3 gen_varspeed.py synth_var.csv       # speed ramp, no merging
python3 gen_latch.py synth_latch.csv        # latch plateau + open end
python3 gen_uniform.py synth_uni.csv        # uniform merging + rev markers
python3 gen_repro7.py repro7.csv repro7     # CODEX debounce-horizon repro
python3 gen_repro7.py repro9.csv repro9     # quiet-resync repro
python3 ../IR_SCOPE_Replay.py <file>
```

| scenario | what it proves | expected (key rows) |
|---|---|---|
| `gen_synthetic.py` — every 3rd trough shallow (1900) | mixed merging detected; 0.50 recovers | 1/3: 38 pulses, 18 merged, mpk 18, p/rev-int 4.71, p/7pk 4.75 · 0.50: 56 pulses, 0 merged, 7.00/7.00 |
| `make_gap_csv.py` — cuts a transport GAP (mid-pulse both edges) + 3-slot MISSED out of `synth.csv` | gap → per-candidate resync (no seeding, no spurious rises); MISSED carries state | every candidate: open 1, unkn 1; validation ~35/35 rises, 34/34 falls outside declared resync spans |
| `gen_varspeed.py` — period ramps 130→300 ms, all troughs deep | local base immune to speed change | all candidates: 0 merged, 0 short, p/rev-int 7.00, p/7pk 7.00 |
| `gen_latch.py` — 3.5 s plateau at 2100, record ends mid-pulse | latch discard visible; overlay outcome bands | 1/3, 0.40, 0.50: latch 1 · 0.60: latch 0 (falls at the plateau) · open 1 everywhere |
| `gen_uniform.py` — troughs alternate deep/shallow, `rev` marker each 7 spokes | uniform merging invisible to intervals, exposed structurally and absolutely | 1/3: p/rev-int 7.00 **but** p/7pk 3.50, mpk 28/28, NOTE fires, p/rev-mk 4.0 · 0.50: 7.00/7.00/7.0 · peaks per marked revolution 7.0 |
| `gen_repro7.py repro7` — GAP, low @100, high @105, low @110 | debounce horizon: ambiguous post-gap pulse must not be attributed | every candidate: 1 pre-gap pulse only, unkn 1 — no 105–110 episode |
| `gen_repro7.py repro9` — post-gap stretch parked mid-band, then low | quiet resync spans counted and rendered | every candidate: unkn 1 |

If a change to `IR_SCOPE_Replay.py` shifts any of these, either the
change is wrong or the expectation table above must be updated in the
same commit — never silently.

# Synthetic NGR test track

The track is the complete 171-marker CW map and surveyed `spacingMm[]` table
from the historical QUORUM source. It totals 52,150 mm. A separate later
document cites 52,264 mm; this simulator preserves the actual recovered table
rather than silently reconciling that discrepancy. Station names are
retained only as geographic markers; station stopping control is not simulated.

The empirical library uses CPR's Otto CW complete-event `shape64` curves,
including their recorded amplitude and duration. Toby has no completed CPR
events, so its historical RVM CSV provides amplitude/duration while it borrows
an explicitly labelled Otto observed shape. This is a practical signal-set
variation, not a claim that the waveform is Toby-native.

The six deterministic levels use seed 20261021. Drift/noise, timing variance,
missing signals, false apparent crossings, and direct-over-magnet dwell are
experimental assumptions unless a source provenance record says otherwise.
Truth is generated solely from simulated continuous position crossing the map;
it is written after each journey and is not passed to any candidate.

Run:

```sh
python3 research/navi_wolf_pack/synthetic_competition.py
python3 -m unittest discover -s research/navi_wolf_pack -p 'test_*.py' -v
```

# NAVI_WOLF_PACK

Offline research only. This directory contains the competition contract, frozen
dataset manifest, scoring protocol, and deliberately named experimental
reconstructions. It cannot flash or command a locomotive.

## Reproduce the currently valid checks

```sh
python3 -m unittest discover -s research/navi_wolf_pack -p 'test_*.py' -v
cd research/NAVI_CPR_0_1
python3 check_package.py verify
python3 portable.py restore
python3 experiment.py evaluate
```

The CPR commands reproduce its own frozen recognition result; they are **not**
a competition win. The final geographic competition is intentionally blocked
until the manifest contains independent ground truth and common raw sessions.

## Architectural boundary

The replay interface permits each model to preserve its own uncertainty and
decision timing. It forbids observation reordering and has no future-sample
access. `DNAExperimental`, `QuorumExperimental`, and `SimplifiedExperimental`
are controls for exercising that interface only; see the candidate inventory
before interpreting their output.

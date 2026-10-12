# Review probes: NAVI_EWO_0_2 and NAVI_EWO_0_3 (October 11, 2026)

Read-only host probes and replays supporting
[`docs/NAVI_EWO_0_2_INDEPENDENT_REVIEW_20261011.md`](../../../docs/NAVI_EWO_0_2_INDEPENDENT_REVIEW_20261011.md) and
[`docs/NAVI_EWO_0_3_INDEPENDENT_REVIEW_20261011.md`](../../../docs/NAVI_EWO_0_3_INDEPENDENT_REVIEW_20261011.md).
They are review evidence, not regression tests or firmware. They are synthetic host
scenarios: they show how the committed code behaves under stated inputs, not how the
railway behaves.

| File | Target commit | Reproduces |
|---|---|---|
| `ewo_0_2_probe.cpp` | `cbdfe94` (`codex/navi-ewo-0-2-20261010`) | P1 mid-interval declaration never confirms; P2 DNA resolution one marker behind; P3 chatter |
| `ewo_0_2_event_switch.cpp` | `cbdfe94` | "duplicate case value" ×2: the event-name switch from `NAVI_EWO_0_2.ino` cannot compile |
| `ewo_0_2_credibility_replay.py` | `cbdfe94` rule, October 9 log | Questionable-span rates: 0% / 33% / 29% (healthy control) / 63% |
| `ewo_0_3_probe.cpp` | `16c65f6` (`codex/navi-ewo-0-3-complete`) | A/C missed magnet → permanent pending; D spurious resolves; E stalled threshold field false-advances |

Build each C++ probe against a detached checkout of its target commit:

```sh
git worktree add --detach /tmp/ewo02 cbdfe94
c++ -std=c++17 -I/tmp/ewo02/firmware/programs/NAVI_EWO_0_2 ewo_0_2_probe.cpp -o p02 && ./p02
c++ -std=c++17 -I/tmp/ewo02/firmware/programs/NAVI_EWO_0_2 ewo_0_2_event_switch.cpp   # expected to fail
git worktree add --detach /tmp/ewo03 16c65f6
c++ -std=c++17 -I/tmp/ewo03/firmware/programs/NAVI_EWO_0_3 ewo_0_3_probe.cpp -o p03 && ./p03
python3 -I ewo_0_2_credibility_replay.py all_20261009.log
```

The October 9 log is `field-records/logs/20261009_MM045_EVENING/all_20261009.log.gz` on
`codex/navi-cruise-stop-20261009`, gunzipped. Probes were checked with GCC 13.3.0 on
Linux. No ESP32 build was performed.

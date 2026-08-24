#!/usr/bin/env python3
"""hwt_gate_replay_continuity_comparison.py — before/after comparison showing
exactly what depended on the withdrawn continuity gate.

INVESTIGATORY. Not part of the corrected default pipeline (hwt_gate_replay.py
never calls the legacy path on its own) -- this script exists solely to
quantify the correction: run the SAME candidate events through the corrected
pipeline (continuity plays no part) and through the withdrawn legacy pipeline
(continuity gated REJECT_SPIKE at max_ratio=0.5, dead_zone=20 -- the exact
values that commit 3fa8208 shipped as defaults), and report every event whose
disposition differs.

    python3 tools/hwt_gate_replay_continuity_comparison.py tools/manifests/grillers.json
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hwt_gate_replay as G   # noqa: E402
from quorum_map import QuorumMap   # noqa: E402

LEGACY_DEAD_ZONE = 20.0
LEGACY_MAX_RATIO = 0.5


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("manifest")
    ap.add_argument("--quorum-ino", default="firmware/QUORUM/QUORUM.ino")
    ap.add_argument("--min-duration-ms", type=float, default=40.0)
    ap.add_argument("--min-abs-flux", type=float, default=300.0)
    ap.add_argument("--out", help="CSV of every event whose disposition differs")
    args = ap.parse_args()

    manifest = G.RunManifest(args.manifest)
    qmap = QuorumMap(args.quorum_ino)
    capture_dir = os.path.dirname(os.path.abspath(args.manifest))
    capture_path = manifest.capture
    if not os.path.isabs(capture_path) and not os.path.exists(capture_path):
        alt = os.path.join(capture_dir, capture_path)
        if os.path.exists(alt):
            capture_path = alt

    events_new, _ctx = G.build_acquisition_events(
        capture_path, manifest, continuity_dead_zone=LEGACY_DEAD_ZONE)
    events_new, _ = G.replay(events_new, qmap, manifest,
                             min_duration_ms=args.min_duration_ms,
                             min_abs_flux=args.min_abs_flux,
                             legacy_continuity_max_ratio=None)   # corrected: continuity unused

    events_old, _ctx = G.build_acquisition_events(
        capture_path, manifest, continuity_dead_zone=LEGACY_DEAD_ZONE)
    events_old, _ = G.replay(events_old, qmap, manifest,
                             min_duration_ms=args.min_duration_ms,
                             min_abs_flux=args.min_abs_flux,
                             legacy_continuity_max_ratio=LEGACY_MAX_RATIO)   # withdrawn gate, reproduced

    print("continuity-removal comparison: %s" % args.manifest)
    print("  capture   %s" % capture_path)
    print("  legacy parameters reproduced: dead_zone=%.1f max_ratio=%.2f"
         % (LEGACY_DEAD_ZONE, LEGACY_MAX_RATIO))
    print("  events    %d" % len(events_new))
    print()

    by_disp_new, by_disp_old = {}, {}
    for e in events_new:
        by_disp_new[e["disp_final"]] = by_disp_new.get(e["disp_final"], 0) + 1
    for e in events_old:
        by_disp_old[e["disp_final"]] = by_disp_old.get(e["disp_final"], 0) + 1

    print("  %-32s %8s %8s" % ("disposition", "OLD", "NEW"))
    for name in G.DISPOSITIONS:
        old_n, new_n = by_disp_old.get(name, 0), by_disp_new.get(name, 0)
        marker = "  <-- changed" if old_n != new_n else ""
        print("  %-32s %8d %8d%s" % (name, old_n, new_n, marker))

    changed = []
    for e_old, e_new in zip(events_old, events_new):
        assert e_old["event_id"] == e_new["event_id"]
        if e_old["disp_final"] != e_new["disp_final"]:
            changed.append((e_old, e_new))

    print()
    print("  events whose disposition CHANGED when continuity was removed: %d / %d (%.1f%%)"
         % (len(changed), len(events_new), 100.0 * len(changed) / len(events_new) if events_new else 0))
    print("  every one of these changed FROM REJECT_SPIKE (continuity was the only gate")
    print("  that could move an event OUT of REJECT_SPIKE by being removed) TO whatever")
    print("  the corrected pipeline decides using duration/flux/timing/polarity alone:")
    to_counts = {}
    for e_old, e_new in changed:
        to_counts[e_new["disp_final"]] = to_counts.get(e_new["disp_final"], 0) + 1
    for name, n in sorted(to_counts.items(), key=lambda kv: -kv[1]):
        print("    -> %-32s %d" % (name, n))

    if args.out:
        import csv
        cols = ["event_id", "phys_duration_ms", "phys_integrated_abs_flux",
               "det_continuity_ratio", "old_disposition", "new_disposition"]
        with open(args.out, "w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=cols)
            w.writeheader()
            for e_old, e_new in changed:
                w.writerow({
                    "event_id": e_new["event_id"],
                    "phys_duration_ms": e_new["phys_duration_ms"],
                    "phys_integrated_abs_flux": e_new["phys_integrated_abs_flux"],
                    "det_continuity_ratio": e_new["det_continuity_ratio"],
                    "old_disposition": e_old["disp_final"],
                    "new_disposition": e_new["disp_final"],
                })
        print("\n  wrote %s (%d rows)" % (args.out, len(changed)))

    return 0


if __name__ == "__main__":
    sys.exit(main())

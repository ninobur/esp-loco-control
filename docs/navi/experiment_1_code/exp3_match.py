#!/usr/bin/env python3
"""For each independently-confirmed segment (known real waveform peak),
check how each Experiment-2 window configuration classified it: clean
single piece, fragmented into multiple pieces, merged with a neighbor,
or missed entirely."""
import json, sys

def load_excursions(path):
    return json.load(open(path))["excursions"]

def classify(segments, excursions, all_peaks):
    out = []
    for s in segments:
        wf = s["waveform"]
        if wf is None:
            out.append({**{k: s[k] for k in ("mm_from","mm_to","pwm_at_arrival","v_mm_s")},
                        "verdict": "no_independent_waveform"})
            continue
        peak_t = wf["peak_t_us"]
        # Exact excursion span only (no padding) -- real inter-magnet gaps
        # at PWM~90 are only ~1.2-1.5s, so any time-based buffer risks
        # manufacturing false "merges" between genuinely separate pieces.
        nearby = [e for e in excursions
                  if e.get("curve_start_t_us_est", 1e18) <= peak_t <= e.get("curve_end_t_us", -1e18)]
        n_other_peaks_covered = 0
        for e in nearby:
            s0, e0 = e.get("curve_start_t_us_est"), e.get("curve_end_t_us")
            n_other_peaks_covered += sum(1 for p in all_peaks if p != peak_t and s0 <= p <= e0)
        if len(nearby) == 0:
            verdict = "missed"
        elif n_other_peaks_covered > 0:
            verdict = "merged_with_neighbor"
        elif len(nearby) == 1 and nearby[0].get("line_segment_n", 0) > 0:
            dep = nearby[0].get("peak_departure")
            ok_amp = dep is not None and dep == dep and abs(abs(dep) - abs(wf["peak_departure"])) < 0.4 * abs(wf["peak_departure"])
            verdict = "clean" if ok_amp else "clean_but_amplitude_off"
        else:
            verdict = f"fragmented_into_{len(nearby)}"
        out.append({"mm_from": s["mm_from"], "mm_to": s["mm_to"], "pwm": s["pwm_at_arrival"],
                     "v_mm_s": round(s["v_mm_s"], 1), "wf_duration_s": round(wf["duration_s"], 3),
                     "wf_peak_dep": round(wf["peak_departure"], 1), "verdict": verdict,
                     "n_pieces_nearby": len(nearby)})
    return out

if __name__ == "__main__":
    seg_path = sys.argv[1]
    label = sys.argv[2]
    window_files = sys.argv[3:]  # pairs: windowMs:path
    segments = json.load(open(seg_path))
    all_peaks = [s["waveform"]["peak_t_us"] for s in segments if s["waveform"]]
    print(f"=== {label}: {len(segments)} segments, {len(all_peaks)} with independent waveform ===")
    for wf_arg in window_files:
        wms, path = wf_arg.split(":", 1)
        excursions = load_excursions(path)
        results = classify(segments, excursions, all_peaks)
        counts = {}
        for r in results:
            counts[r["verdict"]] = counts.get(r["verdict"], 0) + 1
        print(f"-- window={wms}ms -- {counts}")
        for r in results:
            print("   ", r)

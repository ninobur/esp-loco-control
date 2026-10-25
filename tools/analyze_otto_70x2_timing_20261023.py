#!/usr/bin/env python3
"""Read-only Otto 70x2 onset timing analysis for the 2026-09-09 survey.

`mm/wave.t` is the firmware's monotonic timestamp when its 38-count event
opened.  This tool decodes the saved raw, baseline-relative waveform and
derives a different timestamp: the first two consecutive stored 1-ms samples
at or above 70 counts in the opening polarity.  It does not call `t` an onset,
and never uses RTB or MQTT publication time.
"""
import argparse, base64, collections, csv, gzip, json, re, statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCES = (("CW", ROOT / "field-records/logs/20260909_survey/otto_1_13X_cw_waveforms.log.gz"),
           ("CCW", ROOT / "field-records/logs/20260909_survey/otto_1_13X_ccw_waveforms.log.gz"))

def median(x): return statistics.median(x)

def spacing():
    # QUORUM 1.13X used the same frozen 171-marker surveyed route; this is the
    # repository copy retaining that spacing table.
    text = (ROOT / "firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_5_AUTO_ENABLED/RouteMap.h").read_text()
    block = re.search(r"ROUTE_SPACING_MM\[ROUTE_N\] = \{(.*?)\};", text, re.S)
    out = [int(x) for x in re.findall(r"\d+", block.group(1))]
    assert len(out) == 171 and sum(out) == 52150
    return out

def wave_onsets(direction, path):
    """Return only complete, admitted, labelled raw waveforms with a 70x2 open."""
    rows, excluded = [], collections.Counter()
    with gzip.open(path, "rt") as source:
        for line in source:
            fields = line.rstrip("\n").split("\t", 2)
            if len(fields) != 3 or not fields[1].endswith("/mm/wave"):
                continue
            try: event = json.loads(fields[2])
            except ValueError: excluded["bad_json"] += 1; continue
            if event.get("rej") != 0: excluded["not_admitted"] += 1; continue
            if event.get("mm") is None: excluded["no_map_label"] += 1; continue
            if event.get("tr"): excluded["truncated"] += 1; continue
            if event.get("clip"): excluded["clipped"] += 1; continue
            raw = base64.b64decode(event["d"])
            if len(raw) != event["n"]: excluded["bad_sample_count"] += 1; continue
            samples = [(x - 128) * event.get("sc", 1) for x in raw]
            sign = 1 if event["pol"] == "N" else -1
            index = next((i for i in range(len(samples)-1)
                          if sign*samples[i] >= 70 and sign*samples[i+1] >= 70), None)
            if index is None: excluded["no_70x2"] += 1; continue
            # WAVE_PRE samples are before the firmware's 38-count open at t.
            # The sampler is configured at 1 ms; scheduler jitter is not recorded.
            offset = index - event.get("pre", 12)
            rows.append(dict(direction=direction, mm=event["mm"], polarity=event["pol"],
                             pwm_open=event["pwm"], event_open_ms=event["t"],
                             onset_ms=event["t"] + offset, onset_offset_ms=offset,
                             waveform_samples=len(samples), waveform=event["d"]))
    return rows, excluded

def direct_intervals(onsets, route):
    direction = 1 if onsets[0]["direction"] == "CW" else -1
    out, rejected = [], collections.Counter()
    circuit = 0
    for left, right in zip(onsets, onsets[1:]):
        if (right["mm"] - left["mm"]) % 171 != direction % 171:
            rejected["not_adjacent_mapped_labels"] += 1; continue
        if right["mm"] == onsets[0]["mm"]: circuit += 1
        dt_ms = right["onset_ms"] - left["onset_ms"]
        if not (85 <= left["pwm_open"] <= 95 and 85 <= right["pwm_open"] <= 95):
            rejected["not_pwm_90"] += 1; continue
        if not (500 <= dt_ms <= 3000):
            rejected["outside_clean_transit_window"] += 1; continue
        distance = route[left["mm"] if direction > 0 else (left["mm"] - 1) % 171]
        out.append(dict(direction=left["direction"], circuit=circuit, source_mm=left["mm"],
                        destination_mm=right["mm"], distance_mm=distance, dt_ms=dt_ms,
                        pace_ms_per_mm=dt_ms/distance, pwm_source=left["pwm_open"],
                        pwm_destination=right["pwm_open"], source_polarity=left["polarity"],
                        destination_polarity=right["polarity"], source_onset_ms=left["onset_ms"],
                        destination_onset_ms=right["onset_ms"]))
    return out, rejected

def summarize(intervals):
    by_circuit = collections.defaultdict(list)
    for r in intervals: by_circuit[r["circuit"]].append(r)
    scale = {c: median([x["pace_ms_per_mm"] for x in rows]) for c, rows in by_circuit.items()}
    by_mm = collections.defaultdict(list)
    for r in intervals:
        r["relative_time"] = r["pace_ms_per_mm"] / scale[r["circuit"]]
        by_mm[r["source_mm"]].append(r)
    profile = []
    for mm, rows in sorted(by_mm.items()):
        rel = [x["relative_time"] for x in rows]; dts = [x["dt_ms"] for x in rows]
        sd = statistics.stdev(rel) if len(rel) > 1 else None
        profile.append(dict(source_mm=mm, destination_mm=rows[0]["destination_mm"],
                            distance_mm=rows[0]["distance_mm"], n=len(rows),
                            onset_dt_typical_ms=median(dts),
                            typical_relative_time=median(rel), relative_sd=sd,
                            relative_min=min(rel), relative_max=max(rel),
                            candidate_upper_3sigma=(median(rel)+3*sd if sd is not None else None),
                            onset_dt_min_ms=min(dts), onset_dt_max_ms=max(dts)))
    return profile, scale

def prospective(intervals):
    """Sequential circuit holdout; no later circuits or target dt enter a prediction."""
    rows = []
    for held in sorted(set(x["circuit"] for x in intervals)):
        training = [x for x in intervals if x["circuit"] < held]
        test = [x for x in intervals if x["circuit"] == held]
        if not training: continue
        profile, scales = summarize(training)
        factor = {x["source_mm"]: x["typical_relative_time"] for x in profile}
        upper = {x["source_mm"]: x["candidate_upper_3sigma"] for x in profile}
        history = []
        for x in test:
            usable = [q for q in history if q["source_mm"] in factor][-8:]
            if len(usable) == 8 and x["source_mm"] in factor:
                uniform = median([q["pace_ms_per_mm"] for q in usable]) * x["distance_mm"]
                pace = median([q["pace_ms_per_mm"] / factor[q["source_mm"]] for q in usable])
                geographic = pace * factor[x["source_mm"]] * x["distance_mm"]
                rows.append(dict(direction=x["direction"], circuit=held, source_mm=x["source_mm"],
                                 actual_ms=x["dt_ms"], uniform_ms=uniform, geographic_ms=geographic,
                                 candidate_upper_3sigma_ms=(None if upper[x["source_mm"]] is None else
                                                             pace*x["distance_mm"]*upper[x["source_mm"]])))
            history.append(x)
    return rows

def errors(rows, key):
    values = [abs(x[key]-x["actual_ms"]) for x in rows]
    return dict(n=len(values), median_ae_ms=median(values), mae_ms=statistics.mean(values),
                p90_ae_ms=sorted(values)[int(.9*(len(values)-1))],
                early=sum(x[key] < x["actual_ms"] for x in rows),
                late=sum(x[key] > x["actual_ms"] for x in rows))

def limit_summary(rows):
    eligible=[x for x in rows if x["candidate_upper_3sigma_ms"] is not None]
    late=[x for x in eligible if x["actual_ms"] > x["candidate_upper_3sigma_ms"]]
    return dict(eligible=len(eligible), exceeded=len(late),
                fraction=(len(late)/len(eligible) if eligible else None),
                maximum_excess_ms=max((x["actual_ms"]-x["candidate_upper_3sigma_ms"] for x in late),default=None))

def main():
    parser=argparse.ArgumentParser(); parser.add_argument("--out", type=Path, required=True); args=parser.parse_args()
    route=spacing(); all_intervals=[]; inventory={}
    for direction,path in SOURCES:
        onset, excluded=wave_onsets(direction,path); intervals, pair_excluded=direct_intervals(onset,route)
        inventory[direction]=dict(source=str(path.relative_to(ROOT)), onsets=len(onset),
                                  waveform_exclusions=dict(excluded), interval_exclusions=dict(pair_excluded),
                                  clean_intervals=len(intervals))
        all_intervals.extend(intervals)
    profiles={}; predictions=[]
    for direction in ("CW","CCW"):
        selected=[x for x in all_intervals if x["direction"]==direction]
        profiles[direction], _ = summarize(selected); predictions.extend(prospective(selected))
    output=dict(inventory=inventory, profiles=profiles, intervals=all_intervals,
                predictions=predictions,
                prediction_errors={d:{"uniform":errors([x for x in predictions if x["direction"]==d],"uniform_ms"),
                                      "geographic":errors([x for x in predictions if x["direction"]==d],"geographic_ms"),
                                      "descriptive_3sigma":limit_summary([x for x in predictions if x["direction"]==d])}
                                   for d in ("CW","CCW")},
                caution="3-sigma values are descriptive sample extrapolations, not operational limits.")
    args.out.write_text(json.dumps(output,indent=2)+"\n")
    with args.out.with_suffix('.csv').open('w',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=['direction']+list(profiles['CW'][0]));writer.writeheader()
        for direction in ('CW','CCW'):
            for row in profiles[direction]: writer.writerow(dict(direction=direction,**row))
    print(json.dumps({"inventory":inventory,"prediction_errors":output["prediction_errors"]},indent=2))
if __name__=='__main__': main()

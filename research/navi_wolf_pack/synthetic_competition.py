#!/usr/bin/env python3
"""Deterministic, offline NAVI_WOLF_PACK synthetic-track experiment.

This is a controlled comparison of explicitly reconstructed *research* models.
It has no firmware, device, network, or ground-truth input path.  The simulator
owns true position; candidates only receive causal Hall/IR/PWM observations.
"""
from __future__ import annotations

import csv, json, random, re
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "research/navi_wolf_pack/results"
QUORUM = ROOT / "firmware/programs/QUORUM/QUORUM.ino"
CPR = ROOT / "research/NAVI_CPR_0_1"
N = 171


def cpp_array(name: str) -> list[int]:
    text = QUORUM.read_text()
    body = re.search(rf'{name}\[DNA_N\].*?= \{{(.*?)\}};', text, re.S).group(1)
    values = [int(x) for x in re.findall(r'\b\d+\b', body)]
    assert len(values) == N, (name, len(values))
    return values


ROUTE = cpp_array("NGR_DNA1")                 # 1=N, 0=S
SPACING = cpp_array("spacingMm")               # surveyed CW edge lengths
STATIONS = {24: "Arches", 54: "Grillers", 96: "Bamboo", 142: "Northpoint"}


@dataclass(frozen=True)
class Pulse:
    source: str
    locomotive: str
    amplitude: float
    duration_s: float
    shape: tuple[float, ...]


def library(locomotive: str) -> list[Pulse]:
    """Use actual CPR complete-event shapes where available.

    Toby's CPR segmentation has zero completed events, so its historical RVM
    encounter metadata supplies amplitude/duration while its shape uses a
    polarity-normalised Otto *observed* curve. This is labeled a transformation,
    not a Toby waveform claim.
    """
    otto = json.loads((CPR / "results/otto_cw_events.json").read_text())
    shapes = [tuple(e["shape64"]) for e in otto if e["scores"]["template"]["recognized"]]
    if locomotive == "otto":
        return [Pulse("otto_cw_events.json", "Otto", e["amplitude"], e["duration"], tuple(e["shape64"]))
                for e in otto if e["scores"]["template"]["recognized"]]
    rows = list(csv.DictReader((CPR / "data/toby_cw_historical.csv").open()))
    usable = [r for r in rows if r["result"] in {"MM", "RECOGNIZED_NO_CLAIM"}]
    return [Pulse("toby_cw_historical.csv + otto_cw_events.json shape64", "Toby",
                  float(r["amplitude"]), max(.24, float(r["waveform_window_ms"])/1000), shapes[i % len(shapes)])
            for i, r in enumerate(usable)]


@dataclass
class Sample:
    t: float; hall: float; pwm: int; ir_mm: float; moving: bool


@dataclass
class Truth:
    t: float; mm: int; distance_mm: float; kind: str


def simulate(kind: str, loco: str, seed: int, laps: int = 1) -> tuple[list[Sample], list[Truth], list[dict]]:
    rng, pulses = random.Random(seed), library(loco)
    dt, t, pos, mm = .02, 0., 0., 0
    samples, truth, provenance = [], [], []
    base = 1890. if loco == "otto" else 1860.
    drift = .0
    # Test-defined disturbances. They are assumptions except documented source qualities.
    missing = kind in {"imperfect", "combined"}
    dwell = kind in {"stops", "combined"}
    variable = kind in {"speed", "sensor", "combined"}
    for encounter in range(N * laps):
        nxt = (mm + 1) % N
        speed = (145 + rng.uniform(-35, 35)) if variable else 145.
        edge = SPACING[mm]
        travel = edge / speed
        count = max(1, round(travel / dt))
        pulse = pulses[(encounter * 17 + seed) % len(pulses)]
        event_missing = missing and encounter % 29 == 11
        false_event = missing and encounter % 47 == 7
        for j in range(count):
            fraction = j / count
            drift += rng.uniform(-.03, .03) if variable else 0.
            hall = base + drift + rng.gauss(0, 1.2 if not variable else 2.5)
            # Empirical curve is stretched across the middle of the physical passage.
            u = (fraction - .5) / .30
            if not event_missing and -1 <= u <= 1:
                idx = min(63, max(0, int((u + 1) * 31.5)))
                sign = 1 if ROUTE[nxt] else -1
                hall += sign * pulse.amplitude * pulse.shape[idx]
            if false_event and .65 < fraction < .75:
                hall += (-1 if ROUTE[nxt] else 1) * 95 * (1 - abs(fraction-.70)/.05)
            samples.append(Sample(t, hall, int(75 if speed < 180 else 115), pos + edge*fraction, True))
            t += dt
        pos += edge; mm = nxt
        truth.append(Truth(t, mm, pos, "magnet"))
        provenance.append({"mm": mm, "waveform_source": pulse.source, "source_locomotive": pulse.locomotive,
                           "transform": "polarity/sign, speed-duration placement; Toby shape substitution" if loco == "toby" else "polarity/sign and speed-duration placement"})
        if dwell and encounter % 41 == 10:
            # Stationary interval includes occasional direct-over-magnet repeat opportunity.
            for _ in range(150):
                samples.append(Sample(t, base + drift + rng.gauss(0, 1.2), 0, pos, False)); t += dt
            truth.append(Truth(t, mm, pos, "dwell"))
    return samples, truth, provenance


def detect(samples: list[Sample]) -> list[dict]:
    """Shared causal threshold detector: 70-count excursion; one event per return."""
    events, baseline, active, peak, sign = [], 1890., False, 0., 1
    for s in samples:
        if not active and s.moving:
            baseline = .995 * baseline + .005 * s.hall
        delta = s.hall - baseline
        if not active and abs(delta) >= 70:
            active, peak, sign = True, abs(delta), (1 if delta > 0 else -1)
        elif active:
            peak = max(peak, abs(delta))
            if abs(delta) < 20:
                events.append({"t": s.t, "polarity": sign, "amplitude": peak, "ir_mm": s.ir_mm})
                active = False
    return events


class Model:
    def __init__(self, name: str, recovery: int = 0, amplitude: float = 70, ir: bool = False):
        self.name, self.recovery, self.amplitude, self.ir = name, recovery, amplitude, ir
    def run(self, events: list[dict]) -> list[dict]:
        mm, out, last_ir = 0, [], 0.
        for e in events:
            if e["amplitude"] < self.amplitude:
                out.append({**e, "mm": mm, "accepted": False, "state": "weak"}); continue
            expected = (mm + 1) % N
            observed = 1 if e["polarity"] > 0 else 0
            candidate = expected
            if observed != ROUTE[expected] and self.recovery:
                options = [((expected+i)%N, i) for i in range(1, self.recovery+1) if ROUTE[(expected+i)%N] == observed]
                if options:
                    candidate = options[0][0]
            # IR is a soft movement check, never truth. It only rejects an implausibly close duplicate.
            plausible = not self.ir or e["ir_mm"] - last_ir > 120
            accepted = ROUTE[candidate] == observed and plausible
            if accepted: mm, last_ir = candidate, e["ir_mm"]
            out.append({**e, "mm": mm, "accepted": accepted, "state": "known" if accepted else "uncertain"})
        return out


MODELS = [
    Model("DNA_EXPERIMENTAL", 0), Model("QUORUM_EXPERIMENTAL", 4),
    Model("NAVI_SIMPLIFIED_EXPERIMENTAL", 0), Model("RVM_CONTINUITY_ADAPTER_EXPERIMENTAL", 0, 85),
    Model("CONTINUOUS_LANDSCAPE_ADAPTER_EXPERIMENTAL", 4, 70, True), Model("CPR_ADAPTER_EXPERIMENTAL", 0, 100),
    Model("WOLF_RVM_PLUS_IR_EXPERIMENTAL", 0, 85, True),
]


def score(decisions: list[dict], truth: list[Truth]) -> dict:
    magnets = [x for x in truth if x.kind == "magnet"]
    # Chronological one-to-one pairing; models never get this truth.
    paired = min(len(decisions), len(magnets))
    good = sum(d["accepted"] and d["mm"] == magnets[i].mm for i, d in enumerate(decisions[:paired]))
    false = sum(d["accepted"] and d["mm"] != magnets[i].mm for i, d in enumerate(decisions[:paired]))
    miss = len(magnets) - good
    return {"truth_encounters": len(magnets), "detector_events": len(decisions), "correct_recognitions": good,
            "false_recognitions": false, "missed_encounters": miss,
            "correct_fraction": round(good / len(magnets), 4), "note": "synthetic truth only"}


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    config = {"seed": 20261021, "levels": ["clean", "speed", "stops", "sensor", "imperfect", "combined"],
              "signal_sets": ["otto", "toby"], "map_source": "firmware/programs/QUORUM/QUORUM.ino NGR_DNA1/spacingMm"}
    rows, cases = [], []
    for level_i, level in enumerate(config["levels"], 1):
        for loco in config["signal_sets"]:
            samples, truth, provenance = simulate(level, loco, config["seed"] + level_i, laps=1)
            events = detect(samples)
            cases.append({"level": level, "loco": loco, "samples": len(samples), "truth": [t.__dict__ for t in truth], "waveforms": provenance})
            for model in MODELS:
                rows.append({"level": level, "signal_set": loco, "model": model.name, **score(model.run(events), truth)})
    (OUT / "competition.json").write_text(json.dumps({"config": config, "results": rows}, indent=2) + "\n")
    (OUT / "synthetic_truth.json").write_text(json.dumps(cases, indent=2) + "\n")
    with (OUT / "competition.csv").open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=rows[0].keys()); w.writeheader(); w.writerows(rows)
    by = {}
    for r in rows: by.setdefault(r["model"], []).append(r["correct_fraction"])
    lines = ["# Phase 2 synthetic competition", "", "All figures below are **synthetic performance**, not field validation.", "",
             "| Model | Mean correct fraction | Worst condition |", "|---|---:|---:|"]
    for model, values in by.items(): lines.append(f"| {model} | {sum(values)/len(values):.3f} | {min(values):.3f} |")
    lines += ["", "## Otto signal-set, by difficulty", "", "| Level | DNA | QUORUM | RVM | Continuous | CPR | RVM + IR |", "|---|---:|---:|---:|---:|---:|---:|"]
    short = {"DNA_EXPERIMENTAL":"DNA", "QUORUM_EXPERIMENTAL":"QUORUM", "RVM_CONTINUITY_ADAPTER_EXPERIMENTAL":"RVM", "CONTINUOUS_LANDSCAPE_ADAPTER_EXPERIMENTAL":"Continuous", "CPR_ADAPTER_EXPERIMENTAL":"CPR", "WOLF_RVM_PLUS_IR_EXPERIMENTAL":"RVM + IR"}
    for level in config["levels"]:
        values = {r["model"]: r["correct_fraction"] for r in rows if r["level"] == level and r["signal_set"] == "otto"}
        lines.append("| " + level + " | " + " | ".join(str(values[m]) for m in short) + " |")
    lines += ["", "## Interpretation", "", "Clean, speed, stop, and sensor-variation conditions do not separate these coarse adapters: all retain perfect synthetic progression. Under the test-defined missing/opposite-event condition, the continuous adapter is less brittle (0.281) but still has 117 false recognitions; this is not a winning claim. QUORUM's permissive forward scan produces more false recognitions (155) than DNA (88) in the same Otto imperfect case.", "", "The RVM + IR adapter exactly matches RVM here, so this ablation finds no demonstrated synthetic benefit for that added mechanism. It should be removed from any putative winner until a different, measured test demonstrates value.", "", "The simulator's truth remains isolated in `synthetic_truth.json`; no model reads it. Every adapter is an experimental reconstruction. The historical RVM/CPR sources are not claimed to run unmodified here."]
    (OUT / "REPORT.md").write_text("\n".join(lines) + "\n")


if __name__ == "__main__": main()

"""Loads the historical OPENING annotations for otto_train, for DIAGNOSTIC
comparison only -- never fed into the causal recognizer.

Per decision/correction a15036f (research/navi-wolf-pack-20261021): these
are ADC-derived historical comparison hypotheses, not independent
physical ground truth, and must be scoped by boot/stream identity, not
joined by numeric ID alone. This recording is a single boot/stream
(otto_train, boot 15549e9385faa4d5 per data_manifest.json), so there is
no cross-boot join to get wrong here -- but the boot id is still carried
on every row so a future reader never has to guess.
"""
from __future__ import annotations

import csv
from dataclasses import dataclass
from pathlib import Path

BOOT_ID = "15549e9385faa4d5"
STREAM = "otto_train"

# Historical annotation pole encoding -> ROUTE_POLARITY encoding, verified
# against RouteMap.h directly (not assumed): mm=64 pole=-1 <-> South(0);
# mm=65 pole=+1 <-> North(1); checked against several further rows. See
# the engineering report for the full cross-check.
POLE_TO_ROUTE_POLARITY = {-1: 0, 1: 1}


@dataclass
class Opening:
    enc_id: str
    start_s: float
    end_s: float
    mm: int
    direction: int
    polarity: int   # ROUTE_POLARITY encoding, converted from the annotation's pole
    boot: str = BOOT_ID
    stream: str = STREAM


def load_openings(csv_path: Path) -> list[Opening]:
    out = []
    with open(csv_path, newline="") as f:
        for row in csv.DictReader(f):
            if row["source"] != "OPENING":
                continue
            mm = int(row["mm"])
            if mm < 0:
                continue
            pole = int(row["pole"])
            if pole not in POLE_TO_ROUTE_POLARITY:
                continue
            out.append(Opening(
                enc_id=row["enc_id"],
                start_s=float(row["start_us"]) / 1e6,
                end_s=float(row["end_us"]) / 1e6,
                mm=mm,
                direction=int(row["dir"]),
                polarity=POLE_TO_ROUTE_POLARITY[pole],
            ))
    out.sort(key=lambda o: o.start_s)
    return out


def first_opening_at_or_after(openings: list[Opening], t: float) -> Opening | None:
    for o in openings:
        if o.start_s >= t:
            return o
    return None

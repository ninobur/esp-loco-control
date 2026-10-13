#!/usr/bin/env python3
import collections
import datetime
import json
import struct
import sys

sys.path.insert(0, "/Users/davidbrown/NGR/NGR-Files/navi-ewo-0-3-stop-integration/tools")
import navi_sync_format as F

path = sys.argv[1]
first_t = {}
last_t = {}
counts = collections.Counter()
directions = collections.Counter()
header_nav_directions = collections.Counter()
nav_positions = collections.Counter()
pwm = collections.Counter()
sessions = set()
boots = set()
last_sequence = {}
gaps = collections.Counter()
ring_min = {}
ring_max = {}
sample_intervals = collections.Counter()
raw_min, raw_max = 99999, -99999
median_min, median_max = 99999, -99999
ir_first = ir_last = ir_pulse_first = ir_pulse_last = None
ir_directions = collections.Counter()

with open(path, "rb") as stream:
    _magic, started_us, _reserved = struct.unpack(
        F.FILE_HDR_FMT, stream.read(F.FILE_HDR_LEN)
    )

for _received_us, data in F.iter_capture(path):
    header, payload = F.parse_record(data)
    record_type = header.rec_type
    counts[F.REC_NAME[record_type]] += 1
    sessions.add((header.loco_id, header.session_id))
    boots.add(header.loco_boot_id)
    first_t.setdefault(record_type, header.t0_us)
    last_t[record_type] = header.t0_us
    header_nav_directions[header.nav_dir] += 1
    nav_positions[header.nav_mm] += 1
    key = (header.session_id, record_type)
    if key in last_sequence and header.batch_seq > last_sequence[key] + 1:
        gaps[key] += header.batch_seq - last_sequence[key] - 1
    last_sequence[key] = max(last_sequence.get(key, header.batch_seq), header.batch_seq)
    ring_min[key] = min(ring_min.get(key, header.ring_drops), header.ring_drops)
    ring_max[key] = max(ring_max.get(key, header.ring_drops), header.ring_drops)

    if record_type == F.REC_HALL:
        timestamp_us = header.t0_us
        for index, sample in enumerate(F.iter_hall(header, payload)):
            counts["HALL_ITEMS"] += 1
            directions[bool(sample["flags"] & F.HALL_F_DIR_FWD)] += 1
            pwm[sample["pwm_actual"] > 0] += 1
            sample_intervals[sample["dt_us"]] += 1
            raw_min = min(raw_min, *sample["raw"])
            raw_max = max(raw_max, *sample["raw"])
            median_min = min(median_min, sample["median"])
            median_max = max(median_max, sample["median"])
            if index:
                timestamp_us += sample["dt_us"]
        last_t[record_type] = timestamp_us
    elif record_type == F.REC_IR:
        item = F.parse_ir(payload)
        wire = struct.unpack("<HBBI" + "Q" * 10 + "IIQBBHH", item["wire"])
        pulses = wire[7]
        if ir_first is None:
            ir_first = header.t0_us
            ir_pulse_first = pulses
        ir_last = header.t0_us
        ir_pulse_last = pulses
        ir_directions[item["nav_dir"]] += 1


def quantile(histogram, fraction):
    target = (sum(histogram.values()) - 1) * fraction
    accumulated = 0
    for value, count in sorted(histogram.items()):
        if accumulated + count > target:
            return value
        accumulated += count
    return None


result = {
    "capture_started_utc": datetime.datetime.fromtimestamp(
        started_us / 1e6, datetime.timezone.utc
    ).isoformat(),
    "sessions": sorted(sessions),
    "boot_ids": [f"{boot:016X}" for boot in sorted(boots)],
    "counts": dict(counts),
    "hall_duration_s": (last_t[F.REC_HALL] - first_t[F.REC_HALL]) / 1e6,
    "hall_adc_range": [raw_min, raw_max],
    "hall_median_range": [median_min, median_max],
    "hall_dt_us": {
        "p50": quantile(sample_intervals, 0.50),
        "p95": quantile(sample_intervals, 0.95),
        "p99": quantile(sample_intervals, 0.99),
        "max": max(sample_intervals),
    },
    "hall_samples_pwm_positive": pwm[True],
    "hall_samples_pwm_zero": pwm[False],
    "hall_sample_direction_flag": {
        "forward": directions[True],
        "reverse": directions[False],
    },
    "header_nav_dir_datagrams": dict(header_nav_directions),
    "ir_nav_dir_snapshots": dict(ir_directions),
    "ir_duration_s": (ir_last - ir_first) / 1e6,
    "ir_pulse_change": ir_pulse_last - ir_pulse_first,
    "sequence_gaps": {str(key): value for key, value in gaps.items()},
    "ring_drop_ranges": {
        str(key): [ring_min[key], ring_max[key]] for key in ring_min
    },
    "nav_mm_values": sorted(position for position in nav_positions if position != 255),
    "nav_mm_datagrams_unknown": nav_positions[255],
}
print(json.dumps(result, indent=2, sort_keys=True))

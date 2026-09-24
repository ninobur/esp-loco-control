#!/usr/bin/env python3
"""Host regression for IR_SPEED_LOCAL's contrast/validity contract.

This is an independent transcription, not compiled-firmware execution. It
locks in the QC reproduction that a stationary 159-count ADC noise band must
never become consumable speed while a real spoke train must become VALID.

Detector mirrors updateEnvelope(), median5() and sensorTask()'s detection path
in IR_SPEED_LOCAL.ino, including the open-pulse abort, await-low-after-abort
and stale guards. `validity` models the published `latest.validity`. Health
counters, timing diagnostics and MQTT are not transcribed.

Drift tripwire: check_transcription_source() fails if the constants differ
from the firmware or if the comment/whitespace-stripped source of the
transcribed functions changes. When it trips, re-read those functions, update
Detector to match, and then update TRANSCRIBED_SOURCE_SHA256.
"""

import hashlib
import pathlib
import random
import re

SPOKES = 10
CIRCUMFERENCE_MM = 96.52
MM_PER_PULSE = CIRCUMFERENCE_MM / SPOKES
ENV_WIN_N = 2048
ENV_PRIME_N = 512
ENV_UPDATE_MS = 250
MIN_USABLE_SPAN = 120
MARGINAL_SPAN = 300
SPEED_WINDOW_N = 5
SPEED_STALE_MS = 2500
OPEN_ABORT_MS = 2500

FIRMWARE = pathlib.Path(__file__).resolve().parent.parent / "IR_SPEED_LOCAL.ino"
TRANSCRIBED_FUNCTIONS = ("median5", "updateEnvelope", "sensorTask")
TRANSCRIBED_SOURCE_SHA256 = (
    "fb7874a96d63bd72f35af8b9856c99c4b1e4dcf48e4c51daeb61c177087628e9")


class Detector:
    def __init__(self):
        self.window = []
        self.histogram = [0] * 256
        self.window_index = 0
        self.run_min = 0
        self.run_max = 0
        self.primed = False
        self.last_envelope_ms = 0
        self.in_pulse = False
        self.await_low_after_abort = False
        self.open_aborts = 0
        self.rise_ms = 0
        self.previous_rise_ms = 0
        self.intervals = [0] * SPEED_WINDOW_N
        self.interval_index = 0
        self.interval_count = 0
        self.strong_interval_count = 0
        self.completed = 0
        self.last_completed_ms = 0
        self.speed = 0.0
        self.validity = "INVALID_CONTRAST"

    def update_envelope(self, raw, now_ms):
        bucket = raw >> 4
        if len(self.window) == ENV_WIN_N:
            self.histogram[self.window[self.window_index]] -= 1
            self.window[self.window_index] = bucket
        else:
            self.window.append(bucket)
        self.histogram[bucket] += 1
        self.window_index = (self.window_index + 1) % ENV_WIN_N

        filled = len(self.window)
        if now_ms - self.last_envelope_ms < ENV_UPDATE_MS or filled < 64:
            return
        self.last_envelope_ms = now_ms
        need_low = filled * 5 // 100
        need_high = filled * 95 // 100
        cumulative = 0
        low_bucket = high_bucket = -1
        for candidate, count in enumerate(self.histogram):
            cumulative += count
            if low_bucket < 0 and cumulative > need_low:
                low_bucket = candidate
            if high_bucket < 0 and cumulative >= need_high:
                high_bucket = candidate
                break
        if low_bucket >= 0 and high_bucket >= 0:
            self.run_min = low_bucket * 16
            self.run_max = high_bucket * 16 + 15
        self.primed = filled >= ENV_PRIME_N

    def median_interval(self):
        values = sorted(self.intervals[: self.interval_count])
        return values[len(values) // 2] if values else 0

    def reset_intervals(self):
        self.previous_rise_ms = 0
        self.interval_index = self.interval_count = 0
        self.strong_interval_count = 0
        self.speed = 0.0

    def step(self, raw, now_ms):
        self.update_envelope(raw, now_ms)
        span = self.run_max - self.run_min
        contrast_good = self.primed and span >= MIN_USABLE_SPAN
        if not contrast_good:
            self.await_low_after_abort = (self.await_low_after_abort or
                                          self.in_pulse)
            self.in_pulse = False
            self.reset_intervals()
            self.validity = "INVALID_CONTRAST"
            return

        marginal = span < MARGINAL_SPAN
        threshold_high = self.run_min + span * 2 // 3
        threshold_low = self.run_min + span // 3
        if marginal:
            self.strong_interval_count = 0
            self.validity = "MARGINAL"

        if self.in_pulse and now_ms - self.rise_ms > OPEN_ABORT_MS:
            self.open_aborts += 1
            self.in_pulse = False
            self.await_low_after_abort = True
            self.reset_intervals()

        if self.await_low_after_abort and raw < threshold_low:
            self.await_low_after_abort = False

        if (not self.in_pulse and not self.await_low_after_abort and
                raw > threshold_high):
            self.in_pulse = True
            self.rise_ms = now_ms
        elif self.in_pulse and raw < threshold_low:
            self.in_pulse = False
            self.completed += 1
            self.last_completed_ms = now_ms
            if self.previous_rise_ms:
                interval = self.rise_ms - self.previous_rise_ms
                if interval > 0:
                    self.intervals[self.interval_index] = interval
                    self.interval_index = (self.interval_index + 1) % SPEED_WINDOW_N
                    self.interval_count = min(self.interval_count + 1,
                                              SPEED_WINDOW_N)
                    if marginal:
                        self.strong_interval_count = 0
                    else:
                        self.strong_interval_count = min(
                            self.strong_interval_count + 1, SPEED_WINDOW_N)
                    median = self.median_interval()
                    if median > 0:
                        self.speed = MM_PER_PULSE * 1000.0 / median
            self.previous_rise_ms = self.rise_ms
            if marginal:
                self.validity = "MARGINAL"
            elif (self.interval_count >= SPEED_WINDOW_N and
                  self.strong_interval_count >= SPEED_WINDOW_N):
                self.validity = "VALID"
            else:
                self.validity = "REACQUIRING"

        if not marginal and (
                self.last_completed_ms == 0 or
                now_ms - self.last_completed_ms > SPEED_STALE_MS):
            if self.validity != "STALE":
                self.reset_intervals()
                self.validity = "STALE"

def strip_comments(source):
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    source = re.sub(r"//[^\n]*", "", source)
    return re.sub(r"\s+", "", source)


def function_source(source, name):
    match = re.search(r"\bstatic\s+\w+\s+" + name + r"\s*\(", source)
    assert match, f"{name}() not found in {FIRMWARE.name}"
    depth = 0
    for index in range(source.index("{", match.end()), len(source)):
        depth += {"{": 1, "}": -1}.get(source[index], 0)
        if depth == 0:
            return source[match.start():index + 1]
    raise AssertionError(f"unbalanced braces in {name}()")


def check_transcription_source():
    source = FIRMWARE.read_text()
    for name, value in (("SPOKES_PER_REV", SPOKES),
                        ("WHEEL_CIRCUMFERENCE_MM", CIRCUMFERENCE_MM),
                        ("SPEED_STALE_MS", SPEED_STALE_MS),
                        ("OPEN_ABORT_MS", OPEN_ABORT_MS),
                        ("MIN_USABLE_SPAN", MIN_USABLE_SPAN),
                        ("MARGINAL_SPAN", MARGINAL_SPAN),
                        ("ENV_WIN_N", ENV_WIN_N),
                        ("ENV_PRIME_N", ENV_PRIME_N),
                        ("ENV_UPDATE_MS", ENV_UPDATE_MS),
                        ("ENV_PCT_LO", 5),
                        ("ENV_PCT_HI", 95),
                        ("SPEED_WINDOW_N", SPEED_WINDOW_N)):
        match = re.search(r"\b" + name + r"\s*=\s*([0-9.]+)f?\s*;", source)
        assert match, f"{name} not found in {FIRMWARE.name}"
        assert float(match.group(1)) == value, (
            f"{name}: firmware {match.group(1)} != transcription {value}")
    digest = hashlib.sha256("".join(
        strip_comments(function_source(source, name))
        for name in TRANSCRIBED_FUNCTIONS).encode()).hexdigest()
    assert digest == TRANSCRIBED_SOURCE_SHA256, (
        f"{', '.join(TRANSCRIBED_FUNCTIONS)} changed in {FIRMWARE.name} "
        f"(sha256 {digest}). Re-transcribe Detector, then update "
        "TRANSCRIBED_SOURCE_SHA256.")


def exercise(name, sampler, duration_ms=8000, seed=1):
    random.seed(seed)
    detector = Detector()
    valid_speeds = []
    for now in range(duration_ms):
        detector.step(sampler(now), now)
        if detector.validity == "VALID":
            valid_speeds.append(detector.speed)
    span = detector.run_max - detector.run_min
    print(f"{name}: pulses={detector.completed} span={span} "
          f"state={detector.validity} valid_samples={len(valid_speeds)}")
    return detector, valid_speeds


def real_wheel(now_ms):
    return 3200 if now_ms % 63 < 20 else 400


def main():
    check_transcription_source()

    noise, noise_valid = exercise(
        "stationary noise",
        lambda _now: random.randint(0, 170),
    )
    assert noise.completed > 100, "control must exercise noise crossings"
    assert not noise_valid, "stationary noise must never become VALID"
    assert noise.validity == "MARGINAL"

    flat, flat_valid = exercise(
        "stationary flat",
        lambda _now: 2670 + random.randint(-3, 3),
    )
    assert not flat_valid
    assert flat.validity == "INVALID_CONTRAST"

    moving, moving_valid = exercise("real 63 ms spoke train", real_wheel)
    assert moving_valid, "real spoke train must become VALID"
    assert abs(moving.speed - (MM_PER_PULSE * 1000.0 / 63.0)) < 0.01

    # A noise-filled interval ring must not become VALID immediately when real
    # contrast returns. Five current strong intervals are required.
    random.seed(4)
    recovery = Detector()
    first_valid = None
    transition_ms = 4000
    for now in range(6000):
        raw = random.randint(0, 170) if now < transition_ms else real_wheel(now)
        recovery.step(raw, now)
        if recovery.validity == "VALID" and first_valid is None:
            first_valid = now
    assert first_valid is not None
    assert first_valid - transition_ms >= 5 * 63 - 63, (
        "VALID must wait for five current strong intervals after noise"
    )
    print(f"noise-to-signal recovery: first VALID {first_valid-transition_ms} ms "
          "after transition")
    print("PASS")


if __name__ == "__main__":
    main()

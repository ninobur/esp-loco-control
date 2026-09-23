#!/usr/bin/env python3
"""Host regression for IR_SPEED_LOCAL's contrast/validity contract.

This is an independent transcription, not compiled-firmware execution. It
locks in the QC reproduction that a stationary 159-count ADC noise band must
never become consumable speed while a real spoke train must become VALID.
"""

import random

SPOKES = 10
CIRCUMFERENCE_MM = 96.52
MM_PER_PULSE = CIRCUMFERENCE_MM / SPOKES
ENV_WIN_N = 2048
ENV_PRIME_N = 512
ENV_UPDATE_MS = 250
MIN_USABLE_SPAN = 120
MARGINAL_SPAN = 300
SPEED_WINDOW_N = 5


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
        self.rise_ms = 0
        self.previous_rise_ms = 0
        self.intervals = [0] * SPEED_WINDOW_N
        self.interval_index = 0
        self.interval_count = 0
        self.strong_interval_count = 0
        self.completed = 0
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

    def step(self, raw, now_ms):
        self.update_envelope(raw, now_ms)
        span = self.run_max - self.run_min
        contrast_good = self.primed and span >= MIN_USABLE_SPAN
        if not contrast_good:
            self.in_pulse = False
            self.previous_rise_ms = 0
            self.interval_index = self.interval_count = 0
            self.strong_interval_count = 0
            self.speed = 0.0
            self.validity = "INVALID_CONTRAST"
            return

        marginal = span < MARGINAL_SPAN
        if marginal:
            self.strong_interval_count = 0
            self.validity = "MARGINAL"

        threshold_high = self.run_min + span * 2 // 3
        threshold_low = self.run_min + span // 3
        if not self.in_pulse and raw > threshold_high:
            self.in_pulse = True
            self.rise_ms = now_ms
        elif self.in_pulse and raw < threshold_low:
            self.in_pulse = False
            self.completed += 1
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
                    self.speed = MM_PER_PULSE * 1000.0 / median
            self.previous_rise_ms = self.rise_ms
            if marginal:
                self.validity = "MARGINAL"
            elif (self.interval_count >= SPEED_WINDOW_N and
                  self.strong_interval_count >= SPEED_WINDOW_N):
                self.validity = "VALID"
            else:
                self.validity = "REACQUIRING"


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

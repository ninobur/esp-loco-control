#!/usr/bin/env python3
"""A deliberately small causal replay contract for navigation experiments.

This module has no device, network, or firmware dependencies.  It accepts only
chronological, already-observed events and never supplies a model future data.
The three included models are *experimental reconstructions*, not claims of
historical source parity.  Historical-fidelity candidates must instead be
connected through a documented adapter and parity check.
"""
from __future__ import annotations

from dataclasses import asdict, dataclass, field
from typing import Iterable, Protocol


@dataclass(frozen=True)
class Observation:
    t_us: int
    polarity: int
    kind: str = "magnetic_encounter"
    motion: bool = True


@dataclass
class Decision:
    t_us: int
    last_confirmed_mm: int | None
    next_expected_mm: int | None
    recognized: bool = False
    uncertainty: str = "known"
    note: str = ""


class Candidate(Protocol):
    name: str
    def observe(self, observation: Observation) -> Decision: ...


def _next(mm: int, direction: int, size: int) -> int:
    return (mm + direction) % size


@dataclass
class DNAExperimental:
    """Ordered-polarity next-marker reconstruction; no recovery machinery."""
    route: list[int]
    initial_mm: int
    direction: int = 1
    name: str = "DNA_EXPERIMENTAL_RECONSTRUCTION"
    current: int = field(init=False)

    def __post_init__(self) -> None:
        self.current = self.initial_mm

    def observe(self, observation: Observation) -> Decision:
        expected = _next(self.current, self.direction, len(self.route))
        if observation.kind != "magnetic_encounter" or not observation.motion:
            return Decision(observation.t_us, self.current, expected, note="not an advancing encounter")
        if observation.polarity == self.route[expected]:
            self.current = expected
            return Decision(observation.t_us, self.current, _next(self.current, self.direction, len(self.route)), True)
        return Decision(observation.t_us, self.current, expected, False, "contradicted", "unexpected polarity retained as evidence")


@dataclass
class QuorumExperimental:
    """A symmetric candidate-offset experiment, intentionally separate from QUORUM source."""
    route: list[int]
    initial_mm: int
    direction: int = 1
    offsets: tuple[int, ...] = (-3, -2, -1, 0, 1, 2, 3, 4)
    name: str = "QUORUM_SYMMETRIC_EXPERIMENTAL_VARIANT"
    current: int = field(init=False)

    def __post_init__(self) -> None:
        self.current = self.initial_mm

    def observe(self, observation: Observation) -> Decision:
        expected = _next(self.current, self.direction, len(self.route))
        if observation.kind != "magnetic_encounter" or not observation.motion:
            return Decision(observation.t_us, self.current, expected, note="not an advancing encounter")
        candidates = []
        for offset in self.offsets:
            candidate = (expected + offset * self.direction) % len(self.route)
            if self.route[candidate] == observation.polarity:
                candidates.append(candidate)
        if len(candidates) == 1:
            self.current = candidates[0]
            return Decision(observation.t_us, self.current, _next(self.current, self.direction, len(self.route)), True,
                            "known", "unique polarity-compatible candidate")
        return Decision(observation.t_us, self.current, expected, False, "no_quorum",
                        f"{len(candidates)} polarity-compatible candidates")


@dataclass
class SimplifiedExperimental(DNAExperimental):
    """Detection/judgment separation with a physical-event key supplied upstream."""
    name: str = "NAVI_SIMPLIFIED_EXPERIMENTAL_RECONSTRUCTION"
    seen_timestamps: set[int] = field(default_factory=set)

    def observe(self, observation: Observation) -> Decision:
        if observation.t_us in self.seen_timestamps:
            return Decision(observation.t_us, self.current, _next(self.current, self.direction, len(self.route)),
                            False, "known", "duplicate event key suppressed")
        self.seen_timestamps.add(observation.t_us)
        return super().observe(observation)


def run(candidate: Candidate, observations: Iterable[Observation]) -> list[dict]:
    """Run in provided order, rejecting time travel rather than sorting it away."""
    out: list[dict] = []
    last_t = -1
    for observation in observations:
        if observation.t_us < last_t:
            raise ValueError("observations are not chronological")
        last_t = observation.t_us
        out.append(asdict(candidate.observe(observation)))
    return out

"""Causal bounded continuous encounter segmentation and whole-event recognition.

No direction, map, IR, PWM or navigation input. All thresholds are experimental.
"""
from collections import deque
from dataclasses import asdict, dataclass
import math

import numpy as np


@dataclass(frozen=True)
class Config:
    """Research settings; sampling intervals and event times are in seconds.

    The saved models.json contains the calibrated configuration. These defaults
    are useful for isolated component tests, not an accepted locomotive policy.
    """
    baseline: str = 'linear'
    return_s: float = .24
    dt: float = .02
    history: int = 100
    warmup: int = 50
    pre: int = 15
    onset_n: int = 3
    horizon_s: float = 30.


class Baseline:
    """Only admitted background enters this estimator. Prediction precedes update."""
    def __init__(self, kind='linear', size=100):
        self.kind = kind
        self.points = deque(maxlen=size)
        self.level = self.slope = self.at = 0.
        self.noise = 1.
        self.updates = 0

    def predict(self, t):
        """Predict from past admitted background without consuming this sample."""
        return self.level + self.slope * (t - self.at)

    def update(self, t, x):
        self.points.append((t, x))
        if len(self.points) < 2:
            self.level, self.at = x, t
            return
        innovation = x - self.predict(t)
        if self.kind == 'ema':
            self.level = (
                self.predict(t)
                + (1 - math.exp(-.02 / 2))
                * float(np.clip(innovation, -3*self.noise, 3*self.noise))
            )
            self.at, self.slope = t, 0.
        self.updates += 1
        if self.updates % 5:
            return
        a = np.asarray(self.points)
        tt, yy = a[:, 0] - t, a[:, 1]
        if self.kind == 'linear':
            X = np.column_stack([np.ones(len(tt)), tt])
            beta = np.linalg.lstsq(X, yy, rcond=None)[0]
            for _ in range(2):
                e = yy - X @ beta
                scale = max(.5, 1.4826 * float(np.median(abs(e - np.median(e)))))
                w = np.minimum(1., 1.5 * scale / np.maximum(abs(e), 1e-9))
                beta = np.linalg.lstsq(
                    X * np.sqrt(w[:, None]), yy * np.sqrt(w), rcond=None
                )[0]
            self.level, self.slope, self.at = float(beta[0]), float(beta[1]), t
            residual = yy - X @ beta
        else:
            residual = yy - self.level
        self.noise = max(
            .5, 1.4826 * float(np.median(abs(residual - np.median(residual))))
        )

    def initialize(self):
        if self.points:
            self.level = float(np.median([p[1] for p in self.points]))
            self.at = self.points[-1][0]
            self.slope = 0.


class NativeRule:
    """>=70 counts, two consecutive supplied acquisition samples, second timestamp.

    Signs must agree (North/South). Serial loss resets confirmation. One witness
    per consecutive run. Baseline is held at its PRE-BLOCK causal estimate.
    """
    def __init__(self):
        self.last = None
        self.latched = False

    def feed(self, records, baseline):
        if not len(records):
            self.last = None
            self.latched = False
            return []
        x = records['adc'].astype(float) - baseline
        sign = np.where(x >= 70, 1, np.where(x <= -70, -1, 0))
        ser = records['serial'].astype(np.int64)
        us = records['us']
        prior_sign = np.r_[self.last[1] if self.last else 0, sign[:-1]]
        prior_serial = np.r_[
            self.last[0] if self.last else int(ser[0])-2, ser[:-1]
        ]
        pair = (
            (sign != 0)
            & (sign == prior_sign)
            & (((ser-prior_serial) % (1 << 32)) == 1)
        )
        starts = np.flatnonzero(pair & ~np.r_[self.latched, pair[:-1]])
        self.latched = bool(pair[-1])
        self.last = (int(ser[-1]), int(sign[-1]))
        return [
            dict(t=float(us[j])/1e6, polarity=int(sign[j]), baseline=baseline)
            for j in starts
        ]


class Segmenter:
    """Discover, follow, and complete encounters without classifying samples.

    Baseline updates stop while an event develops. A sustained return completes
    an event; peaks and polarity changes have no special state transitions.
    An optional sink receives decisions. Internal diagnostic history is bounded.
    """
    def __init__(self, config=Config(), sink=None):
        self.c = config
        self.base = Baseline(config.baseline, config.history)
        self.pre = deque(maxlen=config.pre + config.onset_n)
        self.warm = 0
        self.onset = 0
        self.active = None
        self.returning = []
        self.quarantine = False
        self.counter = 0
        self.max_buffer = 0
        self.journal = deque(maxlen=256)
        self.sink = sink
        self.last_t = None

    def log(self, kind, t, **kw):
        row = dict(kind=kind, t=float(t), **kw)
        self.journal.append(row)
        if self.sink:
            self.sink(row)
        return row

    def censor(self, t, reason):
        if self.active is not None:
            self.log('incomplete', t, id=self.counter, start=self.active[0][0] - self.c.dt,
                     reason=reason, stored_bins=len(self.active))
        self.active = None
        self.returning = []
        self.pre.clear()
        self.onset = 0

    def feed(self, t, x, witness=None):
        """Consume one available Hall bin; return (completed_event, level, noise).

        t is the bin END, when its value becomes available. A witness records
        native acquisition evidence observed before this call. Completion is
        timestamped now; the estimated physical end is reported retrospectively.
        """
        c = self.c
        if self.last_t is not None and t <= self.last_t:
            raise ValueError('time must increase strictly')
        timegap = self.last_t is not None and t - self.last_t > c.dt * 1.5
        self.last_t = t
        if timegap or not np.isfinite(x):
            self.censor(t, 'recording_gap')
            self.log('gap', t)
            # New baseline requires warmup; no hidden completion across missing data.
            self.base = Baseline(c.baseline, c.history)
            self.warm = 0
            self.quarantine = False
            return None, float('nan'), float('nan')
        if self.warm < c.warmup:
            self.base.update(t, x)
            self.warm += 1
            if self.warm == c.warmup:
                self.base.initialize()
                self.log('baseline_ready', t, level=self.base.level,
                         note='warmup baseline identity unverified')
            return None, self.base.predict(t), self.base.noise
        b, noise = self.base.predict(t), self.base.noise
        r = x - b
        band = max(3., 3*noise)
        sample = (float(t), float(x), float(b), float(r), float(noise), witness)
        self.pre.append(sample)
        if self.quarantine:
            self.returning = (self.returning + [sample]) if abs(r) <= band else []
            if len(self.returning) >= math.ceil(c.return_s/c.dt):
                self.log('rearmed_after_incomplete', t)
                self.quarantine = False
                for p in self.returning:
                    self.base.update(p[0], p[1])
                self.returning = []
            return None, b, noise
        if self.active is None:
            if abs(r) > max(4., 4*noise):
                self.onset += 1
            else:
                self.onset = 0
                self.base.update(t, x)
            if self.onset >= c.onset_n:
                self.counter += 1
                self.active = list(self.pre)
                self.log('provisional_open', t, id=self.counter,
                         start=self.active[0][0] - c.dt, baseline=b, noise=noise)
            return None, b, noise
        self.active.append(sample)
        self.max_buffer = max(self.max_buffer, len(self.active))
        if len(self.active) > math.ceil(c.horizon_s/c.dt) + c.pre + c.onset_n:
            self.censor(t, 'horizon_limit')
            self.quarantine = True
            return None, b, noise
        if abs(r) <= band:
            self.returning.append(sample)
        else:
            self.returning = []
        required = math.ceil(c.return_s / c.dt)
        if len(self.returning) >= required:
            rr = [p[3] for p in self.returning[-required:]]
            if max(rr) - min(rr) > 2*band:
                return None, b, noise
            # End is retrospective; decision is causal at current t.
            end = self.returning[-required][0] - c.dt
            field = self.active[:-required]
            event = dict(id=self.counter, start=field[0][0] - c.dt, end=end,
                         completed=float(t), noise=noise, samples=field,
                         return_residual_rms=float(np.sqrt(np.mean(np.square(rr)))),
                         return_band=band, return_bins=required)
            self.log('complete', t, id=self.counter, start=event['start'], end=end,
                     return_bins=required, return_band=band)
            for p in self.returning:
                self.base.update(p[0], p[1])
            self.active = None
            self.returning = []
            self.pre.clear()
            self.onset = 0
            return event, b, noise
        return None, b, noise

    def finish(self):
        self.censor(self.last_t or 0., 'end_of_recording')


def area_resample(x, n=64):
    """Treat every original bin as a constant interval; integrate, do not skip."""
    x = np.asarray(x, dtype=float)
    edges = np.linspace(0, len(x), n+1)
    integral = np.interp(edges, np.arange(len(x)+1), np.r_[0., np.cumsum(x)])
    return np.diff(integral) / (len(x)/n)


def describe(event):
    """Measure the completed curve, retaining physical scale beside shape."""
    r = np.array([p[3] for p in event['samples']])
    if not len(r) or np.max(abs(r)) <= 0:
        return None
    k = int(np.argmax(abs(r)))
    amplitude = float(abs(r[k]))
    polarity = 1 if r[k] >= 0 else -1
    canonical = r * polarity
    shape = area_resample(canonical / amplitude)
    duration = event['end'] - event['start']
    asym = k / max(1, len(r)-1)
    opposite = float(
        np.maximum(-canonical, 0).sum()
        / max(np.maximum(canonical, 0).sum(), 1e-9)
    )
    witness = [p[5] for p in event['samples'] if p[5] is not None]
    # Native proof is separate from normalized shape or statistical distance.
    return dict(shape=shape, amplitude=amplitude, duration=duration,
                snr=amplitude / event['noise'], polarity=polarity,
                peak_t=event['samples'][k][0] - .01, asymmetry=asym,
                opposite_area=opposite, native70=bool(witness),
                native70_second_us=round(witness[0]['t']*1e6) if witness else None)


def cluster(X, k=8):
    """Fit deterministic empirical shape families, with no waveform assumption."""
    # Deterministic farthest-first seeds, followed by Lloyd means.
    centres = [X[0]]
    for _ in range(min(k, len(X))-1):
        d = np.min(
            np.sum((X[:, None, :] - np.array(centres)[None, :, :])**2, axis=2),
            axis=1,
        )
        centres.append(X[int(np.argmax(d))])
    centres = np.array(centres)
    for _ in range(30):
        ids = np.argmin(
            np.sum((X[:, None, :] - centres[None, :, :])**2, axis=2), axis=1
        )
        new = np.array([
            X[ids == i].mean(0) if np.any(ids == i) else centres[i]
            for i in range(len(centres))
        ])
        if np.max(abs(new-centres)) < 1e-8:
            break
        centres = new
    return centres


class Population:
    """One-class empirical event population, never a baseline-vs-magnet LLR."""
    def __init__(self, train):
        self.train = train
        # Both travel orientations without receiving direction/geographic input.
        S = np.array([d['shape'] for d in train] + [d['shape'][::-1] for d in train])
        self.mean = S.mean(0)
        _, _, vt = np.linalg.svd(S-self.mean, full_matrices=False)
        self.axes = vt[:8]
        self.templates = cluster(S)
        feats = np.array([self.vector(d, reverse=r) for r in (False, True) for d in train])
        self.mu = feats.mean(0)
        self.scale = np.maximum(feats.std(0), .05)
        z = (feats-self.mu)/self.scale
        cov = np.cov(z.T)
        cov = .5*cov + .5*np.diag(np.diag(cov)) + .01*np.eye(cov.shape[0])
        self.precision = np.linalg.inv(cov)
        ad = np.array([[math.log(d['amplitude']), math.log(d['duration'])] for d in train])
        self.admean = ad.mean(0)
        self.adscale = np.maximum(ad.std(0), .1)
        self.thresholds = {}

    def vector(self, d, reverse=False):
        s = d['shape'][::-1] if reverse else d['shape']
        pc = (s-self.mean) @ self.axes.T
        err = float(np.mean((s-self.mean-pc@self.axes)**2))
        return np.r_[pc, err, math.log(max(d['amplitude'], 1e-9)),
                     math.log(max(d['duration'], 1e-9)), math.log(max(d['snr'], 1e-9)),
                     1-d['asymmetry'] if reverse else d['asymmetry'], d['opposite_area']]

    def distances(self, d):
        z = (self.vector(d)-self.mu)/self.scale
        pca = float(z @ self.precision @ z)
        ad = np.array([math.log(d['amplitude']), math.log(d['duration'])])
        adcost = float(np.mean(((ad-self.admean)/self.adscale)**2))
        template = float(np.min(np.mean((self.templates-d['shape'])**2, axis=1)))*64 + adcost
        s = d['shape']
        phase = (np.arange(64)+.5)/64
        w = np.maximum(s, 0)
        denom = max(w.sum(), 1e-9)
        centre = float(w @ phase/denom)
        width = max(.02, float(np.sqrt(w @ ((phase-centre)**2)/denom)))
        gaussian = np.exp(-.5*((phase-centre)/width)**2)
        gaussian *= float(gaussian @ s) / max(float(gaussian @ gaussian), 1e-9)
        fit = float(np.mean((gaussian-s)**2))*64 + adcost
        return dict(pca=pca, template=template, gaussian=fit)

    def calibrate(self, positives):
        scores = [self.distances(d) for d in positives]
        if not scores:
            raise ValueError('no complete positive calibration objects')
        self.thresholds = {k: float(np.quantile([s[k] for s in scores], .99)) for k in scores[0]}

    def decide(self, d):
        scores = self.distances(d)
        return {k: dict(distance=v, threshold=self.thresholds[k],
                        score=1-v/self.thresholds[k], recognized=bool(d['native70'] and v <= self.thresholds[k]))
                for k, v in scores.items()}

    def serialize(self):
        return {k: v.tolist() if isinstance(v, np.ndarray) else v for k, v in vars(self).items() if k != 'train'}

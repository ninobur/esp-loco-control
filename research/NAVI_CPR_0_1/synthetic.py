"""Prespecified illustrative synthetic cases, evaluated without model changes."""
import numpy as np
from cpr import NativeRule, Segmenter, describe
from experiment import dump, load_model, ROOT
from prepare import NATIVE


def cases():
    rng = np.random.default_rng(20261019)
    t = np.arange(0, 16, .02) + .02
    g = lambda c, w, a: a*np.exp(-.5*((t-c)/w)**2)
    hill = g(4, .22, 170)
    weak = g(7, .3, -78)
    noise = rng.normal(0, .5, len(t))
    correlated = np.convolve(rng.normal(0, 1., len(t)+20), np.ones(21)/5, 'valid')
    interrupted = 1000 + hill
    interrupted[t>4.05] = np.nan
    step = np.where(t>=4, 100., 0.)
    asym = np.where(t<=4, g(4, .12, 170), g(4, .45, 170))
    secondary = g(4, .2, 170)-g(4.55, .18, 38)
    shoulder = g(4, .22, 170)+g(4.42, .28, 65)
    return t, {
        'strong_and_weak_both_signs': 1000+noise+hill+weak,
        'below_governing_amplitude': 1000+noise+g(4,.3,55),
        'asymmetric': 1000+noise+asym,
        'secondary_lobe': 1000+noise+secondary,
        'shoulder': 1000+noise+shoulder,
        'close_separate': 1000+noise+g(4,.1,170)-g(4.9,.1,160),
        'close_without_sustained_return': 1000+noise+g(4,.22,170)-g(4.55,.22,160),
        'slow_baseline_drift': 1000+.4*t+noise+hill,
        'abrupt_baseline_step': 1000+noise+step,
        'correlated_noise': 1000+correlated+hill,
        'short_disturbance': 1000+noise+np.where((t>4)&(t<4.04), 130, 0),
        'incomplete': interrupted,
        'stationary': 1000+noise,
    }


def main():
    cfg, model, _ = load_model()
    t, collection = cases()
    outputs = {}
    for name, y in collection.items():
        journal = []; s = Segmenter(cfg, journal.append); gate = NativeRule()
        events = []
        for k, (tt, xx) in enumerate(zip(t, y)):
            proof = None
            if np.isfinite(xx) and s.warm >= cfg.warmup:
                a = np.zeros(1, dtype=NATIVE)
                a['serial'], a['us'], a['adc'] = k, round(tt*1e6), round(xx)
                hits = gate.feed(a, s.base.predict(tt-cfg.dt))
                proof = hits[0] if hits else None
            else:
                gate.last = None; gate.latched = False
            e, _, _ = s.feed(float(tt), float(xx), proof)
            if e:
                d = describe(e)
                events.append(dict(start=e['start'], end=e['end'], completed=e['completed'],
                    amplitude=d['amplitude'], polarity=d['polarity'], native70=d['native70'],
                    scores=model.decide(d)))
        s.finish()
        outputs[name] = dict(events=events, incomplete=[r for r in journal if r['kind']=='incomplete'])
    dump(ROOT/'results/synthetic.json', outputs)
    np.savez_compressed(ROOT/'results/synthetic_traces.npz', t=t, **collection)
    print({k:len(v['events']) for k,v in outputs.items()})


if __name__ == '__main__':
    main()

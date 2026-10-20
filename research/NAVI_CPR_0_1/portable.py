"""Losslessly pack/unpack native input columns; verify original native byte hashes."""
import argparse
from pathlib import Path
import json
import numpy as np
from prepare import ROOT, NATIVE, sha


def main(mode):
    manifest = json.loads((ROOT / 'sources/data_manifest.json').read_text())
    (ROOT / 'raw').mkdir(exist_ok=True)
    for name, meta in manifest['boots'].items():
        raw = ROOT / f'raw/{name}.bin'
        packed = ROOT / f'data/{name}_native.npz'
        if mode == 'pack':
            a = np.memmap(raw, dtype=NATIVE, mode='r')
            np.savez_compressed(packed,
                serial_delta=np.diff(a['serial'].astype(np.int64), prepend=0),
                us_delta=np.diff(a['us'].astype(np.int64), prepend=0),
                adc=a['adc'], pwm=a['pwm'], direction=a['direction'])
        elif not raw.exists():
            with np.load(packed) as z:
                a = np.empty(len(z['adc']), dtype=NATIVE)
                a['serial'] = np.cumsum(z['serial_delta'])
                a['us'] = np.cumsum(z['us_delta'])
                for key in ('adc', 'pwm', 'direction'):
                    a[key] = z[key]
                a.tofile(raw)
        actual = sha(raw)
        if actual != meta['native_sha256']:
            raise ValueError(f'native byte hash mismatch: {name}')
        print(name, 'verified', actual, 'packed bytes', packed.stat().st_size, flush=True)


if __name__ == '__main__':
    p = argparse.ArgumentParser(); p.add_argument('mode', choices=('pack', 'restore'))
    main(p.parse_args().mode)

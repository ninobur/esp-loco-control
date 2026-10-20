"""Build or verify the SHA-256 inventory of the research delivery."""
import argparse
import json
from pathlib import Path
from prepare import ROOT, sha


def main(mode):
    target = ROOT / 'PACKAGE_MANIFEST.json'
    if mode == 'build':
        files = [p for p in ROOT.rglob('*') if p.is_file()
                 and 'raw' not in p.relative_to(ROOT).parts
                 and '__pycache__' not in p.relative_to(ROOT).parts
                 and p != target]
        data = dict(base_commit='9e7b9208df1e9e06f75f208cd8a39fcec6b62cc7',
                    protocol_commit='f9cc949', implementation_commit='f8476e9',
                    calibration_freeze_commit='eeec67a',
                    files={str(p.relative_to(ROOT)):sha(p) for p in sorted(files)})
        target.write_text(json.dumps(data, indent=2)+'\n')
    data = json.loads(target.read_text())
    for name, expected in data['files'].items():
        if sha(ROOT/name) != expected:
            raise AssertionError(f'package hash mismatch: {name}')
    print(f"Verified {len(data['files'])} research artifacts.")


if __name__ == '__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('mode', choices=('build','verify'), default='verify', nargs='?')
    main(parser.parse_args().mode)

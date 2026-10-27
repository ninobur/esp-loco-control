#!/usr/bin/env python3
"""Write CORPUS_MANIFEST.json/.md from the extracted captures (needs NSR sources)."""
import hashlib
import json
import os
from pathlib import Path

import corpus

HERE = Path(__file__).resolve().parent


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def main():
    rows = []
    for j in sorted(corpus.NSR_DIR.glob('*.json')):
        m = json.loads(j.read_text())
        name = j.stem
        used = m['native_samples'] >= corpus.MIN_SAMPLES
        row = dict(capture=os.path.basename(m['source']), sha256=sha256(m['source']),
                   bytes=m['bytes'], locomotive='Otto 9950011' if '9950011' in name else 'Toby 9950012',
                   boots=list(m['boots']), native_samples=m['native_samples'],
                   duration_s=round(m['duration_s'], 1), crc_bad_records=m['bad_records'],
                   native_serial_gaps=m['serial_gaps'], record_counts=m['record_counts'],
                   partition=corpus.partition_of(name) if used else 'excluded (<50k samples)')
        if used:
            s = corpus.load(name)
            row.update(reference_magnets=len(s['refs']),
                       reference_expected=sum(r['kind'] == 'expected' for r in s['refs']),
                       scored_span_s=round(sum(b - a for a, b, _ in s['spans']), 1),
                       oracle_excursions=len(s['exc']))
        rows.append(row)
    (HERE / 'CORPUS_MANIFEST.json').write_text(json.dumps(rows, indent=1))
    lines = ['# Test corpus manifest', '',
             'Source: `~/Pi_Log_Downloads/*.nsr` (NSR1 captures, Oct 25–26 2026). Decoded by '
             '`extract_nsr.py` (CRC-checked, no repair). Template source (Oct 15 Otto, '
             '`aligned_curves.csv`) is not in any partition below.', '',
             '| capture | loco | partition | dur s | native samples | CRC bad | serial gaps | ref magnets (expected) | scored s |',
             '|---|---|---|---:|---:|---:|---:|---:|---:|']
    for r in rows:
        lines.append('| `%s` | %s | %s | %.1f | %d | %d | %d | %s | %s |' % (
            r['capture'], r['locomotive'].split()[0], r['partition'], r['duration_s'], r['native_samples'],
            r['crc_bad_records'], r['native_serial_gaps'],
            '%d (%d)' % (r['reference_magnets'], r['reference_expected']) if 'reference_magnets' in r else '—',
            r.get('scored_span_s', '—')))
    lines += ['', 'SHA-256 of every capture is in `CORPUS_MANIFEST.json`.', '',
              '## Known limitations', '',
              '- The reference is route-anchored oracle evidence (see `truth.py`), not physical ground truth. '
              'It is weakest where the background steps or bursts (D1EC 64–76 s and 176–192 s, '
              'F420 39–100 s, 278F, 54CB).',
              '- Native `direction` is 0 throughout `54CB76FF…` although the loco moved; both directions were tried there.',
              '- `278F53E8…` contains wide flat-topped excursions (slow crawl) beyond the 1.5 s top scale.',
              '- From Oct 26, MM066, MM067 and MM069 read with the polarity opposite to RouteMap.h on both locos.',
              '- IR records were not decoded; no IR-derived speed is used anywhere.',
              '- Toby (`transfer`) has no template of his own; the Otto Oct-15 template is applied unchanged.']
    (HERE / 'CORPUS_MANIFEST.md').write_text('\n'.join(lines) + '\n')


if __name__ == '__main__':
    main()

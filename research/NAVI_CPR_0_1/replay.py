"""Replay original acquisition samples from CSV without map or motion inputs."""
import argparse
import csv
from pathlib import Path
import numpy as np
from experiment import dump, load_model, persist_replay, run_record
from prepare import NATIVE, blocks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--csv', required=True, type=Path,
                        help='original samples with t_us, adc, optional serial columns')
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    rows = list(csv.DictReader(open(args.csv)))
    native = np.zeros(len(rows), dtype=NATIVE)
    for i, row in enumerate(rows):
        native[i]['serial'] = int(row.get('serial', i))
        native[i]['us'] = int(row['t_us'])
        native[i]['adc'] = int(row['adc'])
    args.out.mkdir(parents=True, exist_ok=True)
    binary = args.out / 'input_native.bin'
    trace = args.out / 'input_bins.npz'
    native.tofile(binary)
    metadata = blocks(native, trace)
    cfg, model, _ = load_model()
    replay = run_record('custom', cfg, data_path=trace, native_path=binary)
    persist_replay(replay, model, output_dir=args.out)
    dump(args.out / 'input_summary.json', metadata)
    print(f"{len(replay['events'])} complete candidates; decisions: {args.out / 'custom_journal.jsonl'}")


if __name__ == '__main__':
    main()

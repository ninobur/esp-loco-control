#!/usr/bin/env python3
"""navlab artifact 1 of 5: the log normalizer.

QUORUM_REACHABILITY_RECOVERY_PLAN.md requires "a committed log normalizer
producing records with locomotive, firmware version, direction, route
interval, spacing, commanded/actual PWM history, voltage when available, dt,
peak, duration, polarity, baseline drift, firmware verdict and later event
classification."

One marker event in, one JSONL record out. The record is the unit everything
downstream consumes: the timing-database generator (artifact 2) aggregates
these into envelopes, and the comparison report (artifact 5) cites them.

Later-event classification is HEURISTIC and says so: every label carries a
`label_basis`, and events the heuristics cannot decide are `uncertain`, never
guessed. The labels:
  genuine   - the event sits inside a stretch of >= AGREE_WINDOW subsequent
              accepted events whose polarity all agrees with the map, with no
              quorum correction opening within that window;
  phantom   - the firmware itself discarded it (PHANTOM_REJECTED /
              QUARANTINE_DISCARDED) AND no quorum correction within the
              following window contradicts that verdict; or a following
              adoption removed exactly this event's contribution;
  uncertain - everything else, including every event inside an open quorum
              incident. Uncertainty is data, not failure.

Both capture formats parse (epoch-space Mac captures, ISO-tab Pi runlogs),
matching tests/extract_session.py.

Usage:
  python3 tools/navlab/normalize_log.py --capture <log> [--capture <log2> ...]
      --out <records.jsonl> [--map-source firmware/QUORUM/QUORUM.ino]
"""
import argparse, datetime, json, pathlib, re, sys

AGREE_WINDOW = 5

def parse_lines(path):
    with open(path, 'r', errors='replace') as fh:
        for n, raw in enumerate(fh, 1):
            raw = raw.rstrip('\n')
            if not raw or raw.startswith('#'):
                continue
            if '\t' in raw:
                parts = raw.split('\t', 2)
                if len(parts) < 3: continue
                try: ts = datetime.datetime.fromisoformat(parts[0]).timestamp()
                except ValueError: continue
            else:
                parts = raw.split(' ', 2)
                if len(parts) < 3: continue
                try: ts = float(parts[0])
                except ValueError: continue
            yield n, ts, parts[1], parts[2]

def load_map(src):
    s = open(src).read()
    dna = [int(x) for x in re.findall(r'\d+',
           s.split('const uint8_t NGR_DNA1[DNA_N] PROGMEM = {')[1].split('};')[0])]
    spc = [int(x) for x in re.findall(r'\d+',
           s.split('static const uint16_t spacingMm[DNA_N] PROGMEM = {')[1].split('};')[0])]
    assert len(dna) == len(spc) == 171
    return dna, spc

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--capture', action='append', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--map-source', default=str(pathlib.Path(__file__).resolve()
                    .parents[2] / 'firmware/QUORUM/QUORUM.ino'))
    args = ap.parse_args()
    dna, spc = load_map(args.map_source)
    pole = {1: 'N', 0: 'S'}

    # pass 1: gather per-loco state timelines and raw marker/nav events
    locos = {}
    def L(lid):
        if lid not in locos:
            locos[lid] = dict(fw=None, boot_n=0, sdir=None, mdir=None,
                              commanded=0, voltage=None, events=[], nav=[])
        return locos[lid]

    for path in args.capture:
        src = pathlib.Path(path).name
        for n, ts, topic, payload in parse_lines(path):
            m = re.match(r'ngr/loco/(\d+)/(.+)', topic)
            if not m: continue
            lid, sub = m.group(1), m.group(2)
            st = L(lid)
            if sub == 'state/bootid':
                try: d = json.loads(payload)
                except ValueError: continue
                st['fw'] = d.get('sketch') or st['fw']
            elif sub == 'state/throttle':
                try: st['commanded'] = int(payload)
                except ValueError: pass
            elif sub == 'state/session_direction':
                st['sdir'] = payload.strip()
            elif sub == 'telem/voltage':
                try: st['voltage'] = float(payload)
                except ValueError: pass
            elif sub == 'alert':
                try: d = json.loads(payload)
                except ValueError: continue
                if not isinstance(d, dict): continue
                if d.get('mdir'): st['mdir'] = d['mdir']
                # True boot identity: uptime_ms going backwards. Firmware-name
                # comparison misses same-sketch reflashes and every plain
                # power cycle; session-level hold-outs need the real cut.
                up = d.get('uptime_ms')
                if isinstance(up, (int, float)):
                    if st.get('last_up') is not None and up < st['last_up'] - 5000:
                        st['boot_n'] += 1
                    st['last_up'] = up
            elif sub == 'mm/marker':
                try: d = json.loads(payload)
                except ValueError: continue
                if not isinstance(d, dict) or d.get('mm') is None: continue
                mm = d['mm'] % 171
                # interval that ENDED at mm, direction-dependent, exactly as
                # navLadder's conserveIntervalIndex does it
                cw = (st['sdir'] or 'CW') == 'CW'
                interval = (mm - 1) % 171 if cw else mm
                st['events'].append(dict(
                    ts=round(ts, 3), source=src, line=n,
                    loco=lid, fw=st['fw'], boot=st['boot_n'],
                    session_dir=st['sdir'], motor_dir=st['mdir'],
                    mm=mm, interval=interval, spacing_mm=spc[interval],
                    pwm_actual=d.get('pwm'), pwm_commanded=st['commanded'],
                    voltage_v=d.get('v', st['voltage']),
                    dt_ms=d.get('dt'), peak=d.get('peak'),
                    duration_ms=d.get('ms'), polarity=d.get('obs'),
                    map_pole=pole[dna[mm]], drift=d.get('drift'),
                    timing_gate=d.get('timing_gate'),
                    dt_expected=d.get('dt_expected'),
                ))
            elif sub == 'state/nav':
                try: d = json.loads(payload)
                except ValueError: continue
                if isinstance(d, dict) and d.get('event'):
                    st['nav'].append((ts, d['event']))

    # pass 2: firmware verdict + later-event classification
    total = 0
    with open(args.out, 'w') as out:
        for lid, st in sorted(locos.items()):
            evs, nav = st['events'], st['nav']
            ni = 0
            for i, e in enumerate(evs):
                # firmware verdict: nearest nav decision at/after this event
                while ni < len(nav) and nav[ni][0] < e['ts'] - 0.05:
                    ni += 1
                verdicts = [ev for t, ev in nav[ni:ni+4] if t <= e['ts'] + 0.6]
                e['fw_verdict'] = (
                    'rejected' if 'PHANTOM_REJECTED' in verdicts else
                    'quarantined' if 'QUARANTINED' in verdicts else
                    'disagree' if 'DISAGREE' in verdicts else
                    'agree' if 'AGREE' in verdicts else
                    e['timing_gate'] or 'unknown')
                # later-event classification
                fwd = evs[i+1:i+1+AGREE_WINDOW]
                incident = any(ev in ('QUORUM_OPEN','QUORUM_TIED','NO_QUORUM')
                               for t, ev in nav[ni:ni+12]
                               if t <= e['ts'] + 30)
                clean = (len(fwd) == AGREE_WINDOW and
                         all(f['polarity'] == f['map_pole'] for f in fwd) and
                         all(f['boot'] == e['boot'] for f in fwd))
                if e['fw_verdict'] in ('rejected',) and not incident:
                    e['label'], e['label_basis'] = 'phantom', 'fw_rejected_no_contradiction'
                elif clean and e['polarity'] == e['map_pole'] and not incident:
                    e['label'], e['label_basis'] = 'genuine', f'clean_{AGREE_WINDOW}_window'
                else:
                    e['label'], e['label_basis'] = 'uncertain', \
                        'incident_window' if incident else 'insufficient_context'
                out.write(json.dumps(e) + '\n')
                total += 1
    print(f'{total} records -> {args.out} '
          f'({", ".join(f"{l}:{len(s[chr(101)+chr(118)+chr(101)+chr(110)+chr(116)+chr(115)])}" for l, s in sorted(locos.items()))})')

if __name__ == '__main__':
    sys.exit(main())

"""Shared paths and constants. Sources are read only."""
import pathlib
ROOT = pathlib.Path(__file__).resolve().parents[1]
NG = pathlib.Path.home() / 'NGR/NGR-Files'
HG = NG / 'hall-ground-20261015/derived'          # native Hall + frozen ADC-only encounters
RP = NG / 'navi-rvm-replay-20261016/derived'      # NAVI selections / map export (scoring + annotation only)
D = ROOT / 'derived'; RES = ROOT / 'results'; FIG = ROOT / 'figures'
BOOT1 = '15549e9385faa4d5'   # model construction + held-out-lap evaluation
BOOT2 = '4d8083de79a0e299'   # NOT USED: reserved holdout (see report section 2)
DT_MS = 20                   # the existing 20 ms Hall tracing

import os
VARIANT = os.environ.get('RVM_VARIANT', '')          # '' = primary; 'shape_only' = ablation without log(A/noise); 'causal_level' = trailing level/noise
if VARIANT:
    RES = RES / f'ablation_{VARIANT}'; RES.mkdir(exist_ok=True)

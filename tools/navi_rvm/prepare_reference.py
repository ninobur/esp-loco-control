#!/usr/bin/env python3
"""Freeze existing October 15 reference data; extract causal native-block fixtures.
No source recording or physical route map is modified. Requires numpy.
"""
from pathlib import Path
import argparse, csv, hashlib, json
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'firmware/test-programs/NAVI_RVM'
EVIDENCE = ROOT / 'tests/navi_rvm'
parser = argparse.ArgumentParser()
parser.add_argument('data_root', type=Path)
args = parser.parse_args()
D = args.data_root / 'navi-rvm-replay-20261016/derived'
G = args.data_root / 'hall-ground-20261015/derived'
meta = json.loads((D/'template_meta.json').read_text())
pos = np.genfromtxt(D/'map_positions.csv', delimiter=',', names=True)['map_mm']
lap = int(json.loads((D/'map_meta.json').read_text())['lap_mm'])
grid = np.arange(0, lap, 5)
hashes = {}
def source(p):
    hashes[str(p.relative_to(args.data_root))] = hashlib.sha256(p.read_bytes()).hexdigest()
def array(name, rows, typ):
    return f'inline constexpr {typ} {name}[] = {{\n'+',\n'.join(rows)+'\n};\n'
h = '#pragma once\n#include "RvmTypes.h"\nnamespace rvm_reference {\n'
h += f'inline constexpr uint32_t version=0x20261018;\ninline constexpr int lapMm={lap}, gridMm=5, cells={len(grid)};\n'
for name, direction in [('CW_L1','cw'),('CCW_L1','ccw')]:
    f=D/f'template_{name}.npy'; source(f); a=np.load(f)
    # Data-derived broad background estimate, not a live per-MM baseline gate.
    baseline=float(np.median(a)); peaks=[]
    for p in pos:
        xx=np.arange(int(p)-120,int(p)+140)
        peaks.append(int(xx[np.argmax(np.abs(a[xx%lap]-baseline))])%lap)
    means=np.interp(grid,np.arange(len(a)),a)
    slope=np.abs(np.gradient(a))[grid]
    # Fixed soft corridor: inherited replay jitter + sensor dispersion; limited laps.
    scale=np.sqrt(1.5**2+(2.5*slope)**2+(0.02*(means-baseline))**2)
    h+=array(direction+'Cells',[f'{{{round(m*4)},{max(6,round(s*4))}}}' for m,s in zip(means,scale)],'rvm::Cell')
    h+=array(direction+'Peaks',[str(x) for x in peaks],'uint16_t')
    # Baseline seam is the quietest 30-mm reference neighborhood between peaks.
    seams=[]; baselines=[]
    for j in range(171):
        prev=(j-(1 if direction=='cw' else -1))%171
        sg=1 if direction=='cw' else -1
        distance=((peaks[j]-peaks[prev])*sg)%lap
        xx=peaks[prev]+sg*np.arange(30,max(31,distance-30))
        energy=np.array([np.mean((a[(x+np.arange(-15,16))%lap]-baseline)**2) for x in xx])
        s=int(xx[np.argmin(energy)])%lap; seams.append(s)
        baselines.append(round(float(np.median(a[(s+np.arange(-15,16))%lap]))*4))
    h+=array(direction+'Seams',[str(x) for x in seams],'uint16_t')
    h+=array(direction+'Baselines',[str(x) for x in baselines],'int16_t')
    f=D/f'speed_model_{name}.csv'; source(f)
    rows=list(csv.DictReader(f.open())); speeds=[]
    for ramp in [-1,0,1]:
        rr=sorted([r for r in rows if int(r['ramp'])==ramp],key=lambda r:int(r['bin']))
        speeds += [round(float(np.interp(p,[int(r['bin']) for r in rr],[float(r['med']) for r in rr]))) for p in range(0,256,5)]
    h+=array(direction+'Speeds',[str(x) for x in speeds],'uint16_t')
    h+=f'inline constexpr rvm::Model {direction}={{{direction}Cells,{direction}Peaks,{direction}Seams,{direction}Baselines,{direction}Speeds,cells,lapMm,5,171,version}};\n'
h+='}\n'; (OUT/'RvmReference.h').write_text(h)
source(D/'template_meta.json'); source(D/'map_positions.csv'); source(D/'map_meta.json')

N=np.dtype([('serial','<u4'),('us','<u8'),('adc','<i2'),('pwm','u1'),('direction','u1')])
# Separate calibration and evaluation windows; the exploratory CW case was already inspected. Legacy selections are comparison
# timestamps, never recognizer inputs; their geographic labels are not independent truth.
cases=[('calibration_cw','15549e9385faa4d5',1840.8,1865.0,124,1),
       ('heldout_cw','4d8083de79a0e299',38.0,69.0,146,1),
       ('evaluation_wrap_cw','4d8083de79a0e299',96.5,124.5,157,1),
       ('multilobe_cw','15549e9385faa4d5',1487.0,1506.0,156,1),
       ('evaluation_ccw','15549e9385faa4d5',3167.6,3197.5,121,-1),
       ('grillers_stall','15549e9385faa4d5',400.0,407.0,63,1)]
selections=list(csv.DictReader((D/'selections_scoring_only.csv').open()))
for name,boot,start,end,mm,sg in cases:
    f=G/f'native_{boot}.bin'; a=np.memmap(f,mode='r',dtype=N)
    ids=np.searchsorted(a['us'],[int(start*1e6),int(end*1e6)])
    a=a[ids[0]:ids[1]]
    f2=G/'ir_coordinate_source.csv'
    ir=[r for r in csv.DictReader(f2.open()) if r['boot']==boot]
    it=np.array([int(r['us']) for r in ir]); iu=np.array([int(r['um']) for r in ir])
    rows=[]
    # 20 ms disjoint causal means, assigned last-source time. Firmware also splits
    # at PWM/direction changes; these excerpts use the final setting per block.
    edges=np.r_[0,np.flatnonzero(np.diff(a['us']//20000))+1,len(a)]
    prior=None
    for l,r in zip(edges[:-1],edges[1:]):
        b=a[l:r]; t=int(b['us'][-1]); ii=int(np.searchsorted(it,t,side='right'))-1
        valid=ii>=0 and t-int(it[ii])<1000000
        speed=0.
        if valid:
            jj=max(0,int(np.searchsorted(it,int(it[ii])-1000000)))
            speed=(iu[ii]-iu[jj])*1000/max(1,int(it[ii])-int(it[jj]))
        gap=(prior is not None and int(b['serial'][0])!=prior+1) or np.any(np.diff(b['serial'])!=1)
        rows.append([int(b['serial'][0]),int(b['serial'][-1]),int(b['us'][0]),t,float(b['adc'].mean()),int(b['pwm'][-1]),int(b['direction'][-1]),int(gap),int(valid),round(speed,5)])
        prior=int(b['serial'][-1])
    p=EVIDENCE/'fixtures'/f'{name}.csv'
    with p.open('w') as o:
        w=csv.writer(o);w.writerow(['serial_first','serial_last','time_first_us','time_last_us','hall','pwm','direction','gap','ir_valid','ir_mm_s']);w.writerows(rows)
    # Declaration for excerpt is explicit test setup, not inferred by the recognizer.
    compare=[r for r in selections if r['boot']==boot and start*1e6<=int(r['open_us'])<end*1e6]
    (p.with_suffix('.json')).write_text(json.dumps({'boot':boot,'start_s':start,'end_s':end,'declared_mm':mm,'direction':sg,'native_source':str(f.relative_to(args.data_root)),'native_byte_range':[int(ids[0])*N.itemsize,int(ids[1])*N.itemsize],'native_excerpt_sha256':hashlib.sha256(a.tobytes()).hexdigest(),'scoring_only_legacy_selections':compare},indent=2)+'\n')
(EVIDENCE/'reference_provenance.json').write_text(json.dumps({'source_hashes':hashes,'reference_passes':{k:{kk:vv for kk,vv in v.items() if kk!='opening_coord_mm'} for k,v in meta.items() if k!='CW_L2'},'limitations':['Single recorded pass per direction; no independently validated multi-day statistical corridor.','Spatial interpolation in inherited offline reference; no live Hall samples fabricated.','Reference peak/seam coordinates are waveform annotations, not edits to the geographic route map.','Labels from old NAVI are comparison evidence, not independent physical truth.']},indent=2)+'\n')
print('Wrote fixed reference and',len(cases),'causal recorded excerpts')

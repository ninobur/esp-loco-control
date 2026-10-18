#!/usr/bin/env python3
"""Plot unchanged replay data and the read-only forensic trace; no model fitting."""
from pathlib import Path
import os
os.environ.setdefault('MPLCONFIGDIR','/tmp/navi-rvm-mpl')
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import csv
ROOT=Path(__file__).resolve().parents[2]
D=ROOT/'tests/navi_rvm_manual/forensics'
def read(name):
    return [{k:float(v) for k,v in r.items()} for r in csv.DictReader((D/name).open())]
f=read('frames.csv'); p=read('path_to_153.csv'); ref=read('reference.csv'); a=read('anchors.csv')
f=[r for r in f if 43.5<=r['time_s']<=54.5]
p=[r for r in p if 43.5<=r['time_s']<=51.3]
fig,axes=plt.subplots(3,1,figsize=(13,11),layout='constrained')
ax=axes[0]
ax.plot([r['time_s'] for r in f],[r['corrected_hall'] for r in f],label='Observed Hall + fixed initial offset',color='#165e9b')
ax.plot([r['time_s'] for r in p],[r['reference_mean'] for r in p],label='Reference along eventual MM153 winning ancestry',color='#c7511f',alpha=.85)
ax.axvline(46.619709,color='#98222b',ls='--');ax.axvline(51.2999,color='#98222b',ls='--')
ax.set(title='The MM150 commitment precedes the large negative observation',ylabel='Hall ADC counts',xlabel='Recording time (seconds)')
ax.legend(loc='lower right');ax.grid(alpha=.2)
ax=axes[1]
ax.plot([r['time_s'] for r in f],[r['phase'] for r in f],label='Winning phase at each update',color='#165e9b')
ax.plot([r['time_s'] for r in p],[r['phase'] for r in p],label='Ancestry of the MM153 commitment',color='#c7511f',ls='--')
anchor=next(r for r in a if r['mm']==150)
q=[r for r in f if r['time_s']>=anchor['time_s']]
ax.plot([r['time_s'] for r in q],[anchor['phase']+r['pwm_distance']-anchor['pwm_distance'] for r in q],label='PWM-only estimate from MM150 commitment',color='#548844',ls=':')
for mm,x in [(149,45334),(150,45637),(151,45943),(152,46272),(153,46551),(154,46847)]:
    ax.axhline(x,color='gray',alpha=.2);ax.text(54.5,x,f' MM{mm}',va='center',fontsize=8)
for r in a:
    if 43.5<r['time_s']<54.5:
        ax.scatter(r['time_s'],r['phase'],color='#98222b',zorder=5)
        ax.annotate(f"accepted {int(r['mm'])}",(r['time_s'],r['phase']),xytext=(7,9),textcoords='offset points',fontsize=8)
ax.set(ylabel='Fixed-reference coordinate (mm)',xlabel='Recording time (seconds)',title='Numerical alternatives leap ahead of the approximate PWM prediction')
ax.legend(loc='upper left');ax.grid(alpha=.2)
ax=axes[2]
rr=[r for r in ref if 45200<=r['position_mm']<=47000]
ax.plot([r['position_mm'] for r in rr],[r['mean'] for r in rr],color='#444444',label='Immutable CW reference')
ax.fill_between([r['position_mm'] for r in rr],[r['mean']-r['scale'] for r in rr],[r['mean']+r['scale'] for r in rr],color='#777777',alpha=.2,label='Fixed fuzzy scale')
for mm,x in [(149,45334),(150,45637),(151,45943),(152,46272),(153,46551),(154,46847)]:
    ax.text(x,2075,f'MM{mm}',ha='center',fontsize=9)
q=[r for r in p if 45600<=r['phase']<=45680]
ax.scatter([r['phase'] for r in q],[r['reference_mean'] for r in q],color='#c7511f',s=20,zorder=3,label='Reference endpoints visited across MM150 trough')
ax.annotate('Trough centre bypassed\n45625 → 45650 mm',xy=(45637,1740),xytext=(45830,1740),arrowprops={'arrowstyle':'->'},fontsize=9)
ax.set(xlabel='Fixed-reference coordinate (mm)',ylabel='Hall ADC counts',ylim=(1700,2110),title='MM151–MM153 have similar positive structures; MM150 has a deep negative structure')
ax.legend(loc='lower right');ax.grid(alpha=.2)
fig.savefig(D/'waveform_alignment.png',dpi=160)
fig.savefig(D/'waveform_alignment.pdf')

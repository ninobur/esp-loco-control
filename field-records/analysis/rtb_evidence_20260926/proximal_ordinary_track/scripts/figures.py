"""Figures for REPORT.md. Reads results/ written by settle.py, prox.py, clean.py."""
import os, numpy as np, pandas as pd, matplotlib
matplotlib.use('Agg'); import matplotlib.pyplot as plt
import seg, segs, clean
OUT=seg.OUT; FIG=os.path.join(os.path.dirname(OUT),'figures'); os.makedirs(FIG,exist_ok=True)
C={'otto':'#2a78d6','toby':'#eb6834'}; INK='#3a3a38'; MUTED='#8a8983'
plt.rcParams.update({'font.size':9,'axes.edgecolor':MUTED,'axes.labelcolor':INK,'xtick.color':MUTED,'ytick.color':MUTED,
                     'axes.spines.top':False,'axes.spines.right':False,'axes.grid':True,'grid.color':'#e6e5e0','grid.linewidth':.6,'lines.linewidth':2})
# F1 pooled lobe profile
P=pd.read_csv(os.path.join(OUT,'lobe_profile_pooled.csv'))
fig,axs=plt.subplots(1,2,figsize=(10,3.6),sharey=True)
for ax,side,ttl in zip(axs,['lead','trail'],['Before the core (leading side)','After the core (trailing side)']):
    for loco in ['otto','toby']:
        g=P[(P.loco==loco)&(P.side==side)&(P.n>=4)]; x=g.x_W+0.125
        ax.fill_between(x,g.p10,g.p90,color=C[loco],alpha=.15,lw=0); ax.plot(x,g['median'],color=C[loco],label=f'{loco.capitalize()} median (band p10–p90 across events)')
    ax.axhline(0,color=MUTED,lw=.8); ax.set_ylim(-25,20); ax.set_xlim(0,7)
    ax.set_title(ttl,color=INK,loc='left'); ax.set_xlabel('distance from half-max edge of core (W = core half-max widths)')
axs[0].legend(frameon=False,loc='lower right'); axs[0].set_ylabel('41-ms median minus far ordinary level\n(counts, + = core direction)')
plt.tight_layout(); plt.savefig(os.path.join(FIG,'f1_lobe_profiles.png'),dpi=110); plt.close()
# F2 sliding proximal window error
S=pd.read_csv(os.path.join(OUT,'sliding_x1.0.csv'))
fig,ax=plt.subplots(figsize=(6.5,3.6))
for loco in ['otto','toby']:
    g=S[S.loco==loco]; ax.plot(g.N,g.p95,color=C[loco],marker='o',ms=5); ax.plot(g.N,g['max'],color=C[loco],ls=':',lw=1.4)
    ax.text(g.N.iloc[0]*0.9,g.p95.iloc[0]+0.4,f'{loco.capitalize()} p95',color=INK,ha='right')
ax.set_xscale('log'); ax.set_xticks([5,10,20,50,100,200,600]); ax.set_xticklabels(['5','10','20','50','100','200','600'])
ax.set_xlabel('proximal window length N (clean samples, 1 kHz ≈ ms)'); ax.set_ylabel('|median(prev N) − median(next 100)|\n(counts)')
ax.set_title('How short can a proximal ordinary-track window be?  solid p95, dotted max',color=INK,loc='left',fontsize=9)
plt.tight_layout(); plt.savefig(os.path.join(FIG,'f2_window_length.png'),dpi=110); plt.close()
# F3 naive freeze bias
X=pd.read_csv(os.path.join(OUT,'prox_x1.0.csv')); Ds=[0,0.5,1,1.5,2,2.5,3,3.5]
fig,ax=plt.subplots(figsize=(6.5,3.6))
for loco,off in [('otto',-0.04),('toby',0.04)]:
    g=X[X.loco==loco]; med=[g[f'naive_N100_D{D}'].median() for D in Ds]
    lo=[g[f'naive_N100_D{D}'].min() for D in Ds]; hi=[g[f'naive_N100_D{D}'].max() for D in Ds]
    xs=np.array(Ds)+off; ax.vlines(xs,lo,hi,color=C[loco],lw=1.2,alpha=.6); ax.plot(xs,med,color=C[loco],marker='o',ms=5)
    ax.text(xs[-1]+0.1,med[-1]+(2 if loco=='otto' else -3),loco.capitalize(),color=INK)
ax.axhspan(-2,2,color='#e6e5e0',alpha=.6,lw=0); ax.axhline(0,color=MUTED,lw=.8)
ax.set_xlabel('reference window ends D·W before departure (core start)'); ax.set_ylabel('median(100 samples) − clean pre-interval median\n(counts, core-signed; − = leading lobe)')
ax.set_title('Reference frozen at departure absorbs the leading lobe (median, min–max)',color=INK,loc='left',fontsize=9)
plt.tight_layout(); plt.savefig(os.path.join(FIG,'f3_freeze_lag.png'),dpi=110); plt.close()
# F4 example segmentation
B=clean.build(1.0); want=['toby_difficult_disagree','otto_mid_pwm_N_seq41887']
fig,axs=plt.subplots(2,1,figsize=(11,5.4))
for ax,w in zip(axs,want):
    s=[x for x in B if w in x['name']][0]; d=s['d']; t=d.t.values; r=d.raw.values
    m=np.median(r); ax.plot(t,r,color=C[s['loco']],lw=.6)
    for iv in s['ivs']: ax.axvspan(t[iv['a']],t[iv['b']-1],color='#1baf7a',alpha=.13,lw=0)
    for e in s['ev']: ax.axvspan(e['t_a'],e['t_b'],color=MUTED,alpha=.18,lw=0)
    ax.set_ylim(m-35,m+30); ax.set_xlim(0,t[-1]); ax.set_title(w+'  (green = clean ordinary track; grey = core |dev|>60; unshaded = lobe exclusion; y clipped)',color=INK,loc='left',fontsize=8.5)
    ax.set_ylabel('raw (counts)')
axs[-1].set_xlabel('ms from window start')
plt.tight_layout(); plt.savefig(os.path.join(FIG,'f4_segmentation_examples.png'),dpi=110); plt.close()
print('figures written to',FIG)

"""R1 block strip: 10 ms blocks of native Hall ADC (mean/min/max/n), PWM, direction, gap flag.
Read-only on sources. No interpolation of ADC across gaps: empty blocks are n=0."""
import numpy as np, sys, pathlib
ROOT=pathlib.Path(__file__).resolve().parents[1]
SRC=ROOT.parent/'hall-ground-20261015/derived'
N=np.dtype([('serial','<u4'),('us','<u8'),('adc','<i2'),('pwm','u1'),('direction','u1')])
for boot in ['15549e9385faa4d5','4d8083de79a0e299']:
    a=np.memmap(SRC/f'native_{boot}.bin',mode='r',dtype=N)
    us=a['us'].astype(np.int64); adc=a['adc'].astype(np.float64)
    t0=us[0]//10000*10000; b=(us-t0)//10000; nb=int(b[-1])+1
    n=np.bincount(b,minlength=nb); s=np.bincount(b,adc,nb); s2=np.bincount(b,adc*adc,nb)
    mean=np.where(n>0,s/np.maximum(n,1),np.nan)
    mn=np.full(nb,np.nan); mx=np.full(nb,np.nan)
    np.fmin.at(mn,b,adc); np.fmax.at(mx,b,adc)
    pwm=np.full(nb,-1,np.int16); pwm[b]=a['pwm']; dr=np.full(nb,-1,np.int8); dr[b]=a['direction']
    # serial continuity: a block is 'gap' if it contains or follows a serial jump
    ser=a['serial'].astype(np.int64); jump=np.where(np.diff(ser)!=1)[0]
    gap=np.zeros(nb,bool); gap[b[jump]]=True; gap[b[jump+1]]=True; gap|=n==0
    np.savez_compressed(ROOT/f'derived/blocks_{boot}.npz',t_us=t0+np.arange(nb)*10000+5000,n=n,mean=mean,min=mn,max=mx,
        sd=np.sqrt(np.maximum(s2/np.maximum(n,1)-np.nan_to_num(mean)**2,0)),pwm=pwm,dir=dr,gap=gap)
    print(boot,nb,'blocks',int((n==0).sum()),'empty',int(gap.sum()),'gap-flagged',len(jump),'serial jumps')

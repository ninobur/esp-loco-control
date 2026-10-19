import sys, numpy as np
z = np.load('ir_raw_run8.npz'); raw = z['raw'].astype(float); flg = z['flags']; fs, pt = z['fs'], z['pt']; f0 = int(z['first_sample'])
a = np.polyfit(fs, pt, 1)[0]; b = np.percentile(pt - (fs + 96) * a, 1)   # sample i at Pi time a*i + b; 1st percentile, NOT min (the min is a 0.67 s outlier)
OFF = 1792430929.0725                                                    # Pi = Otto + OFF (NSR status lower envelope)
idx = np.arange(len(raw)) + f0; t_otto = a * idx + b - OFF
raw[raw < 0] = np.nan
nat = np.load(sys.argv[1]); us = nat['us'].astype(np.int64); o = np.argsort(us); hu = us[o] / 1e6; hh = nat['adc'][o].astype(float); hp = nat['pwm'][o]
s0, s1, step = float(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4])
for s in np.arange(s0, s1, step):
    m = (t_otto >= s) & (t_otto < s + step); r = raw[m]; f = flg[m]
    mh = (hu >= s) & (hu < s + step)
    ok = np.isfinite(r)
    print('%7.2f IR n%4d min %5.0f max %5.0f mean %6.1f rise %d fall %d | Hall %6.1f pwm %3d' % (
        s, ok.sum(), np.nanmin(r) if ok.any() else -1, np.nanmax(r) if ok.any() else -1, np.nanmean(r) if ok.any() else -1,
        int((f & 0x2000 > 0).sum()), int((f & 0x4000 > 0).sum()), hh[mh].mean() if mh.any() else -1, hp[mh].max() if mh.any() else -1))

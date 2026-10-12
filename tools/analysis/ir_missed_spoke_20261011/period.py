import sys, collections
sys.path.insert(0, sys.argv[3])
from rule185 import load_fixture, load_events
segs = load_fixture(sys.argv[2]) if sys.argv[1]=='fx' else load_events(sys.argv[2])
spacing=collections.Counter(); per_min=collections.Counter(); ncand=0
for seg in segs:
    last=None
    for k in range(2,len(seg)):
        a=seg[k-1][1]-seg[k-2][1]; b=seg[k][1]-seg[k-1][1]
        if 100*b>185*a:
            ncand+=1
            if last is not None: spacing[seg[k][0]-last]+=1
            last=seg[k][0]
            if seg[k][3]: per_min[seg[k][3][:16]]+=1
print('candidates',ncand,'pulse spacing between successive candidates (same segment):',sorted(spacing.items(),key=lambda x:-x[1])[:12])
if per_min: print('per minute:',sorted(per_min.items()))

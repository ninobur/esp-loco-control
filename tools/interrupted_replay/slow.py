import math
from machine import *
def arc(peak,width):
    s=width/5.2
    return [int(peak*math.exp(-0.5*((t-width/2)/s)**2)) for t in range(width)]
print("="*86)
print("D. SLOW CROSSINGS WITH NO STOP -- can the settle detector fire spuriously?")
print("   (change across a 400 ms window at the apex, vs the +/-8 count band)")
print("="*86)
ok=[]
for w in (700,1400,2500,4000,6000,9000):
    s=w/5.2
    d400=220*(1-math.exp(-0.5*(200/s)**2))
    print("\n%d ms crossing: apex moves %.1f counts in 400 ms  -> %s"
          %(w,d400,"cannot settle" if d400>8 else "CAN settle (apex looks flat)"))
    ok.append(report("   %d ms crossing, no stop"%w,[0]*300+arc(220,w)+[0]*800,213,1))
print("\n"+"="*86)
print("%d of %d slow crossings still yield exactly one advance"%(sum(ok),len(ok)))
print("="*86)

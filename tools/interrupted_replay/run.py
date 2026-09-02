import math
from machine import *

def arc(peak,width,n_before=0):
    s=1400/5.2 if width is None else width/5.2
    return [int(peak*math.exp(-0.5*((t-width/2)/s)**2)) for t in range(width)]

def stop_at(peak=220,frac=0.5,rising=True,width=1400,dwell=4000,gain=213):
    """Cross a magnet, stop where the field is frac*peak, dwell, finish the crossing."""
    s=width/5.2; half=width/2
    off=s*math.sqrt(max(1e-9,-2*math.log(max(1e-9,frac))))
    tstop=int(half-off) if rising else int(half+off)
    g=lambda t:int(peak*math.exp(-0.5*((t-half)/s)**2))
    out=[0]*300+[g(t) for t in range(tstop)]
    out+=[g(tstop)]*dwell
    out+=[g(t) for t in range(tstop,width)]+[0]*600
    return out,gain

ok=[]
print("="*86); print("A. THE REAL CAPTURES"); print("="*86)
for title,rel,want in [
 ("finding 09 A -- DC offset, Toby stationary throughout",
  "20260901_mm41/waveform_20260901T162648_668_slot3.csv",0),
 ("finding 09 B -- DC offset held 9m15s",
  "20260901_mm41/waveform_20260901T162648_668_slot4.csv",0),
 ("finding 11 -- Grillers CCW, parked ON MM59",
  "20260901_ccw_mm60/waveform_20260901T182114_489_slot1.csv",1),
 ("finding 13 -- Arches CCW, parked in fringe, MM106 on departure",
  "20260901_arches2/waveform_20260901T191738_707_slot3.csv",1),
]:
    st,gn=capture_stream(rel); ok.append(report("%s  (gain %d)"%(title,gn),st,gn,want))

print("\n"+"="*86); print("B. SYNTHETIC STOPS -- peak 220, gain 213, 1400 ms crossing"); print("="*86)
for name,frac,rising in [
 ("stop on the RISING edge at 15% of peak (33 counts, under the entry margin)",0.15,True),
 ("stop on the RISING edge at 25% of peak (55 counts)",0.25,True),
 ("stop on the RISING edge at 40% of peak (88 counts)",0.40,True),
 ("stop at the PEAK",0.99,True),
 ("stop on the FALLING edge at 40% of peak",0.40,False),
 ("stop on the FALLING edge at 15% of peak",0.15,False),
]:
    s,g=stop_at(frac=frac,rising=rising); ok.append(report(name,s,g,1))

print("\n"+"="*86); print("C. STOPS THAT ARE NOT MAGNETS"); print("="*86)
fr=[0]*300+[i*41//200 for i in range(200)]+[41]*4000
fr+=[41+int(148*math.exp(-0.5*((t-700)/(1400/5.2))**2)) for t in range(1400)]+[0]*600
ok.append(report("parked in a 41-count FRINGE, real magnet on departure",fr,213,1))

off=[0]*200+[68]*9000+[68-68*i//400 for i in range(400)]+[0]*600
ok.append(report("stationary DC offset, no entry, decays away",off,190,0))

ok.append(report("clean crossing, then stops clear of every magnet",
                 [0]*300+arc(220,1400)+[0]*6000,213,1))

print("\n"+"="*86); print("%d of %d cases behave as specified"%(sum(ok),len(ok))); print("="*86)

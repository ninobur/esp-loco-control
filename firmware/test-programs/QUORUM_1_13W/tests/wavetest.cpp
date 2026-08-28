// 1.13W waveform capture test -- drives the REAL detectorSample() through the
// ADC, exactly as the locomotive does. The replay harness injects MarkerEvents
// directly and never calls the detector, so nothing else in the suite exercises
// this path. Written after the same blind spot hid a bug in NAVI's T16.
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <string>
#include <cstdio>
#include <cmath>
// Mirror of harness.cpp's preamble -- the shim globals, and the forward
// declarations the .ino gets free from Arduino's prototype generator but a
// plain translation unit does not.
unsigned long g_hostMillis = 0;
int           g_hostAnalog = 1800;
HostSerial    Serial;
HostWiFiClass WiFi;
HostTwoWire   Wire;
static void publishWarning(const char* text);
#include "../QUORUM_1_13W.ino"

static int checks=0, fails=0;
static void ck(const char* what,bool ok){
  checks++; if(!ok){ fails++; printf("  FAIL %s\n",what); }
  else printf("  ok   %s\n",what);
}
static void tick(){ g_hostMillis++; detectorSample(); }

int main(){
  pubQueue  = xQueueCreate(32,sizeof(PubMsg));
  waveQueue = xQueueCreate(4,sizeof(WaveCap));
  buildTopics();
  primeMedian(1800); baselineCounts=1800; recomputeThresholds();
  actualPwm=90; commandedPwm=90;

  for(int i=0;i<60;i++) tick();                    // quiet approach
  const int DUR=130, PEAK=200;                     // half-sine north excursion
  for(int i=0;i<DUR;i++){
    g_hostAnalog = 1800 + (int)(PEAK*sin(M_PI*(double)i/DUR));
    tick();
  }
  g_hostAnalog=1800;
  for(int i=0;i<80;i++) tick();                    // quiet, and the exit hold

  WaveCap w;
  bool got = (xQueueReceive(waveQueue,&w,0)==pdTRUE);
  ck("a waveform was captured for the passage", got);
  if(got){
    ck("sample count is the full passage, not a handful", w.n>100);
    ck("peak is carried", w.peak>150);
    ck("not truncated at this duration", w.truncated==0);
    printf("     n=%u dur=%ums peak=%d pole=%c\n",w.n,w.durMs,(int)w.peak,w.pole?'N':'S');
    int mx=-999, mxi=-1;
    for(uint16_t i=0;i<w.n;i++) if(w.s[i]>mx){ mx=w.s[i]; mxi=i; }
    ck("stored peak matches reported peak within scale",
       abs(mx*WAVE_SCALE - (int)w.peak) <= WAVE_SCALE*2);
    bool rise=true, fall=true;
    for(int i=1;i<mxi;i++)            if(w.s[i]<w.s[i-1]-1) rise=false;
    for(uint16_t i=mxi+1;i<w.n-2;i++) if(w.s[i]>w.s[i-1]+1) fall=false;
    ck("monotonic rise to the peak", rise);
    ck("monotonic fall after the peak", fall);
    ck("pre-roll captured the quiet approach", w.s[0]==0 && w.s[1]==0);
    publishWave(w,51);
    PubMsg pm; bool pub=(xQueueReceive(pubQueue,&pm,0)==pdTRUE);
    ck("the record was published", pub);
    if(pub){
      ck("published on the wave topic", std::string(pm.topic).find("/mm/wave")!=std::string::npos);
      ck("carries its label", std::string(pm.payload).find("\"mm\":51")!=std::string::npos);
      printf("PAYLOAD %s\n",pm.payload);
    }
  }
  // ---- THE STRONGEST MAGNET ON THE RAILWAY --------------------------------
  // Regression for 2026-08-28: at WAVE_SCALE 2 an int8 sample stored only
  // +/-254 counts, and 14 of the first 204 real captures came back with flat
  // tops -- up to 51 samples pinned on one record. The scale had been sized
  // against this file's 200-count synthetic instead of the railway's measured
  // 305. A test that only ever fires a weak magnet cannot see that, so this
  // fires the strongest one actually observed.
  printf("\n-- strongest observed magnet (305 counts, MM001 on 2026-08-28) --\n");
  for(int i=0;i<60;i++) tick();
  const int SPEAK=305;
  for(int i=0;i<DUR;i++){
    g_hostAnalog = 1800 + (int)(SPEAK*sin(M_PI*(double)i/DUR));
    tick();
  }
  g_hostAnalog=1800;
  for(int i=0;i<80;i++) tick();
  WaveCap w2;
  bool got2=(xQueueReceive(waveQueue,&w2,0)==pdTRUE);
  ck("the strong passage was captured", got2);
  if(got2){
    ck("NO samples clipped at the railway maximum", w2.clipped==0);
    int mx=-9999;
    for(uint16_t i=0;i<w2.n;i++) if(w2.s[i]>mx) mx=w2.s[i];
    printf("     reported pk=%d  stored max=%d counts  clipped=%u\n",
           (int)w2.peak, mx*WAVE_SCALE, w2.clipped);
    ck("stored curve reaches the true peak, not a ceiling",
       abs(mx*WAVE_SCALE-(int)w2.peak) <= WAVE_SCALE*2);
    ck("headroom remains above the observed maximum", 127*WAVE_SCALE > 305);
  }

  printf("\n%d checks, %d failures\n",checks,fails);
  return fails?1:0;
}

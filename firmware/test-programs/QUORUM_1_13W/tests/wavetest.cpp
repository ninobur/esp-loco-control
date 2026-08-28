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
#include "../QUORUM.ino"

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
  printf("\n%d checks, %d failures\n",checks,fails);
  return fails?1:0;
}

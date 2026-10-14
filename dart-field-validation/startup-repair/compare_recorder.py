"""Compare production record bytes before/after storage relocation, including overflow."""
from pathlib import Path
import hashlib
import json
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
relative = "firmware/programs/NAVI_EWO_0_5/NaviSyncRecorder.h"
old = subprocess.check_output(["git", "show", "44d0275:" + relative], cwd=root, text=True)
old = old.replace('"../../common/IrMovementWire.h"',
                  '"' + str(root / "firmware/common/IrMovementWire.h") + '"')
source = r'''
#include RECORDER_HEADER
#include <cstdio>
#include <cassert>
using namespace navi_sync;
int main() {
  Recorder r;assert(r.storageReady());r.begin(9950011, 0x12345678, 0xabcdef1234567890ULL);
  Context c{};c.navMm=40;c.navDir=1;
  unsigned records=0;
  for(unsigned phase=0;phase<3;++phase) {
    // Alternating drained and overflowed histories, with multiple ring wraps.
    for(unsigned i=0;i<32*37;++i) {
      ConsumptionItem x{};x.id=phase*10000+i;x.decisionUs=1000000+x.id*173;
      x.observationUs=x.decisionUs-93;x.serial=i;x.kind=1;x.navMm=i%171;
      x.target=(i+1)%171;x.direction=-1;x.pwm=i%256;x.commandedPwm=(i+7)%256;
      r.addConsumption(x,c);
      if(i%7==0) {
        NaviSnapshot n{};n.tUs=x.decisionUs;n.hallSerial=i;n.mm=i%171;
        n.target=(i+1)%171;n.irUm=x.id*9652;n.kind=i%17;r.addNavi(n,c);
      }
      for(unsigned j=0;j<2;++j) {
        NativeHallItem n{};n.serial=phase*10000+i*2+j;n.tUs=x.decisionUs+j*70;
        n.raw=int16_t(i-700);n.pwm=i%256;n.direction=1;r.addNativeHall(n,c);
      }
      if(i%17==0) {
        ActionSnapshot a{};a.tUs=x.decisionUs;a.lastConsumptionId=x.id;
        a.commandOrder=i;a.commandReceivedUs=x.observationUs;a.kind=2;
        a.pwm=i%256;a.rampUpMs=150;a.rampDownMs=31;
        snprintf(a.topic,sizeof(a.topic),"test/%u",i);
        snprintf(a.payload,sizeof(a.payload),"record_%u",i);
        r.addAction(a,c);
      }
      if(phase!=1) {
        ConsumptionWire w;while(r.popConsumption(w)){fwrite(&w,sizeof(w),1,stdout);++records;}
        ActionWire a;while(r.popAction(a)){fwrite(&a,sizeof(a),1,stdout);++records;}
        NativeHallWire n;while(r.popNativeHall(n)){fwrite(&n,sizeof(n),1,stdout);++records;}
        NaviWire nav;while(r.popNavi(nav)){fwrite(&nav,sizeof(nav),1,stdout);++records;}
      }
    }
    ConsumptionWire w;while(r.popConsumption(w)){fwrite(&w,sizeof(w),1,stdout);++records;}
    ActionWire a;while(r.popAction(a)){fwrite(&a,sizeof(a),1,stdout);++records;}
    NativeHallWire n;while(r.popNativeHall(n)){fwrite(&n,sizeof(n),1,stdout);++records;}
    NaviWire nav;while(r.popNavi(nav)){fwrite(&nav,sizeof(nav),1,stdout);++records;}
  }
  fprintf(stderr,"records=%u consumption_drops=%u action_drops=%u native_drops=%u navi_drops=%u\n",
    records,r.consumptionDrops(),r.actionDrops(),r.nativeDrops(),r.naviDrops());
}
'''
with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    (tmp / "old.h").write_text(old)
    (tmp / "emit.cpp").write_text(source)
    results = []
    for name, header in [("before", tmp / "old.h"), ("after", root / relative)]:
        exe = tmp / name
        subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", '-DRECORDER_HEADER="' + str(header) + '"',
                        str(tmp / "emit.cpp"), "-o", str(exe)], check=True)
        p = subprocess.run([str(exe)], capture_output=True, check=True)
        results.append(p)
    assert results[0].stdout == results[1].stdout
    assert results[0].stderr == results[1].stderr
    report = {"pass": True, "baseline": "44d0275", "bytes": len(results[1].stdout),
              "sha256": hashlib.sha256(results[1].stdout).hexdigest(),
              "counters": results[1].stderr.decode().strip()}
    Path(__file__).with_name("recorder-comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))

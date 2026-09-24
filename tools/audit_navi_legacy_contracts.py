"""Read-only host probes for the legacy-contract audit, not an acceptance gate.

Expected outputs describe the defects at 57d34d6; intentionally revisit these
assertions when their fixes land. No broker, GPIO, serial or hardware access.
"""
import ast
from pathlib import Path
import re
import subprocess
import tempfile
from contextlib import nullcontext

ROOT = Path(__file__).resolve().parents[1]
VARIANT = ROOT / "firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH"
source = (VARIANT / "NAVI_COHERENCE_0_6_IR_HEALTH.ino").read_text()
# Review the committed dashboard, not collaborators' uncommitted changes.
dashboard = subprocess.check_output(
    ["git", "show", "HEAD:server/ngr_app_v1_11_2.py"], cwd=ROOT, text=True)
end = next(node for node in ast.parse(dashboard).body
           if isinstance(node, ast.FunctionDef) and node.name == "dispatcher_endcto")
end.decorator_list = []
calls = []
context = {
    "mqtt_lock": nullcontext(),
    "console_order": lambda: ["manual_toby", "auto_otto"],
    "pub_loco": lambda *args: calls.append(("loco", *args)),
    "pub_dispatcher": lambda *args: calls.append(("dispatcher", *args)),
}
exec(compile(ast.Module(body=[end], type_ignores=[]), "committed_endcto", "exec"), context)
context["dispatcher_endcto"]()
assert ("dispatcher", "stop/manual_toby") in calls
print("Observed END AUTO: dispatcher STOP also sent to console-listed manual Toby")

start = source.index('} else if (!strcmp(leaf,"stop")') + len("} else ")
stop = source[start:source.index('} else if (!strcmp(leaf,"throttle")', start)] + "}"
assignment = re.search(r"estMmPerS=spanMm\(before\.navMm,before\.navDir\)[^;]*;", source)
assert assignment, "Speed calculation changed: review the new implementation"
code = r'''
#include <cassert>
#include <cstring>
#include <iostream>
#include "Navigator.h"
using namespace navi_one;
bool autoRunning=false,autoEnrolled=false;
int target=80;
constexpr int AUTO_STEP_DOWN_MS=31;
void requestPwm(int n,int,int){target=n;}
struct Cmd {const char* topic;};
void stopCommand(){
  Cmd c{"ngr/dispatcher/cmd/stop/9950012"};
  const char* leaf="9950012";const bool dispatcher=true;
''' + stop + r'''
}
ngr_nav::MotionPoint motion(uint64_t pulses,uint64_t us){
  ngr_nav::MotionPoint p{};p.issue=ngr_nav::MotionIssue::None;p.frame=1;
  p.wire.bootId=1;p.wire.capturedUs=us;p.wire.completedPulses=pulses;
  p.wire.pitchUm=9652;p.wire.opticalReason=ir_movement::TRACKING;return p;
}
int main(){
  assert(!autoEnrolled && !autoRunning && target==80);stopCommand();
  assert(target==0);
  std::cout<<"Observed dispatcher STOP: manual/non-enrolled target 80 -> 0\n";
  Navigator nav;nav.declare(39,1,0);
  NavObservation first{};first.openedAtMs=1000;first.polarity=polarityAt(40);
  first.movement=motion(1000,1000000);
  assert(nav.judge(first)==Ruling::Advanced);
  const auto before=nav.status();
  NavObservation later{};later.openedAtMs=3000;later.polarity=polarityAt(42);
  later.movement=motion(1062,3000000);
  assert(nav.judge(later)==Ruling::MissedAndAdvanced && nav.status().navMm==42);
  uint32_t elapsed=2000,estMmPerS=0;
''' + assignment.group() + r'''
  const auto mapped=(spanMm(40,1)+spanMm(41,1))*1000UL/elapsed;
  assert(estMmPerS==150 && mapped==300);
  std::cout<<"Observed MM040->042 / 2 s: published calculation "<<estMmPerS
           <<" mm/s; complete mapped distance / elapsed time "<<mapped<<" mm/s\n";
}
'''
with tempfile.TemporaryDirectory() as tmp:
    cpp = Path(tmp) / "probe.cpp"
    exe = Path(tmp) / "probe"
    cpp.write_text(code)
    subprocess.run(["clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-I", str(VARIANT),
                    str(cpp), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)

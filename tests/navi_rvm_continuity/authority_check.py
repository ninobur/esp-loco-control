"""Static check: navigation has no motor-control authority in NAVI_RVM_CONTINUITY_MANUAL.
1. The recognition/navigation headers mention no actuator or control symbol.
2. In the .ino, actuator writes (ledcWrite / writePwm) occur only in the allowed
   functions, and requestPwm / motor state assignments only in operator, E-STOP
   and battery paths.
3. The navigation functions of the .ino (rvm task, control, event reporting)
   call none of the actuator functions.
Exit 1 on any violation. Prints the call map."""
import re, sys, pathlib, json
import os
SKETCH = os.environ.get('RVM_SKETCH', 'NAVI_RVM_CONTINUITY_MANUAL')        # or NAVI_RVM_CONTINUITY_MANUAL_TOBY
S = pathlib.Path(__file__).resolve().parents[2] / 'firmware/test-programs' / SKETCH
CALLS = ['ledcWrite', 'writePwm', 'requestPwm', 'digitalWrite']
STATE = ['rampTarget', 'actualPwm', 'commandedPwm', 'motorDirection', 'estopped', 'estopAsserted', 'brakeValue',
         'lowVoltage', 'rampUpMs', 'rampDownMs']
def strip_comments(t):
    t = re.sub(r'/\*.*?\*/', '', t, flags=re.S); return re.sub(r'//[^\n]*', '', t)
def used(body):
    out = [c for c in CALLS if re.search(r'\b%s\s*\(' % c, body)]
    out += [v + ' =' for v in STATE if re.search(r'(?<![\w.])%s\s*(?:=(?!=)|\+=|-=|\+\+|--)' % v, body)
            or re.search(r'(?:\+\+|--)%s\b' % v, body)]
    return out
bad = []
# Recognizer and navigator: no PWM, actuator, safety or network symbol at all.
for h in ['RvmShape.h', 'RvmNavigator.h', 'RvmShapeModel.h']:
    t = strip_comments((S / h).read_text())
    for a in CALLS + STATE + ['Pwm', 'pwm', 'PWM', 'estop', 'Estop', 'throttle', 'motor', 'Arduino', 'mqtt']:
        if a in t: bad.append(f'{h} code mentions {a!r}')
# Tracing carries the block's PWM (for the record only); continuity reads only the Hall.
# Neither may touch an actuator or safety state.
for h in ['RvmTracing.h', 'RvmContinuity.h']:
    t = strip_comments((S / h).read_text())
    for a in CALLS + STATE + ['estop', 'Estop', 'throttle', 'motor', 'Arduino', 'mqtt']:
        if a in t: bad.append(f'{h} code mentions {a!r}')
src = strip_comments((S / f'{SKETCH}.ino').read_text())
# crude function splitter: top-level "static ... name(...) {" or "void name() {"
funcs = {}
for m in re.finditer(r'^(?:static\s+)?[\w:<>\*&\s]+?\b(\w+)\s*\([^;{]*\)\s*\{', src, re.M):
    name = m.group(1); i = m.end(); depth = 1
    while depth and i < len(src):
        depth += {'{': 1, '}': -1}.get(src[i], 0); i += 1
    funcs[name] = src[m.end():i]
uses = {f: used(body) for f, body in funcs.items()}
ALLOWED = {
  'writePwm': {'ledcWrite'},
  'serviceRamp': {'writePwm', 'digitalWrite', 'rampTarget =', 'actualPwm =', 'commandedPwm =', 'estopped =', 'rampDownMs ='},
  'requestPwm': {'rampTarget =', 'commandedPwm =', 'rampUpMs =', 'rampDownMs ='}, 'haltStartup': {'writePwm', 'digitalWrite'},
  'setup': {'writePwm', 'digitalWrite'},
  'handleCommand': {'requestPwm', 'estopped =', 'estopAsserted =', 'motorDirection =', 'brakeValue ='},
  'onMqtt': {'estopAsserted ='}, 'serviceBattery': {'requestPwm', 'lowVoltage ='},
}
NAV = ['rvmTask', 'applyControl', 'onDeclare', 'feedRecognizer', 'postContinuity', 'publishSnapshot', 'postEvent', 'serviceNavEvents', 'publishNavState', 'navControl',
       'declare', 'suspendIfDeclared', 'navSnapshot', 'hallTask']
for f, u in uses.items():
    extra = set(u) - ALLOWED.get(f, set())
    if extra: bad.append(f'{f} uses {sorted(extra)}')
for f in NAV:
    if f not in funcs: bad.append(f'navigation function {f} not found')
    elif uses.get(f): bad.append(f'navigation function {f} touches {uses[f]}')
# Navigation never reads a block's PWM or commanded target: not in the sketch, not in the continuity.
pwm_reads = re.findall(r'\b(?:b|a|blk|block|out)\.(?:pwm|cmd)\b(?!\s*=[^=])[^;]*', src)
if pwm_reads or 'continuity.admit(b,' not in src: bad.append(f'block PWM / command read in the sketch: {pwm_reads}')
cont = strip_comments((S / 'RvmContinuity.h').read_text())
if re.search(r'\.(?:pwm|cmd)\b', cont): bad.append('RvmContinuity.h reads a block PWM or commanded target')
report = dict(functions_with_actuator_access={f: u for f, u in uses.items() if u}, navigation_functions=NAV, violations=bad)
print(json.dumps(report, indent=1))
sys.exit(1 if bad else 0)

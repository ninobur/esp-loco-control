from flask import Flask, render_template_string, request, jsonify, redirect, url_for, make_response
from markupsafe import Markup
import threading
import time
import subprocess
import os
import glob
import json
import paho.mqtt.client as mqtt_client

app = Flask(__name__)

def is_authenticated(req):
    return True


# ============================================================================
# MQTT
# ============================================================================
MQTT_BROKER = "127.0.0.1"
MQTT_PORT   = 1883

loco_state = {
    "9950011": {"online":"0","throttle":"0","direction":"1","brake":"0",
                "estop":"0","auto":"0","ce":"0","lowvolt":"0","warning":"",
                "voltage":"--","current":"--","power":"--","block":"--",
                "session_dir":"UNSET","nav_ready":"0","start_interval":"UNSET",
                "mm_current":"--","mm_landmark":"","mm_status":"--",
                "mm_cert":"--","mm_cmd_pwm":"--","mm_gov_pwm":"--",
                "mm_polarity":"--","mm_direction":"--","mm_interval":"--","mm_dt":"--","mm_pkph":"--","mm_speed_state":"estimated",
                "ramp_pwm":"--","last_measured_pkph":"--",
                "profile_cw":"--","profile_ccw":"--","profile_laps":"--","profile_svc":"--","profile_svc_ts":None},
    "9950012": {"online":"0","throttle":"0","direction":"1","brake":"0",
                "estop":"0","auto":"0","ce":"0","lowvolt":"0","warning":"",
                "voltage":"--","current":"--","power":"--","block":"--",
                "session_dir":"UNSET","nav_ready":"0","start_interval":"UNSET",
                "mm_current":"--","mm_landmark":"","mm_status":"--",
                "mm_cert":"--","mm_cmd_pwm":"--","mm_gov_pwm":"--",
                "mm_polarity":"--","mm_direction":"--","mm_interval":"--","mm_dt":"--","mm_pkph":"--","mm_speed_state":"estimated",
                "ramp_pwm":"--","last_measured_pkph":"--",
                "profile_cw":"--","profile_ccw":"--","profile_laps":"--","profile_svc":"--","profile_svc_ts":None},
    "2095111": {"online":"0","throttle":"0","direction":"1","brake":"0",
                "estop":"0","auto":"0","ce":"0","lowvolt":"0","warning":"",
                "voltage":"--","current":"--","power":"--","block":"--",
                "session_dir":"UNSET","nav_ready":"0","start_interval":"UNSET",
                "mm_current":"--","mm_landmark":"","mm_status":"--",
                "mm_cert":"--","mm_cmd_pwm":"--","mm_gov_pwm":"--",
                "mm_polarity":"--","mm_direction":"--","mm_interval":"--","mm_dt":"--","mm_pkph":"--","mm_speed_state":"estimated",
                "ramp_pwm":"--","last_measured_pkph":"--",
                "profile_cw":"--","profile_ccw":"--","profile_laps":"--","profile_svc":"--","profile_svc_ts":None},
}

mqtt_lock = threading.Lock()
mqtt_conn = None

from collections import deque
import datetime
loco_log = {
    "9950011": deque(maxlen=100),
    "9950012": deque(maxlen=100),
    "2095111": deque(maxlen=100),
}
dispatch_log = deque(maxlen=200)

def on_mqtt_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        print("MQTT connected")
        for lid in loco_state:
            base = f"ngr/loco/{lid}"
            client.subscribe(f"{base}/online")
            client.subscribe(f"{base}/state/#")
            client.subscribe(f"{base}/telem/#")
            client.subscribe(f"{base}/mm/mag")
            client.subscribe(f"{base}/mm/motion")
            client.subscribe(f"{base}/mm/speed")
            client.subscribe(f"{base}/profile/status")
    else:
        print(f"MQTT connect failed rc={rc}")

import re

# v1.9 verbose format:
# MM072 Westpoint obs=S dna=S DNA_OK dt=2005 ... cert=1.000 ... cmdPWM=70 govPWM=70 ...
MM_MAG_VERBOSE_RE = re.compile(
    r'^MM(\d{3})\s+(\S*)\s*obs=(\w)\s+dna=(\w)\s+(\S+)\s+dt=(\d+).*?'
    r'cert=([\d.]+).*?cmdPWM=(-?\d+)\s+govPWM=(-?\d+)'
)

# v2.1 compact format (published to mm/mag, detail goes to diag/mm_detail):
# MM050 N DNA_OK CW Interval 049-050 dt=1234ms cert=0.85
# MM063 GRILLERS N DNA_OK CW Interval 062-063 dt=1234ms cert=0.85
MM_MAG_COMPACT_RE = re.compile(
    r'^MM(\d{3})\s+'           # MM number
    r'(?:(\S+)\s+)?'           # optional landmark name
    r'([NS])\s+'               # polarity
    r'(\S+)\s+'                # DNA status (DNA_OK / DNA_CHECK / NONE)
    r'(?:CW|CCW)\s+'           # direction
    r'Interval\s+\d{3}-\d{3}\s+'  # interval
    r'dt=[\d]+ms\s+'           # dt
    r'cert=([\d.]+)'           # cert
)

# CleanCore (PROD_3.0) format published to mm/mag:
# MM063 Grillers obs=N expected=N pos=EXACT_LOCKED dir=DIRECTION_CONFIRMED dt=1412 dt_expected=1380 timing_mode=MOVING effective_pwm_avg=70 seg_mm=315 speed_mm_s=222.7 peakN=45 peakS=2 hallms=23
# MM050  obs=N expected=N pos=EXACT_LOCKED dir=DIRECTION_CONFIRMED dt=1428 ...  (no landmark = empty string)
MM_MAG_CLEANCORE_RE = re.compile(
    r'^MM(\d{3})\s+'            # MM number
    r'(\S+\s+)?'                # optional landmark (non-space token + trailing space)
    r'obs=([NS])\s+'            # observed polarity
    r'expected=[NS?]\s+'        # expected polarity (not captured)
    r'pos=(\w+)\s+'             # position_state
    r'dir=(\w+)\s+'             # direction_state
    r'dt=(\d+)\s+'              # dt ms
    r'dt_expected=[\d.]+\s+'    # dt_expected (not captured)
    r'timing_mode=\w+\s+'       # timing_mode (not captured)
    r'effective_pwm_avg=(\d+)\s+'  # effective PWM
    r'seg_mm=(\d+)'             # segment distance mm (not captured but anchors the match)
)

def parse_mm_mag(payload):
    """Parse mm/mag payload — handles both v1.9 verbose and v2.1 compact formats.
    Returns dict or None if neither format matches.
    """
    # Try verbose first (v1.9 / diag/mm_detail style)
    m = MM_MAG_VERBOSE_RE.match(payload)
    if m:
        mm, landmark, obs, dna, status, dt, cert, cmd_pwm, gov_pwm = m.groups()
        lm = landmark if (landmark and landmark not in ("NONE", "--", "")) else ""
        dir_m  = re.search(r'\b(CW|CCW)\b', payload)
        intv_m = re.search(r'Interval\s+(\d{3}-\d{3})', payload)
        return {
            "mm_current":   mm,
            "mm_landmark":  lm,
            "mm_status":    status,
            "mm_cert":      cert,
            "mm_cmd_pwm":   cmd_pwm,
            "mm_gov_pwm":   gov_pwm,
            "mm_polarity":  obs,
            "mm_direction": dir_m.group(1) if dir_m else "--",
            "mm_interval":  intv_m.group(1) if intv_m else "--",
            "mm_dt":        dt,
        }

    # Try v2.1 compact format:
    # MM033 S DNA_OK CCW Interval 034-033 dt=1428ms cert=0.98
    # MM063 Grillers N DNA_OK CW Interval 062-063 dt=1412ms cert=1.00
    m = MM_MAG_COMPACT_RE.match(payload)
    if m:
        mm, landmark, pol, status, cert = m.groups()
        lm = landmark if (landmark and landmark not in ("NONE", "--", "")) else ""
        # Extract direction and dt from raw payload
        dir_m = re.search(r'\b(CW|CCW)\b', payload)
        dt_m  = re.search(r'dt=(\d+)ms', payload)
        intv_m = re.search(r'Interval\s+(\d{3}-\d{3})', payload)
        return {
            "mm_current":   mm,
            "mm_landmark":  lm,
            "mm_status":    status,
            "mm_cert":      cert,
            "mm_cmd_pwm":   "--",
            "mm_gov_pwm":   "--",
            "mm_polarity":  pol,
            "mm_direction": dir_m.group(1) if dir_m else "--",
            "mm_interval":  intv_m.group(1) if intv_m else "--",
            "mm_dt":        dt_m.group(1) if dt_m else "--",
        }

    # Try CleanCore format
    m = MM_MAG_CLEANCORE_RE.match(payload)
    if m:
        mm, landmark, pol, pos_state, dir_state, dt, eff_pwm, speed = m.groups()
        lm = (landmark or "").strip()
        if lm in ("NONE", "--", "obs=N", "obs=S", "N", "S", ""):
            lm = ""
        status = {
            "EXACT_LOCKED": "DNA_OK",
            "DECLARED":     "DECLARED",
            "UNKNOWN":      "UNKNOWN",
            "CONTESTED":    "DNA_CHECK",
            "STALE":        "STALE",
            "FAULT":        "FAULT",
        }.get(pos_state, pos_state)
        return {
            "mm_current":   f"{int(mm):03d}",
            "mm_landmark":  lm,
            "mm_status":    status,
            "mm_cert":      "--",   # cert comes from state/dna in CleanCore
            "mm_cmd_pwm":   eff_pwm,
            "mm_gov_pwm":   eff_pwm,
            "mm_polarity":  pol,
            "mm_direction": dir_state,
            "mm_interval":  "--",
            "mm_dt":        dt,
        }

    return None

def on_mqtt_message(client, userdata, msg):
    topic   = msg.topic
    payload = msg.payload.decode("utf-8", errors="ignore")
    for lid in loco_state:
        base = f"ngr/loco/{lid}"
        with mqtt_lock:
            if topic.startswith(base):
                subtopic = topic[len(base)+1:]
                if loco_state[lid]["online"] != "1":
                    pass
                elif subtopic.startswith("telem/"):
                    pass
                elif subtopic == "state/lowvolt" and payload == "0":
                    pass
                elif subtopic == "mm/mag":
                    parsed = parse_mm_mag(payload)
                    if parsed:
                        # Log a compact human-readable line for the event feed
                        mm  = parsed.get("mm_current","--")
                        lm  = parsed.get("mm_landmark","")
                        pol = parsed.get("mm_polarity","--")
                        sts = parsed.get("mm_status","--")
                        dr  = parsed.get("mm_direction","--")
                        ct  = parsed.get("mm_cert","--")
                        label = f"MM{mm}" + (f" {lm}" if lm else "") + f" {pol} {sts} {dr} cert={ct}"
                        ts = datetime.datetime.now().strftime("%H:%M:%S")
                        entry = {"ts": ts, "topic": "mm/mag", "value": label}
                        loco_log[lid].appendleft(entry)
                        dispatch_log.appendleft({"ts": ts, "loco": lid, "topic": "mm/mag", "value": label})
                else:
                    ts = datetime.datetime.now().strftime("%H:%M:%S")
                    entry = {"ts": ts, "topic": subtopic, "value": payload}
                    loco_log[lid].appendleft(entry)
                    dispatch_log.appendleft({"ts": ts, "loco": lid, "topic": subtopic, "value": payload})
            if topic == f"{base}/mm/mag":
                parsed = parse_mm_mag(payload)
                if parsed:
                    loco_state[lid].update(parsed)
            elif   topic == f"{base}/online":        loco_state[lid]["online"]    = payload
            elif topic == f"{base}/state/throttle":  loco_state[lid]["throttle"]  = payload
            elif topic == f"{base}/state/direction": loco_state[lid]["direction"] = payload
            elif topic == f"{base}/state/brake":     loco_state[lid]["brake"]     = payload
            elif topic == f"{base}/state/estop":     loco_state[lid]["estop"]     = payload
            elif topic == f"{base}/state/auto":      loco_state[lid]["auto"]      = payload
            elif topic == f"{base}/state/lowvolt":   loco_state[lid]["lowvolt"]   = payload
            elif topic == f"{base}/state/warning":   loco_state[lid]["warning"]   = payload
            elif topic == f"{base}/state/block":     loco_state[lid]["block"]     = payload
            elif topic == f"{base}/state/ce":        loco_state[lid]["ce"]        = payload
            elif topic == f"{base}/state/session_direction": loco_state[lid]["session_dir"] = payload
            elif topic == f"{base}/state/nav_ready": loco_state[lid]["nav_ready"] = payload
            elif topic == f"{base}/state/start_interval": loco_state[lid]["start_interval"] = payload
            elif topic == f"{base}/telem/voltage":   loco_state[lid]["voltage"]   = payload
            elif topic == f"{base}/telem/current":   loco_state[lid]["current"]   = payload
            elif topic == f"{base}/telem/power":     loco_state[lid]["power"]     = payload
            elif topic == f"{base}/mm/motion":
                try:
                    import json as _json
                    d = _json.loads(payload)
                    rp = d.get("ramp_pwm")
                    if rp is not None:
                        loco_state[lid]["ramp_pwm"] = str(rp)
                except Exception:
                    pass
            elif topic == f"{base}/mm/speed":
                try:
                    import json as _json
                    d = _json.loads(payload)
                    if d.get("source") == "SEGMENT_MEASURED":
                        pkph = d.get("pkph")
                        if pkph is not None:
                            loco_state[lid]["last_measured_pkph"] = f"{pkph:.1f}"
                except Exception:
                    pass
            elif topic == f"{base}/profile/status":
                try:
                    import json as _json
                    import datetime as _dt
                    d = _json.loads(payload)
                    pub_at = d.get("published_at")
                    loco_state[lid]["profile_svc_ts"] = pub_at
                    if pub_at:
                        try:
                            age = (_dt.datetime.now(_dt.timezone.utc) -
                                   _dt.datetime.fromisoformat(pub_at)).total_seconds()
                            loco_state[lid]["profile_svc"] = "stale" if age > 120 else (
                                "online" if d.get("service_online") else "offline")
                        except Exception:
                            loco_state[lid]["profile_svc"] = "online" if d.get("service_online") else "offline"
                    else:
                        loco_state[lid]["profile_svc"] = "offline"
                    def _dir_summary(dd):
                        ev = dd.get("loco_event","")
                        ver = dd.get("version")
                        if dd.get("available") and ev == "loaded" and dd.get("loco_source") == "pi":
                            return f"Pi v{ver}"
                        elif ev == "timeout": return "local seed — Pi timeout"
                        elif ev == "invalid_profile": return "local seed — invalid"
                        elif dd.get("available"): return f"Pi v{ver} (not loaded)"
                        elif ev == "requesting": return "waiting"
                        return "--"
                    loco_state[lid]["profile_cw"]  = _dir_summary(d.get("cw",{}))
                    loco_state[lid]["profile_ccw"] = _dir_summary(d.get("ccw",{}))
                    rc = (d.get("cw",{}).get("run_count") or 0) + (d.get("ccw",{}).get("run_count") or 0)
                    loco_state[lid]["profile_laps"] = str(rc)
                except Exception:
                    pass
            elif topic == f"{base}/state/dna":
                try:
                    dna = json.loads(payload)
                    # certainty — both CleanCore and v2.1
                    if "certainty" in dna:
                        loco_state[lid]["mm_cert"] = f'{dna["certainty"]:.3f}'
                    # CleanCore: candidate_mm is current position
                    if "candidate_mm" in dna:
                        loco_state[lid]["mm_current"] = f'{int(dna["candidate_mm"]):03d}'
                    # CleanCore: position_state → DNA status
                    if "position_state" in dna:
                        loco_state[lid]["mm_status"] = {
                            "EXACT_LOCKED": "DNA_OK",
                            "DECLARED":     "DECLARED",
                            "UNKNOWN":      "UNKNOWN",
                            "CONTESTED":    "DNA_CHECK",
                            "STALE":        "STALE",
                            "FAULT":        "FAULT",
                        }.get(dna["position_state"], dna["position_state"])
                    # CleanCore: effective_pwm_avg for PWM display
                    if "effective_pwm_avg" in dna:
                        loco_state[lid]["mm_cmd_pwm"] = str(dna["effective_pwm_avg"])
                        loco_state[lid]["mm_gov_pwm"] = str(dna["effective_pwm_avg"])
                    # v2.1 fallback
                    if "cmdPWM" in dna:
                        loco_state[lid]["mm_cmd_pwm"] = str(dna["cmdPWM"])
                    if "govPWM" in dna:
                        loco_state[lid]["mm_gov_pwm"] = str(dna["govPWM"])
                except Exception:
                    pass

def pub(topic, value):
    global mqtt_conn
    if mqtt_conn and mqtt_conn.is_connected():
        mqtt_conn.publish(topic, str(value), retain=False)

def pub_loco(lid, subtopic, value):
    pub(f"ngr/loco/{lid}/cmd/{subtopic}", value)

def pub_dispatcher(subcmd):
    pub(f"ngr/dispatcher/cmd/{subcmd}", "1")

# ============================================================================
# CAL LOGGER — per-loco mosquitto_sub processes
# ============================================================================
CAL_LOG_DIR = "/home/david/NGR/logs"

cal_procs = {}   # lid -> Popen or None
cal_files = {}   # lid -> filename of current session

def cal_start(lid):
    if cal_procs.get(lid) and cal_procs[lid].poll() is None:
        return False, "already running"
    topic = f"ngr/loco/{lid}/mm/cal"
    from datetime import datetime
    fname = f"{CAL_LOG_DIR}/cal_{lid}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt"
    try:
        proc = subprocess.Popen(
            ["mosquitto_sub", "-h", "127.0.0.1", "-t", topic],
            stdout=open(fname, "a"),
            stderr=subprocess.DEVNULL
        )
        cal_procs[lid] = proc
        cal_files[lid] = fname
        return True, fname
    except Exception as e:
        return False, str(e)

def cal_stop(lid):
    proc = cal_procs.get(lid)
    if proc and proc.poll() is None:
        proc.terminate()
        try: proc.wait(timeout=3)
        except: proc.kill()
        cal_procs[lid] = None
        return True, cal_files.get(lid, "")
    cal_procs[lid] = None
    return False, "not running"

def cal_status(lid):
    proc = cal_procs.get(lid)
    running = bool(proc and proc.poll() is None)
    return {"running": running, "file": cal_files.get(lid, "") if running else ""}

def cal_list():
    """Return list of CAL log files sorted newest first."""
    files = sorted(glob.glob(f"{CAL_LOG_DIR}/cal_*.txt"), reverse=True)
    result = []
    for f in files[:20]:
        size = os.path.getsize(f)
        result.append({"name": os.path.basename(f), "size": size})
    return result

def mqtt_thread():
    global mqtt_conn
    while True:
        try:
            c = mqtt_client.Client(mqtt_client.CallbackAPIVersion.VERSION2, client_id="ngr-flask")
            c.on_connect = on_mqtt_connect
            c.on_message = on_mqtt_message
            mqtt_conn = c
            c.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
            c.loop_forever()
        except Exception as e:
            print(f"MQTT error: {e}")
            mqtt_conn = None
        time.sleep(5)

threading.Thread(target=mqtt_thread, daemon=True).start()


# ============================================================================
# Nav
# ============================================================================
NAV_STYLE = """
.nav-bar { display:flex; gap:6px; padding:10px 10px 0; max-width:700px; margin:0 auto; }
.nav-btn { flex:1; text-align:center; padding:10px 4px; border-radius:10px;
           background:rgba(40,40,40,0.7); color:#ccc; text-decoration:none;
           font-size:14px; font-weight:bold; border:2px solid #555; }
.nav-btn.active      { background:rgba(60,60,60,0.95); color:#fff; border-color:#aaa; }
.nav-btn.placeholder { opacity:0.4; pointer-events:none; }
"""

def nav(active):
    def c(n):
        if n == active: return "active"
        if n in ("oscar",): return "placeholder"
        return ""
    html = (f'<div class="nav-bar">'
            f'<a href="/"      class="nav-btn {c("console")}">Console</a>'
            f'<a href="/otto"  class="nav-btn {c("otto")}">Otto</a>'
            f'<a href="/toby"  class="nav-btn {c("toby")}">Toby</a>'
            f'<a href="/oscar" class="nav-btn {c("oscar")}">Oscar</a>'
            f'<a href="/hans"  class="nav-btn {c("hans")}">Hans</a>'
            f'</div>')
    return Markup(html)


# ============================================================================
# Shared CSS
# ============================================================================
SHARED_CSS = """
body { margin:0; font-family:Arial,sans-serif;
  background:linear-gradient(135deg,#a8a8a8 0%,#d9d9d9 20%,#8f8f8f 50%,#d7d7d7 80%,#9c9c9c 100%);
  color:#111; }
.container { max-width:700px; margin:auto; padding:10px; }
.panel { background:rgba(40,40,40,0.92); border-radius:14px; padding:14px;
  margin-bottom:14px; box-shadow:0 4px 10px rgba(0,0,0,0.35);
  border:2px solid #666; position:relative; }
.panel:before,.panel:after { content:""; width:12px; height:12px; border-radius:50%;
  background:#bbb; border:2px solid #555; position:absolute; top:8px; }
.panel:before { left:8px; } .panel:after { right:8px; }
h2 { margin-top:0; text-align:center; color:#f1f1f1; letter-spacing:1px; }
.badge { font-size:12px; font-weight:bold; padding:4px 10px; border-radius:20px; letter-spacing:1px; }
.badge-online  { background:#2a7a2a; color:#7fff7f; border:1px solid #4fc34f; }
.badge-offline { background:#5a2020; color:#ff9999; border:1px solid #b32020; }
.telem-grid { display:grid; grid-template-columns:1fr 1fr 1fr 1fr; gap:8px; margin-bottom:10px; }
.telem-cell { background:#222; border-radius:8px; padding:8px 10px; }
.telem-lbl  { font-size:11px; color:#888; margin-bottom:2px; }
.telem-val  { font-size:20px; font-weight:bold; color:#4fc34f; }
.telem-val.warn { color:#d9a21b; }
.estop-btn { width:100%; padding:18px; border-radius:50px; border:3px solid #b32020;
  background:rgba(40,40,40,0.92); color:#e04040; font-size:20px; font-weight:bold;
  letter-spacing:3px; cursor:pointer; text-align:center; text-decoration:none; display:block; }
.estop-btn.active { background:#b32020; color:white; }
.estop-btn:hover  { background:#8a1515; color:white; border-color:#e04040; }
"""


# ============================================================================
# Dispatcher console
# ============================================================================
CONSOLE_HTML = """<!DOCTYPE html>
<html><head>
<title>NGR Dispatcher Console</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
{{ nav_style }}
{{ shared_css }}
.cmd-grid{display:grid;grid-template-columns:1fr 1fr 1fr;gap:8px;}
.col-header{text-align:center;color:#aaa;font-size:12px;font-weight:bold;
  letter-spacing:1px;padding-bottom:6px;border-bottom:1px solid #444;margin-bottom:6px;}
button{border:none;border-radius:10px;padding:12px 4px;width:100%;
  font-size:14px;font-weight:bold;cursor:pointer;margin-bottom:6px;
  box-shadow:inset 0 1px 2px rgba(255,255,255,0.25),0 2px 4px rgba(0,0,0,0.4);}
button:last-child{margin-bottom:0;}
.btn-go    {background:#3fa34d;color:white;}
.btn-stop  {background:#d9a21b;color:white;}
.btn-ce    {background:#2d6ea8;color:white;}
.btn-end   {background:#7a3a00;color:#ffd080;border:2px solid #aa6000;}
.btn-estop {width:100%;padding:18px;border-radius:50px;border:3px solid #b32020;
  background:rgba(40,40,40,0.92);color:#e04040;font-size:20px;font-weight:bold;
  letter-spacing:3px;cursor:pointer;}
.btn-estop:hover{background:#8a1515;color:white;border-color:#e04040;}
.status-val.on{color:#7f7;} .status-val.off{color:#555;}
</style></head><body>
{{ nav_html }}
<div class="container">

  <div class="panel">
    <h2>DISPATCHER</h2>
    <div style="display:grid;grid-template-columns:1fr 1fr;gap:12px;">
      <div>
        <div style="text-align:center;color:#aaa;font-size:12px;font-weight:bold;
          letter-spacing:2px;margin-bottom:8px;border-bottom:1px solid #444;padding-bottom:6px;">OTTO</div>
        <div style="text-align:center;margin-bottom:6px;">
          <div style="font-size:10px;color:#666;">STATUS</div>
          <div style="font-size:15px;font-weight:bold;" id="otto-mode"
            style="color:{{ '#7f7' if otto_auto=='1' else '#888' }};">
            {{ 'CE' if (otto_auto=='1' and otto_ce=='1') else ('CTO' if otto_auto=='1' else 'MAN') }}
          </div>
        </div>
        <div style="text-align:center;margin-bottom:6px;">
          <div style="font-size:10px;color:#666;">BLOCK</div>
          <div style="font-size:15px;font-weight:bold;color:#f1f1f1;" id="otto-block">{{ otto_block }}</div>
        </div>
        <div style="text-align:center;">
          <div style="font-size:10px;color:#666;">ONLINE</div>
          <div class="status-val {{ 'on' if otto_online=='1' else 'off' }}" id="otto-online">
            {{ 'YES' if otto_online=='1' else 'NO' }}</div>
        </div>
      </div>
      <div>
        <div style="text-align:center;color:#aaa;font-size:12px;font-weight:bold;
          letter-spacing:2px;margin-bottom:8px;border-bottom:1px solid #444;padding-bottom:6px;">TOBY</div>
        <div style="text-align:center;margin-bottom:6px;">
          <div style="font-size:10px;color:#666;">STATUS</div>
          <div style="font-size:15px;font-weight:bold;" id="toby-mode">
            {{ 'CE' if (toby_auto=='1' and toby_ce=='1') else ('CTO' if toby_auto=='1' else 'MAN') }}
          </div>
        </div>
        <div style="text-align:center;margin-bottom:6px;">
          <div style="font-size:10px;color:#666;">BLOCK</div>
          <div style="font-size:15px;font-weight:bold;color:#f1f1f1;" id="toby-block">{{ toby_block }}</div>
        </div>
        <div style="text-align:center;">
          <div style="font-size:10px;color:#666;">ONLINE</div>
          <div class="status-val {{ 'on' if toby_online=='1' else 'off' }}" id="toby-online">
            {{ 'YES' if toby_online=='1' else 'NO' }}</div>
        </div>
      </div>
    </div>
  </div>

  <div class="panel">
    <h2>OPERATIONS</h2>
    <div class="cmd-grid">
      <div>
        <div class="col-header">BOTH</div>
        <button class="btn-go"   onclick="dc('go')">GO</button>
        <button class="btn-stop" onclick="dc('stop')">STOP</button>
      </div>
      <div>
        <div class="col-header">OTTO</div>
        <button class="btn-go"   onclick="dc('go/9950011')">GO</button>
        <button class="btn-stop" onclick="dc('stop/9950011')">STOP</button>
      </div>
      <div>
        <div class="col-header">TOBY</div>
        <button class="btn-go"   onclick="dc('go/9950012')">GO</button>
        <button class="btn-stop" onclick="dc('stop/9950012')">STOP</button>
      </div>
    </div>
  </div>

  <div class="panel">
    <div class="cmd-grid">
      <div>
        <button class="btn-ce"  onclick="dc('ce')">CIRCUIT EXPRESS</button>
      </div>
      <div style="grid-column:span 2;">
        <button class="btn-end" onclick="endCto()">END CTO — RETURN TO MANUAL</button>
      </div>
    </div>
  </div>

  <div class="panel">
    <div style="display:grid;grid-template-columns:1fr 1fr;gap:10px;">
      <div>
        <div style="text-align:center;color:#aaa;font-size:11px;font-weight:bold;
          letter-spacing:1px;margin-bottom:6px;">OTTO</div>
        <div style="display:grid;grid-template-columns:1fr 1fr;gap:6px;">
          <div class="telem-cell"><div class="telem-lbl">Voltage</div><div class="telem-val" id="d-otto-voltage">--</div></div>
          <div class="telem-cell"><div class="telem-lbl">Current</div><div class="telem-val" id="d-otto-current">--</div></div>
          <div class="telem-cell"><div class="telem-lbl">Power</div><div class="telem-val" id="d-otto-power">--</div></div>
          <div class="telem-cell"><div class="telem-lbl">Low V</div><div class="telem-val" id="d-otto-lowvolt">--</div></div>
        </div>
      </div>
      <div>
        <div style="text-align:center;color:#aaa;font-size:11px;font-weight:bold;
          letter-spacing:1px;margin-bottom:6px;">TOBY</div>
        <div style="display:grid;grid-template-columns:1fr 1fr;gap:6px;">
          <div class="telem-cell"><div class="telem-lbl">Voltage</div><div class="telem-val" id="d-toby-voltage">--</div></div>
          <div class="telem-cell"><div class="telem-lbl">Current</div><div class="telem-val" id="d-toby-current">--</div></div>
          <div class="telem-cell"><div class="telem-lbl">Power</div><div class="telem-val" id="d-toby-power">--</div></div>
          <div class="telem-cell"><div class="telem-lbl">Low V</div><div class="telem-val" id="d-toby-lowvolt">--</div></div>
        </div>
      </div>
    </div>
  </div>

  <div class="panel">
    <button class="btn-estop" onclick="dc('estop')">&#9888; E-STOP</button>
  </div>

  <div class="panel">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:6px;">
      <span style="color:#888;font-size:11px;letter-spacing:1px;">PACKET LOG</span>
      <button onclick="clearLog('dispatch-log')" style="background:#333;border:1px solid #555;
        color:#aaa;font-size:11px;padding:4px 12px;border-radius:6px;cursor:pointer;">CLEAR</button>
    </div>
    <div id="dispatch-log" style="color:#fff;background:#0a0a0a;font-size:11px;
      font-family:monospace;padding:8px;border-radius:6px;max-height:300px;overflow-y:auto;">
      <div style="color:#aaa;">Waiting for packets...</div>
    </div>
  </div>

</div>
<script>
function dc(cmd){fetch('/dispatcher/cmd/'+cmd,{method:'POST'}).catch(e=>console.error(e));}
function endCto(){
  if(confirm('End CTO and return both locomotives to manual control?')){
    fetch('/dispatcher/endcto',{method:'POST'}).catch(e=>console.error(e));
  }
}
function pollStatus(){
  fetch('/dispatcher/state').then(r=>r.json()).then(s=>{
    document.getElementById('otto-mode').textContent = s.otto_auto==='1'?(s.otto_ce==='1'?'CE':'CTO'):'MAN';
    document.getElementById('toby-mode').textContent = s.toby_auto==='1'?(s.toby_ce==='1'?'CE':'CTO'):'MAN';
    document.getElementById('otto-online').textContent = s.otto_online==='1'?'YES':'NO';
    document.getElementById('toby-online').textContent = s.toby_online==='1'?'YES':'NO';
    document.getElementById('otto-block').textContent = s.otto_block||'--';
    document.getElementById('toby-block').textContent = s.toby_block||'--';
    function fmtT(v){ var n=parseFloat(v); return isNaN(n)||v=='--'?'--':n.toFixed(1); }
    document.getElementById('d-otto-voltage').textContent = fmtT(s.otto_voltage);
    document.getElementById('d-otto-current').textContent = fmtT(s.otto_current);
    document.getElementById('d-otto-power').textContent   = fmtT(s.otto_power);
    document.getElementById('d-otto-lowvolt').textContent = s.otto_lowvolt==='1'?'LOW':'OK';
    document.getElementById('d-toby-voltage').textContent = fmtT(s.toby_voltage);
    document.getElementById('d-toby-current').textContent = fmtT(s.toby_current);
    document.getElementById('d-toby-power').textContent   = fmtT(s.toby_power);
    document.getElementById('d-toby-lowvolt').textContent = s.toby_lowvolt==='1'?'LOW':'OK';
  }).catch(e=>{});
}
var logPausedUntil = {};
function clearLog(id){
  document.getElementById(id).innerHTML='<div style="color:#aaa;font-size:11px;">Cleared — resuming in 5s</div>';
  logPausedUntil[id] = Date.now()+5000;
}
function pollLog(){
  if(logPausedUntil['dispatch-log'] && Date.now()<logPausedUntil['dispatch-log']) return;
  fetch('/dispatcher/log').then(r=>r.json()).then(entries=>{
    if(!entries.length) return;
    const names={'9950011':'Otto','9950012':'Toby'};
    document.getElementById('dispatch-log').innerHTML=entries.map(e=>
      e.ts+'  '+(names[e.loco]||e.loco)+'  '+e.topic+'  '+e.value
    ).join('<br>');
  }).catch(e=>{});
}
setInterval(pollStatus,3000);
setInterval(pollLog,1000);
</script>
</body></html>"""


# ============================================================================
# Loco page template
# ============================================================================
LOCO_HTML = """<!DOCTYPE html>
<html><head>
<title>NGR — {{ name }}</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
{{ nav_style }}
{{ shared_css }}
.container { max-width:700px; margin:auto; padding:8px; }
.panel-tight { background:rgba(40,40,40,0.92); border-radius:12px; padding:8px 12px;
  margin-bottom:8px; box-shadow:0 3px 8px rgba(0,0,0,0.3); border:2px solid #666; }
.header-wrap { position:relative; display:flex; align-items:center;
  justify-content:flex-end; width:100%; }
.loco-title { position:absolute; left:50%; transform:translateX(-50%);
  font-size:20px; font-weight:bold; color:#f1f1f1; letter-spacing:2px; }
.badge { font-size:12px; font-weight:bold; padding:3px 10px; border-radius:20px; letter-spacing:1px; }
.badge-online  { background:#2a7a2a; color:#7fff7f; border:1px solid #4fc34f; }
.badge-offline { background:#5a2020; color:#ff9999; border:1px solid #b32020; }
.mode-row { display:flex; gap:6px; }
.mode-btn { flex:1; text-align:center; padding:8px 4px; border-radius:8px;
  border:2px solid #555; background:rgba(30,30,30,0.8); color:#888;
  font-size:13px; font-weight:bold; letter-spacing:1px; text-decoration:none; display:block; }
.mode-btn.man-active  { background:rgba(50,50,70,0.95); color:#aac; border-color:#88a; }
.mode-btn.auto-active { background:rgba(40,70,40,0.95); color:#7f7; border-color:#4a4; }
.mode-btn.locked { opacity:0.35; pointer-events:none; }
.cto-note { text-align:center; color:#7f7; font-size:11px; letter-spacing:1px; margin-top:4px; }
.dir-row { display:flex; gap:6px; }
.dir-btn { flex:1; text-align:center; padding:9px 2px; border-radius:8px;
  border:2px solid #555; background:rgba(30,30,30,0.8); color:#aaa;
  font-size:13px; font-weight:bold; cursor:pointer; text-decoration:none; }
.dir-btn.active-rev { background:#5a2020; color:#ff9999; border-color:#b32020; }
.dir-btn.active-neu { background:#4a4a10; color:#ffd080; border-color:#aa8800; }
.dir-btn.active-fwd { background:#1a4a1a; color:#7fff7f; border-color:#2a7a2a; }
.dir-btn.locked { opacity:0.35; pointer-events:none; }
.sdir-row { display:flex; gap:6px; }
.sdir-btn { flex:1; text-align:center; padding:9px 2px; border-radius:8px;
  border:2px solid #555; background:rgba(30,30,30,0.8); color:#aaa;
  font-size:13px; font-weight:bold; cursor:pointer; text-decoration:none; }
.sdir-btn.active { background:#1a3a5a; color:#9fd6ff; border-color:#2a6aa8; }
.sdir-btn.locked { opacity:0.35; pointer-events:none; }
.navready-badge { text-align:center; font-size:11px; font-weight:bold; letter-spacing:1px;
  margin-top:6px; padding:4px; border-radius:6px; }
.navready-badge.ready    { color:#7fff7f; background:rgba(42,122,42,0.25); }
.navready-badge.notready { color:#ff9999; background:rgba(90,32,32,0.25); }

/* ---- START INTERVAL SLIDER ---- */
.interval-slider-wrap { padding:4px 0 8px; }
.interval-display {
  text-align:center; font-size:28px; font-weight:bold;
  color:#c0a0ff; letter-spacing:3px; margin-bottom:8px;
  font-family:monospace;
}
.interval-display span { font-size:14px; color:#888; font-weight:normal; letter-spacing:1px; }
input.interval-slider {
  width:100%; height:32px; cursor:pointer;
  -webkit-appearance:none; appearance:none;
  border-radius:16px; outline:none; border:none;
  background:linear-gradient(to right,#4a2a8a 0%,#7050c0 100%);
}
input.interval-slider::-webkit-slider-thumb {
  -webkit-appearance:none; appearance:none;
  width:36px; height:36px; border-radius:50%;
  background:#ddd; border:2px solid #aaa; cursor:pointer;
  box-shadow:0 2px 6px rgba(0,0,0,0.5);
}
.interval-tick-row {
  display:flex; justify-content:space-between;
  color:#555; font-size:10px; font-family:monospace;
  padding:0 2px; margin-top:2px;
}
.interval-send-btn {
  display:block; width:100%; margin-top:10px;
  padding:10px; border-radius:8px; border:2px solid #7050c0;
  background:rgba(60,30,100,0.5); color:#c0a0ff;
  font-size:14px; font-weight:bold; letter-spacing:1px;
  cursor:pointer; text-align:center;
}
.interval-send-btn:hover { background:rgba(80,50,130,0.8); }
.interval-badge { text-align:center; font-size:11px; font-weight:bold; letter-spacing:1px;
  padding:4px; border-radius:6px; margin-top:6px; }
.interval-badge.set    { color:#c0a0ff; background:rgba(60,30,100,0.35); }
.interval-badge.notset { color:#888;    background:rgba(30,30,30,0.4); }
/* ---- END INTERVAL SLIDER ---- */

/* ---- THROTTLE BIG NUMBER ---- */
.throttle-display {
  text-align:center; font-size:28px; font-weight:bold;
  color:#7fff9f; letter-spacing:3px; margin-bottom:8px;
  font-family:monospace;
}
.throttle-display span { font-size:14px; color:#888; font-weight:normal; letter-spacing:1px; }
/* ---- END THROTTLE BIG NUMBER ---- */

/* ---- CURRENT MM PANEL ---- */
.mm-display {
  text-align:center; font-size:52px; font-weight:bold;
  color:#9fd6ff; letter-spacing:3px; margin-bottom:4px;
  font-family:monospace; line-height:1;
}
.mm-landmark {
  text-align:center; font-size:13px; color:#9fd6ff; font-weight:bold;
  letter-spacing:1px; margin-bottom:2px; min-height:16px;
}
.mm-countdown {
  text-align:center; font-size:12px; color:#7ab8e0; font-weight:bold;
  letter-spacing:1px; margin-bottom:8px; min-height:14px; font-style:italic;
}
.mm-status-row { display:flex; justify-content:space-between; gap:6px; }
.mm-stat-cell { flex:1; background:#222; border-radius:7px; padding:6px 4px; text-align:center; }
.mm-stat-lbl { font-size:10px; color:#888; margin-bottom:2px; }
.mm-stat-val { font-size:22px; font-weight:bold; color:#9fd6ff; }
.mm-stat-val.ok    { color:#4fc34f; }
.mm-stat-val.check { color:#d9a21b; }
/* ---- END CURRENT MM PANEL ---- */

.slider-row { display:flex; align-items:center; gap:8px; }
.slider-lbl { color:#aaa; font-size:12px; font-weight:bold; letter-spacing:1px; min-width:70px; }
.slider-val { font-size:16px; font-weight:bold; min-width:30px; text-align:right; }
.slider-val.green  { color:#4fc34f; }
.slider-val.yellow { color:#d9a21b; }
input.green-slider, input.yellow-slider {
  flex:1; height:28px; cursor:pointer;
  -webkit-appearance:none; appearance:none;
  border-radius:14px; outline:none; border:none; background:#333;
}
input.green-slider::-webkit-slider-thumb,
input.yellow-slider::-webkit-slider-thumb {
  -webkit-appearance:none; appearance:none;
  width:32px; height:32px; border-radius:50%;
  background:#eee; border:2px solid #aaa; cursor:pointer;
  box-shadow:0 2px 6px rgba(0,0,0,0.5);
}
.estop-wrap { display:flex; justify-content:center; }
.estop-btn { width:80%; padding:12px; border-radius:50px; border:3px solid #b32020;
  background:rgba(40,40,40,0.92); color:#e04040; font-size:18px; font-weight:bold;
  letter-spacing:3px; cursor:pointer; text-align:center; text-decoration:none; display:block; }
.estop-btn.active { background:#b32020; color:white; }
.estop-btn:hover  { background:#8a1515; color:white; border-color:#e04040; }
/* CAL recording */
.cal-row { display:flex; gap:8px; align-items:center; }
.cal-btn { flex:1; text-align:center; padding:10px 4px; border-radius:8px;
  border:2px solid #555; background:rgba(30,30,30,0.8); color:#aaa;
  font-size:13px; font-weight:bold; cursor:pointer; text-decoration:none; display:block; }
.cal-btn.recording { background:#3a0a0a; color:#ff6060; border-color:#b32020;
  animation: pulse 1.5s infinite; }
.cal-btn.idle { background:rgba(30,50,30,0.8); color:#7f7; border-color:#4a4; }
@keyframes pulse { 0%,100%{opacity:1} 50%{opacity:0.6} }
.cal-file { font-size:10px; color:#666; font-family:monospace; margin-top:4px;
  white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }
.telem-grid { display:grid; grid-template-columns:1fr 1fr 1fr 1fr; gap:6px; margin-bottom:4px; }
.telem-cell { background:#222; border-radius:7px; padding:5px 4px; text-align:center; }
.telem-lbl  { font-size:10px; color:#888; margin-bottom:2px; }
.telem-val  { font-size:15px; font-weight:bold; color:#4fc34f; }
.telem-val.warn { color:#d9a21b; }
.block-display { text-align:center; color:#f1f1f1; font-size:14px; font-weight:bold;
  margin-top:4px; letter-spacing:2px; }
.warning-label { text-align:center; color:#d9a21b; font-size:11px; min-height:14px; margin-top:2px; }
</style></head><body>
{{ nav_html }}
<div class="container">

  <!-- Header -->
  <div class="panel-tight">
    <div class="header-wrap">
      <span class="loco-title">{{ name }}</span>
      <span class="badge {{ 'badge-online' if s.online=='1' else 'badge-offline' }}">
        {{ 'ONLINE' if s.online=='1' else 'OFFLINE' }}</span>
    </div>
  </div>

  {% set cto = s.auto == '1' %}

  <!-- 1. Session Direction -->
  <div class="panel-tight"{% if cto %} style="opacity:0.4;"{% endif %}>
    <div style="text-align:center;color:#555;font-size:10px;letter-spacing:2px;margin-bottom:6px;">
      STEP 1 — SESSION DIRECTION
    </div>
    <div class="sdir-row">
      <button onclick="sendSessionDir('CW')" id="sdir-cw"
         class="sdir-btn {{ 'active' if s.session_dir=='CW' else '' }} {{ 'locked' if cto else '' }}">CLOCKWISE</button>
      <button onclick="sendSessionDir('CCW')" id="sdir-ccw"
         class="sdir-btn {{ 'active' if s.session_dir=='CCW' else '' }} {{ 'locked' if cto else '' }}">COUNTER-CLOCKWISE</button>
    </div>
    <div class="navready-badge {{ 'ready' if s.nav_ready=='1' else 'notready' }}" id="navready-badge">
      {% if s.nav_ready=='1' %}NAV READY — {{ s.session_dir }}
      {% else %}SET SESSION DIRECTION TO ENABLE THROTTLE{% endif %}
    </div>
  </div>

  <!-- 2. Start Interval -->
  <div class="panel-tight"{% if cto %} style="opacity:0.4;"{% endif %}>
    <div style="text-align:center;color:#555;font-size:10px;letter-spacing:2px;margin-bottom:10px;">
      STEP 2 — START INTERVAL
    </div>
    <div class="interval-slider-wrap">
      <div class="interval-display" id="interval-display">
        {% if s.start_interval != 'UNSET' %}
          {{ s.start_interval[:3] }}<span> — </span>{{ s.start_interval[4:] }}
        {% else %}
          <span style="color:#555;">— not set —</span>
        {% endif %}
      </div>
      <input type="range" min="0" max="170" step="1"
             value="{{ interval_mm }}"
             id="interval-slider"
             class="interval-slider"
             {% if cto %}disabled{% endif %}
             oninput="onIntervalSlide(this.value)" />
      <div class="interval-tick-row">
        <span>000</span><span>017</span><span>034</span><span>051</span>
        <span>068</span><span>085</span><span>102</span><span>119</span>
        <span>136</span><span>153</span><span>170</span>
      </div>
      <button class="interval-send-btn" id="interval-send"
              onclick="sendInterval()" {% if cto %}disabled{% endif %}>
        SET INTERVAL
      </button>
    </div>
    <div class="interval-badge {{ 'set' if s.start_interval != 'UNSET' else 'notset' }}" id="interval-badge">
      {% if s.start_interval != 'UNSET' %}
        INTERVAL SET — {{ s.start_interval }}
      {% else %}
        NO INTERVAL SET — OPERATOR INPUT REQUIRED
      {% endif %}
    </div>
  </div>

  <!-- 3. Direction -->
  <div class="panel-tight"{% if cto %} style="opacity:0.4;"{% endif %}>
    <div style="text-align:center;color:#555;font-size:10px;letter-spacing:2px;margin-bottom:6px;">STEP 3 — MOTOR DIRECTION</div>
    <div class="dir-row">
      <button onclick="sendDir(0)"
         id="dir-btn-0"
         class="dir-btn {{ 'active-rev' if s.direction=='0' else '' }} {{ 'locked' if cto else '' }}">Reverse</button>
      <button onclick="sendDir(1)"
         id="dir-btn-1"
         class="dir-btn {{ 'active-neu' if s.direction=='1' else '' }} {{ 'locked' if cto else '' }}">Neutral</button>
      <button onclick="sendDir(2)"
         id="dir-btn-2"
         class="dir-btn {{ 'active-fwd' if s.direction=='2' else '' }} {{ 'locked' if cto else '' }}">Forward</button>
    </div>
  </div>

  <!-- 4. Mode -->
  <div class="panel-tight">
    <div style="text-align:center;color:#555;font-size:10px;letter-spacing:2px;margin-bottom:6px;">
      STEP 4 — MODE
    </div>
    <div class="mode-row">
      {% if s.auto == '0' %}
        <span class="mode-btn man-active">MANUAL</span>
        <a href="/{{ slug }}/mode/1" class="mode-btn">AUTO</a>
      {% else %}
        <span class="mode-btn locked">MANUAL</span>
        <span class="mode-btn auto-active">AUTO</span>
      {% endif %}
    </div>
    {% if s.auto == '1' %}
      <div class="cto-note">AUTO — Dispatcher in control</div>
    {% endif %}
  </div>

  <!-- Throttle -->
  <div class="panel-tight"{% if cto %} style="opacity:0.4;"{% endif %}>
    <div style="text-align:center;color:#555;font-size:10px;letter-spacing:2px;margin-bottom:6px;">THROTTLE</div>
    <div class="throttle-display" id="throttle-display">{{ s.throttle }}<span> / 255</span></div>
    <div class="slider-row">
      <input type="range" min="0" max="255" value="{{ s.throttle }}" step="1"
             id="throttle-slider" class="green-slider" {% if cto %}disabled{% endif %}
             oninput="thrSliderLockedUntil=Date.now()+5000;
                      document.getElementById('thr-val').textContent=this.value;
                      document.getElementById('throttle-display').innerHTML=this.value+'<span> / 255</span>';
                      clearTimeout(window._thrTimer);
                      window._thrTimer=setTimeout(function(){sendCmd('throttle',document.getElementById('throttle-slider').value);},80);"
             onchange="thrSliderLockedUntil=Date.now()+5000; sendCmd('throttle',this.value);" />
      <span class="slider-val green" id="thr-val">{{ s.throttle }}</span>
    </div>
  </div>

  <!-- Brake -->
  <div class="panel-tight"{% if cto %} style="opacity:0.4;"{% endif %}>
    <div class="slider-row">
      <span class="slider-lbl">BRAKE</span>
      <input type="range" min="0" max="255" value="{{ s.brake }}" step="1"
             id="brake-slider" class="yellow-slider" {% if cto %}disabled{% endif %}
             style="direction:rtl;"
             oninput="document.getElementById('brake-val').textContent=this.value"
             onchange="sendCmd('brake',this.value)" />
      <span class="slider-val yellow" id="brake-val">{{ s.brake }}</span>
    </div>
  </div>

  <!-- E-STOP -->
  <div class="panel-tight">
    <div class="estop-wrap">
      <button onclick="toggleEstop()"
         id="estop-btn"
         class="estop-btn {{ 'active' if s.estop=='1' else '' }}">
        {{ 'E-STOP ACTIVE — TAP TO CLEAR' if s.estop=='1' else 'E-STOP' }}
      </button>
    </div>
  </div>

  <!-- CAL Recording -->
  <div class="panel-tight">
    <div style="text-align:center;color:#555;font-size:10px;letter-spacing:2px;margin-bottom:8px;">
      CAL RECORDING
    </div>
    <div class="cal-row">
      <button id="cal-btn" class="cal-btn idle" onclick="toggleCal()">
        &#9679; START RECORDING
      </button>
    </div>
    <div class="cal-file" id="cal-file"></div>
  </div>

  <!-- Telemetry -->
  <div class="panel-tight">
    <div class="telem-grid" style="grid-template-columns:1fr 1fr 1fr 1fr 1fr 1fr;">
      <div class="telem-cell">
        <div class="telem-lbl">Voltage</div>
        <div class="telem-val" id="telem-voltage">{{ s.voltage }}</div>
      </div>
      <div class="telem-cell">
        <div class="telem-lbl">Current</div>
        <div class="telem-val" id="telem-current">{{ s.current }}</div>
      </div>
      <div class="telem-cell">
        <div class="telem-lbl">Power</div>
        <div class="telem-val" id="telem-power">{{ s.power }}</div>
      </div>
      <div class="telem-cell">
        <div class="telem-lbl">Low V</div>
        <div class="telem-val {{ 'warn' if s.lowvolt=='1' else '' }}" id="telem-lowvolt">
          {{ 'LOW' if s.lowvolt=='1' else 'OK' }}</div>
      </div>
      <div class="telem-cell">
        <div class="telem-lbl">PWM</div>
        <div class="telem-val" id="telem-ramp-pwm">{{ s.ramp_pwm }}</div>
      </div>
      <div class="telem-cell">
        <div class="telem-lbl">pKPH</div>
        <div class="telem-val" id="telem-pkph">{{ s.last_measured_pkph }}</div>
      </div>
    </div>
    <div class="block-display" id="block-display">{{ name }}: {{ s.block }}</div>
  </div>

  <!-- Current MM / DNA Status -->
  <div class="panel-tight">
    <!-- Three columns: MM | KpH | %CERT -->
    <div style="display:flex;justify-content:space-around;align-items:flex-end;margin-bottom:4px;">
      <div style="flex:1;display:flex;flex-direction:column;align-items:center;">
        <div class="mm-display" id="mm-display" style="margin:0;">
          {% if s.mm_current != '--' %}{{ s.mm_current }}{% else %}<span style="color:#555;">— —</span>{% endif %}
        </div>
        <div style="color:#9fd6ff;font-size:22px;font-weight:bold;letter-spacing:2px;margin-top:4px;">MM</div>
      </div>
      <div style="flex:1;display:flex;flex-direction:column;align-items:center;">
        <div id="mm-pkph-val" style="font-size:52px;font-weight:bold;font-family:monospace;color:#ffd080;line-height:1;">--</div>
        <div style="color:#ffd080;font-size:22px;font-weight:bold;letter-spacing:2px;margin-top:4px;">KpH</div>
      </div>
      <div style="flex:1;display:flex;flex-direction:column;align-items:center;">
        <div id="mm-cert-pct" style="font-size:52px;font-weight:bold;font-family:monospace;color:#7fff7f;line-height:1;">--</div>
        <div style="color:#7fff7f;font-size:22px;font-weight:bold;letter-spacing:1px;margin-top:4px;">%CERT</div>
      </div>
    </div>
    <!-- Landmark and proximity -->
    <div class="mm-landmark" id="mm-landmark">{{ s.mm_landmark }}</div>
    <div class="mm-countdown" id="mm-countdown"></div>
    <!-- Stat row -->
    <div class="mm-status-row">
      <div class="mm-stat-cell" style="flex:1;">
        <div class="mm-stat-lbl">DNA</div>
        <div class="mm-stat-val {{ 'ok' if s.mm_status=='DNA_OK' else ('check' if s.mm_status=='DNA_CHECK' else '') }}" id="mm-status">{{ s.mm_status }}</div>
      </div>
    </div>
  </div>

  <!-- Profile Memory -->
  <div class="panel-tight">
    <div style="color:#888;font-size:11px;letter-spacing:1px;margin-bottom:6px;">PROFILE MEMORY</div>
    <div style="font-size:13px;line-height:2.0;">
      <div>CW:&nbsp;&nbsp;<span id="prof-cw" style="color:#f1f1f1;">{{ s.profile_cw }}</span></div>
      <div>CCW:&nbsp;<span id="prof-ccw" style="color:#f1f1f1;">{{ s.profile_ccw }}</span></div>
      <div>Learning laps: <span id="prof-laps" style="color:#f1f1f1;">{{ s.profile_laps }}</span></div>
      <div>Profile service: <span id="prof-svc" style="color:#f1f1f1;">{{ s.profile_svc }}</span></div>
    </div>
  </div>

  <!-- Packet log -->
  <div class="panel-tight">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:6px;">
      <span style="color:#888;font-size:11px;letter-spacing:1px;">PACKET LOG</span>
      <button onclick="clearLog('loco-log')" style="background:#333;border:1px solid #555;
        color:#aaa;font-size:11px;padding:4px 12px;border-radius:6px;cursor:pointer;">CLEAR</button>
    </div>
    <div id="loco-log" style="color:#fff;background:#0a0a0a;font-size:11px;
      font-family:monospace;padding:8px;border-radius:6px;max-height:300px;overflow-y:auto;">
      <div style="color:#aaa;font-size:11px;">Waiting for packets...</div>
    </div>
  </div>

</div>
<script>
// CTO state — set at render time but updated live by pollState
var isCto = {{ 'true' if cto else 'false' }};
// ---- CAL recording ----
var calRunning = false;
var lastThrottle = null;
var thrSliderLockedUntil = 0;  // timestamp ms — pollState won't touch slider before this

function toggleCal() {
  var action = calRunning ? 'stop' : 'start';
  fetch('/{{ slug }}/cal/' + action, {method:'POST'})
    .then(r => r.json())
    .then(data => {
      calRunning = data.running;
      updateCalBtn(data);
    }).catch(e => console.error(e));
}

function updateCalBtn(data) {
  var btn  = document.getElementById('cal-btn');
  var file = document.getElementById('cal-file');
  if (data.running) {
    btn.textContent  = '\u25CF RECORDING — TAP TO STOP';
    btn.className    = 'cal-btn recording';
    file.textContent = data.file ? data.file.split('/').pop() : '';
  } else {
    btn.textContent  = '\u25CF START RECORDING';
    btn.className    = 'cal-btn idle';
    file.textContent = data.file ? 'Saved: ' + data.file.split('/').pop() : '';
  }
}

function pollCalStatus() {
  fetch('/{{ slug }}/cal/status')
    .then(r => r.json())
    .then(data => {
      calRunning = data.running;
      updateCalBtn(data);
    }).catch(e => {});
}

setInterval(pollCalStatus, 4000);
pollCalStatus();
// ---- End CAL recording ----

// Initialize interval display from slider position (not from server state)
var pendingInterval = null;  // NNN-NNN string staged but not yet sent

(function(){
  var sl = document.getElementById('interval-slider');
  if(sl) onIntervalSlide(sl.value);

  // Throttle slider — prevent pollState from snapping it back during drag
  var thr = document.getElementById('throttle-slider');
  if(thr){
    function lockThrottle(){ thrSliderLockedUntil = Date.now() + 5000; }
    ['mousedown','touchstart','mousemove','touchmove','input','change'].forEach(function(ev){
      thr.addEventListener(ev, lockThrottle, {passive:true});
    });
  }
})();
function mmToInterval(mm) {
  var lo = mm % 171;
  var hi = (lo + 1) % 171;
  return ('000'+lo).slice(-3) + '-' + ('000'+hi).slice(-3);
}

function onIntervalSlide(val) {
  console.log("interval slider:", val);
  var mm = parseInt(val);
  var str = mmToInterval(mm);
  pendingInterval = str;
  var lo = ('000'+mm).slice(-3);
  var hi = ('000'+((mm+1)%171)).slice(-3);
  document.getElementById('interval-display').innerHTML =
    '<b>'+lo+'</b><span> — </span><b>'+hi+'</b>';
}

function sendInterval() {
  if(!pendingInterval) {
    // Operator must slide before sending — do not auto-send the default position
    document.getElementById('interval-badge').textContent = 'SLIDE TO SELECT AN INTERVAL FIRST';
    document.getElementById('interval-badge').className = 'interval-badge notset';
    return;
  }
  fetch('/{{ slug }}/startinterval/'+pendingInterval, {method:'POST'})
    .then(r=>{
      if(r.ok){
        document.getElementById('interval-badge').textContent = 'INTERVAL SET — '+pendingInterval;
        document.getElementById('interval-badge').className = 'interval-badge set';
      }
    }).catch(e=>console.error(e));
}
// ---- End interval logic ----

// ---- Station MM countdown ----
// Station/landmark list — verified against physical survey and dnaLandmarkName() in v2.1 sketch
var MM_STATIONS = [
  { name: "Southpoint", mm: 0   },
  { name: "Patio",      mm: 15  },
  { name: "Grillers",   mm: 63  },
  { name: "Westpoint",  mm: 72  },
  { name: "Northpoint", mm: 98  },
  { name: "Arches",     mm: 108 },
  { name: "Eastpoint",  mm: 140 },
  { name: "Bamboo",     mm: 157 }
];
var MM_TOTAL = 171;

function mmCountdown(currentMmStr) {
  var cur = parseInt(currentMmStr, 10);
  if (isNaN(cur)) return '';

  // CCW traversal order matches the layout direction
  // Find closest upcoming and most-recently-passed station
  var best = null;
  var bestDist = MM_TOTAL + 1;

  for (var i = 0; i < MM_STATIONS.length; i++) {
    var st = MM_STATIONS[i];
    // Forward (upcoming): distance CCW from cur to station
    var ahead = (st.mm - cur + MM_TOTAL) % MM_TOTAL;
    // Behind (just left): distance from station to cur
    var behind = (cur - st.mm + MM_TOTAL) % MM_TOTAL;

    if (ahead === 0) {
      // At the station — landmark already shown
      return '';
    }

    // Show countdown when within 5 markers ahead or 5 markers behind
    if (ahead > 0 && ahead <= 5 && ahead < bestDist) {
      bestDist = ahead;
      best = { label: st.name + ' \u2192 ' + ahead, type: 'ahead' };
    }
    if (behind > 0 && behind <= 5) {
      // prefer closest-ahead over behind if tie
      if (best === null || best.type !== 'ahead') {
        best = { label: '\u2190 ' + st.name + ' ' + behind, type: 'behind' };
      }
    }
  }

  return best ? best.label : '';
}
// ---- End station MM countdown ----

// ---- pKpH speed display — Otto seed curve ----
// Otto measured curve: PWM → pKpH
var OTTO_CURVE = [
  [40, 11.9], [45, 15.2], [50, 19.7], [55, 23.8],
  [60, 27.9], [65, 31.7], [70, 35.6], [80, 45.0],
  [90, 53.3], [100, 59.4]
];

function ottoCurvePkph(pwm) {
  // Below curve minimum — fixed values
  if (pwm < 15) return 0;
  if (pwm < 30) return 5;
  if (pwm < 40) return 10;
  // Above curve maximum — clamp
  if (pwm >= 100) return 59.4;
  // Linear interpolation between bracketing seed points
  for (var i = 0; i < OTTO_CURVE.length - 1; i++) {
    var lo = OTTO_CURVE[i], hi = OTTO_CURVE[i+1];
    if (pwm >= lo[0] && pwm <= hi[0]) {
      var t = (pwm - lo[0]) / (hi[0] - lo[0]);
      return lo[1] + t * (hi[1] - lo[1]);
    }
  }
  return 0;
}

// Speed window: collect up to 3 valid measured intervals
var speedWindow = [];  // [{distMm, dtMs}]
var speedState  = 'estimated';  // 'estimated' or 'measured'

// spacingMm[n] = CW distance from MMn to MM(n+1) in mm
var spacingMm = [
  330,340,330,315,325,330,315,300,300,295,
  300,290,300,315,315,325,310,300,300,320,
  315,315,305,300,295,300,300,300,300,315,
  330,320,315,310,300,300,300,300,300,300,
  300,300,300,300,300,300,300,300,300,300,
  300,300,300,300,300,300,300,295,320,300,
  315,320,315,325,315,305,300,305,300,300,
  295,295,300,300,300,300,300,300,300,305,
  300,300,305,300,300,330,300,300,305,300,
  300,300,300,300,300,300,300,300,300,300,
  300,300,295,300,300,300,300,300,305,300,
  300,320,320,300,300,300,300,300,300,300,
  300,300,300,300,300,300,280,300,300,290,
  300,300,300,300,300,300,300,300,300,300,
  300,300,300,300,305,300,305,300,295,300,
  300,300,305,300,300,305,320,290,320,300,
  300,305,330,330,320,325,315,355,330,330,
  330
];

function getSegmentMm(mmNum, dir) {
  var n = parseInt(mmNum, 10);
  if (isNaN(n)) return 300;
  if (dir === 'CW') {
    return spacingMm[n] || 300;
  } else {
    // CCW: distance just traversed is spacingMm[n] (from n to n+1 CW = n+1 to n CCW)
    return spacingMm[n] || 300;
  }
}

function clearSpeedWindow() {
  speedWindow = [];
  speedState = 'estimated';
}

function addSpeedSample(mmNum, dir, dtMs) {
  var seg = getSegmentMm(mmNum, dir);
  speedWindow.push({distMm: seg, dtMs: dtMs});
  if (speedWindow.length > 3) speedWindow.shift();
  if (speedWindow.length >= 3) speedState = 'measured';
}

function calcMeasuredPkph() {
  if (speedWindow.length < 3) return null;
  var totalDist = 0, totalTime = 0;
  for (var i = 0; i < speedWindow.length; i++) {
    totalDist += speedWindow[i].distMm;
    totalTime += speedWindow[i].dtMs;
  }
  if (totalTime <= 0) return null;
  // mm/ms → m/s → km/h × 22.5 (G scale)
  return (totalDist / totalTime) * 3.6 * 45.0;
}

function updatePkphBadge(s) {
  var el = document.getElementById('mm-pkph-val');
  if (!el) return;

  var pwm = parseInt(s.throttle) || 0;
  var dtMs = parseInt(s.mm_dt) || 0;
  var mmNum = s.mm_current;
  var dir   = s.mm_direction || 'CW';

  // Add to measured window if we have a valid recent magnet event
  if (dtMs > 0 && mmNum && mmNum !== '--') {
    addSpeedSample(mmNum, dir, dtMs);
  }

  var pkph, color;

  if (speedState === 'measured') {
    pkph = calcMeasuredPkph();
    if (pkph !== null) {
      color = '#7fff7f';  // green = measured
    }
  }

  if (!pkph || speedState === 'estimated') {
    pkph  = ottoCurvePkph(pwm);
    color = '#ffd080';  // yellow = estimated
  }

  // No decimal, no label — just the integer
  el.textContent = Math.round(pkph);
  el.style.color = color;
}
// ---- End pKpH speed display ----

function sendCmd(sub, val) {
  if (isCto) return;
  fetch('/{{ slug }}/cmd/'+sub+'/'+encodeURIComponent(val),{method:'POST'}).catch(e=>console.error(e));
}

function toggleEstop() {
  var btn = document.getElementById('estop-btn');
  var isActive = btn.classList.contains('active');
  // Optimistic immediate update
  if (isActive) {
    btn.textContent = 'E-STOP';
    btn.classList.remove('active');
  } else {
    btn.textContent = 'E-STOP ACTIVE \u2014 TAP TO CLEAR';
    btn.classList.add('active');
  }
  fetch('/{{ slug }}/cmd/estop/' + (isActive ? '0' : '1'), {method:'POST'})
    .catch(e => {
      console.error(e);
      // Revert optimistic update on network error
      if (isActive) {
        btn.textContent = 'E-STOP ACTIVE \u2014 TAP TO CLEAR';
        btn.classList.add('active');
      } else {
        btn.textContent = 'E-STOP';
        btn.classList.remove('active');
      }
    });
}

function sendDir(d) {
  {% if cto %}return;{% endif %}
  fetch('/{{ slug }}/cmd/direction/'+d, {method:'POST'})
    .then(r => { if(r.ok) updateDirButtons(String(d)); })
    .catch(e => console.error(e));
}

function sendSessionDir(d) {
  {% if cto %}return;{% endif %}
  fetch('/{{ slug }}/sessiondir/'+d, {method:'GET'})
    .then(r => {
      document.getElementById('sdir-cw').classList.toggle('active', d==='CW');
      document.getElementById('sdir-ccw').classList.toggle('active', d==='CCW');
    }).catch(e => console.error(e));
}

function updateDirButtons(d) {
  var classes = {0:'active-rev', 1:'active-neu', 2:'active-fwd'};
  [0,1,2].forEach(function(i){
    var btn = document.getElementById('dir-btn-'+i);
    if(!btn) return;
    btn.classList.remove('active-rev','active-neu','active-fwd');
    if(String(i)===String(d)) btn.classList.add(classes[i]);
  });
}
function fmt1(v){ var n=parseFloat(v); return isNaN(n)?v:n.toFixed(1); }
function pollState(){
  fetch('/{{ slug }}/state').then(r=>r.json()).then(s=>{
    document.getElementById('telem-voltage').textContent = fmt1(s.voltage);
    document.getElementById('telem-current').textContent = fmt1(s.current);
    document.getElementById('telem-power').textContent   = fmt1(s.power);
    document.getElementById('telem-lowvolt').textContent = s.lowvolt==='1'?'LOW':'OK';
    document.getElementById('telem-lowvolt').className   = 'telem-val'+(s.lowvolt==='1'?' warn':'');
    // PWM and last measured pKPH
    document.getElementById('telem-ramp-pwm').textContent = s.ramp_pwm||'--';
    document.getElementById('telem-pkph').textContent     = s.last_measured_pkph||'--';
    // Profile Memory panel
    function profColor(v){
      if(!v||v==='--') return '#888';
      if(v.indexOf('timeout')>=0||v.indexOf('invalid')>=0||v.indexOf('seed')>=0) return '#e8a838';
      if(v.indexOf('waiting')>=0) return '#aaa';
      return '#4fc34f';
    }
    function svcColor(v){
      if(v==='online') return '#4fc34f';
      if(v==='stale')  return '#e8a838';
      return '#e05252';
    }
    var cw=s.profile_cw||'--', ccw=s.profile_ccw||'--';
    var laps=s.profile_laps||'--', svc=s.profile_svc||'--';
    document.getElementById('prof-cw').textContent=cw;
    document.getElementById('prof-cw').style.color=profColor(cw);
    document.getElementById('prof-ccw').textContent=ccw;
    document.getElementById('prof-ccw').style.color=profColor(ccw);
    document.getElementById('prof-laps').textContent=laps;
    document.getElementById('prof-svc').textContent=svc;
    document.getElementById('prof-svc').style.color=svcColor(svc);
    // Throttle display — only update from server when operator is not actively using slider
    var thrSlider = document.getElementById('throttle-slider');
    if (Date.now() > thrSliderLockedUntil) {
      document.getElementById('thr-val').textContent = s.throttle;
      document.getElementById('throttle-display').innerHTML = s.throttle+'<span> / 255</span>';
      // Also sync slider position when loco reports zero (confirmed stopped)
      if (thrSlider && parseInt(s.throttle) === 0) {
        thrSlider.value = 0;
      }
    }
    document.getElementById('block-display').textContent = '{{ name }}: '+s.block;
    // Current MM / DNA status — gate on nav_ready to suppress stale retained MQTT values
    var navReady = (s.nav_ready === '1');
    var mmDisp = document.getElementById('mm-display');
    mmDisp.innerHTML = (navReady && s.mm_current && s.mm_current!=='--') ? s.mm_current : '<span style="color:#555;">— —</span>';
    document.getElementById('mm-landmark').textContent = (navReady && s.mm_landmark && s.mm_landmark !== '' && s.mm_landmark !== 'NONE') ? s.mm_landmark : '';
    document.getElementById('mm-countdown').textContent = navReady ? mmCountdown(s.mm_current) : '';
    var mmStat = document.getElementById('mm-status');
    if (navReady) {
      mmStat.textContent = s.mm_status || '--';
      mmStat.className = 'mm-stat-val' + (s.mm_status==='DNA_OK' ? ' ok' : (s.mm_status==='DNA_CHECK' ? ' check' : ''));
    } else {
      mmStat.textContent = '--';
      mmStat.className = 'mm-stat-val';
    }
    var certPct = document.getElementById('mm-cert-pct');
    var certLbl = certPct ? certPct.parentElement.querySelector('div:last-child') : null;
    var certVal = (navReady && s.mm_cert) ? s.mm_cert : '--';
    var certNum = parseFloat(certVal);
    if (certPct) {
      if (navReady && !isNaN(certNum)) {
        certPct.textContent = Math.round(certNum * 100);
        var isDnaOk = (s.mm_status === 'DNA_OK');
        certPct.style.color = isDnaOk ? '#7fff7f' : '#ffd080';
        if (certLbl) certLbl.style.color = isDnaOk ? '#7fff7f' : '#ffd080';
      } else {
        certPct.textContent = '--';
        certPct.style.color = '#555';
        if (certLbl) certLbl.style.color = '#555';
      }
    }
    var lastHit = document.getElementById('mm-last-hit');
    if (lastHit) {
      if (s.mm_current && s.mm_current !== '--') {
        lastHit.textContent = (s.mm_polarity||'') + '  ' + (s.mm_direction||'') + '  Interval ' + (s.mm_interval||'') + '  dt=' + (s.mm_dt||'') + 'ms';
      } else {
        lastHit.textContent = '';
      }
    }
    // pKpH speed badge — clear window on throttle change or stop; suppress when not nav_ready
    var isStopped = (parseInt(s.throttle) === 0);
    if (typeof lastThrottle !== 'undefined' && lastThrottle !== s.throttle) {
      clearSpeedWindow();
    }
    if (!navReady || isStopped) {
      clearSpeedWindow();
      var el = document.getElementById('mm-pkph-val');
      if (el) { el.textContent = navReady ? '0' : '--'; el.style.color = '#555'; }
    } else {
      updatePkphBadge(s);
    }
    lastThrottle = s.throttle;
    // Keep isCto in sync — sendCmd checks this at call time
    isCto = (s.auto === '1');
    var navBadge = document.getElementById('navready-badge');
    if(s.nav_ready==='1'){
      navBadge.textContent='NAV READY — '+s.session_dir;
      navBadge.className='navready-badge ready';
    } else {
      navBadge.textContent='SET SESSION DIRECTION TO ENABLE THROTTLE';
      navBadge.className='navready-badge notready';
    }
    document.getElementById('sdir-cw').classList.toggle('active', s.session_dir==='CW');
    document.getElementById('sdir-ccw').classList.toggle('active', s.session_dir==='CCW');
    updateDirButtons(s.direction);
    // Update interval badge from server state (reflects confirmed publishes)
    if(s.start_interval && s.start_interval!=='UNSET'){
      // Only update badge from server — never move the slider or display from pollState
      var badge = document.getElementById('interval-badge');
      if(badge && badge.className.indexOf('set') === -1){
        badge.textContent='INTERVAL SET — '+s.start_interval;
        badge.className='interval-badge set';
      }
    }

    // Update estop button dynamically
    var estopBtn = document.getElementById('estop-btn');
    if(estopBtn){
      if(s.estop==='1'){
        estopBtn.textContent='E-STOP ACTIVE \u2014 TAP TO CLEAR';
        estopBtn.className='estop-btn active';
      } else {
        estopBtn.textContent='E-STOP';
        estopBtn.className='estop-btn';
      }
    }
  }).catch(e=>{});
}
var logPausedUntil = {};
function clearLog(id){
  document.getElementById(id).innerHTML='<div style="color:#aaa;font-size:11px;">Cleared — resuming in 5s</div>';
  logPausedUntil[id]=Date.now()+5000;
}
function pollLog(){
  if(logPausedUntil['loco-log'] && Date.now()<logPausedUntil['loco-log']) return;
  fetch('/{{ slug }}/log').then(r=>r.json()).then(entries=>{
    if(!entries.length) return;
    document.getElementById('loco-log').innerHTML=entries.map(e=>
      e.ts+'  '+e.topic+'  '+e.value
    ).join('<br>');
  }).catch(e=>{});
}
setInterval(pollState,2000);
setInterval(pollLog,1000);
</script>
</body></html>"""


# ============================================================================
# Loco render helper
# ============================================================================
def render_loco(lid, name, slug, nav_active):
    with mqtt_lock:
        raw = loco_state[lid].copy()

    class S:
        pass
    s = S()
    for k, v in raw.items():
        setattr(s, k, v)

    cto = raw["auto"] == "1"

    # Convert stored start_interval to slider MM value
    interval_mm = 85  # visual center — slider position only, nothing sent until operator presses SET INTERVAL
    si = raw.get("start_interval", "UNSET")
    if si and si != "UNSET":
        try:
            interval_mm = int(si[:3])
        except Exception:
            interval_mm = 85

    return render_template_string(
        LOCO_HTML,
        name=name, lid=lid, slug=slug, s=s, cto=cto,
        interval_mm=interval_mm,
        nav_html=nav(nav_active), nav_style=NAV_STYLE, shared_css=SHARED_CSS,
    )


# ============================================================================
# Root
# ============================================================================
@app.route("/")
def index():
    return redirect(url_for("console"))


# ============================================================================
# Dispatcher routes
# ============================================================================
@app.route("/console")
def console():
    with mqtt_lock:
        oa = loco_state["9950011"]["auto"]
        ta = loco_state["9950012"]["auto"]
        oo = loco_state["9950011"]["online"]
        to = loco_state["9950012"]["online"]
        ob = loco_state["9950011"]["block"]
        tb = loco_state["9950012"]["block"]
        oc = loco_state["9950011"]["ce"]
        tc = loco_state["9950012"]["ce"]
    return render_template_string(
        CONSOLE_HTML,
        otto_auto=oa, toby_auto=ta, otto_online=oo, toby_online=to,
        otto_block=ob, toby_block=tb, otto_ce=oc, toby_ce=tc,
        nav_html=nav("console"), nav_style=NAV_STYLE, shared_css=SHARED_CSS,
    )

@app.route("/dispatcher/state")
def dispatcher_state():
    with mqtt_lock:
        return jsonify({
            "otto_auto":    loco_state["9950011"]["auto"],
            "toby_auto":    loco_state["9950012"]["auto"],
            "otto_online":  loco_state["9950011"]["online"],
            "toby_online":  loco_state["9950012"]["online"],
            "otto_block":   loco_state["9950011"]["block"],
            "toby_block":   loco_state["9950012"]["block"],
            "otto_ce":      loco_state["9950011"]["ce"],
            "toby_ce":      loco_state["9950012"]["ce"],
            "otto_voltage": loco_state["9950011"]["voltage"],
            "otto_current": loco_state["9950011"]["current"],
            "otto_power":   loco_state["9950011"]["power"],
            "otto_lowvolt": loco_state["9950011"]["lowvolt"],
            "toby_voltage": loco_state["9950012"]["voltage"],
            "toby_current": loco_state["9950012"]["current"],
            "toby_power":   loco_state["9950012"]["power"],
            "toby_lowvolt": loco_state["9950012"]["lowvolt"],
        })

@app.route("/dispatcher/cmd/<path:subcmd>", methods=["POST"])
def dispatcher_cmd(subcmd):
    allowed = {"go","stop","go/9950011","go/9950012","stop/9950011","stop/9950012","ce","estop"}
    if subcmd in allowed:
        pub_dispatcher(subcmd)
    return "", 204

@app.route("/dispatcher/endcto", methods=["POST"])
def dispatcher_endcto():
    pub_loco("9950011", "dispatcher_release", "1")
    pub_loco("9950012", "dispatcher_release", "1")
    pub_dispatcher("stop")
    return "", 204


# ============================================================================
# Otto routes
# ============================================================================
@app.route("/otto")
def otto():
    return render_loco("9950011", "Otto", "otto", "otto")

@app.route("/otto/state")
def otto_state():
    with mqtt_lock:
        return jsonify(loco_state["9950011"])

@app.route("/otto/cmd/<subtopic>/<value>", methods=["POST"])
def otto_cmd(subtopic, value):
    with mqtt_lock:
        if loco_state["9950011"]["auto"] == "1":
            return "", 423
    if subtopic in {"throttle","brake","direction","estop"}:
        pub_loco("9950011", subtopic, value)
    return "", 204

@app.route("/otto/dir/<int:d>")
def otto_dir(d):
    with mqtt_lock:
        if loco_state["9950011"]["auto"] == "1":
            return "", 423
    if d in (0,1,2):
        pub_loco("9950011", "direction", d)
    return "", 204

@app.route("/otto/sessiondir/<d>")
def otto_sessiondir(d):
    with mqtt_lock:
        if loco_state["9950011"]["auto"] == "1":
            return "", 423
    if d in ("CW","CCW"):
        pub_loco("9950011", "session_direction", d)
    return "", 204

@app.route("/otto/startinterval/<interval>", methods=["GET","POST"])
def otto_startinterval(interval):
    with mqtt_lock:
        if loco_state["9950011"]["auto"] == "1":
            return ("", 423) if request.method == "POST" else redirect("/otto")
    import re
    if re.match(r'^\d{3}-\d{3}$', interval):
        pub_loco("9950011", "start_interval", interval)
    return ("", 204) if request.method == "POST" else redirect("/otto")

@app.route("/otto/mode/<int:m>")
def otto_mode(m):
    if m == 1:
        pub_loco("9950011", "auto", "1")
    return redirect("/otto")

@app.route("/otto/estop")
def otto_estop():
    with mqtt_lock:
        cur = loco_state["9950011"]["estop"]
    pub_loco("9950011", "estop", "0" if cur=="1" else "1")
    time.sleep(0.15)
    return redirect("/otto")

@app.route("/otto/cal/<action>", methods=["GET","POST"])
def otto_cal(action):
    if action == "start":
        ok, msg = cal_start("9950011")
        st = cal_status("9950011")
        st["file"] = msg if ok else st.get("file","")
        return jsonify(st)
    elif action == "stop":
        cal_stop("9950011")
        st = cal_status("9950011")
        return jsonify(st)
    else:  # status
        return jsonify(cal_status("9950011"))


# ============================================================================
# Toby routes
# ============================================================================
@app.route("/toby")
def toby():
    return render_loco("9950012", "Toby", "toby", "toby")

@app.route("/toby/state")
def toby_state():
    with mqtt_lock:
        return jsonify(loco_state["9950012"])

@app.route("/toby/cmd/<subtopic>/<value>", methods=["POST"])
def toby_cmd(subtopic, value):
    with mqtt_lock:
        if loco_state["9950012"]["auto"] == "1":
            return "", 423
    if subtopic in {"throttle","brake","direction","estop"}:
        pub_loco("9950012", subtopic, value)
    return "", 204

@app.route("/toby/dir/<int:d>")
def toby_dir(d):
    with mqtt_lock:
        if loco_state["9950012"]["auto"] == "1":
            return "", 423
    if d in (0,1,2):
        pub_loco("9950012", "direction", d)
    return "", 204

@app.route("/toby/sessiondir/<d>")
def toby_sessiondir(d):
    with mqtt_lock:
        if loco_state["9950012"]["auto"] == "1":
            return "", 423
    if d in ("CW","CCW"):
        pub_loco("9950012", "session_direction", d)
    return "", 204

@app.route("/toby/startinterval/<interval>", methods=["GET","POST"])
def toby_startinterval(interval):
    with mqtt_lock:
        if loco_state["9950012"]["auto"] == "1":
            return ("", 423) if request.method == "POST" else redirect("/toby")
    import re
    if re.match(r'^\d{3}-\d{3}$', interval):
        pub_loco("9950012", "start_interval", interval)
    return ("", 204) if request.method == "POST" else redirect("/toby")

@app.route("/toby/mode/<int:m>")
def toby_mode(m):
    if m == 1:
        pub_loco("9950012", "auto", "1")
    return redirect("/toby")

@app.route("/toby/estop")
def toby_estop():
    with mqtt_lock:
        cur = loco_state["9950012"]["estop"]
    pub_loco("9950012", "estop", "0" if cur=="1" else "1")
    time.sleep(0.15)
    return redirect("/toby")

@app.route("/toby/cal/<action>", methods=["GET","POST"])
def toby_cal(action):
    if action == "start":
        ok, msg = cal_start("9950012")
        st = cal_status("9950012")
        st["file"] = msg if ok else st.get("file","")
        return jsonify(st)
    elif action == "stop":
        cal_stop("9950012")
        st = cal_status("9950012")
        return jsonify(st)
    else:
        return jsonify(cal_status("9950012"))


# ============================================================================
# Log feeds
# ============================================================================
@app.route("/otto/log")
def otto_log():
    with mqtt_lock:
        return jsonify(list(loco_log["9950011"]))

@app.route("/toby/log")
def toby_log():
    with mqtt_lock:
        return jsonify(list(loco_log["9950012"]))

@app.route("/dispatcher/log")
def dispatcher_log():
    with mqtt_lock:
        return jsonify(list(dispatch_log))


# ============================================================================
# Placeholders
# ============================================================================
@app.route("/oscar")
def oscar():
    return "<h2 style='font-family:Arial;padding:2rem;color:#ccc;background:#222;min-height:100vh;margin:0;'>Oscar — coming soon</h2>"

# ============================================================================
# Hans routes
# ============================================================================
@app.route("/hans")
def hans():
    return render_loco("2095111", "Hans", "hans", "hans")

@app.route("/hans/state")
def hans_state():
    with mqtt_lock:
        return jsonify(loco_state["2095111"])

@app.route("/hans/cmd/<subtopic>/<value>", methods=["POST"])
def hans_cmd(subtopic, value):
    with mqtt_lock:
        if loco_state["2095111"]["auto"] == "1":
            return "", 423
    if subtopic in {"throttle","brake","direction","estop"}:
        pub_loco("2095111", subtopic, value)
    return "", 204

@app.route("/hans/dir/<int:d>")
def hans_dir(d):
    with mqtt_lock:
        if loco_state["2095111"]["auto"] == "1":
            return "", 423
    if d in (0,1,2):
        pub_loco("2095111", "direction", d)
    return "", 204

@app.route("/hans/sessiondir/<d>")
def hans_sessiondir(d):
    with mqtt_lock:
        if loco_state["2095111"]["auto"] == "1":
            return "", 423
    if d in ("CW","CCW"):
        pub_loco("2095111", "session_direction", d)
    return "", 204

@app.route("/hans/startinterval/<interval>", methods=["GET","POST"])
def hans_startinterval(interval):
    with mqtt_lock:
        if loco_state["2095111"]["auto"] == "1":
            return ("", 423) if request.method == "POST" else redirect("/hans")
    import re as _re
    if _re.match(r'^\d{3}-\d{3}$', interval):
        pub_loco("2095111", "start_interval", interval)
    return ("", 204) if request.method == "POST" else redirect("/hans")

@app.route("/hans/mode/<int:m>")
def hans_mode(m):
    if m == 1:
        pub_loco("2095111", "auto", "1")
    return redirect("/hans")

@app.route("/hans/estop")
def hans_estop():
    with mqtt_lock:
        cur = loco_state["2095111"]["estop"]
    pub_loco("2095111", "estop", "0" if cur=="1" else "1")
    time.sleep(0.15)
    return redirect("/hans")

@app.route("/hans/cal/<action>", methods=["GET","POST"])
def hans_cal(action):
    if action == "start":
        ok, msg = cal_start("2095111")
        st = cal_status("2095111")
        st["file"] = msg if ok else st.get("file","")
        return jsonify(st)
    elif action == "stop":
        cal_stop("2095111")
        st = cal_status("2095111")
        return jsonify(st)
    else:
        return jsonify(cal_status("2095111"))

@app.route("/hans/log")
def hans_log():
    with mqtt_lock:
        return jsonify(list(loco_log["2095111"]))


# ============================================================================
if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8080, threaded=True)

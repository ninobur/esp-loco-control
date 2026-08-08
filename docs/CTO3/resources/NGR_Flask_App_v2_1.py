from flask import Flask, render_template_string, request, jsonify, redirect, url_for, make_response
from markupsafe import Markup
from markupsafe import Markup
import threading
import time
import hashlib
import paho.mqtt.client as mqtt_client

app = Flask(__name__)

# PIN authentication removed — can be re-added later
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
                "voltage":"--","current":"--","power":"--","block":"--"},
    "9950012": {"online":"0","throttle":"0","direction":"1","brake":"0",
                "estop":"0","auto":"0","ce":"0","lowvolt":"0","warning":"",
                "voltage":"--","current":"--","power":"--","block":"--"},
}

mqtt_lock = threading.Lock()
mqtt_conn = None



# Per-loco message logs — newest first, capped at 100 entries each
from collections import deque
import datetime
loco_log = {
    "9950011": deque(maxlen=100),
    "9950012": deque(maxlen=100),
}
dispatch_log = deque(maxlen=200)  # combined feed for dispatcher

def on_mqtt_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        print("MQTT connected")
        for lid in loco_state:
            base = f"ngr/loco/{lid}"
            client.subscribe(f"{base}/status/online")
            client.subscribe(f"{base}/state/#")  # includes state/ce
            client.subscribe(f"{base}/telem/#")
    else:
        print(f"MQTT connect failed rc={rc}")

def on_mqtt_message(client, userdata, msg):
    topic   = msg.topic
    payload = msg.payload.decode("utf-8", errors="ignore")
    for lid in loco_state:
        base = f"ngr/loco/{lid}"
        with mqtt_lock:
            # Log operational events only — suppress telemetry and offline noise
            if topic.startswith(base):
                subtopic = topic[len(base)+1:]
                # Drop if loco is offline
                if loco_state[lid]["online"] != "1":
                    pass
                # Drop all telemetry — shown in telemetry panel instead
                elif subtopic.startswith("telem/"):
                    pass
                # Drop lowvolt=0 (normal state, not an event)
                elif subtopic == "state/lowvolt" and payload == "0":
                    pass
                else:
                    ts = datetime.datetime.now().strftime("%H:%M:%S")
                    entry = {"ts": ts, "topic": subtopic, "value": payload}
                    loco_log[lid].appendleft(entry)
                    dispatch_log.appendleft({"ts": ts, "loco": lid, "topic": subtopic, "value": payload})
            if   topic == f"{base}/status/online":   loco_state[lid]["online"]    = payload
            elif topic == f"{base}/state/throttle":  loco_state[lid]["throttle"]  = payload
            elif topic == f"{base}/state/direction": loco_state[lid]["direction"] = payload
            elif topic == f"{base}/state/brake":     loco_state[lid]["brake"]     = payload
            elif topic == f"{base}/state/estop":     loco_state[lid]["estop"]     = payload
            elif topic == f"{base}/state/auto":      loco_state[lid]["auto"]      = payload
            elif topic == f"{base}/state/lowvolt":   loco_state[lid]["lowvolt"]   = payload
            elif topic == f"{base}/state/warning":   loco_state[lid]["warning"]   = payload
            elif topic == f"{base}/state/block":     loco_state[lid]["block"]     = payload
            elif topic == f"{base}/state/ce":        loco_state[lid]["ce"]        = payload
            elif topic == f"{base}/telem/voltage":   loco_state[lid]["voltage"]   = payload
            elif topic == f"{base}/telem/current":   loco_state[lid]["current"]   = payload
            elif topic == f"{base}/telem/power":     loco_state[lid]["power"]     = payload

def pub(topic, value):
    global mqtt_conn
    if mqtt_conn and mqtt_conn.is_connected():
        mqtt_conn.publish(topic, str(value), retain=False)

def pub_loco(lid, subtopic, value):
    pub(f"ngr/loco/{lid}/cmd/{subtopic}", value)

def pub_dispatcher(subcmd):
    pub(f"ngr/dispatcher/cmd/{subcmd}", "1")

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
        if n in ("oscar","hans"): return "placeholder"
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
.loco-header { display:flex; align-items:center; justify-content:space-between; }
.loco-title  { font-size:22px; font-weight:bold; color:#f1f1f1; letter-spacing:2px; }
.badge { font-size:12px; font-weight:bold; padding:4px 10px; border-radius:20px; letter-spacing:1px; }
.badge-online  { background:#2a7a2a; color:#7fff7f; border:1px solid #4fc34f; }
.badge-offline { background:#5a2020; color:#ff9999; border:1px solid #b32020; }
.mode-row { display:flex; gap:8px; margin-bottom:8px; }
.mode-btn { flex:1; text-align:center; padding:10px 4px; border-radius:8px;
  border:2px solid #555; background:rgba(30,30,30,0.8); color:#888;
  font-size:14px; font-weight:bold; letter-spacing:1px; text-decoration:none; display:block; }
.mode-btn.man-active { background:rgba(50,50,70,0.95); color:#aac; border-color:#88a; }
.mode-btn.cto-active { background:rgba(40,70,40,0.95); color:#7f7; border-color:#4a4; }
.mode-btn.locked     { opacity:0.35; pointer-events:none; cursor:default; }
.mode-note    { text-align:center; color:#666; font-size:11px; margin-top:4px; }
.cto-note     { text-align:center; color:#7f7; font-size:13px; letter-spacing:1px; padding:6px 0 2px; }
.thr-row   { display:flex; align-items:center; gap:10px; }
.thr-label { color:#888; font-size:13px; min-width:14px; }
.thr-val   { color:#4fc34f; font-size:20px; font-weight:bold; min-width:36px; text-align:right; }
input[type=range] { flex:1; accent-color:#4fc34f; }
.dir-row { display:flex; gap:8px; }
.dir-btn { flex:1; text-align:center; padding:12px 4px; border-radius:8px;
  border:2px solid #555; background:rgba(30,30,30,0.8); color:#aaa;
  font-size:14px; font-weight:bold; cursor:pointer; text-decoration:none; }
.dir-btn.active-rev { background:#5a2020; color:#ff9999; border-color:#b32020; }
.dir-btn.active-neu { background:#4a4a10; color:#ffd080; border-color:#aa8800; }
.dir-btn.active-fwd { background:#1a4a1a; color:#7fff7f; border-color:#2a7a2a; }
.dir-btn.locked     { opacity:0.35; pointer-events:none; }
.slider-section { padding:4px 0; }
.slider-label { display:flex; justify-content:space-between; color:#888; font-size:12px; margin-bottom:4px; }
.telem-grid { display:grid; grid-template-columns:1fr 1fr 1fr 1fr; gap:8px; margin-bottom:10px; }
.telem-cell { background:#222; border-radius:8px; padding:8px 10px; }
.telem-lbl  { font-size:11px; color:#888; margin-bottom:2px; }
.telem-val  { font-size:20px; font-weight:bold; color:#4fc34f; }
.telem-val.warn { color:#d9a21b; }
.block-display { text-align:center; color:#f1f1f1; font-size:18px; font-weight:bold;
  margin-top:8px; letter-spacing:2px; }
.warning-label { text-align:center; color:#d9a21b; font-size:13px; min-height:18px; margin-top:6px; }
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
.status-row{display:flex;gap:12px;justify-content:center;margin-top:4px;flex-wrap:wrap;}
.status-item{text-align:center;}
.status-lbl{font-size:10px;color:#666;}
.status-val{font-size:15px;font-weight:bold;}
.status-val.on{color:#7f7;} .status-val.off{color:#555;}

.block-section-lbl{text-align:center;color:#555;font-size:10px;letter-spacing:2px;
  text-transform:lowercase;margin-top:10px;margin-bottom:4px;}
.block-row{display:flex;gap:8px;}
.block-cell{flex:1;background:#1a1a1a;border-radius:8px;padding:6px 8px;text-align:center;
  border:1px solid #444;}
.block-loco-lbl{font-size:10px;color:#666;margin-bottom:2px;}
.block-val{font-size:15px;font-weight:bold;color:#f1f1f1;letter-spacing:1px;}
</style></head><body>
{{ nav_html }}
<div class="container">

  <div class="panel">
    <h2>DISPATCHER</h2>
    <div style="display:grid;grid-template-columns:1fr 1fr;gap:12px;">
      <div>
        <div style="text-align:center;color:#aaa;font-size:12px;font-weight:bold;
          letter-spacing:2px;margin-bottom:8px;border-bottom:1px solid #444;padding-bottom:6px;">OTTO</div>
        <div class="status-item" style="margin-bottom:6px;">
          <div class="status-lbl">STATUS</div>
          <div class="status-val" id="otto-mode"
            style="color:{{ '#7f7' if otto_auto=='1' else '#888' }};">
            {{ 'CE' if (otto_auto=='1' and otto_ce=='1') else ('CTO' if otto_auto=='1' else 'MAN') }}
          </div>
        </div>
        <div class="status-item" style="margin-bottom:6px;">
          <div class="status-lbl">BLOCK</div>
          <div class="status-val" id="otto-block" style="color:#f1f1f1;">{{ otto_block }}</div>
        </div>
        <div class="status-item">
          <div class="status-lbl">ONLINE</div>
          <div class="status-val {{ 'on' if otto_online=='1' else 'off' }}" id="otto-online">
            {{ 'YES' if otto_online=='1' else 'NO' }}</div>
        </div>
      </div>
      <div>
        <div style="text-align:center;color:#aaa;font-size:12px;font-weight:bold;
          letter-spacing:2px;margin-bottom:8px;border-bottom:1px solid #444;padding-bottom:6px;">TOBY</div>
        <div class="status-item" style="margin-bottom:6px;">
          <div class="status-lbl">STATUS</div>
          <div class="status-val" id="toby-mode"
            style="color:{{ '#7f7' if toby_auto=='1' else '#888' }};">
            {{ 'CE' if (toby_auto=='1' and toby_ce=='1') else ('CTO' if toby_auto=='1' else 'MAN') }}
          </div>
        </div>
        <div class="status-item" style="margin-bottom:6px;">
          <div class="status-lbl">BLOCK</div>
          <div class="status-val" id="toby-block" style="color:#f1f1f1;">{{ toby_block }}</div>
        </div>
        <div class="status-item">
          <div class="status-lbl">ONLINE</div>
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

  <!-- Live telemetry -->
  <div class="panel">
    <div style="display:grid;grid-template-columns:1fr 1fr;gap:10px;">
      <div>
        <div style="text-align:center;color:#aaa;font-size:11px;font-weight:bold;
          letter-spacing:1px;margin-bottom:6px;">OTTO</div>
        <div style="display:grid;grid-template-columns:1fr 1fr;gap:6px;">
          <div class="telem-cell">
            <div class="telem-lbl">Voltage</div>
            <div class="telem-val" id="d-otto-voltage">--</div>
          </div>
          <div class="telem-cell">
            <div class="telem-lbl">Current</div>
            <div class="telem-val" id="d-otto-current">--</div>
          </div>
          <div class="telem-cell">
            <div class="telem-lbl">Power</div>
            <div class="telem-val" id="d-otto-power">--</div>
          </div>
          <div class="telem-cell">
            <div class="telem-lbl">Low V</div>
            <div class="telem-val" id="d-otto-lowvolt">--</div>
          </div>
        </div>
      </div>
      <div>
        <div style="text-align:center;color:#aaa;font-size:11px;font-weight:bold;
          letter-spacing:1px;margin-bottom:6px;">TOBY</div>
        <div style="display:grid;grid-template-columns:1fr 1fr;gap:6px;">
          <div class="telem-cell">
            <div class="telem-lbl">Voltage</div>
            <div class="telem-val" id="d-toby-voltage">--</div>
          </div>
          <div class="telem-cell">
            <div class="telem-lbl">Current</div>
            <div class="telem-val" id="d-toby-current">--</div>
          </div>
          <div class="telem-cell">
            <div class="telem-lbl">Power</div>
            <div class="telem-val" id="d-toby-power">--</div>
          </div>
          <div class="telem-cell">
            <div class="telem-lbl">Low V</div>
            <div class="telem-val" id="d-toby-lowvolt">--</div>
          </div>
        </div>
      </div>
    </div>
  </div>

  <div class="panel">
    <button class="btn-estop" onclick="dc('estop')">&#9888; E-STOP</button>
  </div>

  <!-- Combined packet log -->
  <div class="panel">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:6px;">
      <span style="color:#888;font-size:11px;letter-spacing:1px;">PACKET LOG</span>
      <button onclick="clearLog('dispatch-log')" style="background:#333;border:1px solid #555;
        color:#aaa;font-size:11px;padding:4px 12px;border-radius:6px;cursor:pointer;">
        CLEAR
      </button>
    </div>
    <div class="log-panel" id="dispatch-log" style="color:#ffffff;background:#0a0a0a;">
      <div style="color:#aaa;font-size:11px;">Waiting for packets...</div>
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
    var ottoStatus = s.otto_auto==='1'?(s.otto_ce==='1'?'CE':'CTO'):'MAN';
    var tobyStatus = s.toby_auto==='1'?(s.toby_ce==='1'?'CE':'CTO'):'MAN';
    document.getElementById('otto-mode').textContent = ottoStatus;
    document.getElementById('otto-mode').style.color = s.otto_auto==='1'?'#7f7':'#888';
    document.getElementById('toby-mode').textContent = tobyStatus;
    document.getElementById('toby-mode').style.color = s.toby_auto==='1'?'#7f7':'#888';
    document.getElementById('otto-online').textContent = s.otto_online==='1'?'YES':'NO';
    document.getElementById('otto-online').className   = 'status-val '+(s.otto_online==='1'?'on':'off');
    document.getElementById('toby-online').textContent = s.toby_online==='1'?'YES':'NO';
    document.getElementById('toby-online').className   = 'status-val '+(s.toby_online==='1'?'on':'off');
    document.getElementById('otto-block').textContent = s.otto_block||'--';
    document.getElementById('toby-block').textContent = s.toby_block||'--';
    // Telemetry
    function fmtT(v){ var n=parseFloat(v); return isNaN(n)||v=='--'?'--':n.toFixed(1); }
    document.getElementById('d-otto-voltage').textContent = fmtT(s.otto_voltage);
    document.getElementById('d-otto-current').textContent = fmtT(s.otto_current);
    document.getElementById('d-otto-power').textContent   = fmtT(s.otto_power);
    document.getElementById('d-otto-lowvolt').textContent = s.otto_lowvolt==='1'?'LOW':'OK';
    document.getElementById('d-otto-lowvolt').className   = 'telem-val'+(s.otto_lowvolt==='1'?' warn':'');
    document.getElementById('d-toby-voltage').textContent = fmtT(s.toby_voltage);
    document.getElementById('d-toby-current').textContent = fmtT(s.toby_current);
    document.getElementById('d-toby-power').textContent   = fmtT(s.toby_power);
    document.getElementById('d-toby-lowvolt').textContent = s.toby_lowvolt==='1'?'LOW':'OK';
    document.getElementById('d-toby-lowvolt').className   = 'telem-val'+(s.toby_lowvolt==='1'?' warn':'');
  }).catch(e=>{});
}
var logPausedUntil = {};
function clearLog(id){
  const el = document.getElementById(id);
  if(el) el.innerHTML = '<div style="color:#aaa;font-size:11px;">Log cleared — resuming in 5s</div>';
  logPausedUntil[id] = Date.now() + 5000;
}
function pollLog(){
  if(logPausedUntil['dispatch-log'] && Date.now() < logPausedUntil['dispatch-log']) return;
  fetch('/dispatcher/log').then(r=>r.json()).then(entries=>{
    if(!entries.length) return;
    const names = {'9950011':'Otto','9950012':'Toby'};
    const el = document.getElementById('dispatch-log');
    el.innerHTML = entries.map(e=>
      '<div class="log-entry">'
      +'<span class="log-ts">'+e.ts+'&nbsp;&nbsp;</span>'
      +'<span class="log-loco">'+( names[e.loco]||e.loco )+'&nbsp;&nbsp;</span>'
      +'<span class="log-topic">'+e.topic+'&nbsp;&nbsp;</span>'
      +'<span class="log-val">'+e.value+'</span>'
      +'</div>'
    ).join('');
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
body { margin:0; font-family:Arial,sans-serif;
  background:linear-gradient(135deg,#a8a8a8 0%,#d9d9d9 20%,#8f8f8f 50%,#d7d7d7 80%,#9c9c9c 100%); }
.container { max-width:700px; margin:auto; padding:8px; }
.panel-tight { background:rgba(40,40,40,0.92); border-radius:12px; padding:8px 12px;
  margin-bottom:8px; box-shadow:0 3px 8px rgba(0,0,0,0.3); border:2px solid #666; }
/* Header */
.loco-header { display:flex; align-items:center; justify-content:flex-end; }
.loco-title  { position:absolute; left:50%; transform:translateX(-50%);
  font-size:20px; font-weight:bold; color:#f1f1f1; letter-spacing:2px; }
.header-wrap { position:relative; display:flex; align-items:center;
  justify-content:flex-end; width:100%; }
.badge { font-size:12px; font-weight:bold; padding:3px 10px; border-radius:20px; letter-spacing:1px; }
.badge-online  { background:#2a7a2a; color:#7fff7f; border:1px solid #4fc34f; }
.badge-offline { background:#5a2020; color:#ff9999; border:1px solid #b32020; }
/* Mode */
.mode-row { display:flex; gap:6px; }
.mode-btn { flex:1; text-align:center; padding:8px 4px; border-radius:8px;
  border:2px solid #555; background:rgba(30,30,30,0.8); color:#888;
  font-size:13px; font-weight:bold; letter-spacing:1px; text-decoration:none; display:block; }
.mode-btn.man-active { background:rgba(50,50,70,0.95); color:#aac; border-color:#88a; }
.mode-btn.auto-active { background:rgba(40,70,40,0.95); color:#7f7; border-color:#4a4; }
.mode-btn.locked { opacity:0.35; pointer-events:none; cursor:default; }
.cto-note { text-align:center; color:#7f7; font-size:11px; letter-spacing:1px; margin-top:4px; }
/* Direction */
.dir-row { display:flex; gap:6px; }
.dir-btn { flex:1; text-align:center; padding:9px 2px; border-radius:8px;
  border:2px solid #555; background:rgba(30,30,30,0.8); color:#aaa;
  font-size:13px; font-weight:bold; cursor:pointer; text-decoration:none; }
.dir-btn.active-rev { background:#5a2020; color:#ff9999; border-color:#b32020; }
.dir-btn.active-neu { background:#4a4a10; color:#ffd080; border-color:#aa8800; }
.dir-btn.active-fwd { background:#1a4a1a; color:#7fff7f; border-color:#2a7a2a; }
.dir-btn.locked { opacity:0.35; pointer-events:none; }
/* Sliders */
.slider-row { display:flex; align-items:center; gap:8px; }
.slider-lbl { color:#aaa; font-size:12px; font-weight:bold; letter-spacing:1px;
  min-width:70px; }
.slider-val { font-size:16px; font-weight:bold; min-width:30px; text-align:right; }
.slider-val.green { color:#4fc34f; }
.slider-val.yellow { color:#d9a21b; }
input.green-slider, input.yellow-slider, input.red-slider {
  flex:1; height:28px; cursor:pointer;
  -webkit-appearance:none; appearance:none; border-radius:14px;
  outline:none; border:none; background:#333;
}
input.green-slider::-webkit-slider-thumb,
input.yellow-slider::-webkit-slider-thumb,
input.red-slider::-webkit-slider-thumb {
  -webkit-appearance:none; appearance:none;
  width:32px; height:32px; border-radius:50%;
  background:#eee; border:2px solid #aaa; cursor:pointer;
  box-shadow:0 2px 6px rgba(0,0,0,0.5);
}
input.green-slider::-webkit-slider-runnable-track {
  height:28px; border-radius:14px; background:#333;
}
input.yellow-slider::-webkit-slider-runnable-track {
  height:28px; border-radius:14px; background:#333;
}
input.red-slider::-webkit-slider-runnable-track {
  height:28px; border-radius:14px; background:#333;
}
/* E-STOP */
.estop-wrap { display:flex; justify-content:center; }
.estop-btn { width:80%; padding:12px; border-radius:50px; border:3px solid #b32020;
  background:rgba(40,40,40,0.92); color:#e04040; font-size:18px; font-weight:bold;
  letter-spacing:3px; cursor:pointer; text-align:center; text-decoration:none; display:block; }
.estop-btn.active { background:#b32020; color:white; }
.estop-btn:hover  { background:#8a1515; color:white; border-color:#e04040; }
/* Telemetry */
.telem-grid { display:grid; grid-template-columns:1fr 1fr 1fr 1fr; gap:6px; margin-bottom:4px; }
.telem-cell { background:#222; border-radius:7px; padding:5px 4px; text-align:center; }
.telem-lbl  { font-size:10px; color:#888; margin-bottom:2px; text-align:center; }
.telem-val  { font-size:15px; font-weight:bold; color:#4fc34f; text-align:center; }
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

  <!-- Mode -->
  <div class="panel-tight">
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

  {% set cto = s.auto == '1' %}

  <!-- Direction -->
  <div class="panel-tight"{% if cto %} style="opacity:0.4;"{% endif %}>
    <div class="dir-row">
      <a href="/{{ slug }}/dir/0"
         class="dir-btn {{ 'active-rev' if s.direction=='0' else '' }} {{ 'locked' if cto else '' }}">Reverse</a>
      <a href="/{{ slug }}/dir/1"
         class="dir-btn {{ 'active-neu' if s.direction=='1' else '' }} {{ 'locked' if cto else '' }}">Neutral</a>
      <a href="/{{ slug }}/dir/2"
         class="dir-btn {{ 'active-fwd' if s.direction=='2' else '' }} {{ 'locked' if cto else '' }}">Forward</a>
    </div>
  </div>

  <!-- Throttle -->
  <div class="panel-tight"{% if cto %} style="opacity:0.4;"{% endif %}>
    <div class="slider-row">
      <span class="slider-lbl">THROTTLE</span>
      <input type="range" min="0" max="255" value="{{ s.throttle }}" step="1"
             id="throttle-slider" class="green-slider" style="flex:1;" {% if cto %}disabled{% endif %}
             oninput="updateThrottleColor(this);document.getElementById('thr-val').textContent=this.value"
             onchange="sendCmd('throttle',this.value)" />
      <span class="slider-val green" id="thr-val">{{ s.throttle }}</span>
    </div>
  </div>

  <!-- Brake — reversed: left=hard, right=coast -->
  <div class="panel-tight"{% if cto %} style="opacity:0.4;"{% endif %}>
    <div class="slider-row">
      <span class="slider-lbl">BRAKE</span>
      <input type="range" min="0" max="255" value="{{ s.brake }}" step="1"
             id="brake-slider" class="yellow-slider" {% if cto %}disabled{% endif %}
             style="direction:rtl;"
             oninput="document.getElementById('brake-val').textContent=this.value"
             onchange="sendCmd('brake',brakeCurve(this.value))" />
      <span class="slider-val yellow" id="brake-val">{{ s.brake }}</span>
    </div>
  </div>

  <!-- E-STOP -->
  <div class="panel-tight">
    <div class="estop-wrap">
      <a href="/{{ slug }}/estop"
         class="estop-btn {{ 'active' if s.estop=='1' else '' }}">
        {{ 'E-STOP ACTIVE — TAP TO CLEAR' if s.estop=='1' else 'E-STOP' }}
      </a>
    </div>
  </div>

  <!-- Telemetry -->
  <div class="panel-tight">
    <div class="telem-grid">
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
    </div>
    <div class="block-display" id="block-display">{{ name }}: {{ s.block }}</div>
    <div class="warning-label" id="warning-label">{{ s.warning }}</div>
  </div>

  <!-- Packet log -->
  <div class="panel-tight">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:6px;">
      <span style="color:#888;font-size:11px;letter-spacing:1px;">PACKET LOG</span>
      <button onclick="clearLog('loco-log')" style="background:#333;border:1px solid #555;
        color:#aaa;font-size:11px;padding:4px 12px;border-radius:6px;cursor:pointer;">
        CLEAR
      </button>
    </div>
    <div class="log-panel" id="loco-log" style="color:#ffffff;background:#0a0a0a;">
      <div style="color:#aaa;font-size:11px;">Waiting for packets...</div>
    </div>
  </div>

</div>
<script>
function fmt1(v){ var n=parseFloat(v); return isNaN(n)?v:n.toFixed(1); }
function updateThrottleColor(slider){
  var pct = slider.value / 255;
  var r = Math.round(144 - (144-26)*pct);
  var g = Math.round(238 - (238-92)*pct);
  var b = Math.round(144 - (144-26)*pct);
  var col = 'rgb('+r+','+g+','+b+')';
  slider.style.background = 'linear-gradient(to right, '+col+' 0%, #1a3a1a '+Math.round(pct*100)+'%, #333 '+Math.round(pct*100)+'%)';
  slider.style.accentColor = col;
}
function updateBrakeColor(slider){
  var pct = slider.value / 255;
  var r = Math.round(255 - (255-139)*pct);
  var g = Math.round(153 - 153*pct);
  var b = Math.round(153 - 153*pct);
  var col = 'rgb('+r+','+g+','+b+')';
  slider.style.background = 'linear-gradient(to left, '+col+' 0%, #3a0000 '+Math.round(pct*100)+'%, #333 '+Math.round(pct*100)+'%)';
  slider.style.accentColor = col;
}
function brakeCurve(raw){
  var s = parseInt(raw);
  if(s <= 127){
    // Gentle curve: ~15% output at 25% input, exponent 3
    return Math.round(Math.pow(s/127, 3) * 127);
  } else {
    // Linear 1:1 above midpoint
    return s;
  }
}
function sendCmd(sub,val){
  {% if not cto %}
  fetch('/{{ slug }}/cmd/'+sub+'/'+encodeURIComponent(val),{method:'POST'}).catch(e=>console.error(e));
  {% endif %}
}
function pollState(){
  fetch('/{{ slug }}/state').then(r=>r.json()).then(s=>{
    document.getElementById('telem-voltage').textContent = fmt1(s.voltage);
    document.getElementById('telem-current').textContent = fmt1(s.current);
    document.getElementById('telem-power').textContent   = fmt1(s.power);
    document.getElementById('telem-lowvolt').textContent = s.lowvolt==='1'?'LOW':'OK';
    document.getElementById('telem-lowvolt').className   = 'telem-val'+(s.lowvolt==='1'?' warn':'');
    document.getElementById('block-display').textContent = '{{ name }}: '+s.block;
    if(s.warning){
      document.getElementById('warning-label').textContent = s.warning;
      setTimeout(()=>{ document.getElementById('warning-label').textContent=''; }, 2500);
    }
    if(s.auto !== '{{ s.auto }}') window.location.reload();
  }).catch(e=>{});
}
var logPausedUntil = {};
function clearLog(id){
  const el = document.getElementById(id);
  if(el) el.innerHTML = '<div style="color:#aaa;font-size:11px;">Log cleared — resuming in 5s</div>';
  logPausedUntil[id] = Date.now() + 5000;
}
function pollLog(){
  if(logPausedUntil['loco-log'] && Date.now() < logPausedUntil['loco-log']) return;
  fetch('/{{ slug }}/log').then(r=>r.json()).then(entries=>{
    if(!entries.length) return;
    const el = document.getElementById('loco-log');
    el.innerHTML = entries.map(e=>
      '<div class="log-entry">'
      +'<span class="log-ts">'+e.ts+'&nbsp;&nbsp;</span>'
      +'<span class="log-topic">'+e.topic+'&nbsp;&nbsp;</span>'
      +'<span class="log-val">'+e.value+'</span>'
      +'</div>'
    ).join('');
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

    return render_template_string(
        LOCO_HTML,
        name=name, lid=lid, slug=slug, s=s, cto=cto,
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
    if not is_authenticated(request):
        return redirect(url_for("index"))
    with mqtt_lock:
        oa = loco_state["9950011"]["auto"]
        ta = loco_state["9950012"]["auto"]
        oo = loco_state["9950011"]["online"]
        to = loco_state["9950012"]["online"]
    with mqtt_lock:
        ob = loco_state["9950011"]["block"]
        tb = loco_state["9950012"]["block"]
        oc = loco_state["9950011"]["ce"]
        tc = loco_state["9950012"]["ce"]
    return render_template_string(
        CONSOLE_HTML,
        otto_auto=oa, toby_auto=ta, otto_online=oo, toby_online=to,
        otto_block=ob, toby_block=tb,
        otto_ce=oc,
        toby_ce=tc,
        nav_html=nav("console"), nav_style=NAV_STYLE, shared_css=SHARED_CSS,
    )

@app.route("/dispatcher/state")
def dispatcher_state():
    if not is_authenticated(request):
        return jsonify({}), 403
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
    if not is_authenticated(request):
        return "", 403
    allowed = {"go", "stop", "go/9950011", "go/9950012", "stop/9950011", "stop/9950012", "ce", "estop"}
    if subcmd in allowed:
        pub_dispatcher(subcmd)
    return "", 204

@app.route("/dispatcher/endcto", methods=["POST"])
def dispatcher_endcto():
    if not is_authenticated(request):
        return "", 403
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
    if subtopic in {"throttle", "brake", "direction", "estop"}:
        pub_loco("9950011", subtopic, value)
    return "", 204

@app.route("/otto/dir/<int:d>")
def otto_dir(d):
    with mqtt_lock:
        if loco_state["9950011"]["auto"] == "1":
            return redirect("/otto")
    if d in (0, 1, 2):
        pub_loco("9950011", "direction", d)
    return redirect("/otto")

@app.route("/otto/mode/<int:m>")
def otto_mode(m):
    if m == 1:
        pub_loco("9950011", "auto", "1")
    # mode/0 (MAN) blocked from loco page — only via END CTO
    return redirect("/otto")

@app.route("/otto/estop")
def otto_estop():
    with mqtt_lock:
        cur = loco_state["9950011"]["estop"]
    pub_loco("9950011", "estop", "0" if cur == "1" else "1")
    time.sleep(0.15)
    return redirect("/otto")


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
    if subtopic in {"throttle", "brake", "direction", "estop"}:
        pub_loco("9950012", subtopic, value)
    return "", 204

@app.route("/toby/dir/<int:d>")
def toby_dir(d):
    with mqtt_lock:
        if loco_state["9950012"]["auto"] == "1":
            return redirect("/toby")
    if d in (0, 1, 2):
        pub_loco("9950012", "direction", d)
    return redirect("/toby")

@app.route("/toby/mode/<int:m>")
def toby_mode(m):
    if m == 1:
        pub_loco("9950012", "auto", "1")
    return redirect("/toby")

@app.route("/toby/estop")
def toby_estop():
    with mqtt_lock:
        cur = loco_state["9950012"]["estop"]
    pub_loco("9950012", "estop", "0" if cur == "1" else "1")
    time.sleep(0.15)
    return redirect("/toby")


# ============================================================================
# Log feed API
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
# Log clear (display only — does not clear the deque buffer)
# ============================================================================
# We use a per-session display offset rather than clearing the buffer,
# so reloading the page restores history. We track a "cleared at" index
# per page session via a simple server-side counter that the JS compares.
# Simpler approach: just return empty list when clear flag is set client-side.
# The JS handles this entirely — no server state needed.

# ============================================================================
# Placeholders
# ============================================================================
@app.route("/oscar")
def oscar():
    return "<h2 style='font-family:Arial;padding:2rem;color:#ccc;background:#222;min-height:100vh;margin:0;'>Oscar — coming soon</h2>"

@app.route("/hans")
def hans():
    return "<h2 style='font-family:Arial;padding:2rem;color:#ccc;background:#222;min-height:100vh;margin:0;'>Hans — coming soon</h2>"


# ============================================================================
if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8080, threaded=True)

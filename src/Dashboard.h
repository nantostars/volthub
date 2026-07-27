#pragma once

// volt·hub web dashboard — mirrors the CYD display design.
// Stored in flash (PROGMEM). Served once; JS polls /api/data every 2 s.
// Endpoints used: GET /api/data, GET/POST /api/settings, /update (OTA).
static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>volt·hub</title>
<style>
@import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Sans:wght@400;500;600;700&family=IBM+Plex+Mono:wght@400;600&display=swap');
:root{
  --bg:#0e1826; --body:#05080d; --card:#17233a; --inset:#0f1a2b;
  --border:rgba(180,210,240,.10); --text:#eaf0f8; --muted:rgba(234,240,248,.55);
  --green:#46cf82; --amber:#ffa24d; --red:#ff5a4d; --blue:#5aa5f5;
  --pale:#b4d2f0; --orange:#ff6900;
  --sans:'IBM Plex Sans',system-ui,-apple-system,Segoe UI,sans-serif;
  --mono:'IBM Plex Mono','SF Mono',Menlo,Consolas,monospace;
}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;padding:0}
body{background:var(--body);color:var(--text);font-family:var(--sans);
  font-size:15px;line-height:1.4;min-height:100vh;display:flex;justify-content:center}
.app{width:100%;max-width:760px;background:var(--bg);min-height:100vh;
  display:flex;flex-direction:column;position:relative}
.num{font-family:var(--mono);font-variant-numeric:tabular-nums}
@keyframes flowDash{to{stroke-dashoffset:-120}}

/* ── top status bar ── */
.topbar{position:sticky;top:0;z-index:20;height:44px;flex:none;display:flex;
  align-items:center;justify-content:space-between;padding:0 14px;background:var(--bg);
  border-bottom:1px solid var(--border)}
.logo{font-weight:700;font-size:18px;letter-spacing:-.02em}
.logo b{color:var(--orange);font-weight:700}
.tb-right{display:flex;align-items:center;gap:12px;font-size:13px}
.chip{display:flex;align-items:center;gap:6px;font-weight:500}
.chip .d{width:8px;height:8px;border-radius:50%;background:var(--muted)}
.tb-ble{color:var(--blue);font-weight:500}
.tb-clock{font-family:var(--mono);font-weight:500}

/* ── content ── */
.content{flex:1;display:flex;flex-direction:column;padding:12px 12px 76px;overflow-y:auto}
.view{display:none}
.view.active{display:flex;flex-direction:column;flex:1;gap:12px}

/* ── cards ── */
/* flex:1 0 auto → cards grow to share vertical space, never shrink below content */
.card{background:var(--card);border:1px solid var(--border);border-radius:14px;
  padding:16px;flex:1 0 auto;display:flex;flex-direction:column;justify-content:center}
.card-h{display:flex;justify-content:space-between;align-items:center;margin-bottom:12px}
.card-t{font-size:15px;font-weight:600}
.card-s{font-size:12px;color:var(--muted)}
.pill{font-size:12px;font-weight:600;padding:5px 12px;border-radius:40px}

/* ── stat grids ── */
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:14px}
.grid3{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}
.grid4{display:grid;grid-template-columns:repeat(4,1fr);gap:10px}
.stat .l{font-size:12px;color:var(--muted)}
.stat .v{font-family:var(--mono);font-size:24px;font-weight:600;margin-top:3px}
.inset{background:var(--inset);border-radius:10px;padding:9px 11px}

/* ── ring ── */
.ringwrap{position:relative;flex:none}
.ringwrap svg{display:block}
.ringwrap .rc{transition:stroke-dashoffset 1s ease,stroke .5s ease}
.ringlabel{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);
  text-align:center}
.ringlabel .big{font-family:var(--mono);font-weight:600;line-height:1}
.ringlabel .sub{font-family:var(--mono);font-size:15px;color:var(--muted);margin-top:4px}

/* ── overview: sources top · battery centre · loads bottom ── */
.ov-top{display:grid;grid-template-columns:1fr 1fr;gap:12px;flex:none}
.ov-mid{flex:1;display:flex;align-items:center;justify-content:center;gap:28px}
.ov-bmeta{text-align:left}
.ov-bot{flex:none}
.node{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:14px;text-align:center}
.node .nh{display:flex;align-items:center;justify-content:center;gap:6px;font-size:13px;color:var(--muted)}
.node .nv{font-family:var(--mono);font-size:42px;font-weight:600;margin-top:6px;line-height:1}
.node .nv small{font-size:20px;color:var(--muted)}
.node .na{font-family:var(--mono);font-size:24px;font-weight:600;margin-top:4px;line-height:1}
.node .na small{font-size:14px;color:var(--muted)}
.dot{width:9px;height:9px;border-radius:50%;background:var(--muted);flex:none}

/* ── big value ── */
.bigw{display:flex;align-items:baseline;gap:8px}
.bigw .n{font-family:var(--mono);font-size:46px;font-weight:600;line-height:1}
.bigw .u{font-size:18px;color:var(--muted)}

/* ── cell bars ── */
.cell{display:flex;align-items:center;gap:12px;margin-top:9px}
.cell .cl{width:34px;font-size:12px;color:var(--muted)}
.cell .track{flex:1;height:11px;background:var(--inset);border-radius:6px;overflow:hidden}
.cell .fill{height:100%;border-radius:6px;background:var(--blue);transition:width .8s ease}
.cell .cv{width:56px;text-align:right;font-family:var(--mono);font-size:14px}

/* ── level bubble ── */
.lv-wrap{display:flex;gap:14px;flex-wrap:wrap;flex:1}
.lv-left{flex:1;min-width:180px;display:flex;flex-direction:column;align-items:center;
  background:var(--card);border:1px solid var(--border);border-radius:14px;padding:16px}
.lv-right{flex:1;min-width:220px;display:flex;flex-direction:column;gap:14px}
.bubble{position:relative;width:170px;height:170px;border-radius:50%;
  background:var(--inset);border:1px solid var(--border)}
.bubble .ring2{position:absolute;inset:34px;border-radius:50%;border:1px dashed var(--border)}
.bubble .hx{position:absolute;top:50%;left:10px;right:10px;height:1px;background:var(--border)}
.bubble .vx{position:absolute;left:50%;top:10px;bottom:10px;width:1px;background:var(--border)}
.bubble .dot2{position:absolute;top:50%;left:50%;width:36px;height:36px;border-radius:50%;
  background:var(--amber);transform:translate(-50%,-50%);transition:transform .4s ease,background .3s ease}
.axrow{display:flex;align-items:center;gap:10px;margin-top:10px}
.axrow .s{width:9px;height:9px;border-radius:50%;flex:none}

/* ── colors ── */
.c-green{color:var(--green)} .c-amber{color:var(--amber)} .c-red{color:var(--red)}
.c-blue{color:var(--blue)} .c-orange{color:var(--orange)} .c-pale{color:var(--pale)}
.c-text{color:var(--text)} .c-muted{color:var(--muted)}

/* ── device rows ── */
.devrow{display:flex;align-items:center;gap:12px;padding:11px 0;border-top:1px solid var(--border)}
.devrow:first-child{border-top:none}
.devrow .dn{flex:1;font-size:14px}
.devrow .ds{font-size:12px}

/* ── bottom tab bar ── */
.tabbar{position:fixed;bottom:0;left:50%;transform:translateX(-50%);width:100%;max-width:760px;
  height:64px;display:flex;background:#0b131f;border-top:1px solid var(--border);z-index:20}
.tab{flex:1;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:3px;
  background:none;border:none;color:var(--muted);font-family:inherit;font-size:10.5px;font-weight:500;
  cursor:pointer;border-top:2px solid transparent;padding-top:2px}
.tab svg{width:21px;height:21px;stroke:currentColor;fill:none;stroke-width:1.9;
  stroke-linecap:round;stroke-linejoin:round}
.tab.active{color:var(--orange);border-top-color:var(--orange);background:rgba(255,105,0,.08);font-weight:600}

/* ── settings form ── */
.st-group{margin-bottom:14px}
.st-gt{font-size:13px;font-weight:600;color:var(--muted);margin-bottom:8px}
.st-field{margin-bottom:10px}
.st-label{display:block;font-size:12px;color:var(--muted);margin-bottom:4px}
.st-input-wrap{position:relative;display:flex}
.st-input{flex:1;background:var(--inset);border:1px solid var(--border);border-radius:9px;
  color:var(--text);font-family:var(--mono);font-size:14px;padding:10px 12px;width:100%}
.st-input:focus{outline:none;border-color:var(--blue)}
.st-input.valid{border-color:var(--green)} .st-input.invalid{border-color:var(--red)}
.st-eye{position:absolute;right:6px;top:50%;transform:translateY(-50%);background:none;border:none;
  color:var(--muted);cursor:pointer;font-size:15px;opacity:.5}
.st-hint{font-size:11px;color:var(--muted);margin-top:4px}
.st-hint.ok{color:var(--green)} .st-hint.err{color:var(--red)}
.st-save-btn{width:100%;background:var(--orange);color:#111;border:none;border-radius:10px;
  font-family:inherit;font-size:15px;font-weight:600;padding:13px;cursor:pointer;margin-top:4px}
.st-save-btn:disabled{opacity:.5}
.st-msg{text-align:center;font-size:13px;margin-top:10px;min-height:18px}
.st-msg.ok{color:var(--green)} .st-msg.err{color:var(--red)} .st-msg.info{color:var(--blue)}
.st-note{font-size:11px;color:var(--muted);line-height:1.6;margin-top:12px;
  background:var(--inset);border-radius:10px;padding:12px}
.st-status-row{display:flex;justify-content:space-between;padding:6px 0;font-size:13px}
.st-status-lbl{color:var(--muted)} .st-status-val{font-family:var(--mono)}
.st-status-val.ok{color:var(--green)} .st-status-val.dim{color:var(--muted)}
.ota-link{display:block;text-align:center;color:var(--blue);font-size:13px;margin-top:12px;text-decoration:none}
</style>
</head>
<body>
<div class="app">

  <!-- ── top status bar ── -->
  <div class="topbar">
    <div class="logo">volt<b>·</b>hub</div>
    <div class="tb-right">
      <span class="chip" id="tb-charge"><span class="d"></span><span>--</span></span>
      <span class="tb-ble" id="tb-ble">BLE 0</span>
      <span class="tb-clock num" id="tb-clock">--:--</span>
    </div>
  </div>

  <!-- ── content ── -->
  <div class="content">

    <!-- ═════ OVERVIEW ═════ -->
    <div class="view active" id="v-overview">
      <!-- sources -->
      <div class="ov-top">
        <div class="node">
          <div class="nh"><span class="dot" id="ov-d-sol"></span>Solar</div>
          <div class="nv c-orange"><span id="ov-solw">--</span><small> W</small></div>
          <div class="na"><span id="ov-sola">--</span><small> A</small></div>
        </div>
        <div class="node">
          <div class="nh"><span class="dot" id="ov-d-dc"></span>DC-DC</div>
          <div class="nv c-blue"><span id="ov-dcw">--</span><small> W</small></div>
          <div class="na"><span id="ov-dca">--</span><small> A</small></div>
        </div>
      </div>
      <!-- battery -->
      <div class="ov-mid">
        <div class="ringwrap">
          <svg width="150" height="150" viewBox="0 0 150 150">
            <circle cx="75" cy="75" r="64" fill="none" stroke="var(--inset)" stroke-width="13"/>
            <circle id="ov-ring" class="rc" cx="75" cy="75" r="64" fill="none" stroke="var(--green)"
              stroke-width="13" stroke-linecap="round" transform="rotate(-90 75 75)"/>
          </svg>
          <div class="ringlabel">
            <div class="big" id="ov-soc" style="font-size:52px">--<span style="font-size:22px;color:var(--muted)">%</span></div>
          </div>
        </div>
        <div class="ov-bmeta">
          <div class="card-s">Battery</div>
          <div id="ov-bstate" style="font-size:20px;font-weight:600;margin:3px 0">--</div>
          <div class="num c-muted" id="ov-bvi" style="font-size:15px">--</div>
        </div>
      </div>
      <!-- loads -->
      <div class="ov-bot">
        <div class="node">
          <div class="nh"><span class="dot" id="ov-d-ld"></span>Loads</div>
          <div class="nv c-pale"><span id="ov-ldw">--</span><small> W</small></div>
          <div class="na"><span id="ov-lda">--</span><small> A</small></div>
        </div>
      </div>
    </div>

    <!-- ═════ BATTERY ═════ -->
    <div class="view" id="v-battery">
      <div class="card">
        <div class="card-h">
          <div><div class="card-t" id="bt-title">Battery</div><div class="card-s">LiFePO4 · BMS · Bluetooth</div></div>
          <span class="pill" id="bt-bms">--</span>
        </div>
        <div style="display:flex;gap:16px;align-items:center">
          <div class="ringwrap">
            <svg width="104" height="104" viewBox="0 0 104 104">
              <circle cx="52" cy="52" r="44" fill="none" stroke="var(--inset)" stroke-width="10"/>
              <circle id="bt-ring" class="rc" cx="52" cy="52" r="44" fill="none" stroke="var(--green)"
                stroke-width="10" stroke-linecap="round" transform="rotate(-90 52 52)"/>
            </svg>
            <div class="ringlabel"><div class="big" id="bt-soc" style="font-size:40px">--<span style="font-size:18px;color:var(--muted)">%</span></div></div>
          </div>
          <div class="grid2" style="flex:1">
            <div class="stat"><div class="l">Voltage</div><div class="v" id="bt-v">--</div></div>
            <div class="stat"><div class="l">Current</div><div class="v" id="bt-a">--</div></div>
            <div class="stat"><div class="l">Charge</div><div class="v" id="bt-ah">--</div></div>
            <div class="stat"><div class="l">Capacity</div><div class="v c-muted" id="bt-cap">--</div></div>
          </div>
        </div>
      </div>
      <div class="card">
        <div class="grid4" style="margin-bottom:14px">
          <div class="stat"><div class="l">SOH</div><div class="v" id="bt-soh">--</div></div>
          <div class="stat"><div class="l">Cycles</div><div class="v" id="bt-cyc">--</div></div>
          <div class="stat"><div class="l">Temp</div><div class="v" id="bt-temp">--</div></div>
          <div class="stat"><div class="l">Delta</div><div class="v" id="bt-delta">--</div></div>
        </div>
        <div class="card-s" style="margin-bottom:9px">Cell voltages <span style="opacity:.7">· recommended 3.00–3.55 V</span></div>
        <div id="bt-cells" style="display:grid;grid-template-columns:repeat(4,1fr);gap:10px"></div>
      </div>
    </div>

    <!-- ═════ SOLAR ═════ -->
    <div class="view" id="v-solar">
      <div class="card">
        <div class="card-h">
          <div><div class="card-t" id="so-title">Solar</div><div class="card-s">Bluetooth</div></div>
          <span class="pill" id="so-state">--</span>
        </div>
        <div class="bigw"><div class="n c-orange" id="so-w">--</div><div class="u">W</div></div>
        <div class="grid3" style="margin-top:14px">
          <div class="inset"><div class="l c-muted" style="font-size:11px">Battery</div><div class="v num" id="so-bv" style="font-size:23px;font-weight:600">--</div></div>
          <div class="inset"><div class="l c-muted" style="font-size:11px">To battery</div><div class="v num" id="so-a" style="font-size:23px;font-weight:600">--</div></div>
          <div class="inset"><div class="l c-muted" style="font-size:11px">Yield today</div><div class="v num c-orange" id="so-y" style="font-size:23px;font-weight:600">--</div></div>
        </div>
      </div>
      <div class="card">
        <div class="card-s" style="margin-bottom:6px">Produzione oggi</div>
        <div class="bigw"><div class="n" id="so-y2" style="font-size:40px">--</div><div class="u">Wh</div></div>
        <div class="card-s" style="margin-top:6px">Nessuno storico orario disponibile sul firmware</div>
      </div>
    </div>

    <!-- ═════ DC-DC ═════ -->
    <div class="view" id="v-dcdc">
      <div class="card">
        <div class="card-h">
          <div><div class="card-t" id="dc-title">DC-DC</div><div class="card-s">Alternator · Bluetooth</div></div>
          <span class="pill" id="dc-state">--</span>
        </div>
        <div class="bigw"><div class="n c-blue" id="dc-w">--</div><div class="u">W</div></div>
        <div class="grid3" style="margin-top:14px">
          <div class="inset"><div class="l c-muted" style="font-size:11px">Alternator in</div><div class="v num" id="dc-iv" style="font-size:23px;font-weight:600">--</div></div>
          <div class="inset"><div class="l c-muted" style="font-size:11px">To battery</div><div class="v num" id="dc-a" style="font-size:23px;font-weight:600">--</div></div>
          <div class="inset"><div class="l c-muted" style="font-size:11px">Output</div><div class="v num" id="dc-ov" style="font-size:23px;font-weight:600">--</div></div>
        </div>
      </div>
      <div class="card">
        <div class="card-s" style="margin-bottom:10px">Charge profile</div>
        <div class="grid2">
          <div class="st-status-row"><span class="st-status-lbl">Current limit</span><span class="st-status-val">50 A</span></div>
          <div class="st-status-row"><span class="st-status-lbl">Input range</span><span class="st-status-val">9-17 V</span></div>
          <div class="st-status-row"><span class="st-status-lbl">Mode</span><span class="st-status-val">Adaptive</span></div>
          <div class="st-status-row"><span class="st-status-lbl">Engine detect</span><span class="st-status-val">Auto</span></div>
        </div>
      </div>
    </div>

    <!-- ═════ LEVEL ═════ -->
    <div class="view" id="v-level">
      <div class="lv-wrap">
        <div class="lv-left">
          <div class="card-s" style="align-self:flex-start;margin-bottom:10px">Bubble level</div>
          <div class="bubble">
            <div class="ring2"></div><div class="hx"></div><div class="vx"></div>
            <div class="dot2" id="lv-dot"></div>
          </div>
          <div style="margin-top:12px"><span class="pill" id="lv-status">--</span></div>
        </div>
        <div class="lv-right">
          <div class="grid2">
            <div class="card" style="margin:0"><div class="l c-muted" style="font-size:11px;text-transform:uppercase">Pitch · F-R</div><div class="num" id="lv-pitch" style="font-size:30px;font-weight:600">--°</div><div class="card-s" id="lv-pitch-h">--</div></div>
            <div class="card" style="margin:0"><div class="l c-muted" style="font-size:11px;text-transform:uppercase">Roll · L-R</div><div class="num" id="lv-roll" style="font-size:30px;font-weight:600">--°</div><div class="card-s" id="lv-roll-h">--</div></div>
          </div>
          <div class="card" style="margin:0">
            <div class="card-s" style="margin-bottom:6px">Ramp / chock guidance</div>
            <div class="axrow"><span class="s" id="lv-rc"></span><div style="flex:1" id="lv-rw">--</div><div class="num" id="lv-ra">--</div></div>
            <div class="axrow"><span class="s" id="lv-pc"></span><div style="flex:1" id="lv-pw">--</div><div class="num" id="lv-pa">--</div></div>
          </div>
        </div>
      </div>
    </div>

    <!-- ═════ SYSTEM ═════ -->
    <div class="view" id="v-system">
      <div class="card">
        <div class="card-s" style="margin-bottom:6px">Connected devices</div>
        <div id="sy-devs"></div>
      </div>
      <div class="card">
        <div class="card-s" style="margin-bottom:6px">Network</div>
        <div id="st-status"></div>
        <a class="ota-link" href="/update">Firmware update (OTA) →</a>
      </div>

      <!-- settings form -->
      <div class="card">
        <div class="card-s" style="margin-bottom:10px">Configurazione</div>

        <div class="st-group"><div class="st-gt">WiFi AP</div>
          <div class="st-field"><label class="st-label" for="st-wifi-ssid">SSID</label>
            <div class="st-input-wrap"><input id="st-wifi-ssid" class="st-input" type="text" placeholder="CamperEnergy" maxlength="32" autocomplete="off"></div></div>
          <div class="st-field"><label class="st-label" for="st-wifi-pass">Password</label>
            <div class="st-input-wrap"><input id="st-wifi-pass" class="st-input" type="password" placeholder="min 8 caratteri" maxlength="63" oninput="validateWifiPass(this)" autocomplete="off"><button class="st-eye" onclick="toggleEye('st-wifi-pass',this)">&#128065;</button></div>
            <div id="st-wifi-pass-hint" class="st-hint">Minimo 8 caratteri (WPA2)</div></div>
        </div>

        <div class="st-group"><div class="st-gt">WiFi Client (opzionale)</div>
          <div class="st-field"><label class="st-label" for="st-sta-ssid">SSID rete esistente</label>
            <div class="st-input-wrap"><input id="st-sta-ssid" class="st-input" type="text" placeholder="vuoto = solo AP" maxlength="32" autocomplete="off"></div></div>
          <div class="st-field"><label class="st-label" for="st-sta-pass">Password</label>
            <div class="st-input-wrap"><input id="st-sta-pass" class="st-input" type="password" placeholder="password rete" maxlength="63" autocomplete="off"><button class="st-eye" onclick="toggleEye('st-sta-pass',this)">&#128065;</button></div></div>
        </div>

        <div class="st-group"><div class="st-gt">LiTime BMS</div>
          <div class="st-field"><label class="st-label" for="st-bms-mac">MAC Address</label>
            <div class="st-input-wrap"><input id="st-bms-mac" class="st-input" type="text" placeholder="AA:BB:CC:DD:EE:FF" maxlength="17" oninput="validateMac(this,'st-bms-mac-hint')" autocomplete="off"></div>
            <div id="st-bms-mac-hint" class="st-hint">Formato: AA:BB:CC:DD:EE:FF</div></div>
        </div>

        <div class="st-group"><div class="st-gt">Victron MPPT</div>
          <div class="st-field"><label class="st-label" for="st-solar-key">Advertising Key</label>
            <div class="st-input-wrap"><input id="st-solar-key" class="st-input" type="password" placeholder="32 caratteri hex" maxlength="32" oninput="validateKey(this,'st-solar-key-hint')" autocomplete="off"><button class="st-eye" onclick="toggleEye('st-solar-key',this)">&#128065;</button></div>
            <div id="st-solar-key-hint" class="st-hint">VictronConnect &#8594; Product info &#8594; Advertising key</div></div>
        </div>

        <div class="st-group"><div class="st-gt">Victron DC-DC</div>
          <div class="st-field"><label class="st-label" for="st-orion-key">Advertising Key</label>
            <div class="st-input-wrap"><input id="st-orion-key" class="st-input" type="password" placeholder="32 caratteri hex" maxlength="32" oninput="validateKey(this,'st-orion-key-hint')" autocomplete="off"><button class="st-eye" onclick="toggleEye('st-orion-key',this)">&#128065;</button></div>
            <div id="st-orion-key-hint" class="st-hint">VictronConnect &#8594; Product info &#8594; Advertising key</div></div>
        </div>

        <div class="st-group"><div class="st-gt">NTP / Orario</div>
          <div class="st-field"><label class="st-label" for="st-ntp-srv">Server NTP</label>
            <div class="st-input-wrap"><input id="st-ntp-srv" class="st-input" type="text" placeholder="pool.ntp.org" maxlength="64" autocomplete="off"></div></div>
          <div class="st-field"><label class="st-label" for="st-ntp-tz">Timezone (POSIX)</label>
            <div class="st-input-wrap"><input id="st-ntp-tz" class="st-input" type="text" placeholder="CET-1CEST,M3.5.0,M10.5.0/3" maxlength="64" autocomplete="off"></div>
            <div class="st-hint">Italia: CET-1CEST,M3.5.0,M10.5.0/3</div></div>
        </div>

        <div class="st-group"><div class="st-gt">Witmotion IMU</div>
          <div class="st-field"><label class="st-label" for="st-imu-mac">MAC Address</label>
            <div class="st-input-wrap"><input id="st-imu-mac" class="st-input" type="text" placeholder="A4:C1:38:XX:XX:XX" maxlength="17" oninput="validateMac(this,'st-imu-mac-hint')" autocomplete="off"></div>
            <div id="st-imu-mac-hint" class="st-hint">Formato: AA:BB:CC:DD:EE:FF</div></div>
        </div>

        <button class="st-save-btn" id="st-save-btn" onclick="saveSettings()">Salva e riavvia</button>
        <div class="st-msg" id="st-msg"></div>
      </div>
    </div>

  </div><!-- /content -->

  <!-- ── bottom tab bar ── -->
  <div class="tabbar">
    <button class="tab active" id="t-overview" onclick="setView('overview')">
      <svg viewBox="0 0 24 24"><rect x="3" y="3" width="7" height="7" rx="1"/><rect x="14" y="3" width="7" height="7" rx="1"/><rect x="14" y="14" width="7" height="7" rx="1"/><rect x="3" y="14" width="7" height="7" rx="1"/></svg><span>Overview</span></button>
    <button class="tab" id="t-battery" onclick="setView('battery')">
      <svg viewBox="0 0 24 24"><rect x="2" y="7" width="16" height="10" rx="2"/><line x1="22" y1="11" x2="22" y2="13"/><line x1="6" y1="10" x2="6" y2="14"/><line x1="10" y1="10" x2="10" y2="14"/></svg><span>Battery</span></button>
    <button class="tab" id="t-solar" onclick="setView('solar')">
      <svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/></svg><span>Solar</span></button>
    <button class="tab" id="t-dcdc" onclick="setView('dcdc')">
      <svg viewBox="0 0 24 24"><path d="M8 3 4 7l4 4M4 7h16M16 21l4-4-4-4M20 17H4"/></svg><span>DC-DC</span></button>
    <button class="tab" id="t-level" onclick="setView('level')">
      <svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><circle cx="12" cy="12" r="2.5"/><path d="M3 12h3M18 12h3M12 3v3M12 18v3"/></svg><span>Level</span></button>
    <button class="tab" id="t-system" onclick="setView('system')">
      <svg viewBox="0 0 24 24"><rect x="4" y="4" width="16" height="16" rx="2"/><rect x="9" y="9" width="6" height="6"/><path d="M9 2v2M15 2v2M9 20v2M15 20v2M2 9h2M2 15h2M20 9h2M20 15h2"/></svg><span>System</span></button>
  </div>

</div><!-- /app -->

<script>
var $=function(id){return document.getElementById(id)};
var lastData=null, curView='overview';

function fmt(v,d){ if(v===undefined||v===null||isNaN(v))return '--'; return Number(v).toFixed(d); }
function socColor(s){ return s>45?'var(--green)':s>18?'var(--amber)':'var(--red)'; }
function signColor(v){ return v>0.05?'var(--green)':v<-0.05?'var(--amber)':'var(--text)'; }

var VIEWS=['overview','battery','solar','dcdc','level','system'];
function setView(v){
  curView=v;
  VIEWS.forEach(function(x){
    $('v-'+x).classList.toggle('active', x===v);
    $('t-'+x).classList.toggle('active', x===v);
  });
  if(v==='system') loadSettings();
  if(lastData) applyData(lastData);
}

function setRing(el, pct, color){
  var r=el.r.baseVal.value, c=2*Math.PI*r;
  if(isNaN(pct)) pct=0;
  el.style.strokeDasharray=c;
  el.style.strokeDashoffset=c*(1-Math.max(0,Math.min(1,pct)));
  el.style.stroke=color;
}
function pill(el,txt,fg,bg){ el.textContent=txt; el.style.color=fg; el.style.background=bg; }

// ── status bar ──
function updateTop(d){
  var b=d.battery||{};
  var p=(b.online&&b.power!==undefined)?b.power:0;
  var st=p>8?1:(p<-8?-1:0);
  var lbl=st===1?'Charging':st===-1?'Discharging':'Idle';
  var col=st===1?'var(--green)':st===-1?'var(--amber)':'var(--muted)';
  var chip=$('tb-charge'); chip.querySelector('.d').style.background=col;
  chip.querySelector('span:last-child').textContent=lbl;
  chip.querySelector('span:last-child').style.color=col;
  var n=0; ['battery','solar','orion','imu'].forEach(function(k){ if(d[k]&&d[k].online)n++; });
  $('tb-ble').textContent='BLE '+n;
  if(d.sys&&d.sys.time) $('tb-clock').textContent=d.sys.time.substring(0,5);
}

// ── overview ──
function updateOverview(d){
  var b=d.battery||{}, on=b.online;
  var soc=on?b.soc:NaN, col=on?socColor(soc):'var(--muted)';
  setRing($('ov-ring'), on?soc/100:1, col);   // offline: full grey ring (visible footprint)
  $('ov-soc').innerHTML= on ? (Math.round(soc)+'<span style="font-size:20px;color:var(--muted)">%</span>') : '--';
  $('ov-soc').style.color=col;
  $('ov-bvi').textContent=on?(fmt(b.voltage,1)+'V · '+fmt(b.current,1)+'A'):'';
  var p=on?b.power:0, st=p>8?'Charging':p<-8?'Discharging':'Idle';
  $('ov-bstate').textContent=on?st:'offline';
  $('ov-bstate').style.color=on?(p>8?'var(--green)':p<-8?'var(--amber)':'var(--muted)'):'var(--muted)';

  var s=d.solar||{}, o=d.orion||{};
  var solW=(s.online&&s.solarPower!==undefined)?s.solarPower:NaN;
  var dcW=(o.online&&o.outVoltage!==undefined&&o.outCurrent!==undefined)?o.outVoltage*o.outCurrent:NaN;
  var solA=(s.online&&s.chargeCurrent!==undefined)?s.chargeCurrent:NaN;
  var dcA=(o.online&&o.outCurrent!==undefined)?o.outCurrent:NaN;
  $('ov-solw').textContent=fmt(solW,0);
  $('ov-dcw').textContent=fmt(dcW,0);
  $('ov-sola').textContent=fmt(solA,1);
  $('ov-dca').textContent=fmt(dcA,1);
  $('ov-solw').style.color=(solW>0.5)?'var(--green)':'var(--muted)';   // green when producing
  $('ov-dcw').style.color=(dcW>0.5)?'var(--green)':'var(--muted)';
  $('ov-d-sol').style.background=(solW>0.5)?'var(--orange)':'var(--muted)';
  $('ov-d-dc').style.background=(dcW>0.5)?'var(--blue)':'var(--muted)';
  var ldW=NaN, bp=on?(b.power||0):0;
  if(on){ ldW=Math.max(0,(isNaN(solW)?0:solW)+(isNaN(dcW)?0:dcW)-bp); }
  var batV=(on&&b.voltage)?b.voltage:12.8;
  var ldA=isNaN(ldW)?NaN:ldW/batV;
  $('ov-ldw').textContent=fmt(ldW,0);
  $('ov-lda').textContent=fmt(ldA,1);
  // amber when battery is discharging (loads drawn from the battery)
  $('ov-ldw').style.color=(bp<-2)?'var(--amber)':((ldW>0.5)?'var(--pale)':'var(--muted)');
  $('ov-d-ld').style.background=(ldW>0.5)?'var(--pale)':'var(--muted)';
}

// ── battery ──
function updateBattery(d){
  var b=d.battery||{}, on=b.online;
  $('bt-title').textContent=on?(b.model||'Battery'):'Battery';
  var col=on?socColor(b.soc):'var(--muted)';
  setRing($('bt-ring'), on?b.soc/100:1, col);   // offline: full grey ring
  $('bt-soc').innerHTML= on ? (Math.round(b.soc)+'<span style="font-size:18px;color:var(--muted)">%</span>') : '--';
  $('bt-soc').style.color=col;
  $('bt-v').textContent=on?fmt(b.voltage,2)+' V':'--';
  $('bt-a').textContent=on?fmt(b.current,1)+' A':'--';
  $('bt-a').style.color=on?signColor(b.current):'var(--muted)';
  $('bt-ah').textContent=on?fmt(b.remainingAh,0)+' Ah':'--';
  $('bt-cap').textContent=on?fmt(b.fullAh,0)+' Ah':'--';
  $('bt-soh').textContent=on?fmt(b.soh,0)+'%':'--';
  $('bt-cyc').textContent=(on&&b.cycles!==undefined)?b.cycles:'--';
  $('bt-temp').textContent=on?fmt(b.cellTemp,0)+'°C':'--';
  $('bt-temp').style.color=(on&&b.cellTemp>45)?'var(--red)':'var(--text)';
  var cells=(on&&b.cells)?b.cells:[];
  var mn=9,mx=0; cells.forEach(function(v){ if(v<mn)mn=v; if(v>mx)mx=v; });
  var delta=cells.length?Math.round((mx-mn)*1000):0;
  $('bt-delta').textContent=on?delta+' mV':'--';
  // delta alarm: white <=50mV, amber 50-100mV, red >100mV
  $('bt-delta').style.color=!on?'var(--muted)':(delta>100?'var(--red)':delta>50?'var(--amber)':'var(--text)');
  pill($('bt-bms'), on?(delta<=30?'Balanced':'Balancing'):'offline',
       on?(delta<=30?'var(--green)':'var(--amber)'):'var(--muted)',
       on?(delta<=30?'rgba(70,207,130,.15)':'rgba(255,162,77,.16)'):'var(--inset)');
  var html='';
  cells.forEach(function(v,i){
    var bad=(v<3.00||v>3.55);
    var col=bad?'var(--red)':'var(--text)';
    var bg=bad?'rgba(255,90,77,0.12)':'var(--inset)';
    var bd=bad?'rgba(255,90,77,0.5)':'var(--border)';
    html+='<div style="background:'+bg+';border:1px solid '+bd+';border-radius:10px;padding:10px 8px;text-align:center">'
        +'<div style="font-size:11px;color:var(--muted)">Cell '+(i+1)+'</div>'
        +'<div class="num" style="font-size:20px;font-weight:600;margin-top:3px;color:'+col+'">'+v.toFixed(2)+'</div>'
        +'<div style="font-size:10px;color:'+col+'">V</div></div>';
  });
  $('bt-cells').innerHTML=html||'<div class="card-s">Nessun dato cella</div>';
}

// ── solar ──
function updateSolar(d){
  var s=d.solar||{}, on=s.online;
  $('so-title').textContent=on?(s.model||'Solar'):'Solar';
  pill($('so-state'), on?(s.state||'--'):'offline', on?'var(--orange)':'var(--muted)', on?'rgba(255,105,0,.16)':'var(--inset)');
  $('so-w').textContent=on?fmt(s.solarPower,0):'--';
  $('so-bv').textContent=on?fmt(s.battVoltage,2)+' V':'--';
  $('so-a').textContent=on?fmt(s.chargeCurrent,1)+' A':'--';
  $('so-y').textContent=on?fmt(s.yieldToday,0)+' Wh':'--';
  $('so-y2').textContent=on?fmt(s.yieldToday,0):'--';
}

// ── dc-dc ──
function updateDcdc(d){
  var o=d.orion||{}, on=o.online;
  $('dc-title').textContent=on?(o.model||'DC-DC'):'DC-DC';
  var w=(on&&o.outVoltage!==undefined&&o.outCurrent!==undefined)?o.outVoltage*o.outCurrent:NaN;
  var st=!on?'offline':(o.outCurrent>0.1?'Charging':'Standby');
  pill($('dc-state'), st, on?'var(--blue)':'var(--muted)', on?'rgba(90,165,245,.16)':'var(--inset)');
  $('dc-w').textContent=fmt(w,0);
  $('dc-iv').textContent=on?fmt(o.inVoltage,1)+' V':'--';
  $('dc-a').textContent=on?fmt(o.outCurrent,1)+' A':'--';
  $('dc-ov').textContent=on?fmt(o.outVoltage,1)+' V':'--';
}

// ── level ──
function updateLevel(d){
  var im=d.imu||{}, on=im.online;
  var pitch=on?im.pitch:0, roll=on?im.roll:0, tol=0.5;
  var okP=Math.abs(pitch)<=tol, okR=Math.abs(roll)<=tol, level=okP&&okR;
  function axc(v){ return Math.abs(v)<=tol?'var(--green)':Math.abs(v)<=2?'var(--amber)':'var(--red)'; }
  var bx=Math.max(-60,Math.min(60,roll*6)), by=Math.max(-60,Math.min(60,-pitch*6));
  var dot=$('lv-dot');
  dot.style.transform='translate(calc(-50% + '+bx+'px), calc(-50% + '+by+'px))';
  dot.style.background=on?(level?'var(--green)':'var(--amber)'):'var(--muted)';
  pill($('lv-status'), on?(level?'Levelled':'Adjust needed'):'no sensor',
       on?(level?'var(--green)':'var(--amber)'):'var(--muted)',
       on?(level?'rgba(70,207,130,.15)':'rgba(255,162,77,.16)'):'var(--inset)');
  $('lv-pitch').textContent=on?((pitch>0?'+':'')+pitch.toFixed(1)+'°'):'--';
  $('lv-pitch').style.color=on?axc(pitch):'var(--muted)';
  $('lv-roll').textContent=on?((roll>0?'+':'')+roll.toFixed(1)+'°'):'--';
  $('lv-roll').style.color=on?axc(roll):'var(--muted)';
  $('lv-pitch-h').textContent=okP?'level':(pitch<0?'nose down':'nose up');
  $('lv-roll-h').textContent=okR?'level':(roll>0?'right high':'left high');
  var mm=42;
  $('lv-rc').style.background=okR?'var(--green)':'var(--amber)';
  $('lv-rw').textContent=roll>tol?'Left wheels':(roll<-tol?'Right wheels':'Side axle');
  $('lv-ra').textContent=on?(okR?'OK':'raise '+Math.round(Math.abs(roll)*mm)+' mm'):'--';
  $('lv-ra').style.color=okR?'var(--green)':'var(--amber)';
  $('lv-pc').style.background=okP?'var(--green)':'var(--amber)';
  $('lv-pw').textContent=pitch<-tol?'Front wheels':(pitch>tol?'Rear wheels':'Front-rear');
  $('lv-pa').textContent=on?(okP?'OK':'raise '+Math.round(Math.abs(pitch)*mm)+' mm'):'--';
  $('lv-pa').style.color=okP?'var(--green)':'var(--amber)';
}

// ── system ──
function updateSystem(d){
  var devs=[
    {n:'Solar', on:d.solar&&d.solar.online},
    {n:'DC-DC', on:d.orion&&d.orion.online},
    {n:'Battery', on:d.battery&&d.battery.online},
    {n:'Tilt sensor', on:d.imu&&d.imu.online}
  ];
  var html='';
  devs.forEach(function(x){
    html+='<div class="devrow"><span class="dot" style="background:'+(x.on?'var(--green)':'var(--muted)')+'"></span>'
        +'<div class="dn">'+x.n+'</div>'
        +'<div class="ds" style="color:'+(x.on?'var(--green)':'var(--muted)')+'">'+(x.on?'online':'offline')+'</div></div>';
  });
  $('sy-devs').innerHTML=html;
}

function updateSysInfo(sys){
  if(!sys) return;
  var el=$('st-status'); if(!el) return;
  var rows='';
  if(sys.fw) rows+='<div class="st-status-row"><span class="st-status-lbl">Firmware</span><span class="st-status-val">v'+sys.fw+'</span></div>';
  rows+='<div class="st-status-row"><span class="st-status-lbl">AP</span><span class="st-status-val">http://'+(sys.apIp||'192.168.4.1')+'</span></div>';
  if(sys.staIp) rows+='<div class="st-status-row"><span class="st-status-lbl">WiFi Client</span><span class="st-status-val ok">&#10003; '+sys.staIp+'</span></div>';
  if(sys.time&&sys.date) rows+='<div class="st-status-row"><span class="st-status-lbl">Orario NTP</span><span class="st-status-val">'+sys.date+'&nbsp;&nbsp;'+sys.time+'</span></div>';
  else { var m=(sys.staIp&&sys.staIp.length>0)?'in attesa...':'non sincronizzato';
    rows+='<div class="st-status-row"><span class="st-status-lbl">Orario NTP</span><span class="st-status-val dim">'+m+'</span></div>'; }
  el.innerHTML=rows;
}

function applyData(d){
  lastData=d;
  updateTop(d);
  if(curView==='overview') updateOverview(d);
  else if(curView==='battery') updateBattery(d);
  else if(curView==='solar') updateSolar(d);
  else if(curView==='dcdc') updateDcdc(d);
  else if(curView==='level') updateLevel(d);
  else if(curView==='system'){ updateSystem(d); if(d.sys) updateSysInfo(d.sys); }
}

// ── settings (contract preserved) ──
function validateMac(el,hintId){
  var ok=/^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$/.test(el.value);
  el.className='st-input '+(el.value.length===0?'':ok?'valid':'invalid');
  var h=$(hintId); if(h){ h.className='st-hint'+(el.value.length===0?'':ok?' ok':' err');
    h.textContent=el.value.length===0?'Formato: AA:BB:CC:DD:EE:FF':ok?'✓ Formato corretto':'Formato non valido'; }
}
function validateKey(el,hintId){
  var ok=/^[0-9A-Fa-f]{32}$/.test(el.value);
  el.className='st-input '+(el.value.length===0?'':ok?'valid':'invalid');
  var h=$(hintId); if(h){ h.className='st-hint'+(el.value.length===0?'':ok?' ok':' err');
    h.textContent=el.value.length===0?'VictronConnect → Product info → Advertising key':ok?'✓ Chiave valida':'Deve essere 32 caratteri hex'; }
}
function toggleEye(id,btn){ var i=$(id); if(!i)return; if(i.type==='password'){i.type='text';btn.style.opacity='1';}else{i.type='password';btn.style.opacity='.5';} }
function validateWifiPass(el){
  var ok=el.value.length===0||el.value.length>=8;
  el.className='st-input'+(el.value.length===0?'':ok?' valid':' invalid');
  var h=$('st-wifi-pass-hint'); if(h){ h.className='st-hint'+(el.value.length===0?'':ok?' ok':' err');
    h.textContent=el.value.length===0?'Minimo 8 caratteri (WPA2)':ok?'✓ OK':'Minimo 8 caratteri'; }
}
function setMsg(t,c){ var e=$('st-msg'); e.textContent=t; e.className='st-msg'+(c?' '+c:''); }
function loadSettings(){
  fetch('/api/settings').then(function(r){return r.json();}).then(function(d){
    $('st-wifi-ssid').value=d.wifiSsid||''; $('st-wifi-pass').value=d.wifiPass||'';
    $('st-sta-ssid').value=d.staSsid||''; $('st-sta-pass').value=d.staPass||'';
    $('st-bms-mac').value=d.bmsMac||''; $('st-solar-key').value=d.solarKey||'';
    $('st-orion-key').value=d.orionKey||''; $('st-ntp-srv').value=d.ntpServer||'';
    $('st-ntp-tz').value=d.ntpTZ||''; $('st-imu-mac').value=d.imuMac||'';
  }).catch(function(){ setMsg('Impossibile caricare le impostazioni','err'); });
}
function saveSettings(){
  var g=function(id){return $(id).value.trim();};
  var ssid=g('st-wifi-ssid'),pass=g('st-wifi-pass'),staSsid=g('st-sta-ssid'),staPass=g('st-sta-pass'),
      mac=g('st-bms-mac'),solar=g('st-solar-key'),orion=g('st-orion-key'),
      ntpSrv=g('st-ntp-srv'),ntpTZ=g('st-ntp-tz'),imuMac=g('st-imu-mac');
  if(pass&&pass.length<8){ setMsg('Password AP: minimo 8 caratteri','err'); return; }
  if(mac&&!/^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$/.test(mac)){ setMsg('MAC BMS non valido','err'); return; }
  if(solar&&!/^[0-9A-Fa-f]{32}$/.test(solar)){ setMsg('Chiave MPPT non valida','err'); return; }
  if(orion&&!/^[0-9A-Fa-f]{32}$/.test(orion)){ setMsg('Chiave DC-DC non valida','err'); return; }
  if(imuMac&&!/^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$/.test(imuMac)){ setMsg('MAC IMU non valido','err'); return; }
  $('st-save-btn').disabled=true; setMsg('Salvataggio...','info');
  var body=JSON.stringify({wifiSsid:ssid,wifiPass:pass,staSsid:staSsid,staPass:staPass,
    bmsMac:mac,solarKey:solar,orionKey:orion,ntpServer:ntpSrv,ntpTZ:ntpTZ,imuMac:imuMac});
  fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:body})
    .then(function(){ setMsg('Salvato! Riavvio...','ok'); waitReconnect(12); })
    .catch(function(){ setMsg('Riavvio in corso...','ok'); waitReconnect(12); });
}
function waitReconnect(s){ if(s<=0){tryReconnect();return;} setMsg('Riavvio… riconnessione tra '+s+'s','info'); setTimeout(function(){waitReconnect(s-1);},1000); }
function tryReconnect(){ setMsg('Riconnessione…','info'); fetch('/api/data').then(function(){location.reload();}).catch(function(){setTimeout(tryReconnect,2000);}); }

// ── poll ──
function poll(){
  fetch('/api/data').then(function(r){return r.json();}).then(applyData).catch(function(){});
}
poll();
setInterval(poll,2000);
</script>
</body>
</html>
)rawliteral";

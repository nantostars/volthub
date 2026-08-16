#pragma once

// volthub web dashboard — mirrors the CYD display design.
// Stored in flash (PROGMEM). Served once; JS polls /api/data every 2 s.
// Endpoints used: GET /api/data, GET/POST /api/settings, /update (OTA).
static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>volthub</title>
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
.lang-sel{display:inline-flex;gap:4px}
.lang-btn{background:var(--inset);color:var(--muted);border:1px solid var(--border);border-radius:7px;padding:3px 10px;font-size:12px;font-weight:600;cursor:pointer}
.lang-btn.on{background:var(--orange);color:#111;border-color:transparent}
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
          <div class="num c-muted" id="ov-beta" style="font-size:15px">--</div>
        </div>
      </div>
      <!-- loads -->
      <div class="ov-bot">
        <div class="node">
          <div class="nh"><span class="dot" id="ov-d-ld"></span><span data-i18n="Battery discharge">Battery discharge</span></div>
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
            <div class="stat" style="grid-column:1/-1"><div class="l" id="bt-eta-l">Runtime</div><div class="v" id="bt-eta">--</div></div>
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
        <div class="card-s" style="margin-bottom:6px" data-i18n="Production today">Production today</div>
        <div class="bigw"><div class="n" id="so-y2" style="font-size:40px">--</div><div class="u">Wh</div></div>
        <div class="st-status-row" id="so-load-row" style="display:none"><span class="st-status-lbl" data-i18n="Load output">Load output</span><span class="st-status-val" id="so-load">--</span></div>
        <div class="st-status-row" id="so-err-row" style="display:none"><span class="st-status-lbl" data-i18n="Error">Error</span><span class="st-status-val" id="so-err" style="color:var(--red)">--</span></div>
        <div class="card-s" style="margin-top:6px" data-i18n="No hourly history available on this firmware">No hourly history available on this firmware</div>
      </div>
    </div>

    <!-- ═════ DC-DC ═════ -->
    <div class="view" id="v-dcdc">
      <div class="card">
        <div class="card-h">
          <div><div class="card-t" id="dc-title">DC-DC</div><div class="card-s">Alternator · Bluetooth</div></div>
          <span class="pill" id="dc-state">--</span>
        </div>
        <div class="card-s" style="margin-top:12px" data-i18n="Input (alternator)">Input (alternator)</div>
        <div class="grid3" style="margin-top:6px">
          <div class="inset"><div class="l c-muted" style="font-size:11px" data-i18n="Voltage">Voltage</div><div class="v num" id="dc-iv" style="font-size:23px;font-weight:600">--</div></div>
          <div class="inset"><div class="l c-muted" style="font-size:11px" data-i18n="Current">Current</div><div class="v num" id="dc-ia" style="font-size:23px;font-weight:600">--</div></div>
          <div class="inset"><div class="l c-muted" style="font-size:11px" data-i18n="Power">Power</div><div class="v num" id="dc-iw" style="font-size:23px;font-weight:600">--</div></div>
        </div>
        <div class="card-s" style="margin-top:14px" data-i18n="Output (battery)">Output (battery)</div>
        <div class="grid3" style="margin-top:6px">
          <div class="inset"><div class="l c-muted" style="font-size:11px" data-i18n="Voltage">Voltage</div><div class="v num" id="dc-ov" style="font-size:23px;font-weight:600">--</div></div>
          <div class="inset"><div class="l c-muted" style="font-size:11px" data-i18n="Current">Current</div><div class="v num" id="dc-oa" style="font-size:23px;font-weight:600">--</div></div>
          <div class="inset"><div class="l c-muted" style="font-size:11px" data-i18n="Power">Power</div><div class="v num c-blue" id="dc-ow" style="font-size:23px;font-weight:600">--</div></div>
        </div>
      </div>
      <div class="card">
        <div class="card-s" style="margin-bottom:10px" data-i18n="Efficiency &amp; status">Efficiency &amp; status</div>
        <div class="grid2">
          <div class="st-status-row"><span class="st-status-lbl" data-i18n="Efficiency">Efficiency</span><span class="st-status-val" id="dc-eff">--</span></div>
          <div class="st-status-row"><span class="st-status-lbl" data-i18n="Status">Status</span><span class="st-status-val" id="dc-status">--</span></div>
        </div>
      </div>
      <!-- A "Charge profile" card used to sit here with hardcoded values (50 A / 9-17 V /
           Adaptive / Auto) from the design mockup: no id, no JS, never read from the charger.
           Current limits, input lock-out and absorption/float/storage voltages are CONFIGURATION
           parameters; the Victron advertisement carries live telemetry only, so they cannot be
           shown as live data. Removed rather than displayed as if measured. -->
    </div>

    <!-- ═════ LEVEL ═════ -->
    <div class="view" id="v-level">
      <div class="lv-wrap">
        <div class="lv-left">
          <div class="card-s" style="align-self:flex-start;margin-bottom:10px" data-i18n="Bubble level">Bubble level</div>
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
        <div class="card-s" style="margin-bottom:6px" data-i18n="Connected devices">Connected devices</div>
        <div id="sy-devs"></div>
      </div>
      <div class="card">
        <div class="card-s" style="margin-bottom:6px" data-i18n="Network">Network</div>
        <div id="st-status"></div>
        <div class="st-status-row"><span class="st-status-lbl" data-i18n="Language">Language</span>
          <span class="lang-sel"><button class="lang-btn on" id="lang-en" onclick="setLang(0)">EN</button><button class="lang-btn" id="lang-it" onclick="setLang(1)">IT</button></span></div>
        <div class="st-status-row"><span class="st-status-lbl" data-i18n="Keep screen on">Keep screen on</span>
          <span class="lang-sel"><button class="lang-btn" id="wake-btn" onclick="toggleWake()">OFF</button></span></div>
        <a class="ota-link" id="ota-link" href="/update" style="display:none" data-i18n="Firmware update (OTA) →">Firmware update (OTA) →</a>
      </div>


      <!-- settings form -->
      <div class="card">
        <div class="card-s" style="margin-bottom:10px" data-i18n="Configuration">Configuration</div>

        <div class="st-group"><div class="st-gt">WiFi AP</div>
          <div class="st-field"><label class="st-label" for="st-wifi-ssid">SSID</label>
            <div class="st-input-wrap"><input id="st-wifi-ssid" class="st-input" type="text" placeholder="CamperEnergy" maxlength="32" autocomplete="off"></div></div>
          <div class="st-field"><label class="st-label" for="st-wifi-pass">Password</label>
            <div class="st-input-wrap"><input id="st-wifi-pass" class="st-input" type="password" placeholder="min 8 characters" data-i18n-ph="min 8 characters" maxlength="63" oninput="validateWifiPass(this)" autocomplete="off"><button class="st-eye" onclick="toggleEye('st-wifi-pass',this)">&#128065;</button></div>
            <div id="st-wifi-pass-hint" class="st-hint" data-i18n="Minimum 8 characters (WPA2)">Minimum 8 characters (WPA2)</div></div>
        </div>

        <div class="st-group"><div class="st-gt" data-i18n="WiFi Client (optional)">WiFi Client (optional)</div>
          <div class="st-field"><label class="st-label" for="st-sta-ssid" data-i18n="Existing network SSID">Existing network SSID</label>
            <div class="st-input-wrap"><input id="st-sta-ssid" class="st-input" type="text" placeholder="empty = AP only" data-i18n-ph="empty = AP only" maxlength="32" autocomplete="off"></div></div>
          <div class="st-field"><label class="st-label" for="st-sta-pass">Password</label>
            <div class="st-input-wrap"><input id="st-sta-pass" class="st-input" type="password" placeholder="network password" data-i18n-ph="network password" maxlength="63" autocomplete="off"><button class="st-eye" onclick="toggleEye('st-sta-pass',this)">&#128065;</button></div></div>
          <div class="st-field"><label class="st-label" style="display:flex;align-items:center;gap:8px;cursor:pointer"><input id="st-ap-off" type="checkbox"><span data-i18n="Turn the AP off while the client is connected">Turn the AP off while the client is connected</span></label>
            <div class="st-hint" data-i18n="Stops the phone auto-joining the AP. It comes back if the client drops, for 10 min after every boot, and from the AP button on the device screen.">Stops the phone auto-joining the AP. It comes back if the client drops, for 10 min after every boot, and from the AP button on the device screen.</div></div>
        </div>

        <div class="st-group"><div class="st-gt">LiTime BMS</div>
          <div class="st-field"><label class="st-label" for="st-bms-mac">MAC Address</label>
            <div class="st-input-wrap"><input id="st-bms-mac" class="st-input" type="text" placeholder="AA:BB:CC:DD:EE:FF" maxlength="17" oninput="validateMac(this,'st-bms-mac-hint')" autocomplete="off"></div>
            <div id="st-bms-mac-hint" class="st-hint">Formato: AA:BB:CC:DD:EE:FF</div></div>
        </div>

        <div class="st-group"><div class="st-gt">Victron MPPT</div>
          <div class="st-field"><label class="st-label" for="st-solar-key">Advertising Key</label>
            <div class="st-input-wrap"><input id="st-solar-key" class="st-input" type="password" placeholder="32 hex characters" data-i18n-ph="32 hex characters" maxlength="32" oninput="validateKey(this,'st-solar-key-hint')" autocomplete="off"><button class="st-eye" onclick="toggleEye('st-solar-key',this)">&#128065;</button></div>
            <div id="st-solar-key-hint" class="st-hint">VictronConnect &#8594; Product info &#8594; Advertising key</div></div>
        </div>

        <div class="st-group"><div class="st-gt">Victron DC-DC</div>
          <div class="st-field"><label class="st-label" for="st-orion-key">Advertising Key</label>
            <div class="st-input-wrap"><input id="st-orion-key" class="st-input" type="password" placeholder="32 hex characters" data-i18n-ph="32 hex characters" maxlength="32" oninput="validateKey(this,'st-orion-key-hint')" autocomplete="off"><button class="st-eye" onclick="toggleEye('st-orion-key',this)">&#128065;</button></div>
            <div id="st-orion-key-hint" class="st-hint">VictronConnect &#8594; Product info &#8594; Advertising key</div></div>
        </div>

        <div class="st-group"><div class="st-gt" data-i18n="NTP / Time">NTP / Time</div>
          <div class="st-field"><label class="st-label" for="st-ntp-srv" data-i18n="NTP server">NTP server</label>
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

        <div class="st-group"><div class="st-gt" data-i18n="Firmware update (OTA)">Firmware update (OTA)</div>
          <div class="st-field"><label class="st-label" style="display:flex;align-items:center;gap:8px;cursor:pointer"><input id="st-ota-en" type="checkbox"><span data-i18n="Enable OTA">Enable OTA</span></label></div>
          <div class="st-field"><label class="st-label" for="st-ota-user" data-i18n="Username">Username</label>
            <div class="st-input-wrap"><input id="st-ota-user" class="st-input" type="text" maxlength="32" autocomplete="off"></div></div>
          <div class="st-field"><label class="st-label" for="st-ota-pass" data-i18n="Password">Password</label>
            <div class="st-input-wrap"><input id="st-ota-pass" class="st-input" type="password" maxlength="63" autocomplete="off"><button class="st-eye" onclick="toggleEye('st-ota-pass',this)">&#128065;</button></div>
            <div class="st-hint" data-i18n="OTA active only with enable + username + password">OTA active only with enable + username + password</div></div>
        </div>

        <div class="st-group"><div class="st-gt" data-i18n="Data log (CSV)">Data log (CSV)</div>
          <div class="st-status-row"><span class="st-status-lbl" data-i18n="Logging">Logging</span>
            <span class="lang-sel"><button class="lang-btn" id="log-btn" onclick="toggleLog()">OFF</button></span></div>
          <div id="log-status"></div>
          <div id="log-actions" style="display:none;margin:6px 0">
            <button class="lang-btn" onclick="logAction({rescan:true})" data-i18n="Detect card">Detect card</button>
            <button class="lang-btn" id="log-eject" onclick="logEject()" data-i18n="Eject card">Eject card</button>
            <button class="lang-btn" onclick="logPurge()" data-i18n="Delete older than…">Delete older than…</button>
          </div>
          <div id="log-files"></div>
          <div id="log-more" style="display:none;margin-top:4px">
            <button class="lang-btn" onclick="moreLogs()" data-i18n="Show older">Show older</button>
          </div>
          <div class="st-hint" data-i18n="One row a minute on a microSD, one every two on internal flash. Applies immediately, no save needed. Status codes: battery 1 charge / 0 idle / -1 discharge; solar and DC-DC use the VE.Direct codes (3 bulk, 4 absorption, 5 float).">One row a minute on a microSD, one every two on internal flash. Applies immediately, no save needed. Status codes: battery 1 charge / 0 idle / -1 discharge; solar and DC-DC use the VE.Direct codes (3 bulk, 4 absorption, 5 float).</div>
        </div>

        <button class="st-save-btn" id="st-save-btn" onclick="saveSettings()" data-i18n="Save and reboot">Save and reboot</button>
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

<!-- Screen wake-lock (HTTP, so navigator.wakeLock is unavailable): a muted, inline,
     looping 128x128 H.264 clip. Keeping a video "playing" stops the phone from sleeping.
     Modern Chromium (incl. Android WebView / DuckDuckGo) ignores hidden/tiny videos for the
     screen-wake heuristic, so the element must be genuinely visible in the viewport: it is
     stretched full-screen on top but at ~2% opacity and non-interactive, so it satisfies the
     visibility rule while staying imperceptible. src is assigned lazily on first enable. -->
<video id="wake-vid" muted playsinline webkit-playsinline loop preload="none" title="" style="position:fixed;inset:0;width:100vw;height:100vh;object-fit:cover;opacity:0.02;pointer-events:none;z-index:2147483647"></video>

<script>
var $=function(id){return document.getElementById(id)};
var lastData=null, curView='overview', _otaHasPass=false;
// Which secrets the device holds. It never sends them back, so the form shows a
// placeholder and posts a field only when the user types a new value.
var _stored={ota:false,wifi:false,sta:false,solar:false,orion:false};

// ── keep-screen-on ──────────────────────────────────────────────────────────
// The dashboard is served over plain HTTP, so the Screen Wake Lock API is
// unavailable (it needs a secure context). Instead we keep a muted, inline,
// looping tiny H.264 clip "playing": a playing video prevents the phone from
// sleeping, and this works over HTTP on any browser. State is a per-browser
// preference in localStorage (default OFF) — nothing is stored on the device.
var WAKE_MP4='data:video/mp4;base64,AAAAIGZ0eXBpc29tAAACAGlzb21pc28yYXZjMW1wNDEAAAM0bW9vdgAAAGxtdmhkAAAAAAAAAAAAAAAAAAAD6AAAD6AAAQAAAQAAAAAAAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgAAAl50cmFrAAAAXHRraGQAAAADAAAAAAAAAAAAAAABAAAAAAAAD6AAAAAAAAAAAAAAAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAABAAAAAAIAAAACAAAAAAAAkZWR0cwAAABxlbHN0AAAAAAAAAAEAAA+gAAAAAAABAAAAAAHWbWRpYQAAACBtZGhkAAAAAAAAAAAAAAAAAABAAAABAABVxAAAAAAALWhkbHIAAAAAAAAAAHZpZGUAAAAAAAAAAAAAAABWaWRlb0hhbmRsZXIAAAABgW1pbmYAAAAUdm1oZAAAAAEAAAAAAAAAAAAAACRkaW5mAAAAHGRyZWYAAAAAAAAAAQAAAAx1cmwgAAAAAQAAAUFzdGJsAAAAuXN0c2QAAAAAAAAAAQAAAKlhdmMxAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAAAAIAAgABIAAAASAAAAAAAAAABFUxhdmM2Mi4yOC4xMDAgbGlieDI2NAAAAAAAAAAAAAAAGP//AAAAL2F2Y0MBQsAe/+EAF2dCwB7ZAgRsBEAAAAMAQAAAAwCDxYuSAQAFaMuDyyAAAAAQcGFzcAAAAAEAAAABAAAAFGJ0cnQAAAAAAAAFsAAAAAAAAAAYc3R0cwAAAAAAAAABAAAABAAAQAAAAAAUc3RzcwAAAAAAAAABAAAAAQAAABxzdHNjAAAAAAAAAAEAAAABAAAABAAAAAEAAAAkc3RzegAAAAAAAAAAAAAABAAAArcAAAALAAAACwAAAAsAAAAUc3RjbwAAAAAAAAABAAADZAAAAGJ1ZHRhAAAAWm1ldGEAAAAAAAAAIWhkbHIAAAAAAAAAAG1kaXJhcHBsAAAAAAAAAAAAAAAALWlsc3QAAAAlqXRvbwAAAB1kYXRhAAAAAQAAAABMYXZmNjIuMTIuMTAwAAAACGZyZWUAAALgbWRhdAAAAnAGBf//bNxF6b3m2Ui3lizYINkj7u94MjY0IC0gY29yZSAxNjUgcjMyMjIgYjM1NjA1YSAtIEguMjY0L01QRUctNCBBVkMgY29kZWMgLSBDb3B5bGVmdCAyMDAzLTIwMjUgLSBodHRwOi8vd3d3LnZpZGVvbGFuLm9yZy94MjY0Lmh0bWwgLSBvcHRpb25zOiBjYWJhYz0wIHJlZj0zIGRlYmxvY2s9MTowOjAgYW5hbHlzZT0weDE6MHgxMTEgbWU9aGV4IHN1Ym1lPTcgcHN5PTEgcHN5X3JkPTEuMDA6MC4wMCBtaXhlZF9yZWY9MSBtZV9yYW5nZT0xNiBjaHJvbWFfbWU9MSB0cmVsbGlzPTEgOHg4ZGN0PTAgY3FtPTAgZGVhZHpvbmU9MjEsMTEgZmFzdF9wc2tpcD0xIGNocm9tYV9xcF9vZmZzZXQ9LTIgdGhyZWFkcz00IGxvb2thaGVhZF90aHJlYWRzPTEgc2xpY2VkX3RocmVhZHM9MCBucj0wIGRlY2ltYXRlPTEgaW50ZXJsYWNlZD0wIGJsdXJheV9jb21wYXQ9MCBjb25zdHJhaW5lZF9pbnRyYT0wIGJmcmFtZXM9MCB3ZWlnaHRwPTAga2V5aW50PTI1MCBrZXlpbnRfbWluPTEgc2NlbmVjdXQ9NDAgaW50cmFfcmVmcmVzaD0wIHJjX2xvb2thaGVhZD00MCByYz1jcmYgbWJ0cmVlPTEgY3JmPTIzLjAgcWNvbXA9MC42MCBxcG1pbj0wIHFwbWF4PTY5IHFwc3RlcD00IGlwX3JhdGlvPTEuNDAgYXE9MToxLjAwAIAAAAA/ZYiEBf///w9FAAFXnycnJycnJyddddddddddddddddddddddddddddddddddddddddddddddddddddddddeAAAAAB0GaOAv4EGAAAAAHQZpUAt4EGAAAAAdBmmAV8CDA';
var wantWake=false;
function updateWakeBtn(){ var b=$('wake-btn'); if(b){ b.textContent=wantWake?'ON':'OFF'; b.classList.toggle('on',wantWake); } }
function wakePlay(){ var v=$('wake-vid'); if(!v)return; if(!v.src)v.src=WAKE_MP4; var p=v.play(); if(p&&p.catch)p.catch(function(){}); }
function wakeStop(){ var v=$('wake-vid'); if(v)v.pause(); }
function toggleWake(){ wantWake=!wantWake; try{localStorage.setItem('wakeOn',wantWake?'1':'0');}catch(e){} updateWakeBtn(); if(wantWake)wakePlay(); else wakeStop(); }
function initWake(){
  var v=$('wake-vid');
  if(v){
    // Some browsers won't hold the wake on a short looped clip; force a re-seek near the end,
    // and if anything pauses it (backgrounding, buffering), resume while the toggle is ON.
    v.addEventListener('timeupdate',function(){ if(v.currentTime>3){ try{v.currentTime=0;}catch(e){} } });
    v.addEventListener('pause',function(){ if(wantWake){ var q=v.play(); if(q&&q.catch)q.catch(function(){}); } });
  }
  try{wantWake=localStorage.getItem('wakeOn')==='1';}catch(e){wantWake=false;}
  updateWakeBtn(); if(wantWake)wakePlay();
}
// The lock drops when the tab is backgrounded/screen off; re-assert on return.
document.addEventListener('visibilitychange',function(){ if(document.visibilityState==='visible'&&wantWake)wakePlay(); });

// ── i18n (English key → Italian; English fallback) ──
var I18N={ it:{
  "Overview":"Panoramica","Battery":"Batteria","Solar":"Solare","Level":"Livella","System":"Sistema",
  "Battery discharge":"Scarica batteria","offline":"offline","online":"online",
  "Charging":"In carica","Discharging":"In scarica","Idle":"Inattivo",
  "Runtime":"Autonomia","To full":"A pieno",
  "Storage":"Memoria",
  "SD card":"Scheda SD",
  "internal flash":"flash interna",
  "free":"liberi",
  "Sampling":"Campionamento",
  "one row every":"una riga ogni",
  "Archive":"Archivio",
  "files":"file",
  "Detect card":"Rileva scheda",
  "Eject card":"Espelli scheda",
  "Delete older than…":"Elimina più vecchi di…",
  "Show older":"Mostra più vecchi",
  "Card unmounted — you can remove it now":"Scheda smontata — ora puoi rimuoverla",
  "Delete logs older than how many days?":"Eliminare i log più vecchi di quanti giorni?",
  "Deleted":"Eliminati",
  "Input (alternator)":"Ingresso (alternatore)","Output (battery)":"Uscita (batteria)",
  "Voltage":"Tensione","Current":"Corrente","Power":"Potenza",
  "Efficiency & status":"Rendimento e stato",
  "Data log (CSV)":"Log dati (CSV)",
  "Logging":"Registrazione",
  "Rows":"Righe",
  "Free space":"Spazio libero",
  "off":"spento",
  "waiting for clock":"in attesa di orario",
  "logging":"in registrazione",
  "storage error":"errore memoria",
  "One row a minute on a microSD, one every two on internal flash. Applies immediately, no save needed. Status codes: battery 1 charge / 0 idle / -1 discharge; solar and DC-DC use the VE.Direct codes (3 bulk, 4 absorption, 5 float).":"Una riga al minuto su microSD, una ogni due sulla flash interna. Ha effetto subito, senza salvare. Codici di stato: batteria 1 carica / 0 inattiva / -1 scarica; solare e DC-DC usano i codici VE.Direct (3 bulk, 4 assorbimento, 5 float).",
  "Load output":"Uscita carichi",
  "Error":"Errore",
  "Efficiency":"Rendimento",
  "Status":"Stato",
  "Battery voltage high":"Tensione batteria alta",
  "Charger temp. high":"Temp. caricatore alta",
  "Charger over current":"Sovracorrente caricatore",
  "Current reversed":"Corrente invertita",
  "Bulk time limit":"Limite tempo bulk",
  "Current sensor issue":"Problema sensore corrente",
  "Terminals overheated":"Morsetti surriscaldati",
  "Converter issue":"Problema convertitore",
  "PV voltage too high":"Tensione PV troppo alta",
  "PV current too high":"Corrente PV troppo alta",
  "Input shutdown (V batt)":"Blocco ingresso (V batt)",
  "Input shutdown (I)":"Blocco ingresso (I)",
  "Communication lost":"Comunicazione persa",
  "Sync config issue":"Problema config. sync",
  "BMS connection lost":"Connessione BMS persa",
  "Network misconfigured":"Rete mal configurata",
  "Calibration data lost":"Dati calibrazione persi",
  "Invalid firmware":"Firmware non valido",
  "User settings invalid":"Impostazioni non valide",
  "Unknown error":"Errore sconosciuto",
  "No input power":"Nessuna alimentazione",
  "Engine off":"Motore spento",
  "Analysing input":"Analisi ingresso",
  "Protection active":"Protezione attiva",
  "Remote input":"Ingresso remoto",
  "Switched off (switch)":"Spento (interruttore)",
  "Switched off (mode)":"Spento (modo)",
  "Off":"Spento",
  "Balanced":"Bilanciata","Balancing":"Bilanciamento",
  "Connected devices":"Dispositivi connessi","Network":"Rete","Configuration":"Configurazione",
  "Save and reboot":"Salva e riavvia","Firmware update (OTA) →":"Aggiornamento firmware (OTA) →",
  "Cell voltages":"Tensioni celle","Bubble level":"Livella a bolla",
  "Production today":"Produzione oggi","Ramp / chock guidance":"Guida cunei / rampe",
  "No hourly history available on this firmware":"Nessuno storico orario disponibile sul firmware",
  "No cell data":"Nessun dato cella","Language":"Lingua","Keep screen on":"Tieni schermo acceso",
  "WiFi Client (optional)":"WiFi Client (opzionale)","NTP / Time":"NTP / Orario",
  "Saving...":"Salvataggio...","Reboot in progress...":"Riavvio in corso...",
  "Tilt sensor":"Inclinometro",
  "Firmware update (OTA)":"Aggiornamento firmware (OTA)","Enable OTA":"Abilita OTA",
  "Username":"Nome utente","OTA active only with enable + username + password":"OTA attivo solo con abilita + utente + password",
  "set":"impostata",
  "NTP time":"Ora NTP",
  "waiting...":"in attesa...",
  "not synced":"non sincronizzato",
  "Could not load settings":"Impossibile caricare le impostazioni",
  "AP password: minimum 8 characters":"Password AP: minimo 8 caratteri",
  "Invalid BMS MAC":"MAC BMS non valido",
  "Invalid MPPT key":"Chiave MPPT non valida",
  "Invalid DC-DC key":"Chiave DC-DC non valida",
  "Invalid IMU MAC":"MAC IMU non valido",
  "Saved! Rebooting...":"Salvato! Riavvio...",
  "Rebooting… reconnecting in":"Riavvio… riconnessione tra",
  "Reconnecting…":"Riconnessione…",
  "Format: AA:BB:CC:DD:EE:FF":"Formato: AA:BB:CC:DD:EE:FF",
  "✓ Correct format":"✓ Formato corretto",
  "Invalid format":"Formato non valido",
  "✓ Valid key":"✓ Chiave valida",
  "Must be 32 hex characters":"Deve essere 32 caratteri hex",
  "Minimum 8 characters (WPA2)":"Minimo 8 caratteri (WPA2)",
  "Minimum 8 characters":"Minimo 8 caratteri",
  "Existing network SSID":"SSID rete esistente",
  "NTP server":"Server NTP",
  "min 8 characters":"min 8 caratteri",
  "empty = AP only":"vuoto = solo AP",
  "network password":"password rete",
  "32 hex characters":"32 caratteri hex",
  "OTA: username required":"OTA: serve un username","OTA: password required":"OTA: serve una password",
  "Turn the AP off while the client is connected":"Spegni l'AP quando il client e' connesso",
  "Stops the phone auto-joining the AP. It comes back if the client drops, for 10 min after every boot, and from the AP button on the device screen.":"Evita che il telefono si agganci da solo all'AP. Torna se il client cade, per 10 min dopo ogni avvio, e dal pulsante AP sullo schermo del device.",
  "off (client connected)":"spento (client connesso)",
  "AP off needs a WiFi client network":"Per spegnere l'AP serve una rete client"
}};
var curLang='en';
function TR(k){ return (curLang==='it'&&I18N.it[k]!==undefined)?I18N.it[k]:k; }
function applyLang(){
  var tabs={'t-overview':'Overview','t-battery':'Battery','t-solar':'Solar','t-dcdc':'DC-DC','t-level':'Level','t-system':'System'};
  for(var id in tabs){ var s=document.querySelector('#'+id+' span'); if(s) s.textContent=TR(tabs[id]); }
  document.querySelectorAll('[data-i18n]').forEach(function(el){ el.textContent=TR(el.getAttribute('data-i18n')); });
  document.querySelectorAll('[data-i18n-ph]').forEach(function(el){ el.placeholder=TR(el.getAttribute('data-i18n-ph')); });
  var le=$('lang-en'), li=$('lang-it');
  if(le&&li){ le.classList.toggle('on',curLang==='en'); li.classList.toggle('on',curLang==='it'); }
  applyStoredHints();   // applyLang just reset every data-i18n-ph placeholder
  if(lastData) applyData(lastData);
}
function setLang(l){
  curLang=l?'it':'en'; applyLang();
  // /api/lang applies live; /api/settings would reboot the device on every save.
  fetch('/api/lang',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({lang:l})}).catch(function(){});
}

function fmt(v,d){ if(v===undefined||v===null||isNaN(v))return '--'; return Number(v).toFixed(d); }
function socColor(s){ return s>45?'var(--green)':s>18?'var(--amber)':'var(--red)'; }
// Runtime estimate: minutes come from the device (battery.etaMin, 0 = not meaningful),
// which derives them from the BMS coulomb counter and a smoothed current.
function fmtEta(m){ if(!m||m<=0) return '--'; var h=Math.floor(m/60), mm=m%60; return h>0?(h+'h '+(mm<10?'0':'')+mm+'m'):(mm+'m'); }
// SOC inside a ring: at 100% the third digit makes the text wider than the ring's inner
// circle and it clips the stroke, so drop one size step for 3-digit values only (0..99 keep
// the large size). bigPx/smallPx are the normal number/percent sizes for that ring.
function setSoc(el, v, on, bigPx, smallPx){
  if(!el) return;
  var three = on && Math.round(v) >= 100;
  el.style.fontSize = (three ? Math.round(bigPx*0.8) : bigPx) + 'px';
  el.innerHTML = on ? (Math.round(v)+'<span style="font-size:'+(three?Math.round(smallPx*0.85):smallPx)+'px;color:var(--muted)">%</span>') : '--';
}
function signColor(v){ return v>0.05?'var(--green)':v<-0.05?'var(--amber)':'var(--text)'; }

var VIEWS=['overview','battery','solar','dcdc','level','system'];
function setView(v){
  curView=v;
  VIEWS.forEach(function(x){
    $('v-'+x).classList.toggle('active', x===v);
    $('t-'+x).classList.toggle('active', x===v);
  });
  if(v==='system'){ loadSettings(); refreshLogs(); }
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
  var lbl=TR(st===1?'Charging':st===-1?'Discharging':'Idle');
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
  setSoc($('ov-soc'), soc, on, 52, 20);
  $('ov-soc').style.color=col;
  $('ov-bvi').textContent=on?(fmt(b.voltage,1)+'V · '+fmt(b.current,1)+'A'):'';
  $('ov-beta').textContent=on?fmtEta(b.etaMin):'';
  $('ov-beta').style.color=(on&&b.etaFull)?'var(--green)':'var(--muted)';
  var p=on?b.power:0, st=p>8?'Charging':p<-8?'Discharging':'Idle';
  $('ov-bstate').textContent=on?TR(st):TR('offline');
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
  // Loads = battery discharge only, straight from the BMS: what the loads pull FROM the
  // battery. bp>0 = charging, bp<0 = discharging, so discharge power = max(0,-bp).
  // Charging or idle -> 0 (Loads must never show a value that is charging the battery).
  var ldW=NaN, bp=on?(b.power||0):0;
  if(on){ ldW=Math.max(0,-bp); }
  var ldA=(on&&b.current!==undefined)?Math.max(0,-b.current):NaN;
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
  setSoc($('bt-soc'), b.soc, on, 40, 18);
  $('bt-soc').style.color=col;
  $('bt-v').textContent=on?fmt(b.voltage,2)+' V':'--';
  $('bt-a').textContent=on?fmt(b.current,1)+' A':'--';
  $('bt-a').style.color=on?signColor(b.current):'var(--muted)';
  $('bt-ah').textContent=on?fmt(b.remainingAh,0)+' Ah':'--';
  $('bt-eta-l').textContent=TR((on&&b.etaFull)?'To full':'Runtime');
  $('bt-eta').textContent=on?fmtEta(b.etaMin):'--';
  $('bt-eta').style.color=(on&&b.etaFull)?'var(--green)':'var(--text)';
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
  pill($('bt-bms'), on?TR(delta<=30?'Balanced':'Balancing'):TR('offline'),
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
  $('bt-cells').innerHTML=html||'<div class="card-s">'+TR('No cell data')+'</div>';
}

// ── solar ──
function updateSolar(d){
  var s=d.solar||{}, on=s.online;
  $('so-title').textContent=on?(s.model||'Solar'):'Solar';
  pill($('so-state'), on?(s.state||'--'):TR('offline'), on?'var(--orange)':'var(--muted)', on?'rgba(255,105,0,.16)':'var(--inset)');
  $('so-w').textContent=on?fmt(s.solarPower,0):'--';
  $('so-bv').textContent=on?fmt(s.battVoltage,2)+' V':'--';
  $('so-a').textContent=on?fmt(s.chargeCurrent,1)+' A':'--';
  $('so-y').textContent=on?fmt(s.yieldToday,0)+' Wh':'--';
  $('so-y2').textContent=on?fmt(s.yieldToday,0):'--';
  // Load output exists only on MPPT models with a load terminal (null otherwise) → hide the row.
  var hasLoad=on&&s.loadCurrent!==undefined&&s.loadCurrent!==null;
  $('so-load-row').style.display=hasLoad?'flex':'none';
  if(hasLoad) $('so-load').textContent=fmt(s.loadCurrent,1)+' A';
  // Error row stays hidden while the charger is fine (the API sends "" for code 0).
  var hasErr=on&&s.error&&s.error.length>0;
  $('so-err-row').style.display=hasErr?'flex':'none';
  if(hasErr) $('so-err').textContent=TR(s.error);
}

// ── dc-dc ──
function updateDcdc(d){
  var o=d.orion||{}, on=o.online;
  $('dc-title').textContent=on?(o.model||'DC-DC'):'DC-DC';
  // Real charge state from the advert (BULK/ABSORPTION/FLOAT/...), same as Solar.
  pill($('dc-state'), on?(o.state||'--'):TR('offline'), on?'var(--blue)':'var(--muted)', on?'rgba(90,165,245,.16)':'var(--inset)');
  var num=function(v){ return (on&&v!==undefined&&v!==null)?v:NaN; };
  var iV=num(o.inVoltage), iA=num(o.inCurrent), oV=num(o.outVoltage), oA=num(o.outCurrent);
  var iW=(!isNaN(iV)&&!isNaN(iA))?iV*iA:NaN, oW=(!isNaN(oV)&&!isNaN(oA))?oV*oA:NaN;
  $('dc-iv').textContent=isNaN(iV)?'--':fmt(iV,1)+' V';
  $('dc-ia').textContent=isNaN(iA)?'--':fmt(iA,1)+' A';
  $('dc-iw').textContent=isNaN(iW)?'--':fmt(iW,0)+' W';
  $('dc-ov').textContent=isNaN(oV)?'--':fmt(oV,1)+' V';
  $('dc-oa').textContent=isNaN(oA)?'--':fmt(oA,1)+' A';
  $('dc-ow').textContent=isNaN(oW)?'--':fmt(oW,0)+' W';
  // Only meaningful under real load; below that it is noise on tiny numbers.
  var eff=(!isNaN(iW)&&iW>5&&!isNaN(oW)&&oW>0)?(oW/iW*100):NaN;
  $('dc-eff').textContent=isNaN(eff)?'--':fmt(eff,0)+' %';
  // Error wins over off-reason; both empty while it is simply running.
  var oe=(on&&o.error&&o.error.length>0)?o.error:'';
  var or=(on&&o.offReason&&o.offReason.length>0)?o.offReason:'';
  $('dc-status').textContent=on?(oe?TR(oe):(or?TR(or):'\u2014')):'--';
  $('dc-status').style.color=oe?'var(--red)':'var(--text)';
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
        +'<div class="dn">'+TR(x.n)+'</div>'
        +'<div class="ds" style="color:'+(x.on?'var(--green)':'var(--muted)')+'">'+TR(x.on?'online':'offline')+'</div></div>';
  });
  $('sy-devs').innerHTML=html;
}

function updateSysInfo(sys){
  if(!sys) return;
  var el=$('st-status'); if(!el) return;
  var rows='';
  if(sys.fw) rows+='<div class="st-status-row"><span class="st-status-lbl">Firmware</span><span class="st-status-val">v'+sys.fw+'</span></div>';
  if(sys.ota!==undefined) rows+='<div class="st-status-row"><span class="st-status-lbl">OTA</span><span class="st-status-val '+(sys.ota?'ok':'dim')+'">'+(sys.ota?'ON':'OFF')+'</span></div>';
  if(sys.heap!==undefined) rows+='<div class="st-status-row"><span class="st-status-lbl">Free RAM</span><span class="st-status-val">'+Math.round(sys.heap/1024)+' KB</span></div>';
  var ol=$('ota-link'); if(ol) ol.style.display=sys.ota?'block':'none';
  if(sys.apOn===false) rows+='<div class="st-status-row"><span class="st-status-lbl">AP</span><span class="st-status-val dim">'+TR('off (client connected)')+'</span></div>';
  else rows+='<div class="st-status-row"><span class="st-status-lbl">AP</span><span class="st-status-val">http://'+(sys.apIp||'192.168.4.1')+'</span></div>';
  if(sys.staIp) rows+='<div class="st-status-row"><span class="st-status-lbl">WiFi Client</span><span class="st-status-val ok">&#10003; '+sys.staIp+'</span></div>';
  if(sys.time&&sys.date) rows+='<div class="st-status-row"><span class="st-status-lbl">'+TR('NTP time')+'</span><span class="st-status-val">'+sys.date+'&nbsp;&nbsp;'+sys.time+'</span></div>';
  else { var m=(sys.staIp&&sys.staIp.length>0)?TR('waiting...'):TR('not synced');
    rows+='<div class="st-status-row"><span class="st-status-lbl">'+TR('NTP time')+'</span><span class="st-status-val dim">'+m+'</span></div>'; }
  el.innerHTML=rows;
}

function applyData(d){
  lastData=d;
  // sync language from the device (initial load + device-side changes)
  if(d.sys&&d.sys.lang!==undefined){ var wl=d.sys.lang?'it':'en'; if(wl!==curLang){ curLang=wl; applyLang(); } }
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
    h.textContent=el.value.length===0?TR('Format: AA:BB:CC:DD:EE:FF'):ok?TR('✓ Correct format'):TR('Invalid format'); }
}
function validateKey(el,hintId){
  var ok=/^[0-9A-Fa-f]{32}$/.test(el.value);
  el.className='st-input '+(el.value.length===0?'':ok?'valid':'invalid');
  var h=$(hintId); if(h){ h.className='st-hint'+(el.value.length===0?'':ok?' ok':' err');
    h.textContent=el.value.length===0?'VictronConnect → Product info → Advertising key':ok?TR('✓ Valid key'):TR('Must be 32 hex characters'); }
}
// ── data log ──────────────────────────────────────────────────────────────
function fmtKB(b){ return (b>=1024)?(Math.round(b/1024)+' KB'):(b+' B'); }
var _logOffset=0, _logLimit=15, _logShown=0;

function fmtBytes(b){
  if(b>=1073741824) return (b/1073741824).toFixed(1)+' GB';
  if(b>=1048576)    return (b/1048576).toFixed(1)+' MB';
  if(b>=1024)       return Math.round(b/1024)+' KB';
  return b+' B';
}
function dayOf(n){ var m=/volthub_(\d{4})(\d{2})(\d{2})/.exec(n); return m?(m[3]+'/'+m[2]+'/'+m[1]):n; }

// The device returns a WINDOW of the list plus a summary: a card can hold a year of files and
// serialising them all would cost tens of KB of heap on the ESP32 — and be unreadable anyway.
function refreshLogs(reset){
  if(reset!==false){ _logOffset=0; _logShown=0; $('log-files').innerHTML=''; }
  fetch('/api/logs?offset='+_logOffset+'&limit='+_logLimit).then(function(r){return r.json();}).then(function(d){
    var b=$('log-btn');
    if(b){ b.textContent=d.enabled?'ON':'OFF'; b.classList.toggle('on',!!d.enabled); }
    var onSd=(d.medium==='sd');
    var rows='';
    rows+='<div class="st-status-row"><span class="st-status-lbl">'+TR('Status')+'</span><span class="st-status-val '+(d.state==='logging'?'ok':'dim')+'">'+TR(d.state)+'</span></div>';
    rows+='<div class="st-status-row"><span class="st-status-lbl">'+TR('Storage')+'</span><span class="st-status-val '+(onSd?'ok':'dim')+'">'+
          (onSd?TR('SD card'):TR('internal flash'))+' · '+fmtBytes(d.free)+' '+TR('free')+'</span></div>';
    if(d.enabled) rows+='<div class="st-status-row"><span class="st-status-lbl">'+TR('Sampling')+'</span><span class="st-status-val">'+TR('one row every')+' '+d.periodS+'s</span></div>';
    // Summary instead of a long list: tells you the size of the archive without enumerating it.
    if(d.count) rows+='<div class="st-status-row"><span class="st-status-lbl">'+TR('Archive')+'</span><span class="st-status-val">'+
          d.count+' '+TR('files')+' · '+fmtBytes(d.bytes)+(d.oldest?' · '+dayOf(d.oldest)+' → '+dayOf(d.newest):'')+'</span></div>';
    $('log-status').innerHTML=rows;
    $('log-actions').style.display=d.enabled?'':'none';
    var ej=$('log-eject'); if(ej) ej.style.display=onSd?'':'none';

    var fl='';
    (d.files||[]).forEach(function(f){
      fl+='<div class="st-status-row"><span class="st-status-lbl"><a class="ota-link" style="display:inline" href="/logdl?f='+encodeURIComponent(f.p)+'" download>'+dayOf(f.n)+'</a></span>'+
          '<span class="st-status-val">'+fmtBytes(f.s)+' <button class="lang-btn" onclick="delLog(\''+f.p+'\')">&#10005;</button></span></div>';
    });
    $('log-files').innerHTML += fl;
    _logShown += (d.files||[]).length;
    $('log-more').style.display=(_logShown < d.count)?'':'none';
  }).catch(function(){});
}
function moreLogs(){ _logOffset=_logShown; refreshLogs(false); }
function logAction(body){
  return fetch('/api/logs',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})
    .then(function(r){return r.json();}).then(function(j){ refreshLogs(); return j; }).catch(function(){});
}
// Pulling the card while the logger writes can damage the file or the FAT.
function logEject(){
  logAction({eject:true}).then(function(j){
    if(j&&j.ok) setMsg(TR('Card unmounted — you can remove it now'),'ok');
  });
}
function logPurge(){
  var d=prompt(TR('Delete logs older than how many days?'),'90');
  if(!d) return;
  logAction({purgeDays:parseInt(d,10)}).then(function(j){
    if(j&&j.removed!==undefined) setMsg(TR('Deleted')+' '+j.removed,'ok');
  });
}
function toggleLog(){
  var on=$('log-btn').textContent==='ON';
  fetch('/api/logs',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({enabled:!on})})
    .then(function(){ refreshLogs(); }).catch(function(){});
}
function delLog(p){ logAction({file:p}); }
// The device has no RTC and often no internet: hand it the phone's clock so the logger can
// start (and the on-screen clock is right). The device ignores it if already NTP-synced.
function pushTime(){
  fetch('/api/time',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({epoch:Math.floor(Date.now()/1000)})}).catch(function(){});
}

function syncEyes(){
  var wraps=document.querySelectorAll('.st-input-wrap');
  for(var i=0;i<wraps.length;i++){
    var inp=wraps[i].querySelector('input'), eye=wraps[i].querySelector('.st-eye');
    if(inp&&eye) eye.style.display = inp.value.length ? '' : 'none';
  }
}
document.addEventListener('input',function(e){
  if(e.target&&e.target.className&&(''+e.target.className).indexOf('st-input')>=0) syncEyes();
});
function toggleEye(id,btn){ var i=$(id); if(!i)return; if(i.type==='password'){i.type='text';btn.style.opacity='1';}else{i.type='password';btn.style.opacity='.5';} }
function validateWifiPass(el){
  var ok=el.value.length===0||el.value.length>=8;
  el.className='st-input'+(el.value.length===0?'':ok?' valid':' invalid');
  var h=$('st-wifi-pass-hint'); if(h){ h.className='st-hint'+(el.value.length===0?'':ok?' ok':' err');
    h.textContent=el.value.length===0?TR('Minimum 8 characters (WPA2)'):ok?'✓ OK':TR('Minimum 8 characters'); }
}
function setMsg(t,c){ var e=$('st-msg'); e.textContent=t; e.className='st-msg'+(c?' '+c:''); }
function applyStoredHints(){
  var mark = function(id,on){ var el=$(id); if(!el) return;
    if(on) el.placeholder='\u2022\u2022\u2022\u2022\u2022\u2022 ('+TR('set')+')'; };
  mark('st-ota-pass',_stored.ota);   mark('st-wifi-pass',_stored.wifi);
  mark('st-sta-pass',_stored.sta);   mark('st-solar-key',_stored.solar);
  mark('st-orion-key',_stored.orion);
}
function loadSettings(){
  fetch('/api/settings').then(function(r){return r.json();}).then(function(d){
    $('st-wifi-ssid').value=d.wifiSsid||''; $('st-sta-ssid').value=d.staSsid||'';
    $('st-bms-mac').value=d.bmsMac||''; $('st-ntp-srv').value=d.ntpServer||'';
    $('st-ntp-tz').value=d.ntpTZ||''; $('st-imu-mac').value=d.imuMac||'';
    $('st-ota-en').checked=!!d.otaEnabled; $('st-ota-user').value=d.otaUser||'';
    $('st-ap-off').checked=!!d.apOffWhenSta;
    // Secret fields stay empty: the device does not send them back.
    ['st-wifi-pass','st-sta-pass','st-solar-key','st-orion-key','st-ota-pass'].forEach(function(id){ $(id).value=''; });
    _stored={ota:!!d.otaHasPass, wifi:!!d.wifiHasPass, sta:!!d.staHasPass,
             solar:!!d.solarKeySet, orion:!!d.orionKeySet};
    _otaHasPass=_stored.ota;
    applyStoredHints();
    syncEyes();
  }).catch(function(){ setMsg(TR('Could not load settings'),'err'); });
}
function saveSettings(){
  var g=function(id){return $(id).value.trim();};
  var ssid=g('st-wifi-ssid'),pass=g('st-wifi-pass'),staSsid=g('st-sta-ssid'),staPass=g('st-sta-pass'),
      mac=g('st-bms-mac'),solar=g('st-solar-key'),orion=g('st-orion-key'),
      ntpSrv=g('st-ntp-srv'),ntpTZ=g('st-ntp-tz'),imuMac=g('st-imu-mac'),
      otaUser=g('st-ota-user'),otaPass=g('st-ota-pass'),otaEn=$('st-ota-en').checked,
      apOff=$('st-ap-off').checked;
  if(pass&&pass.length<8){ setMsg(TR('AP password: minimum 8 characters'),'err'); return; }
  if(mac&&!/^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$/.test(mac)){ setMsg(TR('Invalid BMS MAC'),'err'); return; }
  if(solar&&!/^[0-9A-Fa-f]{32}$/.test(solar)){ setMsg(TR('Invalid MPPT key'),'err'); return; }
  if(orion&&!/^[0-9A-Fa-f]{32}$/.test(orion)){ setMsg(TR('Invalid DC-DC key'),'err'); return; }
  if(imuMac&&!/^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$/.test(imuMac)){ setMsg(TR('Invalid IMU MAC'),'err'); return; }
  if(apOff && !staSsid){ setMsg(TR('AP off needs a WiFi client network'),'err'); return; }
  if(otaEn){
    if(!otaUser){ setMsg(TR('OTA: username required'),'err'); return; }
    if(!otaPass && !_otaHasPass){ setMsg(TR('OTA: password required'),'err'); return; }
  }
  $('st-save-btn').disabled=true; setMsg(TR('Saving...'),'info');
  var body=JSON.stringify({wifiSsid:ssid,wifiPass:pass,staSsid:staSsid,staPass:staPass,
    bmsMac:mac,solarKey:solar,orionKey:orion,ntpServer:ntpSrv,ntpTZ:ntpTZ,imuMac:imuMac,
    otaEnabled:otaEn,otaUser:otaUser,otaPass:otaPass,apOffWhenSta:apOff});
  fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:body})
    .then(function(){ setMsg(TR('Saved! Rebooting...'),'ok'); waitReconnect(12); })
    .catch(function(){ setMsg(TR('Reboot in progress...'),'ok'); waitReconnect(12); });
}
function waitReconnect(s){ if(s<=0){tryReconnect();return;} setMsg(TR('Rebooting… reconnecting in')+' '+s+'s','info'); setTimeout(function(){waitReconnect(s-1);},1000); }
function tryReconnect(){ setMsg(TR('Reconnecting…'),'info'); fetch('/api/data').then(function(){location.reload();}).catch(function(){setTimeout(tryReconnect,2000);}); }

// ── poll ──
// Chained polling: schedule the NEXT request only after the current one settles, so there is
// at most one in-flight request. setInterval would overlap requests when the ESP32 (single
// synchronous WebServer, busy with display + BLE + WiFi) is slow, piling up TCP connections
// until the web server stops responding (recovered only by a reboot). An AbortController
// timeout drops a hung request so a single stall can't wedge the loop.
function poll(){
  var ctrl=('AbortController' in window)?new AbortController():null;
  var to=ctrl?setTimeout(function(){ctrl.abort();},8000):null;
  fetch('/api/data',ctrl?{signal:ctrl.signal}:{}).then(function(r){return r.json();}).then(applyData)
    .catch(function(){})
    .then(function(){ if(to)clearTimeout(to); setTimeout(poll,2000); });
}
initWake();
pushTime();
poll();
</script>
</body>
</html>
)rawliteral";

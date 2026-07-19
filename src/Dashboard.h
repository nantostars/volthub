#pragma once

// Stored in flash (PROGMEM). Served once; auto-refresh is done by JS fetch() every 2 s.
static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
<title>Camper Energy</title>
<style>
  :root {
    --bg:       #0f1117;
    --surface:  #1a1d27;
    --border:   #2a2d3e;
    --text:     #e8eaf0;
    --muted:    #6b7280;
    --green:    #22c55e;
    --yellow:   #f59e0b;
    --red:      #ef4444;
    --blue:     #3b82f6;
    --orange:   #f97316;
    --blue-dim: rgba(59,130,246,0.25);
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: var(--bg);
    color: var(--text);
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
    font-size: 15px;
    min-height: 100vh;
  }

  /* ── Tab toggle (pill) ─────────────────────────────────────── */
  .tab-bar {
    display: flex;
    justify-content: center;
    align-items: center;
    height: 56px;
    background: var(--bg);
    border-bottom: 1px solid var(--border);
    position: sticky;
    top: 0;
    z-index: 10;
  }
  .tab-pill {
    display: flex;
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 999px;
    padding: 3px;
    gap: 2px;
  }
  .tab-btn {
    background: none;
    border: none;
    color: var(--muted);
    font-size: 0.82rem;
    font-weight: 600;
    padding: 5px 13px;
    border-radius: 999px;
    cursor: pointer;
    transition: background 0.2s, color 0.2s;
    letter-spacing: 0.04em;
    white-space: nowrap;
  }
  .tab-btn.active {
    background: var(--blue);
    color: #fff;
  }

  /* ── View containers ───────────────────────────────────────── */
  #view-cards {
    padding: 12px;
  }
  #view-overview {
    display: none;
    flex-direction: column;
    height: calc(100dvh - 56px);
    position: relative;
    padding: 12px;
  }

  /* ── Cards view (unchanged) ────────────────────────────────── */
  .grid {
    display: grid;
    grid-template-columns: 1fr;
    gap: 12px;
    max-width: 480px;
    margin: 0 auto;
  }
  .card {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 14px;
    padding: 16px;
  }
  .card-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 14px;
  }
  .card-title {
    font-size: 0.8rem;
    font-weight: 600;
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--muted);
  }
  .dot {
    width: 8px; height: 8px;
    border-radius: 50%;
    flex-shrink: 0;
  }
  .dot.online  { background: var(--green); box-shadow: 0 0 6px var(--green); }
  .dot.offline { background: var(--red); }

  .soc-row {
    display: flex;
    align-items: baseline;
    gap: 8px;
    margin-bottom: 10px;
  }
  .soc-value {
    font-size: 3rem;
    font-weight: 700;
    line-height: 1;
  }
  .soc-unit { font-size: 1.2rem; color: var(--muted); }
  .bar-wrap {
    background: var(--border);
    border-radius: 6px;
    height: 10px;
    overflow: hidden;
    margin-bottom: 14px;
  }
  .bar-fill {
    height: 100%;
    border-radius: 6px;
    transition: width 0.5s ease;
  }
  .bar-green  { background: var(--green); }
  .bar-yellow { background: var(--yellow); }
  .bar-red    { background: var(--red); }

  .metrics {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
  }
  .metric {
    background: var(--bg);
    border-radius: 10px;
    padding: 10px 12px;
  }
  .metric .label {
    font-size: 0.72rem;
    color: var(--muted);
    text-transform: uppercase;
    letter-spacing: 0.06em;
    margin-bottom: 2px;
  }
  .metric .value {
    font-size: 1.2rem;
    font-weight: 600;
  }
  .metric .unit {
    font-size: 0.75rem;
    color: var(--muted);
    margin-left: 2px;
  }
  .val-pos { color: var(--green); }
  .val-neg { color: var(--orange); }
  .val-neu { color: var(--text); }
  .val-na  { color: var(--muted); font-weight: 400; font-size: 1rem; }
  .val-dim { color: var(--muted); }

  .state-badge {
    display: inline-block;
    font-size: 0.78rem;
    font-weight: 600;
    padding: 3px 10px;
    border-radius: 20px;
    background: var(--bg);
    border: 1px solid var(--border);
    margin-bottom: 14px;
  }
  .state-bulk       { color: var(--yellow); border-color: var(--yellow); }
  .state-absorption { color: var(--blue);   border-color: var(--blue); }
  .state-float      { color: var(--green);  border-color: var(--green); }
  .state-off        { color: var(--muted); }
  .state-fault      { color: var(--red);    border-color: var(--red); }

  .cells-toggle {
    font-size: 0.75rem;
    color: var(--muted);
    cursor: pointer;
    margin-top: 12px;
    user-select: none;
  }
  .cells-grid {
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    gap: 6px;
    margin-top: 8px;
  }
  .cell-v {
    background: var(--bg);
    border-radius: 6px;
    padding: 5px 4px;
    text-align: center;
    font-size: 0.78rem;
  }

  .footer {
    text-align: center;
    font-size: 0.72rem;
    color: var(--muted);
    margin-top: 16px;
    padding-bottom: 8px;
  }
  #last-update { font-weight: 600; color: var(--text); }

  /* ── Overview view ─────────────────────────────────────────── */
  .ov-wrapper {
    display: flex;
    flex-direction: column;
    height: 100%;
    position: relative;
  }
  #ov-svg {
    position: absolute;
    top: 0; left: 0;
    width: 100%; height: 100%;
    pointer-events: none;
    overflow: visible;
  }

  .ov-row1 {
    flex: 1;
    display: flex;
    flex-direction: row;
    gap: 8%;
    align-items: stretch;
  }
  .ov-gap {
    height: 36px;
    flex-shrink: 0;
  }
  .ov-row3 {
    display: flex;
    justify-content: center;
    flex-shrink: 0;
    padding-bottom: 8px;
  }

  /* Overview boxes */
  .ov-box {
    background: var(--surface);
    border: 1.5px solid var(--border);
    border-radius: 14px;
    padding: 12px 14px;
    display: flex;
    flex-direction: column;
    gap: 4px;
    position: relative;
    overflow: hidden;
    flex: 1;
  }
  #ov-batt { flex: 1.4; }
  #ov-loads { width: 38%; }

  .ov-title {
    font-size: 0.9rem;
    font-weight: 700;
    letter-spacing: 0.07em;
    text-transform: uppercase;
    color: var(--muted);
    margin-bottom: 2px;
  }
  .ov-main {
    font-size: clamp(2rem, 5vw, 3rem);
    font-weight: 700;
    line-height: 1.1;
  }
  .ov-sub {
    font-size: 1rem;
    color: var(--muted);
  }
  .ov-bottom {
    display: flex;
    flex-direction: column;
    gap: 2px;
    margin-top: auto;
    padding-top: 4px;
  }
  .ov-stat {
    font-size: 1rem;
    color: var(--muted);
  }
  .ov-stat span {
    color: var(--text);
    font-weight: 600;
  }
  .ov-stat span.val-pos { color: var(--green); }
  .ov-stat span.val-neg { color: var(--orange); }
  .ov-stat span.val-dim { color: var(--muted); }
  .ov-dot {
    position: absolute;
    top: 10px; right: 10px;
    width: 8px; height: 8px;
    border-radius: 50%;
  }
  .ov-dot.online  { background: var(--green); box-shadow: 0 0 6px var(--green); }
  .ov-dot.offline { background: var(--red); }

  /* Battery SOC fill */
  .batt-fill-bg {
    position: absolute;
    bottom: 0; left: 0; right: 0;
    background: var(--blue-dim);
    border-radius: 0 0 12px 12px;
    transition: height 0.6s ease;
    pointer-events: none;
  }

  /* Flow lines */
  .fl-bg   { stroke: #1e2d45; stroke-width: 3; fill: none; }
  .fl-line { stroke: var(--blue); stroke-width: 3; fill: none; stroke-dasharray: 7 11; stroke-linecap: round; opacity: 0; transition: opacity 0.3s; }
  .fl-line.on { opacity: 1; }
  .fl-fwd { animation: flFwd 0.9s linear infinite; }
  .fl-rev { animation: flRev 0.9s linear infinite; }
  @keyframes flFwd { from { stroke-dashoffset: 18; } to { stroke-dashoffset: 0; } }
  @keyframes flRev { from { stroke-dashoffset: 0; } to { stroke-dashoffset: 18; } }

  /* ── Network status block ─────────────────────────────────── */
  .st-status {
    background: var(--surface); border: 1px solid var(--border);
    border-radius: 12px; padding: 12px 14px; margin-bottom: 14px;
    font-size: 0.8rem; display: none;
  }
  .st-status.visible { display: block; }
  .st-status-row { display: flex; justify-content: space-between; align-items: center; padding: 4px 0; }
  .st-status-row + .st-status-row { border-top: 1px solid var(--border); }
  .st-status-lbl { color: var(--muted); font-size: 0.72rem; text-transform: uppercase; letter-spacing: 0.05em; }
  .st-status-val { font-weight: 600; }
  .st-status-val.ok  { color: var(--green); }
  .st-status-val.dim { color: var(--muted); font-weight: 400; }

  /* ── Settings view ────────────────────────────────────────── */
  #view-settings { padding: 12px; }
  .st-group {
    background: var(--surface); border: 1px solid var(--border);
    border-radius: 14px; padding: 16px; margin-bottom: 14px;
  }
  .st-group-title {
    font-size: 0.72rem; font-weight: 700; letter-spacing: 0.08em;
    text-transform: uppercase; color: var(--muted); margin-bottom: 14px;
  }
  .st-field { margin-bottom: 12px; }
  .st-field:last-child { margin-bottom: 0; }
  .st-label {
    font-size: 0.75rem; color: var(--muted); text-transform: uppercase;
    letter-spacing: 0.06em; margin-bottom: 6px; display: block;
  }
  .st-input-wrap { position: relative; display: flex; align-items: center; }
  .st-input {
    width: 100%; background: var(--bg); border: 1px solid var(--border);
    border-radius: 10px; color: var(--text); font-size: 0.95rem;
    font-family: 'SF Mono', 'Fira Code', monospace; padding: 10px 44px 10px 12px;
    outline: none; transition: border-color 0.2s;
  }
  .st-input:focus { border-color: var(--blue); }
  .st-input.invalid { border-color: var(--red); }
  .st-input.valid   { border-color: var(--green); }
  .st-eye {
    position: absolute; right: 10px; background: none; border: none;
    color: var(--muted); cursor: pointer; font-size: 1rem; padding: 4px;
    line-height: 1;
  }
  .st-hint { font-size: 0.7rem; color: var(--muted); margin-top: 4px; }
  .st-hint.err { color: var(--red); }
  .st-hint.ok  { color: var(--green); }

  .st-save-btn {
    width: 100%; padding: 14px; border-radius: 12px;
    background: var(--blue); border: none; color: #fff;
    font-size: 1rem; font-weight: 700; cursor: pointer;
    transition: opacity 0.2s; margin-bottom: 14px;
  }
  .st-save-btn:disabled { opacity: 0.4; cursor: default; }

  .st-msg {
    text-align: center; font-size: 0.9rem; padding: 10px;
    border-radius: 10px; display: none;
  }
  .st-msg.info { background: rgba(59,130,246,0.15); color: var(--blue); display: block; }
  .st-msg.ok   { background: rgba(34,197,94,0.15);  color: var(--green); display: block; }
  .st-msg.err  { background: rgba(239,68,68,0.15);  color: var(--red);  display: block; }

  .st-note {
    background: var(--surface); border: 1px solid var(--border);
    border-radius: 14px; padding: 14px 16px;
    font-size: 0.78rem; color: var(--muted); line-height: 1.6;
  }
  .st-note strong { color: var(--text); }

  /* ── Portrait/mobile responsive layout ─────────────────────── */
  @media (max-width: 520px) {
    .ov-row1 { flex-wrap: wrap; gap: 8px; }
    #ov-solar { order: 1; flex: 1 1 calc(50% - 4px); }
    #ov-orion { order: 2; flex: 1 1 calc(50% - 4px); }
    #ov-batt  { order: 3; flex: 1 1 100%; }
    .ov-gap   { height: 12px; }
    #ov-loads { width: auto; }
  }

  /* ── Clickable overview boxes ──────────────────────────────── */
  #ov-solar, #ov-batt, #ov-orion { cursor: pointer; -webkit-tap-highlight-color: rgba(59,130,246,0.1); }
  #ov-solar:active, #ov-batt:active, #ov-orion:active { border-color: var(--blue); }

  /* ── Detail panel ──────────────────────────────────────────── */
  #dp {
    position: fixed; top: 0; right: 0; bottom: 0; left: 0;
    background: var(--bg); z-index: 200; overflow-y: auto;
    transform: translateX(100%);
    transition: transform 0.28s cubic-bezier(.4,0,.2,1);
    -webkit-overflow-scrolling: touch;
  }
  #dp.open { transform: translateX(0); }
  .dp-header {
    display: flex; align-items: center; gap: 12px;
    padding: 14px 16px; border-bottom: 1px solid var(--border);
    position: sticky; top: 0; background: var(--bg); z-index: 1;
  }
  .dp-back {
    background: var(--surface); border: 1px solid var(--border);
    color: var(--text); font-size: 0.88rem; font-weight: 600;
    padding: 6px 14px; border-radius: 20px; cursor: pointer;
    white-space: nowrap;
  }
  .dp-name { font-size: 0.95rem; font-weight: 700; flex: 1; }
  .dp-dot  { width: 8px; height: 8px; border-radius: 50%; flex-shrink: 0; }
  .dp-dot.online  { background: var(--green); box-shadow: 0 0 6px var(--green); }
  .dp-dot.offline { background: var(--red); }
  .dp-body { padding: 16px; display: flex; flex-direction: column; gap: 20px; max-width: 540px; margin: 0 auto; }

  /* Hero (big number at top of panel) */
  .dp-hero { text-align: center; padding: 14px 0 4px; }
  .dp-hero-val { font-size: clamp(3.2rem, 14vw, 5.5rem); font-weight: 800; line-height: 1; }
  .dp-hero-unit { font-size: 1.6rem; color: var(--muted); margin-left: 4px; }
  .dp-hero-sub  { font-size: 0.9rem; color: var(--muted); margin-top: 8px; }

  /* SOC bar inside hero */
  .dp-soc-bar  { height: 8px; background: var(--border); border-radius: 4px; overflow: hidden; margin: 10px 16px 0; }
  .dp-soc-fill { height: 100%; border-radius: 4px; transition: width 0.5s ease; }

  /* Section */
  .dp-sec-title {
    font-size: 0.68rem; font-weight: 700; letter-spacing: 0.1em;
    text-transform: uppercase; color: var(--muted);
    margin-bottom: 10px; padding-bottom: 6px; border-bottom: 1px solid var(--border);
  }

  /* KPI tiles */
  .dp-kpis { display: grid; gap: 10px; }
  .dp-kpis.c3 { grid-template-columns: repeat(3,1fr); }
  .dp-kpis.c2 { grid-template-columns: repeat(2,1fr); }
  .dp-kpis.c1 { grid-template-columns: 1fr; }
  .dp-kpi { background: var(--surface); border-radius: 12px; padding: 12px 10px; text-align: center; }
  .dp-kv  { font-size: 1.35rem; font-weight: 700; line-height: 1.1; }
  .dp-ku  { font-size: 0.75rem; color: var(--muted); margin-left: 2px; }
  .dp-kl  { font-size: 0.68rem; color: var(--muted); text-transform: uppercase; letter-spacing: 0.06em; margin-top: 4px; }

  /* Orion IN / OUT comparison */
  .dp-inout     { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
  .dp-io-col    { background: var(--surface); border-radius: 12px; padding: 14px 12px; }
  .dp-io-head   { font-size: 0.7rem; font-weight: 700; letter-spacing: 0.08em; text-transform: uppercase; color: var(--blue); text-align: center; margin-bottom: 12px; }
  .dp-io-row    { display: flex; justify-content: space-between; align-items: baseline; padding: 5px 0; border-bottom: 1px solid var(--border); }
  .dp-io-row:last-child { border-bottom: none; }
  .dp-io-lbl    { font-size: 0.72rem; color: var(--muted); }
  .dp-io-val    { font-size: 1rem; font-weight: 600; }
  .dp-io-unit   { font-size: 0.72rem; color: var(--muted); margin-left: 2px; }

  /* Cell voltages grid */
  .dp-cells { display: grid; grid-template-columns: repeat(4,1fr); gap: 7px; }
  .dp-cell  { background: var(--surface); border: 1px solid var(--border); border-radius: 8px; padding: 8px 4px; text-align: center; }
  .dp-cell.cmin { border-color: var(--red); }
  .dp-cell.cmax { border-color: var(--green); }
  .dp-cidx { font-size: 0.65rem; color: var(--muted); }
  .dp-cval { font-size: 0.9rem; font-weight: 600; margin-top: 2px; }

  /* Offline state */
  .dp-offline { text-align: center; padding: 48px 20px; color: var(--muted); }
  .dp-offline-icon { font-size: 3rem; margin-bottom: 12px; }

  /* ── Level view ────────────────────────────────────────────── */
  #view-level {
    padding: 12px;
    display: none;
    flex-direction: column;
    height: calc(100vh - 56px);
    box-sizing: border-box;
    overflow: hidden;
  }
  .lv-grid {
    flex: 1;
    display: flex;
    flex-direction: row;
    gap: 12px;
    min-height: 0;
  }
  @media (orientation: portrait) {
    .lv-grid { flex-direction: column; }
  }
  .lv-card {
    flex: 1;
    min-width: 0;
    min-height: 0;
    display: flex;
    flex-direction: column;
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 14px;
    padding: 16px;
  }
  .lv-title {
    font-size: 0.72rem; font-weight: 700; letter-spacing: 0.08em;
    text-transform: uppercase; color: var(--muted);
    text-align: center; margin-bottom: 10px;
    flex-shrink: 0;
  }
  .lv-svg { flex: 1; min-height: 0; width: 100%; height: 100%; display: block; }
  .lv-status {
    text-align: center; font-size: 0.65rem; font-weight: 700;
    letter-spacing: 0.12em; color: var(--muted); margin-top: 6px; min-height: 1.2em;
    flex-shrink: 0;
  }
  .lv-offline {
    text-align: center; padding: 12px 20px 4px; color: var(--muted);
    font-size: 0.82rem; letter-spacing: 0.04em;
  }
  .lv-offline-icon { font-size: 1.6rem; margin-bottom: 4px; }
</style>
</head>
<body>

<!-- ── Tab bar ──────────────────────────────────────────────── -->
<div class="tab-bar">
  <div class="tab-pill">
    <button class="tab-btn active" id="btn-cards"    onclick="setView('cards')">Cards</button>
    <button class="tab-btn"        id="btn-overview" onclick="setView('overview')">Overview</button>
    <button class="tab-btn"        id="btn-level"    onclick="setView('level')">&#9878; Level</button>
    <button class="tab-btn"        id="btn-settings" onclick="setView('settings')">&#9881; Settings</button>
  </div>

</div>

<!-- ═══════════════════════ VIEW: CARDS ═══════════════════════ -->
<div id="view-cards">
<div class="grid">

  <!-- ── BATTERY ───────────────────────────────────────── -->
  <div class="card" id="card-battery">
    <div class="card-header">
      <span class="card-title">&#128267; LiTime Battery</span>
      <span class="dot offline" id="dot-battery"></span>
    </div>

    <div class="soc-row">
      <span class="soc-value" id="batt-soc">--</span>
      <span class="soc-unit">%</span>
      <span style="margin-left:auto;font-size:0.8rem;color:var(--muted)" id="batt-remain">-- Ah</span>
    </div>
    <div class="bar-wrap">
      <div class="bar-fill bar-green" id="batt-bar" style="width:0%"></div>
    </div>

    <div class="metrics">
      <div class="metric">
        <div class="label">Voltage</div>
        <div class="value val-neu"><span id="batt-volt">--</span><span class="unit">V</span></div>
      </div>
      <div class="metric">
        <div class="label">Current</div>
        <div class="value"><span id="batt-curr">--</span><span class="unit">A</span></div>
      </div>
      <div class="metric">
        <div class="label">Power</div>
        <div class="value"><span id="batt-power">--</span><span class="unit">W</span></div>
      </div>
      <div class="metric">
        <div class="label">SOH</div>
        <div class="value val-neu"><span id="batt-soh">--</span><span class="unit">%</span></div>
      </div>
      <div class="metric">
        <div class="label">Cell Temp</div>
        <div class="value val-neu"><span id="batt-ctemp">--</span><span class="unit">&#176;C</span></div>
      </div>
      <div class="metric">
        <div class="label">FET Temp</div>
        <div class="value val-neu"><span id="batt-ftemp">--</span><span class="unit">&#176;C</span></div>
      </div>
    </div>

    <div class="cells-toggle" id="cells-toggle" onclick="toggleCells()">&#9660; Cell voltages</div>
    <div class="cells-grid" id="cells-grid" style="display:none"></div>
  </div>

  <!-- ── SOLAR CHARGER ─────────────────────────────────── -->
  <div class="card" id="card-solar">
    <div class="card-header">
      <span class="card-title">&#9728; Victron MPPT</span>
      <span class="dot offline" id="dot-solar"></span>
    </div>
    <div id="solar-state" class="state-badge state-off">Off</div>
    <div class="metrics">
      <div class="metric">
        <div class="label">Solar Power</div>
        <div class="value val-pos"><span id="solar-power">--</span><span class="unit">W</span></div>
      </div>
      <div class="metric">
        <div class="label">Charge Current</div>
        <div class="value val-pos"><span id="solar-curr">--</span><span class="unit">A</span></div>
      </div>
      <div class="metric">
        <div class="label">Batt Voltage</div>
        <div class="value val-neu"><span id="solar-bvolt">--</span><span class="unit">V</span></div>
      </div>
      <div class="metric">
        <div class="label">Yield Today</div>
        <div class="value val-neu"><span id="solar-yield">--</span><span class="unit">Wh</span></div>
      </div>
    </div>
  </div>

  <!-- ── ORION XS ───────────────────────────────────────── -->
  <div class="card" id="card-orion">
    <div class="card-header">
      <span class="card-title">&#128665; Victron DC-DC</span>
      <span class="dot offline" id="dot-orion"></span>
    </div>
    <div id="orion-state" class="state-badge state-off">Off</div>
    <div class="metrics">
      <div class="metric">
        <div class="label">Output Voltage</div>
        <div class="value val-neu"><span id="orion-outvolt">--</span><span class="unit">V</span></div>
      </div>
      <div class="metric">
        <div class="label">Output Current</div>
        <div class="value val-pos"><span id="orion-outcurr">--</span><span class="unit">A</span></div>
      </div>
      <div class="metric">
        <div class="label">Input Voltage</div>
        <div class="value val-neu"><span id="orion-involt">--</span><span class="unit">V</span></div>
      </div>
      <div class="metric">
        <div class="label">Input Current</div>
        <div class="value val-neu"><span id="orion-incurr">--</span><span class="unit">A</span></div>
      </div>
    </div>
  </div>

</div><!-- /grid -->
<div class="footer">Last update: <span id="last-update">--</span></div>
</div><!-- /view-cards -->

<!-- ═══════════════════════ VIEW: OVERVIEW ════════════════════ -->
<div id="view-overview">
  <div class="ov-wrapper" id="ov-wrapper">

    <!-- SVG flow lines (drawn by JS) -->
    <svg id="ov-svg"></svg>

    <!-- Row 1: Solar | Battery | Orion -->
    <div class="ov-row1">

      <!-- Solar -->
      <div class="ov-box" id="ov-solar">
        <span class="ov-dot offline" id="ov-dot-solar"></span>
        <div class="ov-title">SOLAR</div>
        <div class="ov-main val-pos"><span id="ov-s-power">--</span> <small style="font-size:0.55em;color:var(--muted)">W</small></div>
        <div class="ov-sub" id="ov-s-state">--</div>
        <div class="ov-bottom">
          <div class="ov-stat">A <span id="ov-s-curr">--</span></div>
          <div class="ov-stat">V <span id="ov-s-volt">--</span></div>
          <div class="ov-stat">Wh <span id="ov-s-yield" class="val-dim">--</span></div>
        </div>
      </div>

      <!-- Battery -->
      <div class="ov-box" id="ov-batt">
        <div class="batt-fill-bg" id="ov-batt-fill" style="height:0%"></div>
        <span class="ov-dot offline" id="ov-dot-batt"></span>
        <div class="ov-title" style="position:relative">BATTERY</div>
        <div class="ov-main" style="position:relative"><span id="ov-b-soc">--</span><small style="font-size:0.5em;color:var(--muted)">%</small></div>
        <div class="ov-sub" id="ov-b-state" style="position:relative">--</div>
        <div class="ov-bottom" style="position:relative">
          <div class="ov-stat">A <span id="ov-b-curr">--</span></div>
          <div class="ov-stat">V <span id="ov-b-volt">--</span></div>
          <div class="ov-stat">W <span id="ov-b-pow">--</span></div>
        </div>
      </div>

      <!-- Orion -->
      <div class="ov-box" id="ov-orion">
        <span class="ov-dot offline" id="ov-dot-orion"></span>
        <div class="ov-title">DC-DC</div>
        <div class="ov-main val-pos"><span id="ov-o-power">--</span> <small style="font-size:0.55em;color:var(--muted)">W</small></div>
        <div class="ov-sub" id="ov-o-state">--</div>
        <div class="ov-bottom">
          <div class="ov-stat">A <span id="ov-o-out">--</span></div>
          <div class="ov-stat">V <span id="ov-o-volt">--</span></div>
        </div>
      </div>

    </div><!-- /ov-row1 -->

    <!-- Vertical gap (space for SVG line) -->
    <div class="ov-gap"></div>

    <!-- Row 3: Loads -->
    <div class="ov-row3">
      <div class="ov-box" id="ov-loads">
        <div class="ov-title" style="text-align:center">LOADS</div>
        <div class="ov-main" style="text-align:center"><span id="ov-l-curr">--</span> <small style="font-size:0.55em;color:var(--muted)">A</small></div>
        <div class="ov-sub" style="text-align:center"><span id="ov-l-power">--</span> W</div>
      </div>
    </div>

  </div><!-- /ov-wrapper -->
</div><!-- /view-overview -->

<!-- ═══════════════════════ VIEW: LEVEL ══════════════════════ -->
<div id="view-level">
  <div class="lv-offline" id="lv-offline" style="display:none">
    <div class="lv-offline-icon">&#128225;</div>
    <div>IMU offline &mdash; nessun dato</div>
  </div>
  <div class="lv-grid" id="lv-grid">
    <div class="lv-card">
      <div class="lv-title">&#8597; Avanti / Indietro (Pitch)</div>
      <svg id="lv-svg-pitch" class="lv-svg" viewBox="0 0 320 275"></svg>
    </div>
    <div class="lv-card">
      <div class="lv-title">&#8596; Sinistra / Destra (Roll)</div>
      <svg id="lv-svg-roll" class="lv-svg" viewBox="0 0 320 275"></svg>
    </div>
  </div>
</div><!-- /view-level -->

<!-- ═══════════════════════ VIEW: SETTINGS ═══════════════════ -->
<div id="view-settings" style="display:none">

  <!-- Network status (populated by JS from /api/data sys object) -->
  <div class="st-status" id="st-status"></div>

  <!-- WiFi AP -->
  <div class="st-group">
    <div class="st-group-title">&#128246; WiFi AP</div>
    <div class="st-field">
      <label class="st-label" for="st-wifi-ssid">SSID</label>
      <div class="st-input-wrap">
        <input id="st-wifi-ssid" class="st-input" type="text"
               placeholder="CamperEnergy" maxlength="32" autocomplete="off">
      </div>
      <div class="st-hint">Nome della rete WiFi (1–32 caratteri)</div>
    </div>
    <div class="st-field">
      <label class="st-label" for="st-wifi-pass">Password</label>
      <div class="st-input-wrap">
        <input id="st-wifi-pass" class="st-input" type="password"
               placeholder="min 8 caratteri" maxlength="63"
               oninput="validateWifiPass(this)" autocomplete="off">
        <button class="st-eye" onclick="toggleEye('st-wifi-pass',this)">&#128065;</button>
      </div>
      <div id="st-wifi-pass-hint" class="st-hint">Minimo 8 caratteri (WPA2)</div>
    </div>
  </div>

  <!-- WiFi Client (STA opzionale) -->
  <div class="st-group">
    <div class="st-group-title">&#127760; WiFi Client (opzionale)</div>
    <div class="st-field">
      <label class="st-label" for="st-sta-ssid">SSID rete esistente</label>
      <div class="st-input-wrap">
        <input id="st-sta-ssid" class="st-input" type="text"
               placeholder="vuoto = solo modalità AP" maxlength="32" autocomplete="off">
      </div>
      <div class="st-hint">Lascia vuoto per disabilitare. Se impostato, l&#8217;ESP32 si connette alla rete e avvia NTP.</div>
    </div>
    <div class="st-field">
      <label class="st-label" for="st-sta-pass">Password</label>
      <div class="st-input-wrap">
        <input id="st-sta-pass" class="st-input" type="password"
               placeholder="password della rete WiFi" maxlength="63" autocomplete="off">
        <button class="st-eye" onclick="toggleEye('st-sta-pass',this)">&#128065;</button>
      </div>
    </div>
  </div>

  <!-- LiTime BMS -->
  <div class="st-group">
    <div class="st-group-title">&#128267; LiTime BMS</div>
    <div class="st-field">
      <label class="st-label" for="st-bms-mac">MAC Address</label>
      <div class="st-input-wrap">
        <input id="st-bms-mac" class="st-input" type="text"
               placeholder="AA:BB:CC:DD:EE:FF" maxlength="17"
               oninput="validateMac(this,'st-bms-mac-hint')" autocomplete="off">
      </div>
      <div id="st-bms-mac-hint" class="st-hint">Formato: AA:BB:CC:DD:EE:FF</div>
    </div>
  </div>

  <!-- SmartSolar -->
  <div class="st-group">
    <div class="st-group-title">&#9728; Victron MPPT</div>
    <div class="st-field">
      <label class="st-label" for="st-solar-key">Advertising Key</label>
      <div class="st-input-wrap">
        <input id="st-solar-key" class="st-input" type="password"
               placeholder="32 caratteri hex" maxlength="32"
               oninput="validateKey(this,'st-solar-key-hint')" autocomplete="off">
        <button class="st-eye" onclick="toggleEye('st-solar-key',this)">&#128065;</button>
      </div>
      <div id="st-solar-key-hint" class="st-hint">VictronConnect &#8594; &#8942; &#8594; Product info &#8594; Advertising key</div>
    </div>
  </div>

  <!-- Orion XS -->
  <div class="st-group">
    <div class="st-group-title">&#128665; Victron DC-DC</div>
    <div class="st-field">
      <label class="st-label" for="st-orion-key">Advertising Key</label>
      <div class="st-input-wrap">
        <input id="st-orion-key" class="st-input" type="password"
               placeholder="32 caratteri hex" maxlength="32"
               oninput="validateKey(this,'st-orion-key-hint')" autocomplete="off">
        <button class="st-eye" onclick="toggleEye('st-orion-key',this)">&#128065;</button>
      </div>
      <div id="st-orion-key-hint" class="st-hint">VictronConnect &#8594; &#8942; &#8594; Product info &#8594; Advertising key</div>
    </div>
  </div>

  <!-- NTP / Orario -->
  <div class="st-group">
    <div class="st-group-title">&#128336; NTP / Orario</div>
    <div class="st-field">
      <label class="st-label" for="st-ntp-srv">Server NTP</label>
      <div class="st-input-wrap">
        <input id="st-ntp-srv" class="st-input" type="text"
               placeholder="pool.ntp.org" maxlength="64" autocomplete="off">
      </div>
    </div>
    <div class="st-field">
      <label class="st-label" for="st-ntp-tz">Timezone (POSIX)</label>
      <div class="st-input-wrap">
        <input id="st-ntp-tz" class="st-input" type="text"
               placeholder="CET-1CEST,M3.5.0,M10.5.0/3" maxlength="64" autocomplete="off">
      </div>
      <div class="st-hint">Italia: CET-1CEST,M3.5.0,M10.5.0/3 &nbsp;&#183;&nbsp; UTC: UTC0</div>
    </div>
  </div>

  <!-- Witmotion IMU -->
  <div class="st-group">
    <div class="st-group-title">&#129517; Witmotion IMU</div>
    <div class="st-field">
      <label class="st-label" for="st-imu-mac">MAC Address</label>
      <div class="st-input-wrap">
        <input id="st-imu-mac" class="st-input" type="text"
               placeholder="A4:C1:38:XX:XX:XX" maxlength="17"
               oninput="validateMac(this,'st-imu-mac-hint')" autocomplete="off">
      </div>
      <div id="st-imu-mac-hint" class="st-hint">Formato: AA:BB:CC:DD:EE:FF &nbsp;&#183;&nbsp; nRF Connect per trovare il MAC</div>
    </div>
  </div>

  <button class="st-save-btn" id="st-save-btn" onclick="saveSettings()">Salva e riavvia</button>
  <div class="st-msg" id="st-msg"></div>

  <!-- Info note -->
  <div class="st-note">
    <strong>Note:</strong><br>
    <strong>WiFi AP</strong> — campi vuoti = mantieni i valori attuali. Dopo il salvataggio riconnettiti alla nuova rete.<br>
    <strong>WiFi Client</strong> — lascia SSID vuoto per disabilitare. Se attivo, l&#8217;ESP32 è raggiungibile sia via AP che via rete locale. L&#8217;NTP si sincronizza solo se il client è connesso.<br>
    <strong>NTP TZ</strong> — usa il formato POSIX, es: <em>CET-1CEST,M3.5.0,M10.5.0/3</em> per l&#8217;Italia.<br>
    <strong>MAC BMS / IMU</strong> — nRF Connect su Android, scansiona e cerca il device (LiTime o Witmotion).<br>
    <strong>Advertising Key</strong> — VictronConnect &#8594; connettiti al device &#8594; &#8942; &#8594; Product info &#8594; Advertising key.
  </div>

</div><!-- /view-settings -->

<!-- ── Detail panel (slides in from right) ──────────────────── -->
<div id="dp">
  <div class="dp-header">
    <button class="dp-back" onclick="closeDetail()">&#8592; Back</button>
    <span class="dp-name" id="dp-name"></span>
    <span class="dp-dot offline" id="dp-dot"></span>
  </div>
  <div class="dp-body" id="dp-body"></div>
</div>

<script>
// ─── helpers ──────────────────────────────────────────────────────────────────
var $ = function(id) { return document.getElementById(id); };

function fmt(v, dec) {
  if (dec === undefined) dec = 1;
  if (v === null || v === undefined || isNaN(v)) return '--';
  return Number(v).toFixed(dec);
}

function setVal(id, v, dec) {
  if (dec === undefined) dec = 1;
  var el = $(id);
  if (!el) return;
  var txt = fmt(v, dec);
  el.textContent = txt;
  el.parentElement.className = 'value ' +
    (txt === '--'  ? 'val-na'  :
     v > 0.05      ? 'val-pos' :
     v < -0.05     ? 'val-neg' : 'val-neu');
}

function setNeutral(id, v, dec) {
  if (dec === undefined) dec = 1;
  var el = $(id);
  if (!el) return;
  el.textContent = fmt(v, dec);
}

function setOnline(dotId, online) {
  var d = $(dotId);
  if (d) { d.className = 'dot ' + (online ? 'online' : 'offline'); }
}

function setOvOnline(dotId, online) {
  var d = $(dotId);
  if (d) { d.className = 'ov-dot ' + (online ? 'online' : 'offline'); }
}

function stateClass(name) {
  var n = (name || '').toLowerCase();
  if (n === 'bulk')       return 'state-bulk';
  if (n === 'absorption') return 'state-absorption';
  if (n === 'float')      return 'state-float';
  if (n === 'fault')      return 'state-fault';
  return 'state-off';
}

function toggleCells() {
  var g = $('cells-grid');
  var t = $('cells-toggle');
  if (g.style.display === 'none') {
    g.style.display = 'grid';
    t.textContent = '▲ Cell voltages';
  } else {
    g.style.display = 'none';
    t.textContent = '▼ Cell voltages';
  }
}

// ─── Tab view ─────────────────────────────────────────────────────────────────
function setView(v) {
  $('view-cards').style.display    = v === 'cards'    ? 'block'  : 'none';
  $('view-overview').style.display = v === 'overview' ? 'flex'   : 'none';
  $('view-level').style.display    = v === 'level'    ? 'flex'   : 'none';
  $('view-settings').style.display = v === 'settings' ? 'block'  : 'none';
  $('btn-cards').className    = 'tab-btn' + (v === 'cards'    ? ' active' : '');
  $('btn-overview').className = 'tab-btn' + (v === 'overview' ? ' active' : '');
  $('btn-level').className    = 'tab-btn' + (v === 'level'    ? ' active' : '');
  $('btn-settings').className = 'tab-btn' + (v === 'settings' ? ' active' : '');
  try { localStorage.setItem('cv', v); } catch(e) {}
  if (v === 'overview') { svgReady = false; setTimeout(drawConnections, 60); }
  if (v === 'settings') { loadSettings(); }
  if (v === 'level')    { initLevelGauges(); if (lastData && lastData.imu) updateLevel(lastData.imu); }
}

// ─── SVG flow connections ──────────────────────────────────────────────────────
var svgReady = false;

function makeLine(svg, cls, id) {
  var bg = document.createElementNS('http://www.w3.org/2000/svg', 'line');
  bg.setAttribute('class', 'fl-bg');
  svg.appendChild(bg);
  var fl = document.createElementNS('http://www.w3.org/2000/svg', 'line');
  fl.setAttribute('class', 'fl-line');
  fl.setAttribute('id', id);
  svg.appendChild(fl);
  return { bg: bg, fl: fl };
}

function setLineCoords(pair, x1, y1, x2, y2) {
  pair.bg.setAttribute('x1', x1); pair.bg.setAttribute('y1', y1);
  pair.bg.setAttribute('x2', x2); pair.bg.setAttribute('y2', y2);
  pair.fl.setAttribute('x1', x1); pair.fl.setAttribute('y1', y1);
  pair.fl.setAttribute('x2', x2); pair.fl.setAttribute('y2', y2);
}

function drawConnections() {
  var svg = $('ov-svg');
  var wrap = $('ov-wrapper');
  if (!svg || !wrap) return;

  var wRect = wrap.getBoundingClientRect();
  var sRect = $('ov-solar').getBoundingClientRect();
  var bRect = $('ov-batt').getBoundingClientRect();
  var oRect = $('ov-orion').getBoundingClientRect();
  var lRect = $('ov-loads').getBoundingClientRect();

  // offset relative to wrapper
  function rel(r) {
    return {
      left:   r.left   - wRect.left,
      right:  r.right  - wRect.left,
      top:    r.top    - wRect.top,
      bottom: r.bottom - wRect.top,
      cx:     (r.left + r.right)  / 2 - wRect.left,
      cy:     (r.top  + r.bottom) / 2 - wRect.top
    };
  }

  var s = rel(sRect);
  var b = rel(bRect);
  var o = rel(oRect);
  var l = rel(lRect);

  svg.innerHTML = '';

  var pSolar = makeLine(svg, 'fl-bg', 'fl-solar');
  setLineCoords(pSolar, s.right, s.cy, b.left, b.cy);

  var pOrion = makeLine(svg, 'fl-bg', 'fl-orion');
  setLineCoords(pOrion, o.left, o.cy, b.right, b.cy);

  var pLoads = makeLine(svg, 'fl-bg', 'fl-loads');
  setLineCoords(pLoads, b.cx, b.bottom, l.cx, l.top);

  svgReady = true;
}

function setFlow(id, dir) {
  var el = $(id);
  if (!el) return;
  if (dir) {
    el.className = 'fl-line on fl-' + dir;
  } else {
    el.className = 'fl-line';
  }
}

// ─── Update cards view ────────────────────────────────────────────────────────
function updateCards(d) {
  var b = d.battery;
  var s = d.solar;
  var o = d.orion;

  setOnline('dot-battery', b.online);
  setNeutral('batt-soc', b.soc, 0);
  $('batt-remain').textContent = fmt(b.remainingAh, 1) + ' Ah';
  setNeutral('batt-volt', b.voltage);
  setVal('batt-curr', b.current);
  setVal('batt-power', b.power, 0);
  setNeutral('batt-soh', b.soh, 0);
  setNeutral('batt-ctemp', b.cellTemp, 0);
  setNeutral('batt-ftemp', b.mosfetTemp, 0);

  var soc = b.soc || 0;
  var bar = $('batt-bar');
  bar.style.width = Math.min(100, soc) + '%';
  bar.className = 'bar-fill ' + (soc > 30 ? 'bar-green' : soc > 15 ? 'bar-yellow' : 'bar-red');

  var cg = $('cells-grid');
  if (b.cells && b.cells.length) {
    cg.innerHTML = b.cells.map(function(v, i) {
      return '<div class="cell-v">C' + (i+1) + '<br>' + v.toFixed(3) + '</div>';
    }).join('');
  }

  setOnline('dot-solar', s.online);
  var sst = $('solar-state');
  sst.textContent = s.state || '--';
  sst.className   = 'state-badge ' + stateClass(s.state || '');
  setVal('solar-power', s.solarPower, 0);
  setVal('solar-curr', s.chargeCurrent);
  setNeutral('solar-bvolt', s.battVoltage);
  setNeutral('solar-yield', s.yieldToday, 0);

  setOnline('dot-orion', o.online);
  var ost = $('orion-state');
  ost.textContent = o.state || '--';
  ost.className   = 'state-badge ' + stateClass(o.state || '');
  setNeutral('orion-outvolt', o.outVoltage);
  setVal('orion-outcurr', o.outCurrent);
  setNeutral('orion-involt', o.inVoltage);
  setNeutral('orion-incurr', o.inCurrent);

  var now = new Date();
  $('last-update').textContent =
    now.getHours().toString().padStart(2,'0') + ':' +
    now.getMinutes().toString().padStart(2,'0') + ':' +
    now.getSeconds().toString().padStart(2,'0');
}

// ─── Update overview view ─────────────────────────────────────────────────────
function updateOverview(d) {
  var b = d.battery;
  var s = d.solar;
  var o = d.orion;

  // Solar box — W headline green if active, A/V/Wh neutral
  setOvOnline('ov-dot-solar', s.online);
  var solarW = s.solarPower || 0;
  $('ov-s-power').textContent = fmt(solarW, 0);
  $('ov-s-power').parentElement.className = 'ov-main ' + (s.online && solarW > 0.5 ? 'val-pos' : 'val-dim');
  $('ov-s-state').textContent  = (s.online && s.state) ? s.state : '--';
  $('ov-s-curr').textContent   = fmt(s.chargeCurrent, 1);
  $('ov-s-volt').textContent   = fmt(s.battVoltage, 1);
  $('ov-s-yield').textContent  = fmt(s.yieldToday, 0);

  // Battery box — SOC uses soc color; A/W use sign color; V neutral
  setOvOnline('ov-dot-batt', b.online);
  if (!b.online) {
    $('ov-b-soc').textContent = '--';
    $('ov-b-soc').parentElement.className = 'ov-main val-dim';
    $('ov-batt-fill').style.height = '0%';
    $('ov-b-state').textContent = '--';
    $('ov-b-state').style.color = 'var(--muted)';
    $('ov-b-curr').textContent = '--'; $('ov-b-curr').className = '';
    $('ov-b-volt').textContent = '--';
    $('ov-b-pow').textContent  = '--'; $('ov-b-pow').className  = '';
  } else {
    var soc = b.soc || 0;
    $('ov-b-soc').textContent = fmt(soc, 0);
    $('ov-b-soc').parentElement.className = 'ov-main ' + (soc > 50 ? 'val-pos' : 'val-neg');
    $('ov-batt-fill').style.height = Math.min(100, soc) + '%';
    var bCurr = b.current || 0;
    var bState, bStateColor;
    if (bCurr > 0.1)       { bState = 'CHARGING';    bStateColor = 'var(--green)'; }
    else if (bCurr < -0.1) { bState = 'DISCHARGING'; bStateColor = 'var(--orange)'; }
    else                   { bState = 'IDLE';         bStateColor = 'var(--muted)'; }
    $('ov-b-state').textContent = bState;
    $('ov-b-state').style.color = bStateColor;
    $('ov-b-curr').textContent  = fmt(bCurr, 1);
    $('ov-b-curr').className    = signCls(bCurr);
    $('ov-b-volt').textContent  = fmt(b.voltage, 1);
    $('ov-b-pow').textContent   = fmt(b.power, 0);
    $('ov-b-pow').className     = signCls(b.power || 0);
  }

  // Orion box — W headline green if active, A/V neutral
  setOvOnline('ov-dot-orion', o.online);
  var orionW = (o.outCurrent || 0) * (o.outVoltage || 0);
  $('ov-o-power').textContent = fmt(orionW, 0);
  $('ov-o-power').parentElement.className = 'ov-main ' + (o.online && orionW > 0.5 ? 'val-pos' : 'val-dim');
  $('ov-o-state').textContent = (o.online && o.state) ? o.state : '--';
  $('ov-o-out').textContent   = fmt(o.outCurrent, 1);
  $('ov-o-volt').textContent  = fmt(o.outVoltage, 1);

  // Loads box
  var battW  = b.power || 0;
  var loadsW = Math.max(0, solarW + orionW - battW);
  var loadsV = b.voltage || 0;
  var loadsA = loadsV > 0 ? loadsW / loadsV : 0;
  $('ov-l-curr').textContent  = fmt(loadsA, 1);
  $('ov-l-power').textContent = fmt(loadsW, 0);

  // Flow animations
  setFlow('fl-solar', (s.online && (s.chargeCurrent || 0) > 0.1) ? 'fwd' : null);
  setFlow('fl-orion', (o.online && (o.outCurrent    || 0) > 0.1) ? 'fwd' : null);
  setFlow('fl-loads', loadsW > 2 ? 'fwd' : null);
}

// ─── Poll ─────────────────────────────────────────────────────────────────────
var lastData = null;

function updateSysInfo(sys) {
  if (!sys) return;
  // Network status block in settings
  var el = $('st-status');
  if (!el) return;
  var rows = '';
  rows += '<div class="st-status-row"><span class="st-status-lbl">AP</span>'
        + '<span class="st-status-val">http://' + (sys.apIp || '192.168.4.1') + '</span></div>';
  if (sys.staIp) {
    rows += '<div class="st-status-row"><span class="st-status-lbl">WiFi Client</span>'
          + '<span class="st-status-val ok">&#10003; ' + sys.staIp + '</span></div>';
  }
  if (sys.time && sys.date) {
    rows += '<div class="st-status-row"><span class="st-status-lbl">Orario NTP</span>'
          + '<span class="st-status-val">' + sys.date + '&nbsp;&nbsp;' + sys.time + '</span></div>';
  } else {
    var ntpMsg = (sys.staIp && sys.staIp.length > 0) ? 'in attesa...' : 'non sincronizzato';
    rows += '<div class="st-status-row"><span class="st-status-lbl">Orario NTP</span>'
          + '<span class="st-status-val dim">' + ntpMsg + '</span></div>';
  }
  el.innerHTML = rows;
  el.className = 'st-status visible';
}

function applyData(d) {
  lastData = d;
  updateCards(d);
  if ($('view-overview').style.display !== 'none') {
    updateOverview(d);
    if (!svgReady) drawConnections();
  }
  if ($('view-level').style.display !== 'none' && d.imu) updateLevel(d.imu);
  if (d.sys) updateSysInfo(d.sys);
}

// ─── Detail panel ─────────────────────────────────────────────────────────────

function showDetail(dev) {
  if (!lastData) return;
  var d = lastData[dev === 'battery' ? 'battery' : dev === 'solar' ? 'solar' : 'orion'];
  var names = { battery: '&#128267; LiTime Battery', solar: '&#9728; Victron MPPT', orion: '&#128665; Victron DC-DC' };
  $('dp-name').innerHTML = names[dev];
  $('dp-dot').className  = 'dp-dot ' + (d.online ? 'online' : 'offline');
  $('dp-body').innerHTML = d.online ? (
    dev === 'battery' ? dpBattery(d) :
    dev === 'solar'   ? dpSolar(d)   : dpOrion(d)
  ) : '<div class="dp-offline"><div class="dp-offline-icon">&#128225;</div><div>Device offline &mdash; no recent data</div></div>';
  $('dp').classList.add('open');
}

function closeDetail() { $('dp').classList.remove('open'); }

// ─── Settings ─────────────────────────────────────────────────────────────────

function validateMac(el, hintId) {
  var ok = /^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$/.test(el.value);
  el.className = 'st-input ' + (el.value.length === 0 ? '' : ok ? 'valid' : 'invalid');
  var h = $(hintId);
  if (h) { h.className = 'st-hint' + (el.value.length === 0 ? '' : ok ? ' ok' : ' err');
           h.textContent = el.value.length === 0 ? 'Formato: AA:BB:CC:DD:EE:FF' : ok ? '✓ Formato corretto' : 'Formato non valido — AA:BB:CC:DD:EE:FF'; }
}

function validateKey(el, hintId) {
  var ok = /^[0-9A-Fa-f]{32}$/.test(el.value);
  el.className = 'st-input ' + (el.value.length === 0 ? '' : ok ? 'valid' : 'invalid');
  var h = $(hintId);
  if (h) { h.className = 'st-hint' + (el.value.length === 0 ? '' : ok ? ' ok' : ' err');
           h.textContent = el.value.length === 0 ? 'VictronConnect → ⋮ → Product info → Advertising key'
                           : ok ? '✓ Chiave valida (32 caratteri hex)' : 'Deve essere esattamente 32 caratteri esadecimali'; }
}

function toggleEye(inputId, btn) {
  var inp = $(inputId);
  if (!inp) return;
  if (inp.type === 'password') { inp.type = 'text';     btn.style.opacity = '1'; }
  else                         { inp.type = 'password'; btn.style.opacity = '0.5'; }
}

function validateWifiPass(el) {
  var ok = el.value.length === 0 || el.value.length >= 8;
  el.className = 'st-input' + (el.value.length === 0 ? '' : ok ? ' valid' : ' invalid');
  var h = $('st-wifi-pass-hint');
  if (h) { h.className = 'st-hint' + (el.value.length === 0 ? '' : ok ? ' ok' : ' err');
           h.textContent = el.value.length === 0 ? 'Minimo 8 caratteri (WPA2)'
                           : ok ? '✓ OK' : 'Minimo 8 caratteri'; }
}

function loadSettings() {
  setMsg('', '');
  fetch('/api/settings')
    .then(function(r) { return r.json(); })
    .then(function(d) {
      $('st-wifi-ssid').value  = d.wifiSsid  || '';
      $('st-wifi-pass').value  = d.wifiPass  || '';
      $('st-sta-ssid').value   = d.staSsid   || '';
      $('st-sta-pass').value   = d.staPass   || '';
      $('st-bms-mac').value    = d.bmsMac    || '';
      $('st-solar-key').value  = d.solarKey  || '';
      $('st-orion-key').value  = d.orionKey  || '';
      $('st-ntp-srv').value    = d.ntpServer || '';
      $('st-ntp-tz').value     = d.ntpTZ     || '';
      $('st-imu-mac').value    = d.imuMac    || '';
    })
    .catch(function() { setMsg('Impossibile caricare le impostazioni attuali', 'err'); });
}

function setMsg(txt, cls) {
  var el = $('st-msg');
  el.textContent = txt;
  el.className   = 'st-msg' + (cls ? ' ' + cls : '');
}

function saveSettings() {
  var ssid    = $('st-wifi-ssid').value.trim();
  var pass    = $('st-wifi-pass').value.trim();
  var staSsid = $('st-sta-ssid').value.trim();
  var staPass = $('st-sta-pass').value.trim();
  var mac     = $('st-bms-mac').value.trim();
  var solar   = $('st-solar-key').value.trim();
  var orion   = $('st-orion-key').value.trim();
  var ntpSrv  = $('st-ntp-srv').value.trim();
  var ntpTZ   = $('st-ntp-tz').value.trim();
  var imuMac  = $('st-imu-mac').value.trim();

  // Validate
  if (ssid    && ssid.length > 32)    { setMsg('SSID AP: massimo 32 caratteri', 'err'); return; }
  if (pass    && pass.length < 8)     { setMsg('Password WiFi AP: minimo 8 caratteri', 'err'); return; }
  if (staSsid && staSsid.length > 32) { setMsg('SSID Client: massimo 32 caratteri', 'err'); return; }
  if (mac     && !/^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$/.test(mac)) {
    setMsg('MAC address non valido', 'err'); return;
  }
  if (solar && !/^[0-9A-Fa-f]{32}$/.test(solar)) {
    setMsg('Chiave SmartSolar non valida (deve essere 32 caratteri hex)', 'err'); return;
  }
  if (orion && !/^[0-9A-Fa-f]{32}$/.test(orion)) {
    setMsg('Chiave Orion XS non valida (deve essere 32 caratteri hex)', 'err'); return;
  }
  if (imuMac && !/^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$/.test(imuMac)) {
    setMsg('MAC IMU non valido', 'err'); return;
  }

  var btn = $('st-save-btn');
  btn.disabled = true;
  setMsg('Salvataggio...', 'info');

  var body = JSON.stringify({
    wifiSsid: ssid, wifiPass: pass,
    staSsid: staSsid, staPass: staPass,
    bmsMac: mac, solarKey: solar, orionKey: orion,
    ntpServer: ntpSrv, ntpTZ: ntpTZ,
    imuMac: imuMac
  });
  fetch('/api/settings', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: body })
    .then(function(r) { return r.json(); })
    .then(function() {
      setMsg('Salvato! Riavvio in corso...', 'ok');
      waitReconnect(12);
    })
    .catch(function() {
      // ESP32 might reboot before sending response — treat as success
      setMsg('Riavvio in corso...', 'ok');
      waitReconnect(12);
    });
}

function waitReconnect(secs) {
  if (secs <= 0) { tryReconnect(); return; }
  setMsg('Riavvio… riconnessione tra ' + secs + 's', 'info');
  setTimeout(function() { waitReconnect(secs - 1); }, 1000);
}

function tryReconnect() {
  setMsg('Riconnessione in corso…', 'info');
  fetch('/api/data')
    .then(function() { location.reload(); })
    .catch(function() { setTimeout(tryReconnect, 2000); });
}

// helpers
function kpi(v, unit, label, cls) {
  return '<div class="dp-kpi"><div class="dp-kv ' + (cls||'val-neu') + '">' + v +
    '<span class="dp-ku">' + unit + '</span></div><div class="dp-kl">' + label + '</div></div>';
}
function sec(title, gridCls, items) {
  return '<div><div class="dp-sec-title">' + title + '</div><div class="dp-kpis ' + gridCls + '">' + items + '</div></div>';
}
function ioRow(lbl, v, unit, cls) {
  return '<div class="dp-io-row"><span class="dp-io-lbl">' + lbl + '</span>' +
    '<span><span class="dp-io-val ' + (cls||'') + '">' + v + '</span><span class="dp-io-unit">' + unit + '</span></span></div>';
}
function signCls(v) { return v > 0.05 ? 'val-pos' : v < -0.05 ? 'val-neg' : 'val-neu'; }

function dpBattery(b) {
  var soc   = b.soc || 0;
  var curr  = b.current || 0;
  var state = curr > 0.1 ? 'Charging' : curr < -0.1 ? 'Discharging' : 'Idle';
  var barCls = soc > 60 ? 'bar-green' : soc > 30 ? 'bar-yellow' : 'bar-red';
  var html = '<div class="dp-hero">'
    + '<div class="dp-hero-val ' + (soc > 30 ? 'val-pos' : 'val-neg') + '">' + fmt(soc,0) + '<span class="dp-hero-unit">%</span></div>'
    + '<div class="dp-soc-bar"><div class="dp-soc-fill ' + barCls + '" style="width:' + Math.min(100,soc) + '%"></div></div>'
    + '<div class="dp-hero-sub">' + state + ' &nbsp;&middot;&nbsp; ' + fmt(b.remainingAh,1) + ' / ' + fmt(b.fullCapacityAh,1) + ' Ah</div>'
    + '</div>';
  html += sec('Energia', 'c3',
    kpi(fmt(b.voltage,2),  'V', 'Voltage',  'val-neu') +
    kpi(fmt(curr,2),       'A', 'Current',  signCls(curr)) +
    kpi(fmt(b.power,0),    'W', 'Power',    signCls(b.power||0)));
  html += sec('Stato', 'c2',
    kpi(fmt(soc,0),   '%', 'SOC', soc > 30 ? 'val-pos' : 'val-neg') +
    kpi(fmt(b.soh,0), '%', 'SOH', 'val-neu'));
  html += sec('Capacit&#224;', 'c2',
    kpi(fmt(b.remainingAh,1),    'Ah', 'Remaining', 'val-neu') +
    kpi(fmt(b.fullCapacityAh,1), 'Ah', 'Full Cap.', 'val-neu'));
  html += sec('Temperature', 'c2',
    kpi(fmt(b.cellTemp,0),   '&#176;C', 'Cell', 'val-neu') +
    kpi(fmt(b.mosfetTemp,0), '&#176;C', 'FET',  'val-neu'));
  if (b.cells && b.cells.length) {
    var mn = Math.min.apply(null, b.cells), mx = Math.max.apply(null, b.cells);
    var delta = Math.round((mx - mn) * 1000);
    var cells = b.cells.map(function(v, i) {
      var extra = Math.abs(v-mn)<0.0005 ? ' cmin' : Math.abs(v-mx)<0.0005 ? ' cmax' : '';
      return '<div class="dp-cell' + extra + '"><div class="dp-cidx">C'+(i+1)+'</div><div class="dp-cval">'+Number(v).toFixed(3)+'</div></div>';
    }).join('');
    html += '<div><div class="dp-sec-title">Celle &nbsp;<span style="font-weight:400;font-size:0.65rem">&#916; ' + delta + ' mV</span></div>'
          + '<div class="dp-cells">' + cells + '</div></div>';
  }
  return html;
}

function dpSolar(s) {
  var html = '<div class="dp-hero">'
    + '<div class="dp-hero-val val-pos">' + fmt(s.solarPower,0) + '<span class="dp-hero-unit">W</span></div>'
    + '<div class="dp-hero-sub"><span class="state-badge ' + stateClass(s.state) + '">' + (s.state||'--') + '</span></div>'
    + '</div>';
  html += sec('Produzione', 'c2',
    kpi(fmt(s.solarPower,0),    'W', 'Solar Power',    'val-pos') +
    kpi(fmt(s.chargeCurrent,2), 'A', 'Charge Current', 'val-pos'));
  var hasBv = s.battVoltage !== null && s.battVoltage !== undefined;
  var hasLc = s.loadCurrent !== null && s.loadCurrent !== undefined && !isNaN(s.loadCurrent);
  var battItems = kpi(fmt(s.battVoltage,2), 'V', 'Battery Voltage', 'val-neu');
  if (hasLc) battItems += kpi(fmt(s.loadCurrent,2), 'A', 'Load Current', 'val-neu');
  html += sec('Batteria', hasLc ? 'c2' : 'c1', battItems);
  html += sec('Oggi', 'c1', kpi(fmt(s.yieldToday,0), 'Wh', 'Yield Today', 'val-neu'));
  return html;
}

function dpOrion(o) {
  var inW  = (o.inVoltage  || 0) * (o.inCurrent  || 0);
  var outW = (o.outVoltage || 0) * (o.outCurrent || 0);
  var html = '<div class="dp-hero">'
    + '<div class="dp-hero-val val-pos">' + fmt(outW,0) + '<span class="dp-hero-unit">W</span></div>'
    + '<div class="dp-hero-sub"><span class="state-badge ' + stateClass(o.state) + '">' + (o.state||'--') + '</span></div>'
    + '</div>';
  html += '<div><div class="dp-sec-title">Input / Output</div>'
    + '<div class="dp-inout">'
    + '<div class="dp-io-col"><div class="dp-io-head">&#8592; Input</div>'
    + ioRow('Voltage', fmt(o.inVoltage,2),  'V', 'val-neu') + ioRow('Current', fmt(o.inCurrent,2),  'A', 'val-neu') + ioRow('Power', fmt(inW,0),  'W', 'val-neu')
    + '</div>'
    + '<div class="dp-io-col"><div class="dp-io-head">Output &#8594;</div>'
    + ioRow('Voltage', fmt(o.outVoltage,2), 'V', 'val-neu') + ioRow('Current', fmt(o.outCurrent,2), 'A', 'val-pos') + ioRow('Power', fmt(outW,0), 'W', 'val-pos')
    + '</div>'
    + '</div></div>';
  if (inW > 1) {
    var eff = Math.min(100, outW / inW * 100);
    html += sec('Efficienza', 'c1', kpi(fmt(eff,1), '%', 'Conversion efficiency', eff > 85 ? 'val-pos' : eff > 70 ? 'val-neu' : 'val-neg'));
  }
  return html;
}

// ─── Level gauges ─────────────────────────────────────────────────────────────

var lvBuilt = false;
var LV_B64_SIDE = "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";
var LV_B64_REAR = "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";

function lvAngleColor(deg) {
  var a = Math.abs(deg);
  return a < 2 ? '#22c55e' : a < 5 ? '#f59e0b' : '#ef4444';
}

function lvStatusInfo(angle, isSide) {
  if (Math.abs(angle) < 0.3) return {text: 'LIVELLATO', color: '#22c55e'};
  var col = lvAngleColor(angle);
  if (isSide) {
    return {text: angle > 0 ? 'REAR LOW' : 'FRONT LOW', color: col};
  } else {
    return {text: angle > 0 ? 'DRIVER SIDE LOW' : 'PASSENGER SIDE LOW', color: col};
  }
}

function lvBuildGauge(svgId, isSide) {
  var svg = $(svgId);
  if (!svg) return;
  var cx = 160, cy = 270, R = 200;
  var h = '';
  for (var deg = -35; deg <= 35; deg++) {
    if (deg % 2 !== 0) continue;
    var isMaj = (deg % 10 === 0), isMed = (deg % 5 === 0 && !isMaj);
    var th = isMaj ? 18 : isMed ? 12 : 7;
    var sw = isMaj ? 2.5 : isMed ? 1.8 : 1.3;
    var col = lvAngleColor(deg);
    var rad = deg * Math.PI / 180;
    var sn = Math.sin(rad), cs = Math.cos(rad);
    var x1 = (cx + R * sn).toFixed(1), y1 = (cy - R * cs).toFixed(1);
    var x2 = (cx + (R + th) * sn).toFixed(1), y2 = (cy - (R + th) * cs).toFixed(1);
    h += '<line x1="' + x1 + '" y1="' + y1 + '" x2="' + x2 + '" y2="' + y2 + '" stroke="' + col + '" stroke-width="' + sw + '" stroke-linecap="round"/>';
  }
  var py = cy - R;
  h += '<g id="' + svgId + '-ptr" transform="rotate(0 ' + cx + ' ' + cy + ')">'
     + '<polygon id="' + svgId + '-ptrcol" points="' + cx + ',' + (py+1) + ' ' + (cx-7) + ',' + (py+14) + ' ' + (cx+7) + ',' + (py+14) + '" fill="#e8eaf0"/>'
     + '</g>';
  h += '<text id="' + svgId + '-ang" x="' + cx + '" y="140"';
  h += ' text-anchor="middle" font-size="32" font-weight="800" fill="#e8eaf0"';
  h += ' font-family="system-ui,-apple-system,sans-serif">--</text>';
  if (isSide) {
    h += '<g id="'+svgId+'-img" transform="rotate(0 160 184)">'
       + '<image href="data:image/png;base64,'+LV_B64_SIDE+'" x="75" y="146" width="170" height="77"/>'
       + '</g>';
  } else {
    h += '<g id="'+svgId+'-img" transform="rotate(0 160 192)">'
       + '<image href="data:image/png;base64,'+LV_B64_REAR+'" x="100" y="143" width="120" height="99"/>'
       + '</g>';
  }
  h += '<text id="' + svgId + '-lbl" x="' + cx + '" y="266"';
  h += ' text-anchor="middle" font-size="11" font-weight="700" letter-spacing="1.5" fill="#6b7280"';
  h += ' font-family="system-ui,-apple-system,sans-serif">--</text>';
  svg.innerHTML = h;
}

function initLevelGauges() {
  if (lvBuilt) return;
  lvBuildGauge('lv-svg-pitch', true);
  lvBuildGauge('lv-svg-roll',  false);
  lvBuilt = true;
}

function updateLevelGauge(svgId, angle, isSide) {
  var ptr    = $(svgId + '-ptr');
  var ptrcol = $(svgId + '-ptrcol');
  var ang    = $(svgId + '-ang');
  var lbl    = $(svgId + '-lbl');
  var col    = lvAngleColor(angle);
  var clamp  = Math.max(-35, Math.min(35, angle));
  if (ptr)    ptr.setAttribute('transform', 'rotate(' + clamp.toFixed(1) + ' 160 270)');
  if (ptrcol) ptrcol.setAttribute('fill', col);
  if (ang) {
    ang.setAttribute('fill', col);
    ang.textContent = (angle >= 0 ? '+' : '') + angle.toFixed(1) + '\u00b0';
  }
  if (lbl) {
    var info = lvStatusInfo(angle, isSide);
    lbl.setAttribute('fill', info.color);
    lbl.textContent = info.text;
  }
  var img = $(svgId + '-img');
  if (img) {
    var imgCy = isSide ? 180 : 192;
    var imgAngle = -clamp;
    img.setAttribute('transform', 'rotate(' + imgAngle.toFixed(1) + ' 160 ' + imgCy + ')');
  }
}

function updateLevel(imu) {
  if (!lvBuilt) initLevelGauges();
  var offline = $('lv-offline'), grid = $('lv-grid');
  var online = imu && imu.online;
  if (offline) offline.style.display = online ? 'none' : 'block';
  if (grid)    grid.style.opacity    = online ? '1' : '0.3';
  if (!online) return;
  updateLevelGauge('lv-svg-pitch', imu.pitch || 0, true);
  updateLevelGauge('lv-svg-roll',  imu.roll  || 0, false);
}

function poll() {
  fetch('/api/data')
    .then(function(r) { return r.json(); })
    .then(applyData)
    .catch(function() {});
}

// Resize: redraw SVG if overview visible
window.addEventListener('resize', function() {
  if ($('view-overview').style.display !== 'none') {
    svgReady = false;
    drawConnections();
  }
});

// Init: restore last view + wire up detail panel clicks
try { setView(localStorage.getItem('cv') || 'cards'); } catch(e) { setView('cards'); }
$('ov-solar').onclick = function() { showDetail('solar');   };
$('ov-batt').onclick  = function() { showDetail('battery'); };
$('ov-orion').onclick = function() { showDetail('orion');   };

poll();
setInterval(poll, 2000);
</script>
</body>
</html>
)rawliteral";

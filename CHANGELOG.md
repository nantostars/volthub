# Changelog — volthub

All notable changes to the firmware. Format inspired by
[Keep a Changelog](https://keepachangelog.com/). **Change-based versioning**:
`FW_VERSION` (`src/Version.h`) is bumped on **every commit** and a matching entry is
added here. Scheme `0.N` (N = change number) until the `1.0` release.

The current version is shown on the device **System** screen and in the **System** tab
of the web dashboard.

---

## 0.71 — CSV data log to internal flash (off by default)
- Logs one row every **2 minutes** to `/volthub_YYYYMMDD.csv`: datetime, battery SOC/V/A/temp and
  charge state, solar battery-side V/A and state, DC-DC alternator-side and service-side V/A and
  state. Statuses are numeric codes (battery 1/0/-1; solar and DC-DC use the VE.Direct CS codes).
  PV voltage/current are absent because the Victron advertisement does not carry them, and the
  power columns are omitted as they are just V×A.
- **LittleFS on the existing `spiffs` partition** (128 KB): no repartitioning, no USB flash — the
  library mounts that label by default and formats it on first use. LittleFS rather than SPIFFS on
  purpose: SPIFFS garbage collection on a nearly-full filesystem (the normal state of a rotating
  log) can stall the device for seconds.
- Rows are buffered in RAM and flushed every 5 samples → **one flash write every 10 minutes**
  instead of 720 a day. ~85 B/row ≈ 60 KB/day against ~110 KB usable, so about 1.8 days fit.
  Retention: one file per day, capped at 7, plus a free-space rule that prunes the oldest below
  15 KB — the space rule is the one that actually fires here, and it keeps working when the clock
  is wrong (date-based names alone would not).
- **Clock**: rows and filenames need a date the ESP32 cannot keep (no RTC, often no internet), so
  logging idles in "waiting for clock". The dashboard now pushes the phone's time to
  `POST /api/time` on load — works offline, and is ignored once NTP has synced.
- Web System tab: live ON/OFF toggle, state, rows, free space, and the file list with download and
  delete. The toggle uses its own `/api/logs` endpoint because every `/api/settings` POST reboots
  the device, which would be absurd for a log switch. Buffered rows are flushed before any reboot.
- Impact: writes are ~450 B every 10 min (page write <1 ms, occasional 20-40 ms sector erase), so
  no perceptible effect on BLE, web or display; wear works out to ~180 erase cycles a year against
  ~100k rated. Firmware grows ~64 KB (62% of the app slot).

## 0.70 — Solar/DC-DC: show everything the telemetry already carries
- Four fields were being decoded and thrown away. The DC-DC **off reason** and the **charger error**
  were not even reaching the API; the DC-DC **input current** and the solar **load output** were on
  `/api/data` but displayed nowhere.
- **DC-DC** detail (device + web) gains a real "Input & efficiency" card — input current, input power
  (V×A) and efficiency (W out / W in, only above 5 W input where it is meaningful) — plus a Status
  line with the **off reason**: why the charger is not charging (No input power · Engine off ·
  Analysing input · Protection active · BMS · Remote input · Switched off …). This is the card that
  0.69 emptied, now filled with measured data instead of mockup values.
- **Solar** detail gains the load output (hidden on models without a load terminal, rather than a
  permanent "--") and the error row.
- Error and off-reason names come from the **official** VE.Direct Protocol 3.33 tables (fields ERR
  and OR, the latter a bitmask — the raw mask is published as `orion.offMask`). Both stay silent
  while the device is fine, so the new rows add no noise. Dual-language (35 new strings).
- `offReason` had never been read before, so `parseOrion()` logs it on change: a wrong byte offset
  would show up immediately as nonsense flags on real hardware.

## 0.69 — DC-DC: remove the fake "Charge profile" values
- The DC-DC detail screen and web tab showed a "Charge profile" card with **hardcoded** values
  (Current limit 50 A · Input range 9-17 V · Mode Adaptive · Engine detect Auto) left over from the
  design mockup: no id, no JS, never read from the charger — they would have kept reading 50 A after
  changing the real limit. Removed from both surfaces rather than shown as if measured; the device
  card now fills the content area.
- Why they cannot be shown for real: current limits, input lock-out thresholds and the
  absorption/float/storage voltages are **configuration parameters**. The Victron instant-readout
  advertisement carries live telemetry only — state, error, in/out voltage and current, off reason
  (16-byte record, we already decode bytes 0-13). Those settings are reachable only through
  VictronConnect's proprietary GATT protocol or a wired VE.Direct link (HEX protocol).
- Dropped the now-orphaned i18n entries (Charge profile, Current limit, Input range, Mode,
  Engine detect, Adaptive).

## 0.68 — IMU: evaluate the MAC check once, not 10x/second
- 0.67 added a placeholder-MAC guard to `WitmotionIMU::update()`, which `imuTask` calls every
  100 ms — so with no IMU connected it built a temporary `String` ten times a second, forever.
  Not a leak (it is freed immediately), but ~864k needless allocations a day is exactly the kind
  of heap churn that fragments an ESP32 heap on a device meant to run for days. The check now runs
  **once** in `begin()` and is cached in `_macOk`; the boot log says when the IMU is disabled
  because the MAC is still the placeholder.
- Audited 0.67 for regressions on the 0.58 leak fix: `VictronBLE.cpp` is untouched, so
  `setMaxResults(0)` is intact, and all five exit paths of the IMU connect clear `_connecting`
  (a stuck flag would have left the scan watchdog permanently disabled).

## 0.67 — BLE: stop GATT reconnect attempts from blinding the Victron scan
- The Victron devices are **not connected** — they are decoded from passive advertisements — so what
  looked like them "disconnecting and reconnecting" was the `online` flag flapping because the scan
  was repeatedly stopped. Every GATT connect attempt stops the scan (NimBLE does it on
  `BLE_HS_EBUSY`, `NimBLEClient.cpp`), and the scan was only restarted by a watchdog ticking every 3s.
- Worst case, and the likely trigger: the IMU MAC defaults to the **placeholder** `AA:BB:CC:DD:EE:FF`,
  so with no tilt sensor configured (or one that is off/out of range) the IMU retried every 30s with
  **two** connect attempts of 5s each → up to ~13s with no scanning, against a 10s staleness window.
- Fixes: skip the IMU connect entirely when the MAC is unset/placeholder; exponential backoff on
  consecutive failures for both IMU and BMS (30s → 60s → … capped at 5 min); only probe the
  alternate BLE address type on the first attempts; connect timeout 5s → 3s; scan watchdog now polls
  every 1s (but never restarts the scan while a connect is in flight, which would fight the
  controller for the radio).
- Victron staleness moved to its own `VICTRON_STALE_MS` = 30s: an advertisement-based device must not
  be declared offline just because we briefly were not listening. `DEVICE_STALE_MS` (10s) still
  applies to the 2s-polled BMS and the IMU.

## 0.66 — AP pill: show the mode (AUTO / ON / OFF) + fix a pill vanishing on tap
- The AP pill showed only whether the AP was up, but a tap changes the *mode*, not the state — so
  tapping while the AP was already on produced no visible change and looked broken. It now shows
  the mode: **AUTO** (up, the automation may drop it) · **ON** (up and pinned, or the auto-off
  option is not in play) · **OFF** (down, a tap brings it back). Every tap changes the text; the
  colour still answers "can I reach it right now?". With the option disabled the pill reads a
  permanent ON rather than a misleading AUTO. The mode is computed in `main.cpp` (which owns the
  WiFi state) and passed as `DisplayUI::ApMode`, so the UI does not duplicate the logic.
- Fixed the AP pill disappearing after tapping the Screen toggle: `drawSystem()`'s card fill erases
  **both** pills, and the AP pill text had not changed, so `pillCached()` skipped its redraw and it
  stayed blank until the next screen change. `drawSystem()` now drops the cached text of the pills
  it erases — it is called directly on tap, i.e. outside `selectScreen()`'s cache reset.
- `selectScreen()` still cleared only the first 5 pill slots after the array grew to 6 in 0.65, so
  the new AP pill was never invalidated on screen change. Now sized from the array itself.
- Documented the whole AP auto-off feature in `README.md` (modes table, escape hatches, timings and
  the known limitation) and added `apOn`/`apAuto` to the documented `sys` API fields.

## 0.65 — WiFi: optionally drop the AP while the client is connected
- New setting (web **System → WiFi Client**, NVS `ap_off_sta`, **default OFF**): turn the softAP off
  while the STA is connected, so the phone stops auto-joining `CamperEnergy` and losing internet.
- The device **cannot** detect whether your phone can actually reach it over the client network
  (client isolation and captive portals are invisible from the ESP32 side), so the safety net is
  not network logic but two physical escape hatches:
  1. an **AP pill on the device System screen** — a tap forces the AP back on (it can only force it
     ON or release the override, never switch it off, so a tap can't lock you out);
  2. the AP is **unconditionally on for 10 min after every boot**, so a power cycle always gets you
     back in, even if the touch panel is unresponsive.
- Automatic behaviour: the AP drops only after the STA has been up **2 min** continuously, and comes
  back after the STA has been down **60 s** (not immediately, so it doesn't flap on a shaky network),
  restarting the 10 min grace window. Never applies when no client network is configured.
- `sys.apOn` / `sys.apAuto` on `/api/data`; the web System status row shows the AP as its URL or
  "off (client connected)". Saving with the option on but no client SSID is rejected. Dual-language.

## 0.64 — docs: bring README and CLAUDE.md up to date
- `README.md`: features now cover what the firmware actually does — runtime estimate, the real
  Victron charge state on both MPPT and DC-DC, the "Keep screen on" toggle, the BMS-based
  definition of *Loads*, and long-run stability. Added a `GET /api/data` field table (per-source
  blocks and their keys) so the JSON can be consumed by a logger / Home Assistant.
- `CLAUDE.md`: documented the long-run findings that were only in the changelog — the mandatory
  `setMaxResults(0)` on the Victron scan (without it NimBLE stores every MAC ever seen and the
  web server dies after hours), chained polling instead of `setInterval`, `no-store` on the root
  page, AP-only fallback on STA failure, and the heap diagnostics with their healthy baseline.

## 0.63 — Battery: runtime estimate (time to empty / time to full)
- The BMS does not transmit a time-to-empty, but it does report a coulomb-counted `remainingAh`,
  so the estimate is derived on the device: `remainingAh / |I|` while discharging, and
  `(fullAh - remainingAh) / I` while charging (shown in green as "To full"). At rest (|I| below
  0.3 A) it shows `--`, since the value would be infinite. Clamped to 99 h.
- The raw current swings with every compressor/inverter start, which would make the number jump
  between hours and days, so the estimate uses a smoothed current: a new `BmsData.avgCurrent`
  (EMA, ~90 s time constant over the ~2 s poll), reset after a reconnect gap so a stale average
  cannot leak in.
- Shown on the device (Overview: third line under the ring · Battery: new "Runtime" stat in the
  free third column) and on the web (Overview battery block · Battery tab, full-width row).
  One shared implementation (`bmsEtaMinutes()`/`bmsFmtEta()` in `LitimeBMS.h`); the web reads the
  already-computed `battery.etaMin` / `battery.etaFull` from `/api/data`. Dual-language
  (Runtime → Autonomia, To full → A pieno).

## 0.62 — Battery ring: 100% no longer overflows onto the ring
- At 100% the third digit made the `SOC %` text wider than the ring's usable inner circle, so it
  clipped/overlapped the stroke. Affected all four rings: device Overview (88 px of text vs ~81 px
  usable), device Battery (~48 vs 42), web Battery tab (~83 vs 78) and — marginally — web Overview
  (~106 vs 109).
- The number now drops one size step for **3-digit values only** (0..99 keep the large size):
  device Overview font6→font4, device Battery font2→font1, web via a shared `setSoc()` helper
  (52→42 px overview, 40→32 px battery tab, `%` scaled to match).

## 0.61 — DC-DC: show the real charge state + complete the official state table
- The DC-DC detail tab showed a state *guessed* from the output current (`Charging` if
  `outCurrent > 0.1`, else `Standby`), even though the real state is decoded from the advert
  (`OrionData.deviceState`) and already exposed on `/api/data` as `orion.state`. It now shows the
  actual mode (BULK / ABSORPTION / FLOAT / STORAGE / …), symmetric with Solar. Device + web.
- `victronStateName()` completed from the **official** source (VE.Direct Protocol 3.33, field CS):
  added the missing codes 9 Inverting, 11 Power supply, 246 Repeated absorption, 248 BatterySafe —
  note 11/246/248 (and 6 Storage) are *charger* states, i.e. exactly what the Orion XS can report
  and what would previously have shown as UNKNOWN. Corrected two names to the official wording:
  1 = LOW POWER (was STANDBY) and 245 = STARTING UP (was WAKE-UP). Long names are shortened to fit
  the 96px device pill (246 → "REPEAT. ABS.").
- Removed the now-unused `Standby` entry from the web i18n dictionary.

## 0.60 — web: "Keep screen on" toggle (stop the phone from sleeping)
- New toggle in the System tab that prevents the phone screen from locking while the dashboard is
  open. The Screen Wake Lock API needs a secure context (HTTPS), but the device serves plain HTTP,
  so it is unavailable; instead a muted, inline, looping 128×128 H.264 clip (~1.6 KB, embedded as a
  data URI) is kept "playing" — a playing video keeps the screen awake, over HTTP on any browser.
- Modern Chromium (incl. Android WebView / DuckDuckGo) ignores hidden/tiny videos for this
  heuristic, so the video element is stretched full-viewport on top at ~2% opacity and
  non-interactive: genuinely "visible" to the browser yet imperceptible. Playback is re-asserted on
  pause and on tab return (`visibilitychange`), with a re-seek to keep the short clip looping.
- Default OFF; the choice is a per-browser preference in `localStorage` — nothing is stored on the
  device (no NVS, no `/api/settings`, no firmware-side state). Dual-language.
- Removed a duplicate `poll()` call at startup that was launching two polling loops.

## 0.59 — Overview: Loads = battery discharge only (from the BMS)
- Loads was a derived energy balance (`solar + dcdc - batteryPower`). With solar producing and no
  real load it read the solar power (the charge going into the battery), which is misleading —
  Loads should never show a value that is *charging* the battery.
- Loads is now the battery **discharge** straight from the BMS: `max(0, -batteryPower)` (and
  `max(0, -batteryCurrent)` for A). It shows what the loads pull *from the battery* (net of what
  solar/DC-DC already cover), is 0 while charging or idle, and the Battery→Loads flow line is active
  only when the battery actually discharges. Device + web.

## 0.58 — BLE: stop the scan-results heap leak (web unreachable after hours)
- The Victron passive scan left NimBLE's default result storage on, so a `NimBLEAdvertisedDevice`
  was kept for every unique MAC ever seen and never freed. On a multi-hour drive that is thousands
  of foreign BLE devices (phones, cars, beacons), leaking heap (~160 KB → ~40 KB in 10 h) until the
  synchronous web server could no longer allocate connections and became unreachable (~24 h). We
  only read data in the `onResult` callback and never call `getResults()`, so the scan now runs in
  callback-only mode (`setMaxResults(0)`), freeing each entry after its callback — heap stays flat.
- Complements 0.57 (chained polling): 0.57 removed the request pile-up, this removes the underlying
  leak that was the real cause of the long-run failure.

## 0.57 — web: fix polling that wedged the web server after a while
- The dashboard polled `/api/data` with `setInterval(2s)`, which fires a new request even if the
  previous one hasn't returned. When the ESP32 (single synchronous WebServer, busy with display +
  BLE + WiFi) answered slowly, requests overlapped and TCP connections piled up until the web
  server stopped responding (device otherwise fine; recovered only by a reboot; seen within ~1h).
  Polling is now chained (next request only after the previous settles) with an 8s abort timeout,
  so at most one request is in flight.
- Added a heap monitor: `sys.heap` in `/api/data`, a "Free RAM" row in the web System tab, and a
  60s `[heap]` serial log, to confirm memory stays flat.

## 0.56 — web: warn when enabling OTA without credentials
- Enabling OTA now blocks the save with an explicit message if the username is empty, or the
  password is empty and none is stored yet (instead of silently saving in an inactive OFF state).
  If a password is already stored, leaving the field empty keeps it. Dual-language.

## 0.55 — web: force no-store on the dashboard (avoid stale cached page)
- After a firmware update a browser could serve a cached older dashboard (e.g. an already-open
  tab from a previous flash), missing new fields/logic (seen as "OTA won't save" from a desktop
  browser while it worked from a fresh phone). The root page is now sent with
  `Cache-Control: no-store, no-cache, must-revalidate` + `Pragma`/`Expires`.

## 0.54 — web i18n: translate remaining Italian labels/states/messages
- The System status (NTP time / waiting / not synced), the Solar "no hourly history" note, the
  Configuration form labels/placeholders/hints, and the save/validation messages were hardcoded
  in Italian and stayed Italian in English mode. Now routed through the i18n dictionary
  (`data-i18n`, new `data-i18n-ph` for placeholders, `TR()` for dynamic strings).

## 0.53 — WiFi: stable AP when the STA (client) fails
- On a failed STA join the code left `WIFI_AP_STA` with auto-reconnect on, so the driver kept
  retrying in the background and each attempt disrupted the shared-radio softAP (AP "comes and
  goes", and the served page loaded truncated → missing tab bar). Now it stops the STA and drops
  to a stable AP-only mode until the next reboot.

## 0.52 — OTA: runtime credentials, disabled by default, configured from web
- Removed the hardcoded OTA credentials from `Config.h` (no secrets in the repo).
- OTA is **disabled by default**; enable it and set username/password from the web
  **System → Configuration** form (stored in NVS). `/update` returns 403 unless enabled
  with both credentials set, then requires HTTP Basic Auth with them.
- OTA status (ON/OFF) shown in System on the device and web; the web upload link appears
  only when OTA is active. Password is never echoed by the API. Dual-language.

## 0.51 — Level: fix flicker of the offline "--" values (CYD)
- The redraw guard only returned when the sensor was online, so with no IMU the "--" values redrew every cycle (~80ms) → flicker. Guard now also skips when staying offline; a forced redraw on screen entry keeps the first paint.

## 0.50 — CYD: fix flicker on Solar Wh and Level values/bubble
- Solar and DC-DC value updates now have a change-guard (redraw only when a shown value changes). The CYD draws directly to the screen, so repeated redraws of the free-font values (padding erase) flickered; the Guition composites in a canvas so it was invisible there.
- Level: value/bubble deadband raised to 0.3° so IMU noise no longer triggers constant redraws.

## 0.49 — naming: "volthub" (no dot) in code and docs
- Project name is written **volthub** in code/comments/docs and the browser tab title. The middle-dot "volt·hub" is kept only as the rendered logo (device wordmark, web topbar).

## 0.48 — chore: gitignore *.orig
- Stop the build-generated `src/idf_component.yml.orig` from showing up as untracked.

## 0.47 — CYD: pin platform to arduino v2 (fix boot loop)
- `[env:cyd]` `platform` was unpinned and resolved to espressif32 v3 (arduino-esp32 3.x / IDF5), where NimBLE 1.4.2 crashes at BT controller init → boot loop. Pinned to `espressif32@6.6.0` (v2), same as the Guition env.

## 0.46 — Overview: fix flow animation direction
- All three flow lines animated backwards (phase advanced +2 instead of -2). Reversed so dashes flow in the physical direction: Solar→Battery, DC-DC→Battery, Battery→Loads.

## 0.45 — i18n: abbreviate long Italian device labels
- Italian strings that would overflow the small device boxes are abbreviated (e.g. Balancing→"Bilanciam.", PITCH F-R→"BECC. A-P", ROLL L-R→"ROLL. S-D"). Web keeps full words (responsive).

## 0.44 — dual language (web): translations + language toggle in System
- Web dashboard translatable EN/IT (`TR()` dict + `data-i18n` on labels, tabs by id, dynamic states).
- Language toggle (EN/IT) in the System tab writes `/api/settings` → switches device + web; the page syncs from `/api/data` → `sys.lang`.

## 0.43 — dual language (device): i18n engine + IT translation
- Device UI translatable EN/IT via a `t()` lookup (English key, Italian table, English fallback).
- Language stored in NVS (`Settings::getLang/setLang`), applied at boot and on change (full redraw).
- System screen shows the current language (read-only); language exposed on `/api/data` → `sys.lang` and via `/api/settings`. Web toggle to follow in 0.44.

## 0.42 — public documentation + open-source license
- Added `README.md` (English): overview, hardware, build/flash, configuration, components, license.
- Added `LICENSE` (MIT, © nantostars).
- Translated this changelog to English.

## 0.41 — versioning system + changelog
- Added `FW_VERSION` (`src/Version.h`), shown in System on device and web (via `/api/data` → `sys.fw`).
- Created this `CHANGELOG.md` with the full history from the origins (v0.0).

## 0.40 — Overview: node value colour by state
- Solar/DC-DC green when producing; Loads amber when the battery is discharging. Device + web.

## 0.39 — Overview: centred and aligned values
- SOC % centred in the ring; V/A stacked and centred below the ring; number+W group centred in the box with the number right-aligned to the W (fixed 3-digit slot) on a common baseline. Per-node anti-flicker cache.

## 0.38 — Level: real-time refresh
- The bubble follows the tilt with a dedicated refresh path (~80ms), decoupled from the 300ms value throttle.

## 0.37 — chore: gitignore `src/idf_component.yml`
- Build-generated manifest excluded from tracking.

## 0.36 — Fonts: large values in Helvetica-Bold
- Large values (font4/font6) use FreeSansBold on Guition (Arduino_GFX, baseline-aware) and CYD (TFT_eSPI `setFreeFont`).

## 0.35 — Battery: Delta alarm threshold
- Cell Delta changes colour: white ≤50mV, amber 50–100mV, red >100mV. Device + web.

## 0.34 — Status bar: separate BLE from clock (Guition)
- Narrowed the BLE field so it no longer overlaps the clock.

## 0.33 — docs: stack upgrade assessment
- `docs/UPGRADE-latest-stack.md`: plan/risks for arduino v3 / NimBLE 2.x / GFX / TFT_eSPI.

## 0.32 — Victron: product-id table from the official source
- `victronModelName()` regenerated verbatim from the VE.Direct Protocol PDF appendix (86 MPPT + Orion XS 0xA3F0/0xA3F1). Fixed errors from unofficial sources.

## 0.31 — Victron: neutral Orion fallback
- Unknown Orion pid → "Orion" instead of "Orion-XS".

## 0.30 — Victron: Orion XS product-ids (later corrected in 0.32)
- First Orion XS attempt from web search.

## 0.29 — Victron: SmartSolar/BlueSolar MPPT product-id table
- Exact model from the Product ID in the detail tabs.

## 0.28 — Overview: flow lines detached from the ring
- Battery-side endpoints stop just outside the ring stroke.

## 0.27 — Guition: align "W" to the watt value (Solar/DC-DC)
- Board-specific offset for the GFX fonts.

## 0.26 — Guition: Overview and Battery tweaks
- W alignment, clock shifted, Battery cells value+V on one line.

## 0.25 — Guition: bright display + QSPI 40 MHz + reliable touch
- Init table from the demo BSP (full brightness); QSPI at 40 MHz (removes texture); touch with 11-byte command + retry + timed release.

## 0.24 — Guition: anti-tearing
- Canvas flushed only when the framebuffer changes.

## 0.23 — Removed `src/idf_component.yml`
- Leftover from the arduino v3 experiment.

## 0.22 — Guition: WORKING display on arduino v2
- Software-rotated canvas in PSRAM (native 320×480 portrait panel).

## 0.21 — Guition: AXS15231B driver with vendor init
- Black-screen fix.

## 0.20 — Naming: generic everywhere, dynamic model in detail tabs
- Generic concepts (Solar/DC-DC/Battery) on overview; real model only in the detail tabs.

## 0.19 — Web Overview: centred node boxes + current A
## 0.18 — Web Overview: new layout (sources top, battery centre, loads bottom)
## 0.17 — Web: clear offline ring
## 0.16 — Web: better vertical space + larger values (mobile)
## 0.15 — Overview (device): inactive connections dashed
## 0.14 — Overview (device): ring vertically centred
## 0.13 — Overview (device): ring visible offline + enlarged node boxes
## 0.12 — Overview (device): W unit to font4 + offline ring
## 0.11 — Overview (device): one step larger characters
## 0.10 — Battery (device): title + BMS pill at the top of card A
## 0.9 — Battery (device): layout fix (merged cards / height)
## 0.8 — Battery: coloured cell grid (from Claude design)
## 0.7 — Status bar: clock fix (hour offset)
## 0.6 — Level: bubble flicker removed (erase-in-place)
## 0.5 — Display: text flicker removed (anti-flicker discipline)
## 0.4 — Web: Dashboard.h with the volthub design (6 tabbed views)
## 0.3 — Display: DisplayUI with the volthub design (6 screens)
## 0.2 — Added the volthub design reference (tokens + palette + UX)
## 0.1 — Removed the JC3248W535EN vendor demo from git tracking
## 0.0 — Baseline: source code import (original graphics)

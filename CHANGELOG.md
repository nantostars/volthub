# Changelog — volthub

All notable changes to the firmware. Format inspired by
[Keep a Changelog](https://keepachangelog.com/). **Change-based versioning**:
`FW_VERSION` (`src/Version.h`) is bumped on **every commit** and a matching entry is
added here. Scheme `0.N` (N = change number) until the `1.0` release.

The current version is shown on the device **System** screen and in the **System** tab
of the web dashboard.

---

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

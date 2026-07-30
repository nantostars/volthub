# Changelog — volt·hub

All notable changes to the firmware. Format inspired by
[Keep a Changelog](https://keepachangelog.com/). **Change-based versioning**:
`FW_VERSION` (`src/Version.h`) is bumped on **every commit** and a matching entry is
added here. Scheme `0.N` (N = change number) until the `1.0` release.

The current version is shown on the device **System** screen and in the **System** tab
of the web dashboard.

---

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
## 0.4 — Web: Dashboard.h with the volt·hub design (6 tabbed views)
## 0.3 — Display: DisplayUI with the volt·hub design (6 screens)
## 0.2 — Added the volt·hub design reference (tokens + palette + UX)
## 0.1 — Removed the JC3248W535EN vendor demo from git tracking
## 0.0 — Baseline: source code import (original graphics)

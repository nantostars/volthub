# volthub — Camper Energy and Leveling Monitor firmware

ESP32 Arduino/PlatformIO project that monitors a camper energy system via BLE and serves a WiFi web dashboard.

**Goal:** unified energy **and levelling** dashboard for LiTime BMS + Victron SmartSolar + Victron Orion XS DC-DC + Witmotion IMU (pitch/roll when parking), accessible from Android/browser — without a GX device or Raspberry Pi.

---

## Hardware

### CYD — ESP32-2432S035R (primary board)

| Item | Value |
|------|-------|
| MCU | ESP32 dual-core 240 MHz |
| Display | 3.5" 480×320, landscape (`setRotation(1)`) — panel is **ST7796**, driven with `ILI9488_DRIVER` (see note) |
| Display SPI | MOSI=13, MISO=12, SCLK=14, CS=15, DC=2, BL=27 (active HIGH) |
| Touch | XPT2046 resistive, SPI shared bus, CS=33, IRQ=36 |
| Touch formula | `sx = 479 - map(ry, 320, 3860, 0, 479)` · `sy = map(rx, 480, 3860, 0, 319)` |
| microSD | SPI on its **own** bus (VSPI): CLK=18, MOSI/CMD=23, MISO/DAT0=19, CS=5 — **not** shared with the display (HSPI 14/13/12/15) |
| Free peripherals | RGB LED 4/17/16 · LDR (light sensor) 34 · audio amp 26 — unused by this firmware |
| Flash | `min_spiffs.csv` (two OTA slots) |
| Upload | 460800 baud · port e.g. `/dev/cu.usbserial-XXXX` (macOS; varies) |

**⚠ Driver mismatch, working on purpose:** the panel is an **ST7796** (per the board reference) but
`platformio.ini` builds with `-DILI9488_DRIVER`. The two are command-compatible enough that the
display works, and it has been used this way since the beginning. TFT_eSPI does ship `ST7796_DRIVER`;
switching would be a blind change on a board that is currently at v0.54 in the field, so leave it
unless a real display anomaly shows up on the CYD (wrong colours, inverted, offset) — then try it
with the hardware in hand. Pin reference: <https://github.com/chacuavip10/CYD-3.5inch_ESP32-3248S035>

**Flash command:**
```bash
pio run -e cyd -t upload --upload-port /dev/cu.usbserial-XXXX
```

### Guition JC3248W535C (`[env:guition]` — implemented, pending hardware verification)

| Item | Value |
|------|-------|
| MCU | ESP32-S3, dual-core 240 MHz |
| Flash | 16 MB QIO 120 MHz |
| PSRAM | 8 MB OPI |
| Display | 3.5" IPS, AXS15231B driver, **QSPI** (SPI2_HOST), 320×480 portrait → `setRotation(1)` → landscape 480×320 |
| Touch | AXS15231B integrated (same chip), I2C @ 400 kHz, polled (no IRQ) |
| Library | `moononournation/GFX Library for Arduino @ 1.4.9` + `espressif32@6.6.0` |
| PlatformIO env | `[env:guition]` with `-DBOARD_GUITION -DBOARD_HAS_PSRAM`, `qio_opi` memory |

**QSPI display pins** (confirmed from `esp_bsp.h`):

| Signal | GPIO |
|--------|------|
| CS | 45 |
| PCLK | 47 |
| DATA0–3 | 21, 48, 40, 39 |
| RST | NC |
| DC | 8 |
| TE | 38 |
| BL | 1 |

**Touch I2C pins:**

| Signal | GPIO |
|--------|------|
| SCL | 8 (shared with display DC — hardware multiplexed, not a conflict) |
| SDA | 4 |
| RST | -1 (not used) |
| INT | -1 (not used) |

**Code architecture (`#ifdef BOARD_GUITION`):**
- `DisplayUI.h`: conditional include (`Arduino_GFX_Library.h` vs `TFT_eSPI.h`), conditional member (`_bus/_gfx` vs `_tft`)
- `DisplayUI.cpp`: macro `#define _D (*_gfx)` aliases all geometric primitives. Only `begin()`, `handleTouch()`, `fillText()`, `_setFont()`, `_textWidth()` have board-specific branches.
- Touch read: `_readTouch()` polls AXS15231B I2C every 20ms (8-byte cmd `{0xb5,0xab,0xa5,0x5a,0x00,0x00,0x00,0x08}`, 8-byte response, addr `0x3B`)
- Font mapping: `_setFont(n)` → `setTextSize(1/2/3/6)` for fonts 1/2/4/6 (bitmap scaled)

**⚠ Pending hardware verification (3 items — see Known Risks):**
1. Init sequence colors
2. Touch coordinate transform
3. Font sizes

---

## Architecture

### BLE stack

- **Library:** NimBLE-Arduino ^1.4.2 (supports concurrent scan + active GATT connection)
- **LiTime BMS:** active GATT connection to characteristics `ffe1` (write) / `ffe2` (notify). Poll command every 2s: `{0x00,0x00,0x04,0x01,0x13,0x55,0xAA,0x17}`. Response: 104-byte frame.
- **Runtime estimate (derived, NOT sent by the BMS):** the frame carries no time-to-empty, so `bmsEtaMinutes()`/`bmsFmtEta()` in `LitimeBMS.h` derive it from the coulomb-counted `remainingAh` — `remainingAh / |I|` discharging, `(fullAh − remainingAh) / I` charging (`toFull`, shown green). Below `BMS_ETA_DEADBAND_A` (0.3 A) → `0` = `--` (would be infinite); clamped to `BMS_ETA_MAX_MIN` (99 h). It uses `BmsData.avgCurrent`, an **EMA with ~90 s time constant** computed in `handleNotify()` (reset after a >30 s gap, i.e. a reconnect) — the instantaneous current swings with every load step and would make the number jump between hours and days. Single implementation shared by the device UI and `/api/data` (`battery.etaMin` / `battery.etaFull`); the web only formats it.
- **Victron SmartSolar + Orion XS:** passive BLE advertisement scan. Payload decrypted with AES-128-CTR (mbedTLS, built into ESP32 core).
- **⚠ Every GATT connect attempt STOPS the scan** (NimBLE does it on `BLE_HS_EBUSY`, see `NimBLEClient.cpp`). Since Victron are passive advertisements, a stopped scan makes them *look* offline — that is what "the Victron keep disconnecting" actually means. Consequences baked into the code: the IMU is skipped entirely when its MAC is the placeholder/unset (`macConfigured()`), BMS and IMU back off exponentially on consecutive failures (`retryDelayMs()`, 30s → … → `BLE_RECONNECT_MAX_MS` 5 min), connect timeout is 3s, the alternate BLE address type is probed only on the first attempts, and `scanWatchdogTask` polls every 1s **but skips while `bms.isConnecting() || imu.isConnecting()`** (restarting the scan mid-connect fights the controller). Victron use their own `VICTRON_STALE_MS` (30s) instead of `DEVICE_STALE_MS` (10s, right for the 2s-polled BMS).
- **⚠ The scan MUST stay callback-only — `scan->setMaxResults(0)` in `VictronBLE::startScan()`.** By default NimBLE keeps a `NimBLEAdvertisedDevice` for **every unique MAC it has ever seen** and never frees it. We only read data in `onResult()` and never call `getResults()`, so that store is pure leak: BLE MAC randomization (phones/wearables rotate their address ~every 15 min) feeds a steady stream of "new" devices even while parked, leaking ~12 KB/h until the synchronous `WebServer` can no longer allocate connections — the dashboard becomes unreachable after hours while the device otherwise runs fine (only a reboot recovers it). Fixed in v0.58; verified flat at ~174 KB free after 24 h. **Never remove that line**, and re-check it after any NimBLE upgrade.
- **Witmotion IMU (WT9011DCL):** active GATT. Service `FFE5` (UUID `0000ffe5-0000-1000-8000-00805f9a34fb` — non-standard base `9a`), notify `FFE4`, write `FFE9`. 20-byte frame: `0x55 0x61 roll(2) pitch(2) yaw(2) ax(2) ay(2) az(2) gx(2) gy(2)`. Angles = int16LE / 32768 × 180°. No checksum in payload. 15s boot stagger to avoid racing BMS connect.

### Victron BLE advertisement byte offsets

Offsets are in the raw NimBLE `getManufacturerData()` buffer, **including** the 2-byte company ID prefix:

```
[0..1]  Company ID: 0xE1 0x02
[2]     Marker: 0x10
[3]     Second prefix byte (varies per device)
[4..5]  Product ID (uint16 LE)
[6]     Readout type
[7..8]  IV (uint16 LE) → AES-CTR counter seed
[9]     Key-check byte (must equal key[0])
[10+]   Encrypted payload (AES-128-CTR)
```

**Charge state (SmartSolar + Orion XS):** both decode a state byte from the decrypted payload (`SolarData.chargeState` / `OrionData.deviceState` = `dec[0]`), mapped by `victronStateName()` in `VictronBLE.h`. That table is verbatim from the **official** VE.Direct Protocol 3.33, field CS ("State of operation"): 0 Off · 1 Low power · 2 Fault · 3 Bulk · 4 Absorption · 5 Float · 6 Storage · 7 Equalize · 9 Inverting · 11 Power supply · 245 Starting-up · 246 Repeated absorption · 247 Auto equalize · 248 BatterySafe · 252 External control. Codes 6/11/246/248 are **charger-only** (Orion XS); unlisted codes and `0xFF` (no data) → `UNKNOWN`. Both detail screens (device pill + web pill) show this real state — never a value derived from current. Names are kept ≤13 chars for the 96px device pill.

### WiFi / Web server

- **Library:** standard Arduino `WebServer.h`
- **Default mode:** AP (`WIFI_AP`), SSID `CamperEnergy`, password `camper1234`
- **API:** dashboard polls `/api/data` (JSON) every 2s
- **OTA:** `GET /update` → upload page · `POST /update` → flash + reboot. **Disabled by default**; enabled + credentials set at runtime from the web System page (stored in NVS: `Settings::getOtaEnabled/getOtaUser/getOtaPass`, `otaActive()`). The `/update` handlers return 403 unless `otaActive()` (enabled AND both credentials set), then require HTTP Basic Auth with the stored credentials. No credentials in `Config.h`. Binary: `.pio/build/<env>/firmware.bin`.

**AP auto-off (`Settings::getApOffWhenSta`, NVS `ap_off_sta`, default OFF):** drops the softAP while the STA is connected, so the phone stops auto-joining `CamperEnergy` and losing internet. State machine in `apAutoUpdate()` (`main.cpp`, ticked every 2s): AP off only after the STA has been up **2 min** (`STA_STABLE_MS`), back on after it has been down **60 s** (`STA_LOST_MS`, not instantly — avoids flapping), and never when no client SSID is configured. **The device cannot tell whether your phone can actually reach it over the client network** — client isolation and captive portals are invisible from the ESP32 side — so the protection is deliberately NOT network logic but two physical escape hatches: (1) the **AP pill** on the System screen (`_apTogglePending` → `consumeApToggle()` → `apManualOn`; it can only force the AP **on** or release the override, never turn it off, so a tap can't lock you out); (2) `AP_GRACE_MS` = **10 min of unconditional AP after every boot**, so a power cycle is always a way back in. Exposed as `sys.apOn` / `sys.apAuto`.

The pill shows the **mode**, not just up/down, so every tap has visible feedback: `DisplayUI::ApMode` — **AUTO** (up, automation may drop it) · **ON** (up and pinned, *or* the auto-off option isn't in play) · **OFF** (down). Computed by `apPillMode()` in `main.cpp` and passed to `updateSysInfo()`, so the UI never duplicates the WiFi logic. Note `drawSystem()` is the one draw function called **directly on tap** (outside `selectScreen()`'s cache reset): its card fill erases both pills, so it must clear their `_pill[]` cache entries or the pill whose text didn't change stays blank.

**⚠ `/api/settings` is UNAUTHENTICATED — never echo a secret from it.** Anything it returns is readable by anyone on the network. It reports only whether a secret is stored (`wifiHasPass`, `staHasPass`, `solarKeySet`, `orionKeySet`, `otaHasPass`) and never the value; the Victron keys count as "set" only when 32 chars **and** not the all-zero default. On POST, a secret field is written **only when non-empty**, so an untouched form cannot wipe it — that rule also applies to `staPass`, which previously accepted an empty string and would now erase the client password on every save. Until v0.83 this endpoint handed out both WiFi passwords and both 32-char Victron AES keys in clear text.

**Long-run stability (the web server is a single synchronous `WebServer`):**
- **Polling is chained, never `setInterval`.** `poll()` schedules the next request only after the previous one settles, with an 8s `AbortController` timeout. With `setInterval(2s)` a slow reply (the ESP32 is also driving display + BLE + WiFi) let requests overlap and pile up TCP connections until the server stopped answering. At most one request may be in flight (v0.57).
- **Root page is served `no-store`** (`Cache-Control: no-store, no-cache, must-revalidate` + `Pragma`/`Expires`). The dashboard is embedded in the firmware and changes across versions, so a cached copy from a previous flash silently lacks new fields/logic — it looked like "OTA won't save" from a desktop browser while a fresh phone worked (v0.55).
- **STA failure must not degrade the AP.** On a failed client join, leaving `WIFI_AP_STA` with auto-reconnect made the driver retry in the background and each attempt disrupted the shared-radio softAP (AP "comes and goes", pages served truncated). `wifiSetup()` now stops the STA and drops to stable AP-only until the next reboot (v0.53).
- **Heap diagnostics** (kept after the v0.58 leak hunt): `sys.heap` in `/api/data`, a "Free RAM" row in the web System tab, and a 60s `[heap]` serial log. Healthy = flat (~174 KB free after 24 h); a steady decline means something is leaking.

### CSV data log (`DataLogger`)

Off by default (`Settings::getLogEnabled`, NVS `log_en`). One row every `LOG_PERIOD_MS` (2 min) to `/volthub_YYYYMMDD.csv`.

- **Filesystem: LittleFS on the existing `spiffs` partition** (128 KB from `min_spiffs.csv`). The Arduino LittleFS library mounts the partition *labelled* `spiffs` by default, so this needs **no repartitioning and no USB flash** — it formats on first mount. Use LittleFS, **not SPIFFS**: SPIFFS garbage collection on a nearly-full filesystem (a rotating log's normal state) can stall the device for seconds; the flash cache is disabled during erases, so that stall hits both cores.
- **Buffered**: rows accumulate in RAM and flush every `LOG_FLUSH_ROWS` (5) → one write per 10 min, not 720/day. `flush()` must be called before any reboot (it already is in `handlePostSettings`).
- **Retention is `LOG_MIN_FREE` (15 KB free), not a file count.** ~60 KB/day on 128 KB ⇒ ~1.8 days ⇒ only ~2 files ever coexist, so `LOG_MAX_FILES` (3) can never fire in normal use — it exists purely as a guard against a jumping clock spawning several small files. Don't document it as a 7-day/N-day history: the space is the limit.
- **The clock gates everything**: no RTC on the board, so without a valid time there is no filename and no usable timestamp — the logger sits in `LOG_WAIT_TIME`. `POST /api/time` lets the dashboard hand over the phone's clock (works offline); it is refused once `time(nullptr)` shows a real date, so NTP always wins.
- **Live-apply endpoints:** `/api/settings` **reboots the device on every POST**, so anything that only needs to take effect immediately gets its own endpoint — `POST /api/logs` (log toggle, file delete) and `POST /api/lang` (UI language). Keep new instant settings out of `/api/settings`.
- **The log toggle is NOT in `/api/settings`** — that handler reboots on every save. It lives on `POST /api/logs` (`{enabled:bool}`) and applies live. Same endpoint deletes a file (`{file:name}`); `GET /api/logs` returns status + listing, `GET /logdl?f=` streams a file.
- **Changing the column set is a breaking change for files already on disk**: `rotateIfNeeded()` compares the existing file's first line with `CSV_HEADER` and falls back to `volthub_YYYYMMDD_<n>.csv` when they differ, so two schemas never end up in one file. Keep that check working when adding columns.
- Statuses are numeric to keep rows ~80 B: battery `1/0/-1`, solar/DC-DC use the raw VE.Direct CS code. Missing readings are written as **empty fields**, never 0.

**Logging to a microSD is designed but NOT implemented** — see `docs/SDCARD-logging.md`. Key facts already established: the Guition TF slot is SDMMC 1-bit on GPIO 11/12/13 (dedicated peripheral, no contention with the QSPI display) and those pins are free; `LittleFS` and `SD_MMC` both derive from `fs::FS`, so the logger only needs a backend pointer plus a capacity shim.

### NimBLE GATT quirks (IMU)

- Must call `getServices(true)` then iterate the returned vector directly. `getService(uuid)` returns null even after forced discovery — same for `getCharacteristics(true)`.
- IMU MAC starts with `FD` (bits=11) → `BLE_ADDR_RANDOM`. Auto-detected from first byte; falls back to PUBLIC.
- After subscribing, send unlock packet to write char: `0xFF 0xAA 0x69 0x88 0xB5`.
- NVS key for IMU MAC: `imu_mac` (`Settings::getWitmotionMac` / `setWitmotionMac`).

### NTP sync

- `configTime(0, 0, srv, "time.google.com", "216.239.35.0")` — three servers in cascade for resilience.
- TZ fallback: if `settings.getNtpTZ()` is empty, uses `NTP_TZ` from `Config.h`.
- **Important bug (fixed):** retry loop must check `!esp_sntp_enabled()`, NOT `sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET`. In IMMED mode the status stays RESET while waiting for response, so checking it would restart SNTP every 60s and prevent sync. Requires `#include "esp_sntp.h"`.

---

## CYD Display — `DisplayUI`

### Screen state machine

```
SCR_OVERVIEW  →  SCR_DETAIL_BATT / SCR_DETAIL_SOLAR / SCR_DETAIL_ORION
              →  SCR_LEVEL
              →  SCR_SETTINGS
```

Back button (hitbox x=4,y=4,w=58,h=30) → SCR_OVERVIEW from any screen.

### Overview layout (480×320 landscape)

```
x=8          x=171        x=334
┌────────────┐ ┌──────────┐ ┌────────────┐  y=8
│   SOLAR    │ │ BATTERY  │ │   DC-DC    │  BOX_H=200
└────────────┘ └────┬─────┘ └────────────┘  y=208
                    │ flow line
┌────────────┐ ┌────▼─────┐ ┌────────────┐  y=216
│  SETTINGS  │ │  LOADS   │ │   LEVEL    │  LOADS_H=96
└────────────┘ └──────────┘ └────────────┘  y=312
```

**Flow lines:** 3px on `CONN_MID_Y`. Active = blue animated (6px dash + 8px gap). Phase advances `+12 mod 14` every 60ms (= −2 mod 14) to make dashes flow in the correct physical direction: Solar→Battery (→), DC-DC→Battery (←, `reverse=true`), Battery→Loads (↓).

### Font sizes (TFT_eSPI built-in fonts)

| Use | Font | Size |
|-----|------|------|
| Big value (W, SOC%) | font4 | 26px |
| State string (CHARGING, BULK…) | font2 | 16px |
| Sub-values (A, V, W, Wh) | font4 | 26px |
| Labels (PITCH, ROLL, box titles) | font1 | 8px |
| Large values (Level screen) | font6 | 48px |

**SOC inside a ring — 3-digit rule:** at 100% the extra digit makes `SOC %` wider than the ring's usable inner span and it would clip the stroke, so the number drops **one font step for 3-digit values only** (0..99 keep the large size): Overview `font6→font4`, Battery screen `font2→font1`. Same rule on the web via `setSoc(el, v, on, bigPx, smallPx)` (number ×0.8, `%` ×0.85). Usable width is not the full inner diameter: at the digits' top/bottom the circle narrows to `2·√(rᵢ² − (h/2)²)`.

**DC-DC detail layout:** the two sides of the converter are kept visually separate — top card = two labelled groups (*Input (alternator)* / *Output (battery)*), each with its own Voltage / Current / Power box (power = V×A, computed); bottom card = *Efficiency & status* only (efficiency is suppressed below 5 W input, where it is noise; status shows the charger error, else the off reason, else nothing). Don't mix quantities from the two sides in one row — that was the pre-0.75 mistake.

### Color palette (RGB565)

| Name | Hex | Usage |
|------|-----|-------|
| `UI_BG` | `#0f1117` | Background |
| `UI_SURFACE` | `#1a1d27` | Card fill |
| `UI_BORDER` | `#2a2d3e` | Card border, inactive |
| `UI_TEXT` | `#e8eaf0` | Primary text |
| `UI_MUTED` | `#6b7280` | Secondary / disabled |
| `UI_GREEN` | `#22c55e` | Positive / online / charging |
| `UI_YELLOW` | `#f59e0b` | Warning |
| `UI_RED` | `#ef4444` | Negative / offline / fault |
| `UI_BLUE` | `#3b82f6` | Flow lines |
| `UI_ORANGE` | `#f97316` | Discharge current |

### Color logic per box

- **SOLAR** — W headline: `UI_GREEN` if >0.5W, else `UI_MUTED`. Sub-values A/V: `UI_TEXT`. Wh: `UI_MUTED`.
- **BATTERY** — SOC: `socCol()` (green >50%, yellow >20%, red ≤20%). State: green=CHARGING, orange=DISCHARGING, muted=IDLE / offline. A and W: `signCol()` (green if positive, orange if negative). V: `UI_TEXT`.
- **DC-DC** — W headline: `UI_GREEN` if outW >0.5W, else `UI_MUTED`. A and V: `UI_TEXT`.
- **LOADS** — A: `UI_MUTED` (derived). W: `UI_TEXT`.

### Sub-value order (consistent across CYD and web)

- SOLAR: A → V → Wh
- BATTERY: A → V → W
- DC-DC: A → V
- LOADS: A → W

### Anti-flicker rules

- **`fillText(x, y, w, h, txt, font, col, bg)`** — wraps `setTextPadding(w-2)` + `drawString()`. **Never use `print()`** for updatable fields: shorter replacement text leaves residual pixels.
- `updateOverviewValues()` rate-limited to 300ms (`_lastValMs`). Flow animation runs independently at 60ms.
- Arrow sentinels `_lvUpC/_lvDnC/_lvLtC/_lvRtC` (init=1 in `drawOverview()`): LEVEL box arrows redrawn only on color change.
- SCR_LEVEL bubble: `updateLevelValues()` restores bar colors under old bubble position using zone lookup tables (`pZY/pZH/pZC` for pitch, `rZX/rZW/rZC` for roll), then draws new bubble. No full-panel `fillRect`.

### SETTINGS box and screen

**Overview box** (x=8, y=216, w=138, h=96): title "SETTINGS" + centered gear icon drawn with `fillCircle` (outer r=18, 8 tooth circles r=5 at 45° intervals, inner hole r=10 in `UI_SURFACE`). Tapping → `SCR_SETTINGS`.

**SCR_SETTINGS screen:**
- Standard header (`drawDetailHeader`)
- SCHERMO toggle row (y=48..95): pill 52×22px at x=362, radius=9. Green = SEMPRE ON, border-only = AUTO OFF. Flicker-free via `setTextPadding`.
- 5 info rows (y=106, step=36px), values drawn with `setTextPadding(308)` + `drawString()`:

| Row | Label | Unset fallback |
|-----|-------|----------------|
| 0 | AP SSID | `--` |
| 1 | AP IP | `--` |
| 2 | CLIENT SSID | `non configurato` (muted) |
| 3 | CLIENT IP | `non connesso` (muted) |
| 4 | ORARIO NTP | `in attesa...` (muted) |

NTP row shows **current time** via `time(nullptr)` + `strftime` — not last-sync timestamp.

`updateSysInfo()` is a public method called from `main.cpp` every 5s. If `_screen == SCR_SETTINGS`, it redraws the info rows in place (no flicker).

### Screen timeout

- `_alwaysOn=false` (default): backlight off after 60s of no touch (`PIN_BL=27 LOW`).
- `_alwaysOn=true`: backlight always on.
- Wake: first touch turns backlight on, tap is swallowed (no navigation).
- Toggle lives in SCR_SETTINGS. Not persisted across reboot.

### LEVEL box arrows (overview)

Arrow center: `ay = LOADS_Y + 41`. Key Y offsets from `LOADS_Y`:

| Element | Offset |
|---------|--------|
| Erase rect top | +20 |
| Divider line top | +20 |
| UP triangle tip | +20 |
| UP shaft top | +28 |
| DOWN triangle tip | +63 |
| DOWN shaft top | +45 |
| Value text | +66 |
| Label text | +84 |

### Level screen (SCR_LEVEL)

- Pitch: vertical bar, BAR_X=102, y=70–240, BAR_CY=155. Bubble: `by = BAR_CY - clamp(pitch,±10°) * 85/10`.
- Roll: horizontal bar, BAR_X=260, y=110–146, BAR_CY=128. Bubble: `bx = BAR_X+100 + clamp(roll,±10°) * 100/10`.
- Zone proportions pitch (170px): RED42 | YEL26 | GRN34 | YEL26 | RED42. Zone Y starts: `{70,112,138,172,198}`, heights `{42,26,34,26,42}`.
- Zone proportions roll (200px): RED50 | YEL30 | GRN40 | YEL30 | RED50. Zone X starts: `{260,310,340,380,410}`, widths `{50,30,40,30,50}`.

---

## Web Dashboard

Served from `Dashboard.h` (embedded HTML/CSS/JS). Polls `/api/data` every 2s.

### Overview tab

Mirrors the CYD overview: same box titles (SOLAR / BATTERY / DC-DC / LOADS), same field order, same color logic.

**Font sizes:**

| Class | Size |
|-------|------|
| `.ov-title` | 0.9rem |
| `.ov-main` | `clamp(2rem, 5vw, 3rem)` |
| `.ov-sub` | 1rem |
| `.ov-stat` | 1rem |

**Layout:**
- Landscape: Solar | Battery | DC-DC in one flex row, Loads below.
- Portrait (`max-width: 520px`): Solar + DC-DC top row (50% each), Battery full-width below. CSS `order` — no HTML change needed.
- Secondary stats (A/V/Wh): `.ov-bottom` uses `flex-direction: column; gap: 2px`.

**CSS specificity note:** `.ov-stat span { color: var(--text) }` has higher specificity (0,1,1) than `.val-pos` (0,1,0). Explicit overrides added: `.ov-stat span.val-pos/val-neg/val-dim`.

**Color logic (`updateOverview()`):**
- Solar W: `val-pos` if `s.online && solarW > 0.5`, else `val-dim`
- DC-DC W: same pattern
- Battery SOC: `val-pos` if soc > 50, else `val-neg`; `val-dim` when offline
- Battery state: CHARGING → `var(--green)`, DISCHARGING → `var(--orange)`, IDLE/offline → `var(--muted)` (via `style.color`)
- Battery A/W: `signCls()` → `val-pos`/`val-neg`/`val-neu`
- Solar Wh: always `val-dim` (muted)

**Offline handling:**
- Solar/Orion state: `(s.online && s.state) ? s.state : '--'`
- Battery offline: ALL fields → `--`, fill bar → 0%, SOC → `val-dim`. No `|| 0` fallback.

**Loads:** battery **discharge only**, straight from the BMS — `loadsW = max(0, -battW)` · `loadsA = max(0, -battCurrent)` (battW/battCurrent: + = charging, − = discharging). Loads shows what the loads pull **from the battery** (net of what solar/DC-DC already cover); it is 0 while charging or idle and never shows a charging value. This is the net battery draw, not the absolute total load (no dedicated load shunt exists). Battery→Loads flow line is active only when `battW < -2`.

### Level tab

- `#view-level`: `height: calc(100vh - 56px); overflow: hidden` — no scroll. `setView()` sets `display: flex` (not `block`).
- `.lv-grid`: `flex-direction: row` landscape, `column` portrait.
- `.lv-svg`: `flex: 1; width: 100%; height: 100%` with SVG `preserveAspectRatio="xMidYMid meet"`.

### System tab — "Keep screen on"

Toggle (`#wake-btn`, `toggleWake()`) that stops the phone screen from sleeping while the dashboard is open. The device serves plain **HTTP**, so the Screen Wake Lock API (`navigator.wakeLock`) is unavailable — it needs a secure context. Instead a muted/inline/looping 128×128 H.264 clip (`#wake-vid`, ~1.6 KB, embedded as `WAKE_MP4` data URI) is kept **playing**; a playing video keeps the screen awake, over HTTP on any browser. **Modern Chromium (Android WebView / DuckDuckGo included) ignores hidden/tiny videos for this** — so `#wake-vid` is stretched **full-viewport on top at ~2% opacity, `pointer-events:none`** (genuinely visible to the browser, imperceptible to the user). `src` is assigned lazily on first enable; playback re-asserted on `pause` and `visibilitychange`, with a `timeupdate` re-seek to loop the short clip. State is a per-browser `localStorage` flag (`wakeOn`), **default OFF** — nothing stored on the device (no NVS / `/api/settings`).

---

## Known Risks / TODO

- **Victron key-check:** key-check byte (must equal `key[0]`) used to distinguish SmartSolar vs Orion. Fails if both BLE keys share the same first byte.
- **LiTime 4S vs 8S:** cell count inferred from non-zero values — verify on real hardware.
- **WiFi+BLE coexistence:** ESP32 shares the 2.4GHz radio. Three active BLE connections (BMS + IMU + scan for Victron) may cause scheduling jitter.
- **Guition — init sequence colors:** GFX Library 1.4.9 ships only the 360×640 init sequence. If colors/gamma look wrong: (a) upgrade to GFX Library ≥1.5.0 + pioarduino platform (adds `axs15231b_320480_type1_init_operations` / `type2`) and update `platformio.ini` + `begin()` constructor; OR (b) define a custom init array from `JC3248W535EN/1-Demo/DEMO_LVGL/esp_lcd_axs15231b.c` `vendor_specific_init_default[]` converted to Arduino_GFX byte format.
- **Guition — touch coordinate transform:** `_readTouch()` applies `sx=ty, sy=319-tx`. If tap targets are offset/mirrored, adjust the transform in `DisplayUI.cpp:_readTouch()`. Common variants: `sx=479-ty, sy=tx` (X mirrored), `sx=ty, sy=tx` (no Y-invert), etc. Test by tapping each corner and logging coordinates.
- **Guition — font sizes:** `_setFont()` uses `setTextSize(1/2/3/6)` giving ~7/14/21/42px. If text overflows or is too small in boxes, increase setTextSize values or replace with proportional GFX fonts (e.g. `FreeSans18pt7b` for font4). Font `.h` files are in the Adafruit GFX library (already a transitive dependency).
- **Victron key-check:** key-check byte (must equal `key[0]`) used to distinguish SmartSolar vs Orion. Fails if both BLE keys share the same first byte.
- **LiTime 4S vs 8S:** cell count inferred from non-zero values — verify on real hardware.
- **WiFi+BLE coexistence:** ESP32 shares the 2.4GHz radio. Three active BLE connections (BMS + IMU + scan for Victron) may cause scheduling jitter.

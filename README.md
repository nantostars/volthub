# volthub — Camper Energy Monitor

A self-contained energy dashboard for a camper/RV electrical system, running on an
ESP32 touchscreen display. It reads a **LiTime BMS**, a **Victron SmartSolar** MPPT,
a **Victron Orion‑XS** DC‑DC charger and a **Witmotion** tilt sensor over Bluetooth LE,
and shows everything on the built‑in screen **and** on a phone/browser via Wi‑Fi — with
**no GX device, Raspberry Pi or cloud** required.

> Firmware for two touchscreen boards: the **Guition JC3248W535C** (ESP32‑S3, primary)
> and the **ESP32‑2432S035R "CYD"** (ESP32). Both share the same UI and web dashboard.

---

## Features

- **Live overview** — solar, DC‑DC and loads power, battery ring (SOC), voltage/current,
  animated energy‑flow lines, colour‑coded by state (producing / discharging).
  *Loads* is the battery **discharge** reported by the BMS, i.e. what the loads actually draw
  from the battery (net of what solar/DC‑DC already cover); it reads 0 while charging or idle.
- **Runtime estimate** — time to empty while discharging, time to full while charging, derived
  from the BMS coulomb counter (`remainingAh`) and a smoothed current, so it does not jump on
  every compressor start. Shown on the overview and the Battery screen.
- **Detail screens** — Battery (per‑cell voltages, SOH, cycles, temperature, balance
  delta with alarm thresholds), Solar, DC‑DC, and a **spirit‑level** tab using the IMU.
- **Real charge state** — the actual Victron mode (Bulk / Absorption / Float / Storage /
  BatterySafe …) for both the MPPT and the DC‑DC, decoded from the advertisement and named
  after the official VE.Direct state table — not guessed from the current.
- **Web dashboard** — same data on any phone/browser over Wi‑Fi; live polling of a JSON API.
  Includes a **"Keep screen on"** toggle so the phone does not lock while you watch the dashboard
  (works over plain HTTP, where the Screen Wake Lock API is unavailable).
- **Exact device models** — Victron model name resolved from the BLE Product ID
  (official VE.Direct product‑id table).
- **Dual language** — English / Italian, switchable from the web System tab.
- **OTA updates** — disabled by default; enable it and set the credentials from the web
  System page (nothing hardcoded, no credentials in the repo).
- **NTP clock**, configurable Wi‑Fi (AP + optional client), persisted settings (NVS).
- **Built to run for days** — no‑leak BLE scanning, non‑overlapping web polling and a free‑RAM
  readout in the System tab, so the dashboard stays reachable on long trips.

## Hardware

The firmware targets two low‑cost, all‑in‑one ESP32 touchscreen boards. No custom wiring
is required — the displays and touch controllers are integrated on the board.

| | **Guition JC3248W535C** (primary) | **ESP32‑2432S035R "CYD"** |
|---|---|---|
| MCU | ESP32‑S3, 240 MHz, 8 MB PSRAM, 16 MB flash | ESP32, 240 MHz |
| Display | 3.5" IPS, AXS15231B, **QSPI**, 320×480 → landscape 480×320 | 3.5" ILI9488, SPI, 480×320 |
| Touch | AXS15231B integrated, I²C | XPT2046 resistive, SPI |
| PlatformIO env | `guition` | `cyd` |

### Monitored BLE devices

| Device | Protocol | Notes |
|---|---|---|
| LiTime BMS | active GATT (notify `ffe2` / write `ffe1`) | 2 s poll, 104‑byte frame |
| Victron SmartSolar MPPT | passive BLE advertisement | AES‑128‑CTR decrypted (key from VictronConnect) |
| Victron Orion‑XS DC‑DC | passive BLE advertisement | AES‑128‑CTR decrypted |
| Witmotion WT9011DCL IMU | active GATT (service `FFE5`) | pitch/roll/yaw for the level screen |

## Build & flash

The project uses [PlatformIO](https://platformio.org/).

```bash
# Build
pio run -e guition        # ESP32-S3 Guition board
pio run -e cyd            # ESP32 CYD board

# Flash (replace the port with yours)
pio run -e guition -t upload --upload-port /dev/cu.usbmodemXXXX   # Guition (USB-JTAG)
pio run -e cyd     -t upload --upload-port /dev/cu.usbserial-XXXX # CYD
```

- **Guition tip:** if the board is in a crash loop, hold the **BOOT** button during flashing.
- **OTA:** off by default. In the web dashboard go to **System → Configuration → Firmware
  update (OTA)**, tick *Enable OTA*, set a username and password, and save. Then open
  `http://<device-ip>/update` (HTTP Basic Auth with those credentials) and upload
  `.pio/build/<env>/firmware.bin`. OTA works only while enabled with both credentials set.

## Configuration

- **Compile‑time defaults** live in `src/Config.h` (Wi‑Fi AP SSID/password, NTP,
  default device MACs). No secrets/credentials are kept in the repo.
- **OTA** is enabled and configured at runtime from the web System page (stored in NVS),
  disabled by default.
- **Victron keys** are the per‑device AES keys from the VictronConnect app
  (Product info → *Show encryption data*). Set them at runtime from the web **System → Configuration** form; they are stored in NVS.
- **Runtime settings** (Wi‑Fi client, device MACs, language, screen timeout) are set from
  the web dashboard and persisted in NVS.
- **Default Wi‑Fi:** Access Point `CamperEnergy` / `camper1234`; the dashboard is at
  `http://192.168.4.1`.

## Web dashboard & API

The dashboard is embedded in the firmware (`src/Dashboard.h`) and served over Wi‑Fi. It
polls `GET /api/data` (JSON) every 2 s. Settings are read/written via `/api/settings`.

`GET /api/data` returns one object per source — handy if you want to feed the data somewhere
else (logger, Home Assistant, …). Every block has an `online` flag; when it is `false` the
other fields are omitted rather than zeroed.

| Block | Fields |
|---|---|
| `battery` | `online`, `voltage`, `current` (+ = charging), `power`, `soc`, `soh`, `cycles`, `remainingAh`, `fullAh`, `cellTemp`, `mosfetTemp`, `cells[]`, `model`, `etaMin`, `etaFull` |
| `solar` | `online`, `state`, `stateCode`, `battVoltage`, `chargeCurrent`, `solarPower`, `yieldToday`, `loadCurrent`, `pid`, `model` |
| `orion` | `online`, `state`, `stateCode`, `inVoltage`, `inCurrent`, `outVoltage`, `outCurrent`, `pid`, `model` |
| `imu` | `online`, `pitch`, `roll`, `yaw`, `temp` |
| `sys` | `fw`, `lang`, `ota`, `heap`, `apIp`, `staIp`, `date`, `time` |

`etaMin` is the runtime estimate in minutes (`0` = not meaningful: at rest, offline or already
full); `etaFull` tells whether it counts down to full (charging) or to empty (discharging).

## Versioning & changelog

Change‑based versioning: `FW_VERSION` in `src/Version.h` is bumped on every commit and a
matching entry is added to [`CHANGELOG.md`](CHANGELOG.md). The current version is shown on
the device **System** screen and in the web **System** tab.

## Open‑source components

This project builds on the following open‑source software:

| Component | Purpose | License |
|---|---|---|
| [NimBLE‑Arduino](https://github.com/h2zero/NimBLE-Arduino) | BLE stack | Apache‑2.0 |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | JSON API | MIT |
| [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) | CYD display driver | MIT |
| [Arduino_GFX](https://github.com/moononournation/Arduino_GFX) | Guition (AXS15231B QSPI) display driver | BSD‑3‑Clause |
| [Adafruit GFX fonts](https://github.com/adafruit/Adafruit-GFX-Library) (FreeSansBold) | large UI numerals | BSD |
| [Arduino‑ESP32](https://github.com/espressif/arduino-esp32) core | framework | LGPL‑2.1 |
| Mbed TLS (via ESP‑IDF) | AES‑128‑CTR for Victron decryption | Apache‑2.0 |

Victron product‑id names are derived from Victron Energy's public **VE.Direct Protocol**
documentation. Victron, SmartSolar, Orion‑XS, LiTime and Witmotion are trademarks of their
respective owners; this project is not affiliated with or endorsed by them.

## License

Released under the [MIT License](LICENSE) © 2026 nantostars.

Third‑party components remain under their own licenses (see the table above).

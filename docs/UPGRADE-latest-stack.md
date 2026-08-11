# Upgrade assessment: latest stack (arduino v3 / NimBLE 2.x / GFX / …)

> **Reusable planning document.** If and when the upgrade is attempted, start here.
> Assessment only — no code changes until the decision is made.
>
> Written 2026-07-23. Refresh the "target" versions at execution time.

---

## 1. Current stack (known-good, deliberate)

| Component | Current version | Notes |
|---|---|---|
| Platform | `espressif32@6.6.0` | = arduino-esp32 **v2** / **IDF 4.4** |
| BLE | `NimBLE-Arduino @ ^1.4.2` | works on v2; **EOL** (no longer maintained) |
| GFX (Guition) | `GFX Library for Arduino @ 1.4.9` | `[env:guition]` only |
| Display (CYD) | `TFT_eSPI @ ^2.5.43` | `[env:cyd]` only |
| JSON | `ArduinoJson @ ^7.0.0` | both envs |
| Crypto | mbedTLS (in the ESP32 core) | AES-128-CTR for the Victron advertisements |

This stack is **pinned on purpose**: v2 + NimBLE 1.4.2 + GFX 1.4.9 is the known-good combination
that avoids the BLE crash on IDF5 (see §3). The Guition display and touch are confirmed working
on it.

## 2. Target stack (refresh at execution time)

| Component | Target | Constraint |
|---|---|---|
| Platform | **pioarduino** platform-espressif32 (arduino-esp32 **v3** / **IDF5**) | classic `espressif32` stays on v2 → the pioarduino fork is required |
| BLE | **NimBLE-Arduino 2.x** | mandatory on IDF5 (1.4.x does not build / crashes) |
| GFX (Guition) | GFX 1.6.x / 1.7.x | ships native AXS15231B init sequences (type1/type2) |
| Display (CYD) | latest TFT_eSPI 2.5.4x (v3-compatible) | verify arduino v3 support |
| JSON | ArduinoJson 7.x | **unchanged, zero work** |

pioarduino platform reference (example — use the latest release):
`https://github.com/pioarduino/platform-espressif32/releases`

## 3. It is all-or-nothing — and this project already has the proof

The three upgrades are **not independent**: they are a single forced jump.
- Arduino v3 does not exist on `espressif32@6.6.0` → pioarduino is required.
- IDF5 breaks NimBLE 1.4.2 → NimBLE 2.x is mandatory.

**Direct evidence in this project:** the `guition-v3-wip` branch went into a **boot loop / Guru
Meditation LoadProhibited at BT controller init** precisely because it paired arduino v3 with
NimBLE 1.4.2. That is not a bug to fix: it is the incompatibility that *forces* NimBLE 2.x. Keep
that branch as a reference if you like; the good solution (v2) is on `main`.

## 4. Complexity per component

| Component | Effort | Why |
|---|---|---|
| **NimBLE 1.4.2 → 2.x** | 🔴 High | The bottleneck. Changed APIs: `NimBLEAdvertisedDeviceCallbacks`→`NimBLEScanCallbacks`, `onResult` signatures, subscribe/notify, address handling, `getServices`/`getCharacteristics`. There are **three distinct BLE patterns** to port and re-test: Victron passive scan, BMS active GATT, IMU active GATT (with its quirks: `getServices(true)`, forced discovery, non-standard `9a` base UUID, unlock packet). |
| **Platform → pioarduino v3** | 🟡 Medium | `platformio.ini` change on **both** envs. pioarduino toolchain to install. Some IDF5 APIs to adapt. |
| **GFX 1.4.9 → 1.6/1.7 (Guition)** | 🟡 Medium-low | Native AXS15231B init may simplify or change the custom driver's constructors (`Arduino_AXS15231B_Guition.h`). Re-verify the result: init / COLMOD 0x55 / software rotation / 40 MHz. Risk of reopening texture, flicker and touch issues. |
| **TFT_eSPI (CYD)** | 🟡 Medium | Known friction with arduino v3 (SPI/DMA APIs). Bump and re-verify the CYD display. |
| **mbedTLS 2.x → 3.x (Victron AES-CTR)** | 🟡 Low-medium | IDF5 brings mbedTLS 3.x; some APIs used in advertisement decryption may be deprecated. |
| **Wire / I2C (touch)** | 🟢-🟡 Low | New I2C master driver in IDF5; the `Wire` wrapper holds, but AXS15231B touch needs re-checking. |
| **WebServer / WiFi / OTA** | 🟢 Low | Stable APIs. |
| **ArduinoJson** | 🟢 None | Already v7. |
| **Partitions / OTA** | ⚠️ Check | v3/IDF5 binaries are **larger**: confirm the firmware still fits `min_spiffs.csv` (two OTA slots). If it grows too much, revisit the partition scheme. |

## 5. The real cost is RE-VERIFICATION, not writing code

Porting the code is roughly two focused days. Most of the risk sits in **hardware testing**, which
cannot be compressed:
- **WiFi + three BLE connections coexisting** on IDF5: timing-dependent, with intermittent bugs
  that are hard to reproduce. It works today on IDF4; the IDF5 radio scheduler is different.
- **Guition display + touch**: to be re-tuned against the new GFX (risk of reopening closed issues).
- **CYD display** with an updated TFT_eSPI.
- **BMS / IMU / Victron**: regressions to re-verify one by one against the real hardware.

Rough estimate: **~2 days of porting plus several days of hardware debugging and verification**,
with an unpredictable tail on the BLE side.

## 6. Recommended migration plan (de-risking)

Ordered so the biggest risk surfaces **early**, while backing out is still cheap:

1. **Dedicated branch** off `main` (e.g. `upgrade-v3`); never work on `main`, which stays the
   known-good rollback point.
2. **BLE first, in isolation.** Build a minimal firmware (no display) on arduino v3 + NimBLE 2.x
   that ports the three BLE patterns and exercises **WiFi+BLE coexistence** on real hardware. If
   BLE does not hold up, the upgrade stops here and no time was spent on the rest.
3. **Platform + build** for both envs on pioarduino; fix IDF5 warnings/APIs, mbedTLS, Wire. Check
   the binaries still fit the OTA partitions.
4. **Guition**: updated GFX, re-tune display (init / COLMOD / rotation / 40 MHz) and touch.
5. **CYD**: updated TFT_eSPI, re-verify display + XPT2046 touch.
6. **Integration**: everything running together (WiFi + 3 BLE + display + web + OTA), soak test.

## 7. Hardware verification checklist (per subsystem)

- [ ] LiTime BMS: GATT connection, 2 s poll, 104-byte frame decoded, cells/SOC correct
- [ ] Victron SmartSolar: advertisement scan, AES-CTR decryption, model from PID, values
- [ ] Victron Orion: same, plus SmartSolar/Orion discrimination (key-check byte / record type 0x0F)
- [ ] Witmotion IMU: GATT, unlock packet, pitch/roll/yaw angles
- [ ] Coexistence: 3 BLE + WiFi AP + web polling for hours, with no jitter, disconnections or crashes
- [ ] Guition: display (no texture/flicker), touch accurate first try, tabs, screen timeout
- [ ] CYD: display, XPT2046 touch
- [ ] Web: dashboard, `/api/data`, every tab, OTA (upload + reboot)
- [ ] NTP, NVS (keys/MACs/settings), binary size below the OTA slot

## 8. When to do it (decision criterion)

The current stack works. The upgrade is worth it **only with a concrete reason** that exists
*only* on v3, for example:

- a feature or API available only on NimBLE 2.x or IDF5 is needed;
- a future library requires arduino v3;
- you want off NimBLE 1.4.x (EOL) for long-term security and maintenance.

Without one of those it is high risk for low benefit: postpone.

## 9. Practical gotchas (learned on this project)

- Use **`~/.platformio/penv/bin/pio`** — a pyenv `pio` may lack the `lzma` module the pioarduino
  toolchain needs.
- Guition in a crash loop makes uploads fail: **hold BOOT during flashing**.
- Guition USB serial: `-DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1`.
- Checking the display with a webcam:
  `ffmpeg -f avfoundation -video_size 1920x1080 -i "0" -frames:v 20 out.jpg`.
- Do not trust WebSearch/WebFetch summaries for exact data (product IDs, register tables): go to
  the official source (e.g. `pdftotext -layout` on the Victron PDF).

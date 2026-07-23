# Valutazione upgrade allo stack più recente (arduino v3 / NimBLE 2.x / GFX / …)

> Documento di pianificazione **riutilizzabile**. Quando si deciderà di fare l'upgrade,
> partire da qui. Solo valutazione: nessuna modifica al codice finché non si decide.
>
> Redatto: 2026-07-23. Aggiornare le versioni "target" al momento dell'esecuzione.

---

## 1. Stack attuale (known-good, deliberato)

| Componente | Versione attuale | Note |
|---|---|---|
| Piattaforma | `espressif32@6.6.0` | = arduino-esp32 **v2** / **IDF 4.4** |
| BLE | `NimBLE-Arduino @ ^1.4.2` | funziona su v2; **EOL** (non più mantenuta) |
| GFX (Guition) | `GFX Library for Arduino @ 1.4.9` | solo `[env:guition]` |
| Display (CYD) | `TFT_eSPI @ ^2.5.43` | solo `[env:cyd]` |
| JSON | `ArduinoJson @ ^7.0.0` | entrambi gli env |
| Crypto | mbedTLS (nel core ESP32) | AES-128-CTR per advert Victron |

Lo stack è **scelto apposta**: v2 + NimBLE 1.4.2 + GFX 1.4.9 è il "known-good" che evita il
crash BLE su IDF5 (vedi §3). Display Guition e touch sono confermati perfetti su questo stack.

## 2. Stack target (da aggiornare all'esecuzione)

| Componente | Target | Vincolo |
|---|---|---|
| Piattaforma | **pioarduino** platform-espressif32 (arduino-esp32 **v3** / **IDF5**) | l'`espressif32` classico resta a v2 → serve il fork pioarduino |
| BLE | **NimBLE-Arduino 2.x** | obbligatorio su IDF5 (1.4.x non compila/crasha) |
| GFX (Guition) | GFX 1.6.x / 1.7.x | ha init AXS15231B nativi (type1/type2) |
| Display (CYD) | TFT_eSPI ultima 2.5.4x (v3-compatible) | verificare supporto arduino v3 |
| JSON | ArduinoJson 7.x | **invariato, zero lavoro** |

Riferimento piattaforma pioarduino (esempio, usare la release più recente):
`https://github.com/pioarduino/platform-espressif32/releases`

## 3. È "tutto o niente" — e c'è già una prova

I tre upgrade **non sono indipendenti**: sono un unico salto obbligato.
- Arduino v3 non esiste su `espressif32@6.6.0` → serve pioarduino.
- IDF5 rompe NimBLE 1.4.2 → obbligatorio NimBLE 2.x.

**Prova diretta nel progetto:** il branch `guition-v3-wip` andava in **boot-loop / Guru
Meditation LoadProhibited all'init del BT controller** proprio perché aveva arduino v3 con
NimBLE 1.4.2. Non è un bug da fixare: è l'incompatibilità che *impone* NimBLE 2.x. Quel branch
è conservabile come riferimento; la soluzione buona (v2) è su `main`.

## 4. Complessità per componente

| Componente | Sforzo | Motivo |
|---|---|---|
| **NimBLE 1.4.2 → 2.x** | 🔴 Alto | Il collo di bottiglia. API cambiate: `NimBLEAdvertisedDeviceCallbacks`→`NimBLEScanCallbacks`, firme `onResult`, subscribe/notify, gestione address, `getServices/getCharacteristics`. Ci sono **3 pattern BLE distinti** da riportare e ri-testare: scan passivo Victron, GATT attivo BMS, GATT attivo IMU (con le quirks: `getServices(true)`, discovery forzata, base UUID non-standard `9a`, unlock packet). |
| **Piattaforma → pioarduino v3** | 🟡 Medio | Cambio in `platformio.ini` su **entrambi** gli env. Toolchain pioarduino da installare. Qualche API IDF5 da adeguare. |
| **GFX 1.4.9 → 1.6/1.7 (Guition)** | 🟡 Medio-basso | Init AXS15231B nativi potrebbero semplificare o cambiare i costruttori del driver custom (`Arduino_AXS15231B_Guition.h`). Riverificare resa: init/COLMOD 0x55/rotazione software/40 MHz. Rischio di riaprire trama/flicker/touch. |
| **TFT_eSPI (CYD)** | 🟡 Medio | Attriti noti con arduino v3 (API SPI/DMA). Bump + riverifica display CYD. |
| **mbedTLS 2.x → 3.x (Victron AES-CTR)** | 🟡 Basso-medio | IDF5 porta mbedTLS 3.x; possibili API deprecate nella decifratura advert. |
| **Wire / I2C (touch)** | 🟢-🟡 Basso | Nuovo driver I2C master in IDF5; il wrapper `Wire` regge ma va riverificato il touch AXS15231B. |
| **WebServer / WiFi / OTA** | 🟢 Basso | API stabili. |
| **ArduinoJson** | 🟢 Nullo | Già v7. |
| **Partizioni / OTA** | ⚠️ Verifica | Binari v3/IDF5 **più grandi**: confermare che il firmware entri ancora in `min_spiffs.csv` (due slot OTA). Se cresce troppo, rivedere lo schema partizioni. |

## 5. Il vero costo è la RI-VERIFICA, non la scrittura

Il porting del codice è ~2 giorni di lavoro focalizzato. Il grosso del rischio è il **test su
hardware**, non comprimibile:
- **Coesistenza WiFi + 3 connessioni BLE** su IDF5: timing-dependent, bug intermittenti e
  difficili da riprodurre. Su IDF4 oggi funziona; lo scheduler radio di IDF5 è diverso.
- **Display + touch Guition**: da ri-tarare con GFX nuova (rischio di riaprire problemi chiusi).
- **Display CYD** con TFT_eSPI aggiornata.
- **BMS / IMU / Victron**: regressioni da riverificare una per una col rispettivo hardware.

Stima grezza: **~2 giorni di porting + diversi giorni di debug/verifica hardware**, con code di
coda imprevedibili sul BLE.

## 6. Piano di migrazione consigliato (de-risking)

Ordine pensato per far emergere il rischio maggiore **presto** e poter tornare indietro:

1. **Branch dedicato** da `main` (es. `upgrade-v3`), mai lavorare su `main`. `main` resta il
   known-good di rollback.
2. **Prima il BLE, in isolamento.** Fare un firmware minimale (niente display) su arduino v3 +
   NimBLE 2.x che porti i 3 pattern BLE e provi la **coesistenza WiFi+BLE** con l'hardware reale.
   Se il BLE non regge, l'upgrade si ferma qui e non si è buttato tempo sul resto.
3. **Piattaforma + build** entrambi gli env su pioarduino; sistemare warning/API IDF5, mbedTLS,
   Wire. Verificare che i binari entrino nelle partizioni OTA.
4. **Guition**: GFX aggiornata, ri-tarare display (init/COLMOD/rotazione/40 MHz) e touch.
5. **CYD**: TFT_eSPI aggiornata, riverifica display + touch XPT2046.
6. **Integrazione**: tutto acceso insieme (WiFi + 3 BLE + display + web + OTA), soak test.

## 7. Checklist di verifica hardware (per sottosistema)

- [ ] BMS LiTime: connessione GATT, poll 2s, frame 104 byte decodificato, celle/SOC corretti
- [ ] Victron SmartSolar: scan advert, decifratura AES-CTR, modello da PID, valori
- [ ] Victron Orion: idem, distinzione SmartSolar/Orion (key-check byte / record-type 0x0F)
- [ ] IMU Witmotion: GATT, unlock packet, angoli pitch/roll/yaw
- [ ] Coesistenza: 3 BLE + WiFi AP + polling web per ore, senza jitter/disconnessioni/crash
- [ ] Guition: display (no trama/flicker), touch preciso al primo colpo, tab, timeout schermo
- [ ] CYD: display, touch XPT2046
- [ ] Web: dashboard, /api/data, tutte le tab, OTA (upload + reboot)
- [ ] NTP, NVS (chiavi/MAC/settings), dimensione binario < slot OTA

## 8. Quando farlo (criterio di decisione)

Lo stack attuale funziona. Ha senso l'upgrade **solo con una ragione concreta** che esista
*solo* su v3, ad es.:
- serve una feature/API disponibile solo su NimBLE 2.x o IDF5;
- serve una libreria futura che richiede arduino v3;
- si vuole uscire da NimBLE 1.4.x (EOL) per sicurezza/manutenzione a lungo termine.

Senza una di queste, è alto rischio / basso beneficio: rimandare.

## 9. Gotcha pratici (dal lavoro su questo progetto)

- Usare **`~/.platformio/penv/bin/pio`** (il `pio` di pyenv non ha il modulo `lzma`, che serve
  alla toolchain pioarduino).
- Guition in crash-loop → l'upload fallisce: **tenere premuto BOOT durante il flash**.
- Serial USB Guition: `-DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1`.
- Verifica display via webcam: `ffmpeg -f avfoundation -video_size 1920x1080 -i "0" -frames:v 20 out.jpg`.
- Non fidarsi dei riassunti WebSearch/WebFetch per dati precisi (es. PID): usare la fonte
  ufficiale (es. `pdftotext -layout` sul PDF Victron).

---

*Vincoli di progetto: repo locale, autore nantostars, nessun push su repo condivisi.*

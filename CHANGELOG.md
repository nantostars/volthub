# Changelog — volt·hub

Tutte le modifiche rilevanti del firmware. Formato ispirato a
[Keep a Changelog](https://keepachangelog.com/). Versioning **basato sulle change**:
si incrementa `FW_VERSION` (`src/Version.h`) a **ogni commit** e si aggiunge qui una voce.
Schema `0.N` (N = numero della change) fino alla release `1.0`.

La versione corrente è visibile nella schermata **System** del display e nella tab
**System** della dashboard web.

---

## 0.41 — sistema di versioning + changelog
- Aggiunta `FW_VERSION` (`src/Version.h`), mostrata in System su device e web (via `/api/data` → `sys.fw`).
- Creato questo `CHANGELOG.md` con lo storico dalle origini (v0.0).

## 0.40 — Overview: colore valori nodi per stato
- Solar/DC-DC verdi quando in produzione; Loads arancione quando la batteria è in scarica. Device + web.

## 0.39 — Overview: valori centrati e allineati
- SOC % centrato nel ring; V/A incolonnati e centrati sotto il ring; gruppo numero+W centrato nel box con numero allineato alla W (slot fisso 3 cifre) su baseline comune. Cache anti-flicker per nodo.

## 0.38 — Level: refresh in tempo reale
- La bolla segue l'inclinazione con un path di refresh dedicato (~80ms), decoupled dal throttle valori a 300ms.

## 0.37 — chore: gitignore `src/idf_component.yml`
- Manifest auto-generato dal build escluso dal tracking.

## 0.36 — Font: valori grandi in Helvetica-Bold
- I valori grandi (font4/font6) usano FreeSansBold su Guition (Arduino_GFX, baseline-aware) e CYD (TFT_eSPI `setFreeFont`).

## 0.35 — Battery: Delta con soglia d'allarme
- Il Delta celle cambia colore: bianco ≤50mV, arancione 50–100mV, rosso >100mV. Device + web.

## 0.34 — Status bar: separa BLE dall'orario (Guition)
- Ridotta la larghezza del campo BLE per non sovrapporsi all'orario.

## 0.33 — docs: valutazione upgrade stack
- `docs/UPGRADE-latest-stack.md`: piano/rischi per arduino v3 / NimBLE 2.x / GFX / TFT_eSPI.

## 0.32 — Victron: tabella product-id dalla fonte ufficiale
- `victronModelName()` rigenerata verbatim dall'appendice del VE.Direct Protocol PDF (86 MPPT + Orion XS 0xA3F0/0xA3F1). Corretti errori delle fonti non ufficiali.

## 0.31 — Victron: fallback Orion neutro
- Pid Orion sconosciuto → "Orion" invece di "Orion-XS".

## 0.30 — Victron: product-id Orion XS (poi corretti in 0.32)
- Primo tentativo Orion XS da ricerca web.

## 0.29 — Victron: tabella product-id SmartSolar/BlueSolar MPPT
- Modello esatto dal Product ID nelle tab di dettaglio.

## 0.28 — Overview: linee di flusso staccate dal ring
- Gli estremi lato-batteria si fermano fuori dallo stroke del ring.

## 0.27 — Guition: allinea "W" al valore watt (Solar/DC-DC)
- Offset board-specific per i font GFX.

## 0.26 — Guition: ritocchi Overview e Battery
- W allineata, orario spostato, celle Battery valore+V su una riga.

## 0.25 — Guition: display luminoso + QSPI 40 MHz + touch affidabile
- Init table dal BSP del demo (luminosità piena); QSPI a 40 MHz (via trama); touch con comando 11 byte + retry + rilascio a timeout.

## 0.24 — Guition: anti-tearing
- Flush del canvas solo quando il framebuffer cambia.

## 0.23 — Rimosso `src/idf_component.yml`
- Residuo dell'esperimento arduino v3.

## 0.22 — Guition: display FUNZIONANTE su arduino v2
- Canvas software-rotato in PSRAM (pannello portrait nativo 320×480).

## 0.21 — Guition: driver AXS15231B con init del vendor
- Fix schermo nero.

## 0.20 — Nomi: generici ovunque, modello dinamico nei dettagli
- Concetti generici (Solar/DC-DC/Battery) in overview; modello reale solo nelle tab di dettaglio.

## 0.19 — Web Overview: box nodi centrati + corrente A
## 0.18 — Web Overview: nuovo layout (sorgenti sopra, battery centro, loads sotto)
## 0.17 — Web: ring offline chiaro
## 0.16 — Web: sfrutta lo spazio verticale + valori più grandi (mobile)
## 0.15 — Overview (device): connessioni inattive tratteggiate
## 0.14 — Overview (device): ring centrato in verticale
## 0.13 — Overview (device): ring visibile offline + box nodi ingranditi
## 0.12 — Overview (device): unità W a font4 + ring offline
## 0.11 — Overview (device): caratteri di uno step più grandi
## 0.10 — Battery (device): titolo + pill BMS in cima alla card A
## 0.9 — Battery (device): fix layout (card unite / altezza)
## 0.8 — Battery: celle a griglia colorate (da Claude design)
## 0.7 — Status bar: correzione orologio (offset ora)
## 0.6 — Level: eliminato il flicker della bolla (erase-in-place)
## 0.5 — Display: eliminato il flicker del testo (disciplina anti-flicker)
## 0.4 — Web: Dashboard.h col design volt·hub (6 view a tab)
## 0.3 — Display: DisplayUI col design volt·hub (6 schermate)
## 0.2 — Aggiunto riferimento design volt·hub (token + palette + UX)
## 0.1 — Rimosso il demo vendor JC3248W535EN dal tracking git
## 0.0 — Baseline: import del codice sorgente (grafica originale)

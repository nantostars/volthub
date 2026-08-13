# Logging to a microSD card (design note)

> **Reusable planning document — nothing implemented yet.** Written 2026-08-14, when a card was
> first inserted in the Guition. Pick it up from here.
>
> Goal: when a card is present, log **one row per minute** (instead of one every two) and keep a
> long history. When it is absent, keep working exactly as today on internal flash.

---

## 1. Where we start from

Today `DataLogger` writes to **LittleFS on the 128 KB `spiffs` partition**: one row every 2 min,
~79 B/row, ~55 KB/day, so roughly 2 days of history before the free-space rule prunes the oldest
file. Rows are buffered in RAM and flushed every 5 samples. See the `CSV data log` section of
`CLAUDE.md` for the current rules.

The limit is space, not the design: the sampling period, the retention policy and the row set were
all chosen around 128 KB.

## 2. The decisive hardware finding

The Guition's TF slot is **SDMMC in 1-bit mode**, not SPI — from the vendor demo
(`JC3248W535EN/1-Demo/Demo_Arduino/DEMO_MJPEG/pincfg.h`):

| Signal | GPIO |
|---|---|
| `SD_MMC_CMD` | 11 |
| `SD_MMC_CLK` | 12 |
| `SD_MMC_D0`  | 13 |

Two consequences:

- **No bus contention with the display.** The panel is QSPI on SPI2_HOST; the card sits on the
  dedicated SDMMC peripheral. This removes the risk that would otherwise dominate the design.
- **GPIO 11/12/13 are free** in our firmware — the display uses 45/47/21/48/40/39/8/38/1 and the
  touch 8/4. Verified before writing this note.

Use `SD_MMC.setPins(clk, cmd, d0)` then `SD_MMC.begin("/sdcard", true)` (the `true` selects 1-bit
mode) on ESP32-S3.

### The CYD slot is on its own SPI bus too

From the board reference (<https://github.com/chacuavip10/CYD-3.5inch_ESP32-3248S035>):

| Signal | GPIO |
|---|---|
| CLK | 18 |
| MOSI (CMD) | 23 |
| MISO (DAT0) | 19 |
| CS | 5 |

That is **VSPI**, while the display and the touch controller share **HSPI** (14/13/12, CS 15 and 33).
So on the CYD as well the card does **not** contend with the display, and the "no periodic probing"
caveat below turns out not to apply to either board. The two backends differ only in the driver
(`SD_MMC` 1-bit on the Guition, `SD` over SPI on the CYD), which the `Backend` struct already
absorbs — it holds an `fs::FS*` and does not care how the medium was mounted.

## 3. The abstraction: one already exists

`LittleFS` and `SD_MMC` both derive from `fs::FS`, and `DataLogger` already funnels every access
through `open` / `exists` / `remove`. So the logger holds a `fs::FS*` instead of naming LittleFS
directly. The only thing missing from the base class is capacity, which a small per-backend struct
covers:

```
Backend { fs::FS* fs; total(); used(); label; periodMs; retentionPolicy; }
```

No custom wrapper, no `#ifdef` in the sampling path: rotation, buffering and row building stay as
they are.

## 4. Selection logic

| At boot | Backend | Period | Retention |
|---|---|---|---|
| Card mounts | **SD** | 60 s | by age, with a size cap as a runaway guard |
| No card, or mount fails | **internal flash** | 120 s | current free-space rule (15 KB floor) |
| Neither available | logging stays idle, reported as an error | — | — |

**No continuous polling.** Detect once at boot, plus a *rescan* action in the web System tab. An
optional slow retry (a mount attempt every few minutes, only while logging is enabled and the card
is absent) is affordable on **both** boards, since neither shares a bus with the display — a
constraint I expected to have and turned out not to. Still keep it slow: the point is convenience
("insert the card and it starts"), not detection latency.

## 5. Removal and failures

This is the case that deserves the most care: in a camper the card gets pulled out to read it.

- A failed write marks the card lost and **falls back to internal flash**, so logging continues
  instead of dying silently.
- A backend switch **starts a new file**, so no single file ever mixes two sampling cadences. The
  suffix mechanism added in 0.76 for column changes (`volthub_YYYYMMDD_1.csv`) covers this too.
- The state must be visible in the **web System tab**, not only in the serial log: *"card absent —
  writing to internal flash"* is something the user needs to see.
- Keep closing the file after every flush. On FAT that is the difference between losing the five
  buffered rows and losing the file.

## 6. Retention on the card

At one row per minute a day is about **115 KB**; a full year is roughly **42 MB** — nothing on any
card. So the policy is not "delete old files" but **"keep everything, with a safety cap"**: e.g. at
most 400 files or 500 MB, pruning the oldest only beyond that. In practice it never deletes, while
the device still cannot fill a card on its own if something misbehaves. `volthub_YYYYMMDD.csv`
sorts lexicographically, so pruning stays trivial.

## 7. Interface

- `/api/logs` reports **which medium is in use** and its capacity, not just the file list.
- Files already written to internal flash must stay downloadable after a card is inserted: list
  both, tagged with their source, and add a source parameter to the download endpoint.

## 8. An option this opens

With a card, space stops being the constraint that dictated the column set. Per-cell voltages,
yield, off reason could all be logged. Worth deciding separately — and note that changing columns
deliberately triggers the schema-suffix path, so old files are never mixed with new ones.

## 9. Open questions

1. ~~CYD pins unknown~~ — **resolved** (see above): own SPI bus, no contention. What remains to be
   checked on that board is only whether `SPI.begin()` on VSPI coexists cleanly with TFT_eSPI's
   HSPI instance, which is routine but untested here.
2. **Current draw**: a card in write can pull ~100 mA in bursts. Worth watching on a camper supply —
   a marginal 5 V rail is what once made the display look washed out.
3. **FAT32 is required.** An exFAT card will simply fail to mount and look broken; document it.
4. Whether the internal-flash period should stay at 120 s once a card is the normal case, or drop to
   60 s for consistency at the cost of halving the fallback history.

## 10. Order of work (de-risking)

1. **Bring-up in isolation**: mount the card and write a test file, checking that display, touch and
   BLE are unaffected. Same approach as the display bring-up — surface the hardware risk first,
   before building anything on top.
2. Backend abstraction in `DataLogger` (still LittleFS only, no behaviour change).
3. Selection, fallback and per-backend policies.
4. Web System tab: medium in use, capacity, rescan, downloads from both media.

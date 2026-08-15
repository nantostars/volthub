#pragma once
#include <Arduino.h>
#include <FS.h>
#include "LitimeBMS.h"
#include "VictronBLE.h"

// ─── CSV data logger ─────────────────────────────────────────────────────────
//
// Writes one row per sample to a CSV file, on a microSD card when one is present and on the
// internal flash otherwise. The two media differ only in capacity, so everything that depends on
// capacity — sampling period, retention, directory layout — is a property of the active medium
// and nothing else in the class branches on it.
//
// Sampling is one row a minute on BOTH media; only capacity-driven behaviour differs:
//   SD    : /volthub/YYYY/MM/volthub_YYYYMMDD.csv, one file a day, keep a year
//   flash : /volthub_YYYYMMDD[_N].csv, rolled over every 40 KB, ~17 h of history in 128 KB
//
// Card wiring (both slots sit on their own bus — neither contends with the display):
//   Guition : SDMMC 1-bit, CLK 12 / CMD 11 / D0 13   (dedicated peripheral, display is QSPI)
//   CYD     : SPI on VSPI, SCK 18 / MISO 19 / MOSI 23 / CS 5   (display+touch are on HSPI)
//
// Time: rows are useless without a clock and the filename needs a date, so logging stays idle
// until the time is valid (NTP, or pushed by the browser via POST /api/time).

#define LOG_PERIOD_MS        60000UL    // one sample a minute, on either medium
#define LOG_FLUSH_ROWS       5          // buffer this many rows between writes
#define LOG_ROW_MAX          160        // generous upper bound for one CSV row

// Retention. On the card it is driven by AGE plus a free-space floor: a count cap would be an
// arbitrary number, and the card may hold other things besides our logs.
#define LOG_SD_KEEP_DAYS     365
#define LOG_SD_MIN_FREE      (100ULL * 1024 * 1024)   // keep 100 MB free on the card
// On internal flash space is the only real limit (~111 KB/day at one row a minute, against
// ~110 KB usable), so the free-space rule is what actually fires; the count cap is a guard
// against a jumping clock spawning many small files.
#define LOG_FLASH_MAX_FILES  3
#define LOG_FLASH_MIN_FREE   15000UL
// At one row a minute a whole day is ~111 KB, more than the ~95 KB usable here — and the pruner
// never deletes the file it is writing, so a day-long file would fill the partition and stop the
// log. On flash we therefore also roll over by SIZE, keeping a couple of files the pruner can
// actually delete: ~8.5 h per file, so the rolling window is ~17 h.
#define LOG_FLASH_MAX_FILE   40960UL

#define LOG_SD_ROOT          "/volthub"

class DataLogger {
public:
    enum State  : uint8_t { LOG_OFF = 0, LOG_WAIT_TIME, LOG_ACTIVE, LOG_ERROR };
    enum Medium : uint8_t { MED_NONE = 0, MED_FLASH, MED_SD };

    void begin(bool enabled);
    void setEnabled(bool en);
    bool enabled() const { return _enabled; }

    // Call from loop(); cheap when it is not time to sample yet.
    void update(uint32_t nowMs, const BmsData& b, const SolarData& s, const OrionData& o);
    // Write anything still buffered (call before a reboot: settings save, OTA).
    void flush();

    // Try to mount a card now (the web "rescan" action). True if the SD is active afterwards.
    bool rescan();
    // Flush, close and unmount so the card can be pulled safely; falls back to internal flash.
    bool eject();

    State       state() const { return _state; }
    const char* stateName() const;
    Medium      medium() const { return _medium; }
    const char* mediumName() const;              // "sd" | "flash" | "none"
    const char* currentFile() const { return _file; }
    uint32_t    rowsWritten() const { return _rows; }
    uint32_t    periodMs() const { return LOG_PERIOD_MS; }
    uint64_t    freeBytes() const;
    uint64_t    totalBytes() const;

    // The listing/download handlers walk the filesystem themselves.
    fs::FS*     fs() const { return _fs; }
    const char* rootDir() const { return _medium == MED_SD ? LOG_SD_ROOT : "/"; }
    bool        onSd() const { return _medium == MED_SD; }

    // Delete every log older than `days`; returns how many files were removed.
    uint16_t    purgeOlderThan(uint16_t days);

    static const char* header();

private:
    bool mountSd();
    void unmountSd();
    bool mountFlash();
    bool timeValid() const;
    void buildRow(char* dst, size_t n, const BmsData& b, const SolarData& s, const OrionData& o);
    void rotateIfNeeded();
    bool ensureDir(const char* path);
    void makePath(char* dst, size_t n, const char* day, int suffix) const;
    void prune();
    void pruneFlash();
    void pruneSd();

    fs::FS*  _fs        = nullptr;
    Medium   _medium    = MED_NONE;
    bool     _enabled   = false;
    bool     _sdFailed  = false;    // a write failed: do not keep retrying this session
    State    _state     = LOG_OFF;
    char     _file[64]  = {0};      // full path of the open file
    char     _day[9]    = {0};      // "YYYYMMDD" of that file
    uint32_t _rows      = 0;
    uint32_t _lastMs    = 0;
    int      _seq       = 0;      // suffix of the open file within the day
    char     _buf[LOG_ROW_MAX * LOG_FLUSH_ROWS + 8] = {0};
    size_t   _len       = 0;
};

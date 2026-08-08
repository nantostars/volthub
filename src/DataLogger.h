#pragma once
#include <Arduino.h>
#include "LitimeBMS.h"
#include "VictronBLE.h"

// ─── CSV data logger ─────────────────────────────────────────────────────────
//
// Appends one row every LOG_PERIOD_MS to /volthub_YYYYMMDD.csv on LittleFS.
//
// Storage: the existing `spiffs` partition (128 KB from min_spiffs.csv) — LittleFS mounts that
// label by default, so NO repartitioning and no USB flash are needed. LittleFS rather than
// SPIFFS on purpose: on a nearly-full filesystem (the normal state of a rotating log) SPIFFS
// garbage collection can stall the device for seconds, LittleFS does not.
//
// Budget: a row is ~85 bytes, 720 rows/day at 2 min → ~60 KB/day against ~110 KB usable, so
// roughly 1.8 days fit. Rows are buffered in RAM and flushed every LOG_FLUSH_ROWS samples, i.e.
// one flash write every 10 minutes instead of 720 a day.
//
// Time: rows are useless without a clock and the filename needs a date, so logging stays idle
// until the time is valid (NTP, or pushed by the browser via POST /api/time).

#define LOG_PERIOD_MS     120000UL   // one sample every 2 minutes
#define LOG_FLUSH_ROWS    5          // → one flash write every 10 minutes
// Retention is decided by FREE SPACE, not by file count: ~60 KB/day against ~110 KB usable
// means roughly 1.8 days fit, so at most 2 files ever coexist and a count cap can never fire in
// normal operation. LOG_MAX_FILES is only a guard against a pathological case — a clock that
// jumps between dates creates several SMALL files, which stay under the free-space threshold for
// a while and would otherwise clutter the list.
#define LOG_MAX_FILES     3          // clutter guard, NOT the retention policy
#define LOG_MIN_FREE      15000UL    // real retention rule: prune the oldest below this
#define LOG_ROW_MAX       160        // generous upper bound for one CSV row

class DataLogger {
public:
    enum State : uint8_t {
        LOG_OFF = 0,        // disabled by the user
        LOG_WAIT_TIME,      // enabled, but the clock is not set yet
        LOG_ACTIVE,         // writing
        LOG_ERROR           // filesystem unavailable / full
    };

    void begin(bool enabled);
    void setEnabled(bool en);
    bool enabled() const { return _enabled; }

    // Call from loop(); cheap when it is not time to sample yet.
    void update(uint32_t nowMs, const BmsData& b, const SolarData& s, const OrionData& o);

    // Write anything still buffered (call before a reboot: settings save, OTA).
    void flush();

    State       state() const { return _state; }
    const char* stateName() const;
    const char* currentFile() const { return _file; }
    uint32_t    rowsWritten() const { return _rows; }
    size_t      freeBytes() const;
    size_t      totalBytes() const;

    // CSV column legend, also shown in the web System tab so the numeric codes are readable.
    static const char* header();

private:
    bool mount();
    bool timeValid() const;
    void buildRow(char* dst, size_t n, const BmsData& b, const SolarData& s, const OrionData& o);
    void rotateIfNeeded();
    void prune();

    bool     _enabled   = false;
    bool     _mounted   = false;
    State    _state     = LOG_OFF;
    char     _file[32]  = {0};      // "/volthub_YYYYMMDD.csv"
    char     _day[9]    = {0};      // "YYYYMMDD" of the open file
    uint32_t _rows      = 0;
    uint32_t _lastMs    = 0;
    char     _buf[LOG_ROW_MAX * LOG_FLUSH_ROWS + 8] = {0};
    size_t   _len       = 0;
};

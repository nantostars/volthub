#include "DataLogger.h"
#include <LittleFS.h>
#include <time.h>

// Column legend. Statuses are numeric codes to keep rows small:
//   batt_st : 1 = charging, 0 = idle, -1 = discharging (same ±8 W thresholds as the UI)
//   sol_st / dc_st : official VE.Direct CS codes (3 = Bulk, 4 = Absorption, 5 = Float, …)
// Empty field = value not available (kept empty on purpose so Excel/pandas read it as missing
// instead of a real 0 that would poison averages).
static const char* CSV_HEADER =
    "datetime,batt_soc,batt_v,batt_a,batt_temp,batt_st,"
    "sol_batt_v,sol_batt_a,sol_st,"
    "dc_alt_v,dc_alt_a,dc_batt_v,dc_batt_a,dc_st\n";

const char* DataLogger::header() { return CSV_HEADER; }

// Append "value," or just "," when the reading is unavailable.
static void addF(char* dst, size_t n, size_t& len, float v, int dec, bool last = false) {
    if (len >= n - 2) return;
    if (!isnan(v)) len += snprintf(dst + len, n - len, "%.*f", dec, v);
    len += snprintf(dst + len, n - len, last ? "\n" : ",");
}
static void addI(char* dst, size_t n, size_t& len, long v, bool valid, bool last = false) {
    if (len >= n - 2) return;
    if (valid) len += snprintf(dst + len, n - len, "%ld", v);
    len += snprintf(dst + len, n - len, last ? "\n" : ",");
}

bool DataLogger::timeValid() const {
    return time(nullptr) > 1600000000L;   // ~2020: anything below means the clock is unset
}

bool DataLogger::mount() {
    if (_mounted) return true;
    // formatOnFail: the partition has never been used, so the first mount formats it.
    _mounted = LittleFS.begin(true);
    if (!_mounted) Serial.println("[log] LittleFS mount FAILED");
    else Serial.printf("[log] LittleFS ok — %u/%u bytes used\n",
                       (unsigned)LittleFS.usedBytes(), (unsigned)LittleFS.totalBytes());
    return _mounted;
}

void DataLogger::begin(bool enabled) {
    _enabled = enabled;
    _state   = enabled ? LOG_WAIT_TIME : LOG_OFF;
    if (enabled && !mount()) _state = LOG_ERROR;
}

void DataLogger::setEnabled(bool en) {
    if (en == _enabled) return;
    if (!en) flush();                       // don't lose buffered rows when switching off
    _enabled = en;
    if (!en) { _state = LOG_OFF; return; }
    _state = mount() ? LOG_WAIT_TIME : LOG_ERROR;
}

size_t DataLogger::freeBytes() const {
    if (!_mounted) return 0;
    size_t t = LittleFS.totalBytes(), u = LittleFS.usedBytes();
    return t > u ? t - u : 0;
}
size_t DataLogger::totalBytes() const { return _mounted ? LittleFS.totalBytes() : 0; }

const char* DataLogger::stateName() const {
    switch (_state) {
        case LOG_OFF:       return "off";
        case LOG_WAIT_TIME: return "waiting for clock";
        case LOG_ACTIVE:    return "logging";
        default:            return "storage error";
    }
}

// Retention = keep LOG_MIN_FREE bytes free. That is the rule that actually runs: ~60 KB/day on a
// 128 KB partition means about 1.8 days, so only ~2 files ever coexist. LOG_MAX_FILES is not a
// retention policy, it is a guard for the case where a wrong/jumping clock spawns several small
// files that individually would not trip the space rule.
void DataLogger::prune() {
    if (!_mounted) return;
    for (int guard = 0; guard < 16; guard++) {
        int  count = 0;
        char oldest[32] = {0};
        File dir = LittleFS.open("/");
        for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
            const char* n = f.name();
            if (strncmp(n, "volthub_", 8) != 0 && strncmp(n, "/volthub_", 9) != 0) continue;
            count++;
            const char* bare = (n[0] == '/') ? n + 1 : n;
            if (!oldest[0] || strcmp(bare, oldest + 1) < 0) snprintf(oldest, sizeof(oldest), "/%s", bare);
        }
        dir.close();
        bool tooMany = count > LOG_MAX_FILES;
        bool tooFull = freeBytes() < LOG_MIN_FREE;
        if ((!tooMany && !tooFull) || !oldest[0] || count <= 1) return;
        // Never delete the file we are currently writing unless it is the only way to free space.
        if (strcmp(oldest, _file) == 0 && !tooFull) return;
        Serial.printf("[log] pruning %s (files=%d free=%u)\n", oldest, count, (unsigned)freeBytes());
        LittleFS.remove(oldest);
    }
}

void DataLogger::rotateIfNeeded() {
    time_t now = time(nullptr);
    struct tm ti; localtime_r(&now, &ti);
    char day[9];
    strftime(day, sizeof(day), "%Y%m%d", &ti);
    if (strcmp(day, _day) == 0 && _file[0]) return;   // same day, file already open

    flush();                                          // close out the previous day cleanly
    strncpy(_day, day, sizeof(_day) - 1);
    snprintf(_file, sizeof(_file), "/volthub_%s.csv", day);
    if (!LittleFS.exists(_file)) {
        File f = LittleFS.open(_file, FILE_WRITE);
        if (f) { f.print(CSV_HEADER); f.close(); }
        Serial.printf("[log] new file %s\n", _file);
    }
    prune();
}

void DataLogger::buildRow(char* dst, size_t n, const BmsData& b,
                          const SolarData& s, const OrionData& o) {
    size_t len = 0;
    time_t now = time(nullptr);
    struct tm ti; localtime_r(&now, &ti);
    len += strftime(dst, n, "%Y-%m-%d %H:%M:%S,", &ti);

    // battery
    addI(dst, n, len, (long)b.soc, b.valid);
    addF(dst, n, len, b.valid ? b.voltage : NAN, 2);
    addF(dst, n, len, b.valid ? b.current : NAN, 1);
    addI(dst, n, len, (long)b.cellTemp, b.valid);
    long st = 0;
    if (b.valid) st = (b.power > 8.0f) ? 1 : (b.power < -8.0f ? -1 : 0);
    addI(dst, n, len, st, b.valid);
    // solar (battery side; the advertisement carries no PV voltage/current)
    addF(dst, n, len, s.valid ? s.battVoltage   : NAN, 2);
    addF(dst, n, len, s.valid ? s.chargeCurrent : NAN, 1);
    addI(dst, n, len, (long)s.chargeState, s.valid);
    // dc-dc: alternator side then service-battery side
    addF(dst, n, len, o.valid ? o.inVoltage  : NAN, 2);
    addF(dst, n, len, o.valid ? o.inCurrent  : NAN, 1);
    addF(dst, n, len, o.valid ? o.outVoltage : NAN, 2);
    addF(dst, n, len, o.valid ? o.outCurrent : NAN, 1);
    addI(dst, n, len, (long)o.deviceState, o.valid, true);
}

void DataLogger::flush() {
    if (!_len) return;
    if (!_mounted || !_file[0]) { _len = 0; return; }
    File f = LittleFS.open(_file, FILE_APPEND);
    if (!f) { Serial.println("[log] append failed"); _state = LOG_ERROR; _len = 0; return; }
    f.write((const uint8_t*)_buf, _len);
    f.close();
    _len = 0;
    if (freeBytes() < LOG_MIN_FREE) prune();
}

void DataLogger::update(uint32_t nowMs, const BmsData& b, const SolarData& s, const OrionData& o) {
    if (!_enabled) return;
    if (!_mounted && !mount()) { _state = LOG_ERROR; return; }
    if (!timeValid()) { _state = LOG_WAIT_TIME; return; }   // no clock → no filename, no rows
    if (_lastMs && (nowMs - _lastMs) < LOG_PERIOD_MS) return;
    _lastMs = nowMs;

    rotateIfNeeded();
    if (_state != LOG_ERROR) _state = LOG_ACTIVE;

    char row[LOG_ROW_MAX];
    buildRow(row, sizeof(row), b, s, o);
    size_t rl = strlen(row);
    if (_len + rl >= sizeof(_buf)) flush();
    memcpy(_buf + _len, row, rl);
    _len += rl;
    _rows++;
    if (_rows % LOG_FLUSH_ROWS == 0) flush();
}

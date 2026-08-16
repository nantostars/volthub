#include "DataLogger.h"
#include <LittleFS.h>
#include <time.h>

#ifdef BOARD_GUITION
  #include <SD_MMC.h>
  // SDMMC 1-bit, from the vendor demo pincfg.h. Dedicated peripheral: the display is on QSPI,
  // so the card never contends with it.
  static const int SD_PIN_CLK = 12, SD_PIN_CMD = 11, SD_PIN_D0 = 13;
#else
  #include <SD.h>
  #include <SPI.h>
  // VSPI, separate from the display + touch which share HSPI (14/13/12).
  static const int SD_PIN_SCK = 18, SD_PIN_MISO = 19, SD_PIN_MOSI = 23, SD_PIN_CS = 5;
  static SPIClass sdSpi(VSPI);
#endif

// Column legend. Statuses are numeric codes to keep rows small:
//   batt_st : 1 = charging, 0 = idle, -1 = discharging (same ±8 W thresholds as the UI)
//   sol_st / dc_st : official VE.Direct CS codes (3 = Bulk, 4 = Absorption, 5 = Float, …)
// Empty field = value not available (kept empty on purpose so Excel/pandas read it as missing
// instead of a real 0 that would poison averages).
static const char* CSV_HEADER =
    "datetime,batt_soc,batt_ah,batt_v,batt_a,batt_temp,batt_st,"
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

// The last path component: SD returns full paths from openNextFile(), LittleFS bare names.
static const char* baseName(const char* p) {
    const char* s = strrchr(p, '/');
    return s ? s + 1 : p;
}

// "volthub_YYYYMMDD.csv" (or "..._N.csv") → YYYYMMDD as a number, 0 if it is not one of ours.
static uint32_t dayOfName(const char* name) {
    if (strncmp(name, "volthub_", 8) != 0) return 0;
    uint32_t d = 0;
    for (int i = 8; i < 16; i++) {
        if (name[i] < '0' || name[i] > '9') return 0;
        d = d * 10 + (uint32_t)(name[i] - '0');
    }
    return d;
}

bool DataLogger::timeValid() const {
    return time(nullptr) > 1600000000L;   // ~2020: anything below means the clock is unset
}

// ─── media ───────────────────────────────────────────────────────────────────

bool DataLogger::mountSd() {
#ifdef BOARD_GUITION
    if (!SD_MMC.setPins(SD_PIN_CLK, SD_PIN_CMD, SD_PIN_D0)) return false;
    if (!SD_MMC.begin("/sdcard", true)) return false;          // true = 1-bit mode
    if (SD_MMC.cardType() == CARD_NONE) { SD_MMC.end(); return false; }
    _fs = &SD_MMC;
    Serial.printf("[log] SD mounted (SDMMC 1-bit) - %llu MB\n", SD_MMC.totalBytes() / (1024ULL * 1024));
#else
    sdSpi.begin(SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);
    if (!SD.begin(SD_PIN_CS, sdSpi)) { sdSpi.end(); return false; }
    if (SD.cardType() == CARD_NONE) { SD.end(); sdSpi.end(); return false; }
    _fs = &SD;
    Serial.printf("[log] SD mounted (SPI) - %llu MB\n", SD.totalBytes() / (1024ULL * 1024));
#endif
    _medium = MED_SD;
    return true;
}

void DataLogger::unmountSd() {
    if (_medium != MED_SD) return;
#ifdef BOARD_GUITION
    SD_MMC.end();
#else
    SD.end();
    sdSpi.end();
#endif
    _fs = nullptr;
    _medium = MED_NONE;
}

bool DataLogger::mountFlash() {
    // formatOnFail: the partition has never been used, so the first mount formats it.
    if (!LittleFS.begin(true)) { Serial.println("[log] LittleFS mount FAILED"); return false; }
    _fs = &LittleFS;
    _medium = MED_FLASH;
    Serial.printf("[log] internal flash - %u/%u bytes used\n",
                  (unsigned)LittleFS.usedBytes(), (unsigned)LittleFS.totalBytes());
    return true;
}

uint64_t DataLogger::totalBytes() const {
    if (_medium == MED_FLASH) return LittleFS.totalBytes();
#ifdef BOARD_GUITION
    if (_medium == MED_SD) return SD_MMC.totalBytes();
#else
    if (_medium == MED_SD) return SD.totalBytes();
#endif
    return 0;
}

uint64_t DataLogger::freeBytes() const {
    uint64_t t = totalBytes(), u = 0;
    if (_medium == MED_FLASH) u = LittleFS.usedBytes();
#ifdef BOARD_GUITION
    else if (_medium == MED_SD) u = SD_MMC.usedBytes();
#else
    else if (_medium == MED_SD) u = SD.usedBytes();
#endif
    return t > u ? t - u : 0;
}

const char* DataLogger::mediumName() const {
    switch (_medium) { case MED_SD: return "sd"; case MED_FLASH: return "flash"; default: return "none"; }
}

const char* DataLogger::stateName() const {
    switch (_state) {
        case LOG_OFF:       return "off";
        case LOG_WAIT_TIME: return "waiting for clock";
        case LOG_ACTIVE:    return "logging";
        default:            return "storage error";
    }
}

// ─── lifecycle ───────────────────────────────────────────────────────────────

void DataLogger::begin(bool enabled) {
    _enabled = enabled;
    if (!enabled) { _state = LOG_OFF; return; }        // nothing is mounted until it is needed
    if (!mountSd() && !mountFlash()) { _state = LOG_ERROR; return; }
    _state = LOG_WAIT_TIME;
}

void DataLogger::setEnabled(bool en) {
    if (en == _enabled) return;
    if (!en) {                                         // switching off: leave nothing open
        flush();
        unmountSd();
        _enabled = false; _state = LOG_OFF; _file[0] = 0; _day[0] = 0;
        return;
    }
    _enabled = true; _sdFailed = false;
    if (_medium == MED_NONE && !mountSd() && !mountFlash()) { _state = LOG_ERROR; return; }
    _state = LOG_WAIT_TIME;
}

// The card stopped answering (pulled out, or a bad contact). Drop to internal flash and KEEP the
// buffered rows: they are valid samples, they just have to land somewhere else.
void DataLogger::loseCard(const char* why) {
    Serial.printf("[log] card lost (%s) - falling back to internal flash\n", why);
    unmountSd();
    _sdFailed = true;                       // do not grab it back silently; use rescan()
    _file[0] = 0; _day[0] = 0; _seq = 0;    // next sample opens a file on flash
    if (_enabled && !mountFlash()) _state = LOG_ERROR;
}

// A removed card makes totalBytes() read 0 while the driver times out on every request, so this
// is both the detection and the reason to unmount quickly: it stops the 1 Hz error storm.
void DataLogger::poll() {
    if (_medium != MED_SD) return;
    uint32_t now = millis();
    if (_lastPollMs && (now - _lastPollMs) < 2000) return;
    _lastPollMs = now;
    if (totalBytes() == 0) loseCard("no response");
}

bool DataLogger::rescan() {
    if (_medium == MED_SD) return true;                // already on a card
    flush();
    if (_medium == MED_FLASH) { LittleFS.end(); _fs = nullptr; _medium = MED_NONE; }
    _sdFailed = false;
    _file[0] = 0; _day[0] = 0;                         // next sample opens a file on the new medium
    bool got = mountSd();
    if (!got) mountFlash();
    _state = _enabled ? LOG_WAIT_TIME : LOG_OFF;
    return got;
}

bool DataLogger::eject() {
    if (_medium != MED_SD) return false;
    flush();
    unmountSd();
    _file[0] = 0; _day[0] = 0;
    _sdFailed = true;                                  // do not silently grab it back
    if (_enabled) _state = mountFlash() ? LOG_WAIT_TIME : LOG_ERROR;   // keep logging meanwhile
    Serial.println("[log] card unmounted - safe to remove");
    return true;
}

// ─── paths ───────────────────────────────────────────────────────────────────

bool DataLogger::ensureDir(const char* path) {
    if (!_fs || _fs->exists(path)) return true;
    return _fs->mkdir(path);
}

// SD gets /volthub/YYYY/MM/... : a flat directory with hundreds of entries is slow to scan and
// unpleasant to read on a computer. Internal flash holds ~2 files, so it stays flat.
void DataLogger::makePath(char* dst, size_t n, const char* day, int suffix) const {
    char stem[32];
    if (suffix == 0) snprintf(stem, sizeof(stem), "volthub_%s.csv", day);
    else             snprintf(stem, sizeof(stem), "volthub_%s_%d.csv", day, suffix);
    if (_medium == MED_SD) snprintf(dst, n, "%s/%.4s/%.2s/%s", LOG_SD_ROOT, day, day + 4, stem);
    else                   snprintf(dst, n, "/%s", stem);
}

// ─── rotation ────────────────────────────────────────────────────────────────

// True when the file's first line is exactly the current header, i.e. same column set.
static bool headerMatches(fs::FS* fs, const char* path) {
    File f = fs->open(path, FILE_READ);
    if (!f) return false;
    String first = f.readStringUntil('\n');
    f.close();
    first.trim();
    String want(CSV_HEADER); want.trim();
    return first == want;
}

void DataLogger::rotateIfNeeded() {
    time_t now = time(nullptr);
    struct tm ti; localtime_r(&now, &ti);
    char day[9];
    strftime(day, sizeof(day), "%Y%m%d", &ti);
    if (strcmp(day, _day) == 0 && _file[0]) {
        // Same day. On flash also roll over by size, so the pruner always has an older file to
        // remove; on a card a daily file is ~111 KB and stays whole.
        if (_medium != MED_FLASH) return;
        File cur = _fs->open(_file, FILE_READ);
        size_t sz = cur ? cur.size() : 0;
        if (cur) cur.close();
        if (sz < LOG_FLASH_MAX_FILE) return;
        flush();
        _seq++;
    } else {
        _seq = 0;
    }

    flush();                                          // close out the previous day cleanly
    strncpy(_day, day, sizeof(_day) - 1);

    if (_medium == MED_SD) {                          // /volthub, /volthub/YYYY, /volthub/YYYY/MM
        char dir[48];
        ensureDir(LOG_SD_ROOT);
        snprintf(dir, sizeof(dir), "%s/%.4s", LOG_SD_ROOT, day);              ensureDir(dir);
        snprintf(dir, sizeof(dir), "%s/%.4s/%.2s", LOG_SD_ROOT, day, day + 4); ensureDir(dir);
    }

    // Pick a name whose existing header matches the current column set. After a firmware update
    // that changes the columns - or after switching medium mid-day - appending to a file written
    // with another layout would silently corrupt it, so move to a suffixed name instead.
    for (int i = _seq; i < _seq + 10; i++) {
        makePath(_file, sizeof(_file), day, i);
        if (!_fs->exists(_file)) {
            File f = _fs->open(_file, FILE_WRITE);
            if (f) { f.print(CSV_HEADER); f.close(); }
            Serial.printf("[log] new file %s\n", _file);
            break;
        }
        if (headerMatches(_fs, _file)) {              // same schema
            if (_medium != MED_FLASH) break;          // card: keep appending
            File c = _fs->open(_file, FILE_READ);     // flash: only if there is room left in it
            size_t sz = c ? c.size() : 0; if (c) c.close();
            if (sz < LOG_FLASH_MAX_FILE) break;
            continue;                                  // full -> try the next suffix
        }
        Serial.printf("[log] %s has a different column set, trying next name\n", _file);
    }
    prune();
}

// ─── retention ───────────────────────────────────────────────────────────────

void DataLogger::prune() { if (_medium == MED_SD) pruneSd(); else pruneFlash(); }

// Internal flash: keep LOG_FLASH_MIN_FREE bytes free. That is the rule that actually runs
// (~60 KB/day on a 128 KB partition => ~2 days, so only ~2 files coexist). LOG_FLASH_MAX_FILES is
// a guard for a wrong/jumping clock spawning several small files.
void DataLogger::pruneFlash() {
    if (!_fs) return;
    for (int guard = 0; guard < 16; guard++) {
        int  count = 0;
        char oldest[64] = {0};
        File dir = _fs->open("/");
        for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
            const char* bare = baseName(f.name());
            if (!dayOfName(bare)) continue;
            count++;
            if (!oldest[0] || strcmp(bare, oldest + 1) < 0) snprintf(oldest, sizeof(oldest), "/%s", bare);
        }
        dir.close();
        bool tooMany = count > LOG_FLASH_MAX_FILES;
        bool tooFull = freeBytes() < LOG_FLASH_MIN_FREE;
        if ((!tooMany && !tooFull) || !oldest[0] || count <= 1) return;
        if (strcmp(oldest, _file) == 0 && !tooFull) return;
        Serial.printf("[log] pruning %s (files=%d free=%u)\n", oldest, count, (unsigned)freeBytes());
        _fs->remove(oldest);
    }
}

// Card: retention is by AGE, plus a free-space floor so the device never fills a card that may
// hold other things. Walking /volthub/YYYY/MM stays cheap because the tree is small and ordered.
void DataLogger::pruneSd() {
    if (!_fs || !timeValid()) return;
    time_t cutoff = time(nullptr) - (time_t)LOG_SD_KEEP_DAYS * 86400;
    struct tm ct; localtime_r(&cutoff, &ct);
    char cutDay[9]; strftime(cutDay, sizeof(cutDay), "%Y%m%d", &ct);
    uint32_t cutNum = strtoul(cutDay, nullptr, 10);
    bool needSpace = freeBytes() < LOG_SD_MIN_FREE;

    File root = _fs->open(LOG_SD_ROOT);
    if (!root) return;
    for (File y = root.openNextFile(); y; y = root.openNextFile()) {
        if (!y.isDirectory()) continue;
        char ypath[48]; snprintf(ypath, sizeof(ypath), "%s/%s", LOG_SD_ROOT, baseName(y.name()));
        File ydir = _fs->open(ypath);
        if (!ydir) continue;
        for (File m = ydir.openNextFile(); m; m = ydir.openNextFile()) {
            if (!m.isDirectory()) continue;
            char mpath[64]; snprintf(mpath, sizeof(mpath), "%s/%s", ypath, baseName(m.name()));
            File mdir = _fs->open(mpath);
            if (!mdir) continue;
            for (File f = mdir.openNextFile(); f; f = mdir.openNextFile()) {
                const char* bare = baseName(f.name());
                uint32_t d = dayOfName(bare);
                if (!d) continue;
                if (d >= cutNum && !needSpace) continue;
                char full[96]; snprintf(full, sizeof(full), "%s/%s", mpath, bare);
                if (strcmp(full, _file) == 0) continue;        // never the file we are writing
                Serial.printf("[log] pruning %s\n", full);
                _fs->remove(full);
                if (needSpace) needSpace = freeBytes() < LOG_SD_MIN_FREE;
            }
            mdir.close();
        }
        ydir.close();
    }
    root.close();
}

uint16_t DataLogger::purgeOlderThan(uint16_t days) {
    if (!_fs || !timeValid()) return 0;
    time_t cutoff = time(nullptr) - (time_t)days * 86400;
    struct tm ct; localtime_r(&cutoff, &ct);
    char cutDay[9]; strftime(cutDay, sizeof(cutDay), "%Y%m%d", &ct);
    uint32_t cutNum = strtoul(cutDay, nullptr, 10);
    uint16_t removed = 0;
    flush();

    if (_medium != MED_SD) {                                   // flat layout
        File dir = _fs->open("/");
        for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
            const char* bare = baseName(f.name());
            uint32_t d = dayOfName(bare);
            if (!d || d >= cutNum) continue;
            char full[64]; snprintf(full, sizeof(full), "/%s", bare);
            if (strcmp(full, _file) == 0) continue;
            if (_fs->remove(full)) removed++;
        }
        dir.close();
        return removed;
    }
    File root = _fs->open(LOG_SD_ROOT);
    if (!root) return 0;
    for (File y = root.openNextFile(); y; y = root.openNextFile()) {
        if (!y.isDirectory()) continue;
        char ypath[48]; snprintf(ypath, sizeof(ypath), "%s/%s", LOG_SD_ROOT, baseName(y.name()));
        File ydir = _fs->open(ypath);
        if (!ydir) continue;
        for (File m = ydir.openNextFile(); m; m = ydir.openNextFile()) {
            if (!m.isDirectory()) continue;
            char mpath[64]; snprintf(mpath, sizeof(mpath), "%s/%s", ypath, baseName(m.name()));
            File mdir = _fs->open(mpath);
            if (!mdir) continue;
            for (File f = mdir.openNextFile(); f; f = mdir.openNextFile()) {
                const char* bare = baseName(f.name());
                uint32_t d = dayOfName(bare);
                if (!d || d >= cutNum) continue;
                char full[96]; snprintf(full, sizeof(full), "%s/%s", mpath, bare);
                if (strcmp(full, _file) == 0) continue;
                if (_fs->remove(full)) removed++;
            }
            mdir.close();
        }
        ydir.close();
    }
    root.close();
    return removed;
}

// ─── sampling ────────────────────────────────────────────────────────────────

void DataLogger::buildRow(char* dst, size_t n, const BmsData& b,
                          const SolarData& s, const OrionData& o) {
    size_t len = 0;
    time_t now = time(nullptr);
    struct tm ti; localtime_r(&now, &ti);
    len += strftime(dst, n, "%Y-%m-%d %H:%M:%S,", &ti);

    // battery
    addI(dst, n, len, (long)b.soc, b.valid);
    // Coulomb-counted charge left. Worth the 5 bytes: the current reading is quantised in
    // ~0.5 A steps, so integrating it for energy is far less accurate than this counter.
    addF(dst, n, len, b.valid ? b.remainingAh : NAN, 1);
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
    if (!_fs || !_file[0]) { _len = 0; return; }
    File f = _fs->open(_file, FILE_APPEND);
    if (!f && _medium == MED_SD) {
        // Almost always a pulled card. Switch medium and write the same rows there instead of
        // discarding them; rotateIfNeeded() on the next sample opens the flash file.
        loseCard("append failed");
        return;                              // rows stay in the buffer for the next flush
    }
    if (!f) {
        Serial.println("[log] append failed");
        _len = 0;
        _state = LOG_ERROR;
        return;
    }
    f.write((const uint8_t*)_buf, _len);
    f.close();
    _len = 0;
    if (_medium == MED_FLASH && freeBytes() < LOG_FLASH_MIN_FREE) prune();
}

void DataLogger::update(uint32_t nowMs, const BmsData& b, const SolarData& s, const OrionData& o) {
    if (!_enabled) return;
    poll();                                  // notice a pulled card within ~2 s
    if (_medium == MED_NONE) {                        // nothing mounted (first run, or ejected)
        if (!(!_sdFailed && mountSd()) && !mountFlash()) { _state = LOG_ERROR; return; }
    }
    if (!timeValid()) { _state = LOG_WAIT_TIME; return; }   // no clock -> no filename, no rows
    if (_lastMs && (nowMs - _lastMs) < periodMs()) return;
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

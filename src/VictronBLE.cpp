#include "VictronBLE.h"
#include <string.h>
#include <math.h>
#include <mbedtls/aes.h>

// ─── Victron BLE advertisement format ─────────────────────────────────────────
//
// Raw manufacturer-specific data (as returned by NimBLE getManufacturerData(),
// which includes the 2-byte company ID at the start).
// Verified empirically on both SmartSolar 75/15 AND Orion XS 50A via nRF Connect.
//
//   [0..1]  Company ID: 0xE1, 0x02  (Victron = 0x02E1 little-endian)
//   [2]     Extra type: 0x10         ← same on both devices
//   [3]     0x00                     ← same on both devices
//   [4..5]  Product ID (uint16 LE)
//   [6]     Readout type (0x00 = encrypted advertisement)
//   [7..8]  IV (uint16 LE) — AES-CTR counter seed
//   [9]     Key-check byte — must equal key[0] for the correct device key
//   [10..]  Encrypted payload (AES-128-CTR, up to ~13 bytes)
//
// Decrypted payload layout:
//   Solar Charger (solar_charger.py):
//     [0]     charge_state (uint8)
//     [1]     charger_error (uint8)
//     [2..3]  battery_voltage (int16 LE, /100 → V;  0x7FFF = invalid)
//     [4..5]  charge_current (int16 LE, /10  → A;  0x7FFF = invalid)
//     [6..7]  yield_today    (uint16 LE, *10  → Wh; 0xFFFF = invalid)
//     [8..9]  solar_power    (uint16 LE, *1   → W;  0xFFFF = invalid)
//     [10..11] external_load 9-bit (uint16 LE masked &0x1FF, /10 → A; 0x1FF = invalid)
//
//   Orion XS (orion_xs.py):
//     [0]     device_state   (uint8)
//     [1]     charger_error  (uint8)
//     [2..3]  output_voltage (uint16 LE, /100 → V;  0xFFFF = invalid)
//     [4..5]  output_current (uint16 LE, /10  → A;  0xFFFF = invalid)
//     [6..7]  input_voltage  (uint16 LE, /100 → V;  0xFFFF = invalid)
//     [8..9]  input_current  (uint16 LE, /10  → A;  0xFFFF = invalid)
//     [10..13] off_reason    (uint32 LE)
//
// Reference: https://github.com/keshavdv/victron-ble

static constexpr uint8_t  VICTRON_CID_LO   = 0xE1;
static constexpr uint8_t  VICTRON_CID_HI   = 0x02;
static constexpr uint8_t  VICTRON_MARKER   = 0x10;
static constexpr size_t   MFR_HEADER_LEN   = 10;   // company_id(2)+prefix(2)+model(2)+type(1)+iv(2)+keycheck(1)
static constexpr size_t   MIN_MFR_LEN      = MFR_HEADER_LEN + 4;

// ─── Helpers ──────────────────────────────────────────────────────────────────

static void hexToBytes(const char* hex, uint8_t* out, size_t outLen) {
    for (size_t i = 0; i < outLen; i++) {
        char hi = hex[i * 2];
        char lo = hex[i * 2 + 1];
        auto nibble = [](char c) -> uint8_t {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return 0;
        };
        out[i] = (nibble(hi) << 4) | nibble(lo);
    }
}

static uint16_t readU16LE(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int16_t readS16LE(const uint8_t* p) {
    return (int16_t)readU16LE(p);
}

static uint32_t readU32LE(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// AES-128-CTR decrypt.  key=16 bytes, iv=uint16 as 128-bit LE counter seed.
// in/out must be ≥16 bytes; input is zero-padded if len<16.
static bool aes128ctrDecrypt(const uint8_t* key, uint16_t iv,
                              const uint8_t* in, size_t len, uint8_t* out) {
    uint8_t padded[16] = {};
    memcpy(padded, in, len < 16 ? len : 16);

    // Counter: IV placed at bytes 0-1 little-endian, rest zero
    uint8_t counter[16] = {};
    counter[0] = iv & 0xFF;
    counter[1] = (iv >> 8) & 0xFF;

    uint8_t streamBlock[16] = {};
    size_t  ncOff = 0;

    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    int ret = mbedtls_aes_setkey_enc(&ctx, key, 128);
    if (ret == 0)
        ret = mbedtls_aes_crypt_ctr(&ctx, 16, &ncOff, counter, streamBlock, padded, out);
    mbedtls_aes_free(&ctx);
    return ret == 0;
}

// ─── VictronBLE ───────────────────────────────────────────────────────────────

VictronBLE::VictronBLE() : _mux(portMUX_INITIALIZER_UNLOCKED) {
    memset(&_solar, 0, sizeof(_solar));
    memset(&_orion, 0, sizeof(_orion));
    _solar.battVoltage  = NAN;
    _solar.chargeCurrent= NAN;
    _solar.yieldToday   = NAN;
    _solar.solarPower   = NAN;
    _solar.loadCurrent  = NAN;
    _orion.outVoltage   = NAN;
    _orion.outCurrent   = NAN;
    _orion.inVoltage    = NAN;
    _orion.inCurrent    = NAN;
}

void VictronBLE::begin(const char* solarKey, const char* orionKey,
                       const char* solarMac, const char* orionMac) {
    hexToBytes(solarKey, _solarKey, 16);
    hexToBytes(orionKey, _orionKey, 16);
    _solarMac = String(solarMac);
    _orionMac = String(orionMac);
    _solarMac.toUpperCase();
    _orionMac.toUpperCase();
}

void VictronBLE::startScan() {
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(this, true);  // true = want duplicates (live data each advertisement)
    scan->setActiveScan(false);           // passive: no SCAN_REQ, saves power
    scan->setInterval(160);              // 100 ms
    scan->setWindow(80);                 // 50 ms
    // Must call the 3-arg async overload (duration, completeCB, is_continue).
    // start(0, false) would hit the blocking overload and hang forever.
    // onScanEnd restarts the scan if NimBLE stops it (e.g. during a BMS connection event).
    scan->start(0, VictronBLE::onScanEnd, false);
}

void VictronBLE::stopScan() {
    NimBLEDevice::getScan()->stop();
}

SolarData VictronBLE::getSolar() const {
    SolarData copy;
    // Victron callbacks fire from NimBLE task (Core 0); main loop reads on Core 1
    portENTER_CRITICAL(const_cast<portMUX_TYPE*>(&_mux));
    copy = _solar;
    portEXIT_CRITICAL(const_cast<portMUX_TYPE*>(&_mux));
    return copy;
}

OrionData VictronBLE::getOrion() const {
    OrionData copy;
    portENTER_CRITICAL(const_cast<portMUX_TYPE*>(&_mux));
    copy = _orion;
    portEXIT_CRITICAL(const_cast<portMUX_TYPE*>(&_mux));
    return copy;
}

// NimBLEScanCallbacks override — called from NimBLE task
void VictronBLE::onResult(NimBLEAdvertisedDevice* dev) {
    if (!dev->haveManufacturerData()) return;

    std::string raw = dev->getManufacturerData();
    const uint8_t* mfr = reinterpret_cast<const uint8_t*>(raw.data());
    size_t         len = raw.size();

    if (len < MIN_MFR_LEN)              return;
    if (mfr[0] != VICTRON_CID_LO)      return;
    if (mfr[1] != VICTRON_CID_HI)      return;
    if (mfr[2] != VICTRON_MARKER)      return;

    processVictron(mfr, len);
}

bool VictronBLE::processVictron(const uint8_t* mfr, size_t len) {
    uint8_t dec[16] = {};

    // Attempt solar key first
    if (tryDecrypt(mfr, len, _solarKey, dec)) {
        parseSolar(dec);
        return true;
    }
    // Then orion key
    if (tryDecrypt(mfr, len, _orionKey, dec)) {
        parseOrion(dec);
        return true;
    }
    return false;
}

bool VictronBLE::tryDecrypt(const uint8_t* mfr, size_t mfrLen,
                             const uint8_t* key, uint8_t* out) {
    // mfr[9] is the key-check byte (= encrypted_data[0])
    // It must match key[0] before we attempt decryption
    if (mfr[9] != key[0]) return false;

    uint16_t iv = readU16LE(&mfr[7]);      // bytes 7-8
    const uint8_t* enc = &mfr[10];         // actual ciphertext starts at byte 10
    size_t encLen = mfrLen - 10;

    return aes128ctrDecrypt(key, iv, enc, encLen, out);
}

void VictronBLE::parseSolar(const uint8_t* dec) {
    int16_t  rawVolt  = readS16LE(&dec[2]);
    int16_t  rawCurr  = readS16LE(&dec[4]);
    uint16_t rawYield = readU16LE(&dec[6]);
    uint16_t rawPower = readU16LE(&dec[8]);
    uint16_t rawLoad  = readU16LE(&dec[10]) & 0x01FF;  // 9-bit field

    SolarData s;
    s.chargeState   = dec[0];
    s.error         = dec[1];
    s.battVoltage   = (rawVolt  != (int16_t)0x7FFF) ? rawVolt  / 100.0f : NAN;
    s.chargeCurrent = (rawCurr  != (int16_t)0x7FFF) ? rawCurr  / 10.0f  : NAN;
    s.yieldToday    = (rawYield != 0xFFFF)           ? rawYield * 10.0f  : NAN;
    s.solarPower    = (rawPower != 0xFFFF)           ? (float)rawPower   : NAN;
    s.loadCurrent   = (rawLoad  != 0x01FF)           ? rawLoad  / 10.0f  : NAN;
    s.lastSeen      = millis();
    s.valid         = true;

    portENTER_CRITICAL(&_mux);
    _solar = s;
    portEXIT_CRITICAL(&_mux);
}

// Called by NimBLE whenever the scan ends (connection event, host reset, etc.).
// Do NOT restart here — this runs inside the NimBLE host task and immediate
// re-entry causes a tight restart loop. Restart is handled by the scan watchdog task.
void VictronBLE::onScanEnd(NimBLEScanResults /*results*/) {
    // intentionally empty — watchdog in main.cpp handles restart
}

void VictronBLE::parseOrion(const uint8_t* dec) {
    uint16_t rawOutV = readU16LE(&dec[2]);
    uint16_t rawOutC = readU16LE(&dec[4]);
    uint16_t rawInV  = readU16LE(&dec[6]);
    uint16_t rawInC  = readU16LE(&dec[8]);

    OrionData o;
    o.deviceState = dec[0];
    o.error       = dec[1];
    o.outVoltage  = (rawOutV != 0xFFFF) ? rawOutV / 100.0f : NAN;
    o.outCurrent  = (rawOutC != 0xFFFF) ? rawOutC / 10.0f  : NAN;
    o.inVoltage   = (rawInV  != 0xFFFF) ? rawInV  / 100.0f : NAN;
    o.inCurrent   = (rawInC  != 0xFFFF) ? rawInC  / 10.0f  : NAN;
    o.offReason   = readU32LE(&dec[10]);
    o.lastSeen    = millis();
    o.valid       = true;

    portENTER_CRITICAL(&_mux);
    _orion = o;
    portEXIT_CRITICAL(&_mux);
}

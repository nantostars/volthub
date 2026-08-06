#include "LitimeBMS.h"
#include "Config.h"
#include <string.h>

// ─── BLE UUIDs (from mirosieber/Litime_BMS_ESP32) ────────────────────────────
static const char* SVC_UUID    = "0000ffe0-0000-1000-8000-00805f9b34fb";
static const char* NOTIFY_UUID = "0000ffe1-0000-1000-8000-00805f9b34fb";
static const char* WRITE_UUID  = "0000ffe2-0000-1000-8000-00805f9b34fb";

// ─── Query command ────────────────────────────────────────────────────────────
const uint8_t LitimeBMS::QUERY_CMD[8] = {0x00, 0x00, 0x04, 0x01, 0x13, 0x55, 0xAA, 0x17};

LitimeBMS* LitimeBMS::_instance = nullptr;

// ─── Helpers ──────────────────────────────────────────────────────────────────

static uint32_t readU32LE(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static int32_t readS32LE(const uint8_t* p) {
    return (int32_t)readU32LE(p);
}
static uint16_t readU16LE(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1]<<8);
}
static int16_t readS16LE(const uint8_t* p) {
    return (int16_t)readU16LE(p);
}

// ─── LitimeBMS ────────────────────────────────────────────────────────────────

LitimeBMS::LitimeBMS()
    : _client(nullptr), _wrChar(nullptr),
      _lastQuery(0), _lastConnectAttempt(0), _connecting(false),
      _mux(portMUX_INITIALIZER_UNLOCKED) {
    memset(&_data, 0, sizeof(_data));
    _instance = this;
}

void LitimeBMS::begin(const char* macAddress) {
    _addr = NimBLEAddress(macAddress);
}

bool LitimeBMS::isConnected() const {
    return _client && _client->isConnected();
}

BmsData LitimeBMS::getData() const {
    BmsData copy;
    portENTER_CRITICAL(const_cast<portMUX_TYPE*>(&_mux));
    copy = _data;
    portEXIT_CRITICAL(const_cast<portMUX_TYPE*>(&_mux));
    return copy;
}

void LitimeBMS::update() {
    if (!isConnected()) {
        uint32_t now = millis();
        if (!_connecting && (now - _lastConnectAttempt > BMS_RECONNECT_DELAY_MS)) {
            _lastConnectAttempt = now;
            connect();  // blocking — caller (bmsTask) runs this off the main loop
        }
        return;
    }

    uint32_t now = millis();
    if (now - _lastQuery >= BMS_QUERY_INTERVAL_MS) {
        _lastQuery = now;
        sendQuery();
    }
    vTaskDelay(pdMS_TO_TICKS(10));  // yield to other tasks between queries
}

void LitimeBMS::connect() {
    _connecting = true;
    Serial.printf("[BMS] Connecting to %s\n", _addr.toString().c_str());

    if (_client == nullptr) {
        _client = NimBLEDevice::createClient();
        _client->setConnectTimeout(5);   // 5s — shorter window reduces radio contention
    }

    if (!_client->connect(_addr)) {
        Serial.println("[BMS] Connection failed");
        _connecting = false;
        return;
    }

    NimBLERemoteService* svc = _client->getService(SVC_UUID);
    if (!svc) {
        Serial.println("[BMS] Service not found");
        _client->disconnect();
        _connecting = false;
        return;
    }

    NimBLERemoteCharacteristic* notifyChr = svc->getCharacteristic(NOTIFY_UUID);
    _wrChar = svc->getCharacteristic(WRITE_UUID);

    if (!notifyChr || !_wrChar) {
        Serial.println("[BMS] Characteristics not found");
        _client->disconnect();
        _connecting = false;
        return;
    }

    if (!notifyChr->subscribe(true, LitimeBMS::notifyCallback)) {
        Serial.println("[BMS] Subscribe failed");
        _client->disconnect();
        _connecting = false;
        return;
    }

    Serial.println("[BMS] Connected and subscribed");
    _connecting = false;
    _lastQuery  = 0;  // trigger immediate first query
}

void LitimeBMS::disconnect() {
    if (_client && _client->isConnected()) {
        _client->disconnect();
    }
}

void LitimeBMS::sendQuery() {
    if (_wrChar) {
        _wrChar->writeValue(QUERY_CMD, sizeof(QUERY_CMD), true);
    }
}

// Static trampoline → instance method
void LitimeBMS::notifyCallback(NimBLERemoteCharacteristic* /*chr*/,
                                uint8_t* data, size_t len, bool /*isNotify*/) {
    if (_instance) _instance->handleNotify(data, len);
}

void LitimeBMS::handleNotify(const uint8_t* data, size_t len) {
    if (len < 104) return;  // minimum expected response size

    BmsData d = {};

    d.voltage  = readU32LE(&data[8])  / 1000.0f;
    d.current  = readS32LE(&data[48]) / 1000.0f;
    d.power    = d.voltage * d.current;

    // Smoothed current for the runtime estimate: EMA with a ~90s time constant over the ~2s
    // poll cadence. The raw current swings with every load step, which would make the estimate
    // jump between hours and days. Reset after a long gap (reconnect) so a stale average from
    // before the outage can't leak into the new one.
    const uint32_t nowMs = millis();
    if (isnan(_iAvg) || (_lastNotifyMs && (nowMs - _lastNotifyMs) > 30000)) _iAvg = d.current;
    else _iAvg += (2.0f / 90.0f) * (d.current - _iAvg);
    _lastNotifyMs = nowMs;
    d.avgCurrent  = _iAvg;

    d.cellCount = 0;
    for (int i = 0; i < LITIME_MAX_CELLS; i++) {
        uint16_t raw = readU16LE(&data[16 + i * 2]);
        float    cv  = raw / 1000.0f;
        d.cellVoltages[i] = cv;
        if (raw > 0) d.cellCount = i + 1;
    }

    d.cellTemp      = readS16LE(&data[52]);
    d.mosfetTemp    = readS16LE(&data[54]);
    d.remainingAh   = readU16LE(&data[62]) / 100.0f;
    d.fullCapacityAh = readU16LE(&data[64]) / 100.0f;
    d.soc           = readU16LE(&data[90]);
    d.soh           = readU32LE(&data[92]);
    d.dischargesCount = readU32LE(&data[96]);
    d.dischargesAh  = readU32LE(&data[100]) / 1000.0f;
    d.lastSeen      = millis();
    d.valid         = true;

    portENTER_CRITICAL(&_mux);
    _data = d;
    portEXIT_CRITICAL(&_mux);
}

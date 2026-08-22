#include "BmsDriver.h"
#include "Config.h"
#include <string.h>

BleBmsDriver* BleBmsDriver::_active = nullptr;

BleBmsDriver::BleBmsDriver()
    : _client(nullptr), _wrChar(nullptr),
      _lastQuery(0), _lastConnectAttempt(0), _connecting(false),
      _mux(portMUX_INITIALIZER_UNLOCKED) {
    memset(&_data, 0, sizeof(_data));
}

// The notify trampoline is static, so it needs one designated instance. Claiming it here rather
// than in the constructor matters: constructing a second driver would otherwise steal the
// callback from the one actually in use, and its frames would be handed to the wrong parser.
void BleBmsDriver::begin(const char* macAddress) {
    _addr   = NimBLEAddress(macAddress);
    _active = this;
}

bool BleBmsDriver::isConnected() const {
    return _client && _client->isConnected();
}

BmsData BleBmsDriver::getData() const {
    BmsData copy;
    portENTER_CRITICAL(const_cast<portMUX_TYPE*>(&_mux));
    copy = _data;
    portEXIT_CRITICAL(const_cast<portMUX_TYPE*>(&_mux));
    return copy;
}

void BleBmsDriver::update() {
    if (!isConnected()) {
        uint32_t now = millis();
        if (!_connecting && (now - _lastConnectAttempt > retryDelayMs())) {
            _lastConnectAttempt = now;
            connect();  // blocking — caller (bmsTask) runs this off the main loop
        }
        return;
    }

    uint32_t now = millis();
    if (now - _lastQuery >= BMS_QUERY_INTERVAL_MS) {
        _lastQuery = now;
        onPoll(_pollSeq++);
    }
    vTaskDelay(pdMS_TO_TICKS(10));  // yield to other tasks between queries
}

// Back off on consecutive failures. A GATT connect attempt stops the BLE scan, so retrying an
// unreachable BMS on a fixed 30s period would keep blinding the passive Victron scan.
uint32_t BleBmsDriver::retryDelayMs() const {
    uint32_t d = BMS_RECONNECT_DELAY_MS;
    for (uint8_t i = 0; i < _failCount && d < BLE_RECONNECT_MAX_MS; i++) d *= 2;
    return d > BLE_RECONNECT_MAX_MS ? (uint32_t)BLE_RECONNECT_MAX_MS : d;
}

void BleBmsDriver::connect() {
    _connecting = true;
    _active = this;          // whoever is connecting owns the notify callback
    Serial.printf("[BMS] Connecting to %s (%s)\n", _addr.toString().c_str(), name());

    if (_client == nullptr) {
        _client = NimBLEDevice::createClient();
        _client->setConnectTimeout(3);   // the BLE scan is stopped while connecting
    }

    // Every failure path below: report, drop the link, count it (the backoff reads _failCount)
    // and let update() try again later.
    auto fail = [this](const char* why) {
        Serial.printf("[BMS] %s\n", why);
        if (_client && _client->isConnected()) _client->disconnect();
        onDisconnected();
        _connecting = false;
        if (_failCount < 8) _failCount++;
    };

    if (!_client->connect(_addr))            { fail("Connection failed");        return; }

    NimBLERemoteService* svc = _client->getService(svcUuid());
    if (!svc)                                { fail("Service not found");        return; }

    NimBLERemoteCharacteristic* notifyChr = svc->getCharacteristic(notifyUuid());
    _wrChar = svc->getCharacteristic(writeUuid());
    if (!notifyChr || !_wrChar)              { fail("Characteristics not found"); return; }

    if (!notifyChr->subscribe(true, BleBmsDriver::notifyTrampoline))
                                             { fail("Subscribe failed");         return; }

    Serial.println("[BMS] Connected and subscribed");
    _connecting = false;
    _failCount  = 0;
    _lastQuery  = 0;  // trigger immediate first query
    onConnected();
}

void BleBmsDriver::writeCmd(const uint8_t* data, size_t len) {
    if (_wrChar) _wrChar->writeValue(data, len, true);
}

// Static trampoline → active instance
void BleBmsDriver::notifyTrampoline(NimBLERemoteCharacteristic* /*chr*/,
                                    uint8_t* data, size_t len, bool /*isNotify*/) {
    if (_active) _active->onNotify(data, len);
}

void BleBmsDriver::publish(BmsData& d) {
    d.power = d.voltage * d.current;

    // Smoothed current for the runtime estimate: EMA with a ~90s time constant over the ~2s
    // poll cadence. The raw current swings with every load step, which would make the estimate
    // jump between hours and days. Reset after a long gap (reconnect) so a stale average from
    // before the outage can't leak into the new one.
    const uint32_t nowMs = millis();
    if (isnan(_iAvg) || (_lastNotifyMs && (nowMs - _lastNotifyMs) > 30000)) _iAvg = d.current;
    else _iAvg += (2.0f / 90.0f) * (d.current - _iAvg);
    _lastNotifyMs = nowMs;
    d.avgCurrent  = _iAvg;

    d.lastSeen = nowMs;
    d.valid    = true;

    portENTER_CRITICAL(&_mux);
    _data = d;
    portEXIT_CRITICAL(&_mux);
}

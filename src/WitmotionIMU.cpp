#include "WitmotionIMU.h"
#include "Config.h"

// WITMOTION uses base UUID 00805f9a34fb (non-standard, differs from BT SIG 9b).
// The 9a variant must be listed first; 9b kept as fallback for other firmware.
struct BleProfile { const char* svc; const char* notify; const char* write; };
static const BleProfile PROFILES[] = {
    { "0000ffe5-0000-1000-8000-00805f9a34fb",
      "0000ffe4-0000-1000-8000-00805f9a34fb",
      "0000ffe9-0000-1000-8000-00805f9a34fb" },
    { "0000ffe5-0000-1000-8000-00805f9b34fb",
      "0000ffe4-0000-1000-8000-00805f9b34fb",
      "0000ffe9-0000-1000-8000-00805f9b34fb" },
    { "0000ffe0-0000-1000-8000-00805f9a34fb",
      "0000ffe1-0000-1000-8000-00805f9a34fb",
      "0000ffe2-0000-1000-8000-00805f9a34fb" },
};

ImuData       WitmotionIMU::_data;
WitmotionIMU* WitmotionIMU::_instance = nullptr;

// The default MAC in Config.h is a placeholder: with no IMU configured there is nothing to
// connect to, and every attempt would stop the Victron scan for seconds. Skip entirely.
// Evaluated ONCE in begin(): update() runs every 100ms, and building a temporary String there
// would churn the heap ~10x/s forever (no leak, but needless fragmentation on a device that
// must stay up for days).
static bool macIsUsable(const String& mac) {
    if (mac.length() < 17) return false;
    String m = mac; m.toUpperCase();
    return m != "AA:BB:CC:DD:EE:FF" && m != "00:00:00:00:00:00";
}

void WitmotionIMU::begin(const char* mac) {
    _mac         = mac;
    _instance    = this;
    // Stagger first attempt 15 s after boot so it doesn't race with BMS.
    // Formula: _lastAttempt = now - (RETRY - 15000) → first fire at now+15000.
    // uint32 wrap is intentional and correct here.
    _lastAttempt = (uint32_t)(millis() - (BMS_RECONNECT_DELAY_MS - 15000UL));
    _macOk       = macIsUsable(_mac);
    Serial.printf("[IMU] target: %s%s\n", mac, _macOk ? "" : "  (placeholder/unset - IMU disabled)");
}

// Back off on consecutive failures (30s, 60s, 120s … capped): a missing/powered-off IMU
// must not blind the passive Victron scan every 30s forever.
uint32_t WitmotionIMU::retryDelayMs() const {
    uint32_t d = BMS_RECONNECT_DELAY_MS;
    for (uint8_t i = 0; i < _failCount && d < BLE_RECONNECT_MAX_MS; i++) d *= 2;
    return d > BLE_RECONNECT_MAX_MS ? (uint32_t)BLE_RECONNECT_MAX_MS : d;
}

void WitmotionIMU::update() {
    if (_connected) return;
    if (!_macOk) return;   // nothing to connect to: never disturb the BLE scan
    uint32_t now = millis();
    if (now - _lastAttempt < retryDelayMs()) return;
    _lastAttempt = now;
    _connecting  = true;
    auto failedAttempt = [&]() { _connecting = false; if (_failCount < 8) _failCount++; };

    // Infer address type from MAC first byte: top-2 bits=11 → Random Static
    uint8_t firstByte = (uint8_t)strtol(_mac.c_str(), nullptr, 16);
    uint8_t addrType  = ((firstByte & 0xC0) == 0xC0) ? BLE_ADDR_RANDOM : BLE_ADDR_PUBLIC;

    Serial.printf("[IMU] Connecting to %s (type=%s)...\n",
                  _mac.c_str(), addrType == BLE_ADDR_RANDOM ? "random" : "public");

    auto tryConnect = [&](uint8_t type) -> bool {
        _client = NimBLEDevice::createClient();
        _client->setClientCallbacks(this, false);
        _client->setConnectTimeout(3);   // shorter: the scan is stopped meanwhile
        if (_client->connect(NimBLEAddress(_mac.c_str(), type))) return true;
        NimBLEDevice::deleteClient(_client);
        _client = nullptr;
        return false;
    };

    if (!tryConnect(addrType)) {
        // Only probe the alternate address type on the first attempts: it doubles the time
        // the radio spends connecting (and therefore not scanning for Victron adverts).
        bool ok = false;
        if (_failCount < 2) {
            uint8_t altType = (addrType == BLE_ADDR_RANDOM) ? BLE_ADDR_PUBLIC : BLE_ADDR_RANDOM;
            Serial.printf("[IMU] Retry with type=%s\n", altType == BLE_ADDR_RANDOM ? "random" : "public");
            ok = tryConnect(altType);
        }
        if (!ok) {
            Serial.println("[IMU] Connect failed");
            failedAttempt();
            return;
        }
    }

    // Force GATT discovery; iterate the returned vector directly —
    // getService() post-discovery has UUID matching issues in NimBLE 1.4.3.
    std::vector<NimBLERemoteService*>* allSvcs = _client->getServices(true);
    NimBLERemoteService* svc  = nullptr;
    const BleProfile*    prof = nullptr;
    if (allSvcs) {
        for (const auto& p : PROFILES) {
            NimBLEUUID target(p.svc);
            for (NimBLERemoteService* s : *allSvcs) {
                if (s->getUUID() == target) { svc = s; prof = &p; break; }
            }
            if (svc) break;
        }
    }
    if (!svc || !prof) {
        Serial.println("[IMU] No known service. Available:");
        if (allSvcs)
            for (NimBLERemoteService* s : *allSvcs)
                Serial.printf("[IMU]  svc: %s\n", s->getUUID().toString().c_str());
        _client->disconnect();
        failedAttempt();
        return;
    }
    Serial.printf("[IMU] Using service %s\n", prof->svc);

    // Force characteristic discovery and iterate — same UUID-lookup issue as services.
    std::vector<NimBLERemoteCharacteristic*>* allChars = svc->getCharacteristics(true);
    NimBLERemoteCharacteristic* nc = nullptr;
    NimBLERemoteCharacteristic* wc = nullptr;
    if (allChars) {
        NimBLEUUID notifyUUID(prof->notify), writeUUID(prof->write);
        for (NimBLERemoteCharacteristic* c : *allChars) {
            if (c->getUUID() == notifyUUID) nc = c;
            if (c->getUUID() == writeUUID)  wc = c;
        }
    }
    if (!nc || !nc->canNotify()) {
        Serial.println("[IMU] Notify char not found");
        if (allChars)
            for (NimBLERemoteCharacteristic* c : *allChars)
                Serial.printf("[IMU]  char: %s\n", c->getUUID().toString().c_str());
        _client->disconnect();
        failedAttempt();
        return;
    }

    if (!nc->subscribe(true, notifyCB)) {
        Serial.println("[IMU] Subscribe failed");
        _client->disconnect();
        failedAttempt();
        return;
    }

    if (wc && wc->canWrite()) {
        uint8_t unlock[] = {0xFF, 0xAA, 0x69, 0x88, 0xB5};
        wc->writeValue(unlock, sizeof(unlock), false);
    }

    Serial.println("[IMU] Connected, subscribed");
    _connected  = true;
    _connecting = false;
    _failCount  = 0;
}

void WitmotionIMU::onConnect(NimBLEClient*) {}

void WitmotionIMU::onDisconnect(NimBLEClient*) {
    Serial.println("[IMU] Disconnected");
    _connected = false;
    if (_client) {
        NimBLEDevice::deleteClient(_client);
        _client = nullptr;
    }
    _data.valid = false;
}

void WitmotionIMU::notifyCB(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
    static uint8_t cbCount = 0;
    if (cbCount < 3) {
        Serial.printf("[IMU] notify[%u] %u bytes:", cbCount, (unsigned)len);
        for (size_t k = 0; k < len && k < 24; k++) Serial.printf(" %02X", data[k]);
        Serial.println();
        cbCount++;
    }
    if (_instance) _instance->parsePackets(data, len);
}

void WitmotionIMU::parsePackets(const uint8_t* data, size_t len) {
    auto rd16 = [](const uint8_t* p) -> int16_t {
        return (int16_t)(p[0] | ((uint16_t)p[1] << 8));
    };

    // WT9011DCL 20-byte combined packet: 55 61 roll(2) pitch(2) yaw(2) ax(2) ay(2) az(2) gx(2) gy(2)
    // No checksum in payload — BLE handles error correction at transport level.
    if (len >= 8 && data[0] == 0x55 && data[1] == 0x61) {
        _data.roll  = rd16(data + 2) / 32768.0f * 180.0f;
        _data.pitch = rd16(data + 4) / 32768.0f * 180.0f;
        _data.yaw   = rd16(data + 6) / 32768.0f * 180.0f;
        if (!_data.valid)
            Serial.printf("[IMU] First angle: pitch=%.1f roll=%.1f yaw=%.1f\n",
                          _data.pitch, _data.roll, _data.yaw);
        _data.valid    = true;
        _data.lastSeen = millis();
        return;
    }

    // Classic JY-901 11-byte frames: 55 53 roll(2) pitch(2) yaw(2) temp(2) csum
    for (size_t i = 0; i + 11 <= len; ) {
        if (data[i] != 0x55) { i++; continue; }
        uint8_t sum = 0;
        for (int k = 0; k < 10; k++) sum += data[i + k];
        if (sum != data[i + 10]) { i++; continue; }
        if (data[i + 1] == 0x53) {
            _data.roll  = rd16(data + i + 2) / 32768.0f * 180.0f;
            _data.pitch = rd16(data + i + 4) / 32768.0f * 180.0f;
            _data.yaw   = rd16(data + i + 6) / 32768.0f * 180.0f;
            _data.temp  = rd16(data + i + 8) / 100.0f;
            if (!_data.valid)
                Serial.printf("[IMU] First angle: pitch=%.1f roll=%.1f yaw=%.1f\n",
                              _data.pitch, _data.roll, _data.yaw);
            _data.valid    = true;
            _data.lastSeen = millis();
        }
        i += 11;
    }
}

ImuData WitmotionIMU::getData() const { return _data; }

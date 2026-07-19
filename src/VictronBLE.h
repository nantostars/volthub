#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>

// ─── Victron charge-state enum (solar charger + Orion XS share these codes) ──
// These come from the Victron GX Protocol manual / victron-ble Python library
static inline const char* victronStateName(uint8_t s) {
    switch (s) {
        case 0:   return "OFF";
        case 1:   return "STANDBY";
        case 2:   return "FAULT";
        case 3:   return "BULK";
        case 4:   return "ABSORPTION";
        case 5:   return "FLOAT";
        case 6:   return "STORAGE";
        case 7:   return "EQUALIZE";
        case 245: return "WAKE-UP";
        case 247: return "AUTO EQUALIZE";
        case 252: return "EXTERNAL CTRL";
        default:  return "UNKNOWN";
    }
}

// ─── Data structs ─────────────────────────────────────────────────────────────

struct SolarData {
    uint8_t chargeState;   // 0xFF = no data
    uint8_t error;
    float   battVoltage;   // V (NAN = invalid)
    float   chargeCurrent; // A
    float   yieldToday;    // Wh
    float   solarPower;    // W
    float   loadCurrent;   // A
    uint32_t lastSeen;     // millis()
    bool    valid;
};

struct OrionData {
    uint8_t  deviceState;
    uint8_t  error;
    float    outVoltage;   // V
    float    outCurrent;   // A
    float    inVoltage;    // V
    float    inCurrent;    // A
    uint32_t offReason;
    uint32_t lastSeen;
    bool     valid;
};

// ─── VictronBLE scanner class ─────────────────────────────────────────────────

class VictronBLE : public NimBLEAdvertisedDeviceCallbacks {
public:
    VictronBLE();

    // Call once in setup() after NimBLEDevice::init()
    // solarKey / orionKey: 32 hex chars (16 bytes), from VictronConnect
    // solarMac / orionMac: "AA:BB:CC:DD:EE:FF" or "" to auto-detect by key
    void begin(const char* solarKey, const char* orionKey,
               const char* solarMac = "", const char* orionMac = "");

    void startScan();  // starts continuous passive scan
    void stopScan();

    // Called by NimBLE when scan ends for any reason (e.g. BMS connection event).
    // Restarts the scan immediately so Victron advertisements keep flowing.
    static void onScanEnd(NimBLEScanResults results);

    // Thread-safe reads – copy current snapshot
    SolarData getSolar() const;
    OrionData getOrion() const;

private:
    // NimBLEScanCallbacks override
    void onResult(NimBLEAdvertisedDevice* dev) override;

    bool processVictron(const uint8_t* mfrData, size_t len);

    // Returns true and fills `out` (≥16 bytes) if `key` matches the key-check byte
    bool tryDecrypt(const uint8_t* mfrData, size_t mfrLen,
                    const uint8_t* key, uint8_t* out);

    void parseSolar(const uint8_t* dec);
    void parseOrion(const uint8_t* dec);

    uint8_t  _solarKey[16];
    uint8_t  _orionKey[16];
    String   _solarMac;
    String   _orionMac;

    SolarData _solar;
    OrionData _orion;

    portMUX_TYPE _mux;
};

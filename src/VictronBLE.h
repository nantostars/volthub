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
    uint16_t productId;    // Victron product ID (advert [4..5]) → model lookup
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
    uint16_t productId;    // Victron product ID (advert [4..5]) → model lookup
    uint32_t lastSeen;
    bool     valid;
};

// Victron product ID → model name. Family fallback is reliable (category known
// from the key that decrypts). Add specific product IDs here as they are observed
// (the raw productId is exposed in /api/data to help identify them).
// Model name from the Victron BLE Product ID (advert bytes [4..5], uint16 LE).
// SmartSolar/BlueSolar MPPT table = the public Victron product-id list (stable across the
// open-source Victron BLE projects). Orion-XS PIDs are not yet confirmed against a real
// device → generic fallback for now (fill from /api/data `orion.pid` when connected).
static inline const char* victronModelName(uint16_t pid, bool isOrion) {
    if (isOrion) {
        // Orion-XS product IDs from the public Victron list. Not read line-by-line from the
        // primary source (VE.Direct PDF text is compressed) — corroborated across searches.
        // Safe: a wrong/unknown pid just falls back to the generic "Orion-XS".
        // Confirm against the device's real `orion.pid` in /api/data when connected.
        switch (pid) {
            case 0xA38B: return "Orion XS 12/12-50";
            case 0xA3F0: return "Orion XS 1400";   // less certain — verify
            // TODO: Orion XS 12/12-70 (recente) — pid da confermare dal device.
            default: return "Orion-XS";
        }
    }
    switch (pid) {
        // BlueSolar MPPT
        case 0xA042: return "BlueSolar MPPT 75/15";
        case 0xA043: return "BlueSolar MPPT 100/15";
        case 0xA044: return "BlueSolar MPPT 100/30";
        case 0xA045: return "BlueSolar MPPT 100/50";
        case 0xA046: return "BlueSolar MPPT 150/70";
        case 0xA047: return "BlueSolar MPPT 150/100";
        case 0xA049: return "BlueSolar MPPT 100/50 rev2";
        case 0xA04A: return "BlueSolar MPPT 100/30 rev2";
        case 0xA04B: return "BlueSolar MPPT 150/35";
        case 0xA04C: return "BlueSolar MPPT 75/10";
        case 0xA04D: return "BlueSolar MPPT 150/45";
        case 0xA04E: return "BlueSolar MPPT 150/60";
        case 0xA04F: return "BlueSolar MPPT 150/85";
        case 0xA066: return "BlueSolar MPPT 100/20";
        case 0xA067: return "BlueSolar MPPT 100/20 48V";
        case 0xA06F: return "BlueSolar MPPT 150/45 rev2";
        case 0xA070: return "BlueSolar MPPT 150/60 rev2";
        case 0xA071: return "BlueSolar MPPT 150/70 rev2";
        // SmartSolar MPPT
        case 0xA050: return "SmartSolar MPPT 250/100";
        case 0xA051: return "SmartSolar MPPT 150/100";
        case 0xA052: return "SmartSolar MPPT 150/85";
        case 0xA053: return "SmartSolar MPPT 75/15";
        case 0xA054: return "SmartSolar MPPT 75/10";
        case 0xA055: return "SmartSolar MPPT 100/15";
        case 0xA056: return "SmartSolar MPPT 100/30";
        case 0xA057: return "SmartSolar MPPT 100/50";
        case 0xA058: return "SmartSolar MPPT 150/35";
        case 0xA059: return "SmartSolar MPPT 150/100 rev2";
        case 0xA05A: return "SmartSolar MPPT 150/85 rev2";
        case 0xA05B: return "SmartSolar MPPT 250/70";
        case 0xA05C: return "SmartSolar MPPT 250/85";
        case 0xA05D: return "SmartSolar MPPT 250/60";
        case 0xA05E: return "SmartSolar MPPT 250/45";
        case 0xA05F: return "SmartSolar MPPT 100/20";
        case 0xA060: return "SmartSolar MPPT 100/20 48V";
        case 0xA061: return "SmartSolar MPPT 150/45";
        case 0xA062: return "SmartSolar MPPT 150/60";
        case 0xA063: return "SmartSolar MPPT 150/70";
        case 0xA064: return "SmartSolar MPPT 250/85 rev2";
        case 0xA065: return "SmartSolar MPPT 250/100 rev2";
        case 0xA068: return "SmartSolar MPPT 250/60 rev2";
        case 0xA069: return "SmartSolar MPPT 250/70 rev2";
        case 0xA06A: return "SmartSolar MPPT 150/45 rev2";
        case 0xA06B: return "SmartSolar MPPT 150/60 rev2";
        case 0xA06C: return "SmartSolar MPPT 150/70 rev2";
        case 0xA06D: return "SmartSolar MPPT 150/85 rev3";
        case 0xA06E: return "SmartSolar MPPT 150/100 rev3";
        // SmartSolar MPPT VE.Can
        case 0xA102: return "SmartSolar MPPT VE.Can 150/70";
        case 0xA103: return "SmartSolar MPPT VE.Can 150/45";
        case 0xA104: return "SmartSolar MPPT VE.Can 150/60";
        case 0xA105: return "SmartSolar MPPT VE.Can 150/85";
        case 0xA106: return "SmartSolar MPPT VE.Can 150/100";
        case 0xA107: return "SmartSolar MPPT VE.Can 250/45";
        case 0xA108: return "SmartSolar MPPT VE.Can 250/60";
        case 0xA109: return "SmartSolar MPPT VE.Can 250/70";
        case 0xA10A: return "SmartSolar MPPT VE.Can 250/85";
        case 0xA10B: return "SmartSolar MPPT VE.Can 250/100";
        default: return "SmartSolar MPPT";
    }
}

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

    void parseSolar(const uint8_t* dec, uint16_t pid);
    void parseOrion(const uint8_t* dec, uint16_t pid);

    uint8_t  _solarKey[16];
    uint8_t  _orionKey[16];
    String   _solarMac;
    String   _orionMac;

    SolarData _solar;
    OrionData _orion;

    portMUX_TYPE _mux;
};

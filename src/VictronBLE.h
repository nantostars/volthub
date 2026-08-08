#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>

// ─── Victron charge-state enum (solar charger + Orion XS share these codes) ──
// Verbatim from the OFFICIAL source: Victron "VE.Direct Protocol" 3.33, field CS
// ("State of operation"). Columns in that table mark which states apply to MPPT vs
// Charger: 6/11/246/248 are Charger-only, i.e. exactly the ones the Orion XS DC-DC can
// report. Names are shortened where needed to fit the 96px device pill (~13 chars).
static inline const char* victronStateName(uint8_t s) {
    switch (s) {
        case 0:   return "OFF";
        case 1:   return "LOW POWER";      // official: Low power (inverter load search)
        case 2:   return "FAULT";
        case 3:   return "BULK";
        case 4:   return "ABSORPTION";
        case 5:   return "FLOAT";
        case 6:   return "STORAGE";        // charger
        case 7:   return "EQUALIZE";       // official: Equalize (manual)
        case 9:   return "INVERTING";
        case 11:  return "POWER SUPPLY";   // charger
        case 245: return "STARTING UP";    // official: Starting-up
        case 246: return "REPEAT. ABS.";   // charger: Repeated absorption
        case 247: return "AUTO EQUALIZE";  // official: Auto equalize / Recondition
        case 248: return "BATTERYSAFE";    // charger
        case 252: return "EXTERNAL CTRL";  // official: External Control
        default:  return "UNKNOWN";
    }
}

// ─── Charger error codes ─────────────────────────────────────────────────────
// Verbatim from the OFFICIAL source: VE.Direct Protocol 3.33, field ERR ("error code of the
// device, relevant when it is in the fault state"). Shared by the MPPT and the Orion XS.
// Wording shortened to fit the device rows; 0 returns an empty string so callers can hide
// the field entirely while everything is fine.
static inline const char* victronErrorName(uint8_t e) {
    switch (e) {
        case 0:   return "";                       // no error → nothing to show
        case 2:   return "Battery voltage high";
        case 17:  return "Charger temp. high";
        case 18:  return "Charger over current";
        case 19:  return "Current reversed";
        case 20:  return "Bulk time limit";
        case 21:  return "Current sensor issue";
        case 26:  return "Terminals overheated";
        case 28:  return "Converter issue";
        case 33:  return "PV voltage too high";
        case 34:  return "PV current too high";
        case 38:  return "Input shutdown (V batt)";
        case 39:  return "Input shutdown (I)";
        case 65:  return "Communication lost";
        case 66:  return "Sync config issue";
        case 67:  return "BMS connection lost";
        case 68:  return "Network misconfigured";
        case 116: return "Calibration data lost";
        case 117: return "Invalid firmware";
        case 119: return "User settings invalid";
        default:  return "Unknown error";
    }
}

// ─── Off reason (DC-DC) ──────────────────────────────────────────────────────
// Official VE.Direct 3.33 field OR — a BITMASK ("why a unit is switched off"). Several bits
// can be set at once, so report the most informative one; the raw mask is also published on
// the API for anyone who wants the full picture. Empty string = running (nothing to report).
static inline const char* victronOffReasonName(uint32_t m) {
    if (m == 0)             return "";
    if (m & 0x00000001)     return "No input power";
    if (m & 0x00000080)     return "Engine off";            // engine shutdown detection
    if (m & 0x00000100)     return "Analysing input";
    if (m & 0x00000010)     return "Protection active";
    if (m & 0x00000040)     return "BMS";
    if (m & 0x00000008)     return "Remote input";
    if (m & 0x00000002)     return "Switched off (switch)";
    if (m & 0x00000004)     return "Switched off (mode)";
    if (m & 0x00000020)     return "Paygo";
    return "Off";
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
// Table extracted VERBATIM from the OFFICIAL Victron "VE.Direct Protocol" document
// (victronenergy.com, product-id appendix). Orion-Tr Smart chargers are intentionally
// absent: they have no VE.Direct and are not in this registry, so there is no official
// PID for them (unknown Orion pid → generic "Orion").
static inline const char* victronModelName(uint16_t pid, bool isOrion) {
    if (isOrion) {
        switch (pid) {
            case 0xA3F0: return "Orion XS 12/12-50A";  // ex "Smart BuckBoost", rinominato da Victron
            case 0xA3F1: return "Orion XS 1400";
            default:     return "Orion";
        }
    }
    switch (pid) {
        case 0x0300: return "BlueSolar MPPT 70/15";
        case 0xA040: return "BlueSolar MPPT 75/50";
        case 0xA041: return "BlueSolar MPPT 150/35";
        case 0xA042: return "BlueSolar MPPT 75/15";
        case 0xA043: return "BlueSolar MPPT 100/15";
        case 0xA044: return "BlueSolar MPPT 100/30";
        case 0xA045: return "BlueSolar MPPT 100/50";
        case 0xA046: return "BlueSolar MPPT 150/70";
        case 0xA047: return "BlueSolar MPPT 150/100";
        case 0xA049: return "BlueSolar MPPT 100/50 rev2";
        case 0xA04A: return "BlueSolar MPPT 100/30 rev2";
        case 0xA04B: return "BlueSolar MPPT 150/35 rev2";
        case 0xA04C: return "BlueSolar MPPT 75/10";
        case 0xA04D: return "BlueSolar MPPT 150/45";
        case 0xA04E: return "BlueSolar MPPT 150/60";
        case 0xA04F: return "BlueSolar MPPT 150/85";
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
        case 0xA066: return "BlueSolar MPPT 100/20";
        case 0xA067: return "BlueSolar MPPT 100/20 48V";
        case 0xA068: return "SmartSolar MPPT 250/60 rev2";
        case 0xA069: return "SmartSolar MPPT 250/70 rev2";
        case 0xA06A: return "SmartSolar MPPT 150/45 rev2";
        case 0xA06B: return "SmartSolar MPPT 150/60 rev2";
        case 0xA06C: return "SmartSolar MPPT 150/70 rev2";
        case 0xA06D: return "SmartSolar MPPT 150/85 rev3";
        case 0xA06E: return "SmartSolar MPPT 150/100 rev3";
        case 0xA06F: return "BlueSolar MPPT 150/45 rev2";
        case 0xA070: return "BlueSolar MPPT 150/60 rev2";
        case 0xA071: return "BlueSolar MPPT 150/70 rev2";
        case 0xA072: return "BlueSolar MPPT 150/45 rev3";
        case 0xA073: return "SmartSolar MPPT 150/45 rev3";
        case 0xA074: return "SmartSolar MPPT 75/10 rev2";
        case 0xA075: return "SmartSolar MPPT 75/15 rev2";
        case 0xA076: return "BlueSolar MPPT 100/30 rev3";
        case 0xA077: return "BlueSolar MPPT 100/50 rev3";
        case 0xA078: return "BlueSolar MPPT 150/35 rev3";
        case 0xA079: return "BlueSolar MPPT 75/10 rev2";
        case 0xA07A: return "BlueSolar MPPT 75/15 rev2";
        case 0xA07B: return "BlueSolar MPPT 100/15 rev2";
        case 0xA07C: return "BlueSolar MPPT 75/10 rev3";
        case 0xA07D: return "BlueSolar MPPT 75/15 rev3";
        case 0xA07E: return "SmartSolar MPPT 100/30 12V";
        case 0xA07F: return "All-In-1 SmartSolar MPPT 75/15 12V";
        case 0xA080: return "SmartSolar MPPT 250/60 rev3";
        case 0xA081: return "SmartSolar MPPT 250/70 rev3";
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
        case 0xA10C: return "SmartSolar MPPT VE.Can 150/70 rev2";
        case 0xA10D: return "SmartSolar MPPT VE.Can 150/85 rev2";
        case 0xA10E: return "SmartSolar MPPT VE.Can 150/100 rev2";
        case 0xA10F: return "BlueSolar MPPT VE.Can 150/100";
        case 0xA112: return "BlueSolar MPPT VE.Can 250/70";
        case 0xA113: return "BlueSolar MPPT VE.Can 250/100";
        case 0xA114: return "SmartSolar MPPT VE.Can 250/70 rev2";
        case 0xA115: return "SmartSolar MPPT VE.Can 250/100 rev2";
        case 0xA116: return "SmartSolar MPPT VE.Can 250/85 rev2";
        case 0xA117: return "BlueSolar MPPT VE.Can 150/100 rev2";
        default:     return "SmartSolar MPPT";
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

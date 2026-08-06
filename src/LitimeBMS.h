#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>

// Max cells reported by the BMS protocol (LiTime 140Ah = 4S, but protocol reserves 16)
static constexpr int LITIME_MAX_CELLS = 16;

struct BmsData {
    float    voltage;          // V  (pack total)
    float    current;          // A  (positive = charging, negative = discharging)
    float    avgCurrent;       // A  smoothed (EMA ~90s) — used for the runtime estimate
    float    power;            // W  = voltage * current
    float    cellVoltages[LITIME_MAX_CELLS]; // V each
    int      cellCount;        // how many non-zero cells
    int16_t  cellTemp;         // °C
    int16_t  mosfetTemp;       // °C
    float    remainingAh;      // Ah
    float    fullCapacityAh;   // Ah
    uint16_t soc;              // 0-100 %
    uint32_t soh;              // 0-100 %
    uint32_t dischargesCount;
    float    dischargesAh;
    uint32_t lastSeen;
    bool     valid;
};

// ─── Runtime estimate ────────────────────────────────────────────────────────
// The BMS does NOT transmit a time-to-empty/time-to-full, but it does give a
// coulomb-counted remainingAh, so we derive it here — one implementation shared by the
// device UI and the web API. Uses the SMOOTHED current: the instantaneous one swings with
// every compressor/inverter start and would make the number jump between hours and days.
static constexpr float BMS_ETA_DEADBAND_A = 0.3f;    // below this the estimate is meaningless
static constexpr int   BMS_ETA_MAX_MIN    = 99 * 60; // clamp, so we never show "500h"

// Returns minutes remaining, or 0 when not meaningful (offline, at rest, already full/empty).
// Sets toFull = true while charging (time to 100% instead of time to empty).
static inline int bmsEtaMinutes(const BmsData& d, bool& toFull) {
    toFull = false;
    if (!d.valid || isnan(d.avgCurrent)) return 0;
    const float i = d.avgCurrent;
    float mins;
    if (i < -BMS_ETA_DEADBAND_A) {                    // discharging → time to empty
        if (d.remainingAh <= 0) return 0;
        mins = d.remainingAh / (-i) * 60.0f;
    } else if (i > BMS_ETA_DEADBAND_A) {              // charging → time to full
        const float missing = d.fullCapacityAh - d.remainingAh;
        if (missing <= 0) return 0;
        toFull = true;
        mins = missing / i * 60.0f;
    } else {
        return 0;                                     // at rest: would be infinite
    }
    if (mins < 1) mins = 1;
    if (mins > (float)BMS_ETA_MAX_MIN) return BMS_ETA_MAX_MIN;
    return (int)lroundf(mins);
}

// "8h 30m" / "45m" — compact enough for the device stat boxes (buf >= 10 bytes).
static inline void bmsFmtEta(char* buf, size_t n, int minutes) {
    if (minutes <= 0) { snprintf(buf, n, "--"); return; }
    const int h = minutes / 60, m = minutes % 60;
    if (h > 0) snprintf(buf, n, "%dh %02dm", h, m);
    else       snprintf(buf, n, "%dm", m);
}

// ─── LitimeBMS ───────────────────────────────────────────────────────────────

class LitimeBMS {
public:
    LitimeBMS();

    // Call once in setup() after NimBLEDevice::init()
    void begin(const char* macAddress);

    // Call regularly from the main loop
    void update();

    bool isConnected() const;

    // Thread-safe read — copies current snapshot
    BmsData getData() const;

private:
    void connect();
    void disconnect();
    void sendQuery();

    // NimBLE notification callback (static trampoline → instance method)
    static void notifyCallback(NimBLERemoteCharacteristic* chr,
                               uint8_t* data, size_t len, bool isNotify);
    void handleNotify(const uint8_t* data, size_t len);

    // EMA state for BmsData::avgCurrent (touched only from the NimBLE notify task)
    float    _iAvg = NAN;
    uint32_t _lastNotifyMs = 0;

    // Protocol response parser (see BMSClient.cpp from mirosieber/Litime_BMS_ESP32)
    // Response is 104+ bytes, little-endian:
    //   [8..11]   total voltage  uint32 /1000 → V
    //   [12..15]  cell sum       uint32 /1000 → V
    //   [16..47]  cell voltages  uint16[16] /1000 → V
    //   [48..51]  current        int32  /1000 → A
    //   [52..53]  cell temp      int16  direct °C
    //   [54..55]  mosfet temp    int16  direct °C
    //   [62..63]  remaining Ah   uint16 /100 → Ah
    //   [64..65]  full cap Ah    uint16 /100 → Ah
    //   [90..91]  SOC            uint16 %
    //   [92..95]  SOH            uint32 %
    //   [96..99]  discharges cnt uint32
    //   [100..103] discharges Ah uint32 /1000

    static const uint8_t QUERY_CMD[8];

    NimBLEAddress          _addr;
    NimBLEClient*          _client;
    NimBLERemoteCharacteristic* _wrChar;

    BmsData     _data;
    portMUX_TYPE _mux;

    uint32_t    _lastQuery;
    uint32_t    _lastConnectAttempt;
    bool        _connecting;

    // Singleton so the static callback can reach the instance
    static LitimeBMS* _instance;
};

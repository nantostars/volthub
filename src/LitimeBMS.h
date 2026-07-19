#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>

// Max cells reported by the BMS protocol (LiTime 140Ah = 4S, but protocol reserves 16)
static constexpr int LITIME_MAX_CELLS = 16;

struct BmsData {
    float    voltage;          // V  (pack total)
    float    current;          // A  (positive = charging, negative = discharging)
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

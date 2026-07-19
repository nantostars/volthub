#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>

// Data from a single WITMOTION WT9011DCL (or compatible) IMU
struct ImuData {
    float pitch   = 0.0f;   // front/back tilt (°, + = nose up)
    float roll    = 0.0f;   // left/right tilt (°, + = right side up)
    float yaw     = 0.0f;
    float temp    = 0.0f;
    bool  valid   = false;
    uint32_t lastSeen = 0;
};

// GATT client for WITMOTION BLE sensors.
// Standard WITMOTION BLE service: FFE5 / notify FFE4
// Protocol: 11-byte frames  55 53 <roll16> <pitch16> <yaw16> <temp16> <csum>
class WitmotionIMU : public NimBLEClientCallbacks {
public:
    void    begin(const char* mac);
    void    update();           // call from FreeRTOS task every 100 ms
    ImuData getData() const;

    void onConnect   (NimBLEClient* pClient) override;
    void onDisconnect(NimBLEClient* pClient) override;

private:
    static void notifyCB(NimBLERemoteCharacteristic* pChar,
                         uint8_t* pData, size_t length, bool isNotify);
    void parsePackets(const uint8_t* data, size_t len);

    String        _mac;
    NimBLEClient* _client      = nullptr;
    bool          _connected   = false;
    uint32_t      _lastAttempt = 0;

    static ImuData       _data;
    static WitmotionIMU* _instance;
};

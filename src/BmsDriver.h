#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>
#include "BmsData.h"

// ─── BleBmsDriver ────────────────────────────────────────────────────────────
//
// Everything a BLE BMS needs that is NOT the protocol: connecting with a timeout, backing off
// after failures (a GATT connect stops the Victron scan, so a dead battery must not be retried
// every 30 s forever), subscribing to notifications, polling on a fixed cadence, smoothing the
// current, and publishing a snapshot the main loop can read safely.
//
// A protocol supplies six things: three UUIDs, what to write when it is time to poll, what to do
// with a notification, and a name. See LitimeBMS and JbdBMS — each is ~80 lines.
//
// Only ONE driver exists at a time: the radio cannot hold a second GATT link without starving the
// passive Victron scan, and the battery source is a user setting (Settings::getBmsType). That is
// why the notification trampoline can use a single static instance pointer.

class BleBmsDriver {
public:
    BleBmsDriver();
    virtual ~BleBmsDriver() {}

    // Call once in setup() after NimBLEDevice::init()
    void begin(const char* macAddress);

    // Call regularly from the BMS task
    void update();

    bool isConnected() const;
    // True while a connect attempt is in flight — the scan watchdog must not restart the
    // scan underneath it (the controller cannot scan and open a connection at once).
    bool isConnecting() const { return _connecting; }

    // Thread-safe read — copies current snapshot
    BmsData getData() const;

    // Human-readable protocol name, for the boot log and the web API
    virtual const char* name() const = 0;

protected:
    // ─── what a protocol must provide ────────────────────────────────────────
    virtual const char* svcUuid()    const = 0;
    virtual const char* notifyUuid() const = 0;
    virtual const char* writeUuid()  const = 0;

    // Called every BMS_QUERY_INTERVAL_MS. seq counts polls, so a protocol that needs several
    // requests (JBD: basic info, then cell voltages) can alternate them.
    virtual void onPoll(uint32_t seq) = 0;

    // A notification arrived. Protocols that receive their frame in chunks reassemble here —
    // it is protocol-specific (JBD delimits with 0xDD…0x77, LiTime gets its 104 bytes whole).
    virtual void onNotify(const uint8_t* data, size_t len) = 0;

    // Optional: called right after a successful subscribe (unlock packets, wake-ups…).
    virtual void onConnected() {}
    // Optional: called when a connect attempt fails or the link drops, to reset parser state.
    virtual void onDisconnected() {}

    // ─── helpers for the protocol ────────────────────────────────────────────
    void writeCmd(const uint8_t* data, size_t len);

    // Publish a parsed frame: fills avgCurrent (EMA), power, lastSeen and valid, then swaps it
    // into the shared snapshot. The protocol only fills what it decoded.
    void publish(BmsData& d);

private:
    void connect();
    uint32_t retryDelayMs() const;
    static void notifyTrampoline(NimBLERemoteCharacteristic* chr,
                                 uint8_t* data, size_t len, bool isNotify);

    NimBLEAddress   _addr;
    NimBLEClient*   _client;
    NimBLERemoteCharacteristic* _wrChar;

    BmsData      _data;
    portMUX_TYPE _mux;

    // EMA state for BmsData::avgCurrent (touched only from the NimBLE notify task)
    float    _iAvg = NAN;
    uint32_t _lastNotifyMs = 0;

    uint32_t _lastQuery;
    uint32_t _lastConnectAttempt;
    uint32_t _pollSeq = 0;
    bool     _connecting;
    uint8_t  _failCount = 0;

    // Single active driver, so the static NimBLE callback can reach it
    static BleBmsDriver* _active;
};

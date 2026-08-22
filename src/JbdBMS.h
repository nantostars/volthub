#pragma once
#include "BmsDriver.h"
#include "JbdProtocol.h"

// JBD / Xiaoxiang BMS over BLE — see JbdProtocol.h for the frame format and the credit.
//
// Two differences from LiTime shape the driver: the snapshot needs TWO requests (basic info and
// cell voltages, alternated on successive polls), and the answers can be split across
// notifications, so frames are reassembled between 0xDD and the length the header declares.
//
// ⚠ Never run against a real JBD battery — implemented from the protocol, verified only by
// test/jbd_parse_test.cpp on recorded frames.

class JbdBMS : public BleBmsDriver {
public:
    const char* name() const override { return "JBD"; }

protected:
    const char* svcUuid()    const override { return "0000ff00-0000-1000-8000-00805f9b34fb"; }
    const char* notifyUuid() const override { return "0000ff01-0000-1000-8000-00805f9b34fb"; }
    const char* writeUuid()  const override { return "0000ff02-0000-1000-8000-00805f9b34fb"; }

    void onPoll(uint32_t seq) override;
    void onNotify(const uint8_t* data, size_t len) override;
    void onDisconnected() override;

private:
    void feed(const uint8_t* data, size_t len);   // reassembly
    void handleFrame(const uint8_t* f, size_t n);

    static const uint8_t REQ_BASIC[7];
    static const uint8_t REQ_CELLS[7];

    // The snapshot is built across two frames, so it lives between polls. Only the notify task
    // touches it; publish() is what makes a copy visible to everyone else.
    BmsData _stage = {};
    bool    _haveBasic = false;

    uint8_t _rx[128];
    size_t  _rxLen = 0;
};

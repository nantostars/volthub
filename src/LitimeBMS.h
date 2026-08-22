#pragma once
#include "BmsDriver.h"

// LiTime 12V LiFePO4 — same protocol as Power Queen, Redodo and Ampere Time (one OEM, several
// brands). Active GATT link: we write an 8-byte query every BMS_QUERY_INTERVAL_MS and get a
// 104-byte little-endian frame back on the notify characteristic.
//
// Protocol from mirosieber/Litime_BMS_ESP32 (BMSClient.cpp).

class LitimeBMS : public BleBmsDriver {
public:
    const char* name() const override { return "LiTime"; }

protected:
    const char* svcUuid()    const override { return "0000ffe0-0000-1000-8000-00805f9b34fb"; }
    const char* notifyUuid() const override { return "0000ffe1-0000-1000-8000-00805f9b34fb"; }
    const char* writeUuid()  const override { return "0000ffe2-0000-1000-8000-00805f9b34fb"; }

    void onPoll(uint32_t seq) override;
    void onNotify(const uint8_t* data, size_t len) override;

private:
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
};

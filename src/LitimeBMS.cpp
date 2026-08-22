#include "LitimeBMS.h"
#include <string.h>

const uint8_t LitimeBMS::QUERY_CMD[8] = {0x00, 0x00, 0x04, 0x01, 0x13, 0x55, 0xAA, 0x17};

// ─── Helpers ──────────────────────────────────────────────────────────────────

static uint32_t readU32LE(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static int32_t readS32LE(const uint8_t* p) {
    return (int32_t)readU32LE(p);
}
static uint16_t readU16LE(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1]<<8);
}
static int16_t readS16LE(const uint8_t* p) {
    return (int16_t)readU16LE(p);
}

// ─── LitimeBMS ────────────────────────────────────────────────────────────────

// One request, one full answer: the whole snapshot arrives in a single frame.
void LitimeBMS::onPoll(uint32_t /*seq*/) {
    writeCmd(QUERY_CMD, sizeof(QUERY_CMD));
}

void LitimeBMS::onNotify(const uint8_t* data, size_t len) {
    if (len < 104) return;  // minimum expected response size

    BmsData d = {};

    d.voltage  = readU32LE(&data[8])  / 1000.0f;
    d.current  = readS32LE(&data[48]) / 1000.0f;

    d.cellCount = 0;
    for (int i = 0; i < BMS_MAX_CELLS; i++) {
        uint16_t raw = readU16LE(&data[16 + i * 2]);
        float    cv  = raw / 1000.0f;
        d.cellVoltages[i] = cv;
        if (raw > 0) d.cellCount = i + 1;
    }

    d.cellTemp      = readS16LE(&data[52]);
    d.mosfetTemp    = readS16LE(&data[54]);
    d.remainingAh   = readU16LE(&data[62]) / 100.0f;
    d.fullCapacityAh = readU16LE(&data[64]) / 100.0f;
    d.soc           = readU16LE(&data[90]);
    d.soh           = readU32LE(&data[92]);
    d.dischargesCount = readU32LE(&data[96]);
    d.dischargesAh  = readU32LE(&data[100]) / 1000.0f;

    publish(d);   // fills power, avgCurrent (EMA), lastSeen, valid
}

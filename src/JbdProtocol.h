#pragma once
#include <stddef.h>
#include <stdint.h>
#include "BmsData.h"

// ─── JBD / Xiaoxiang frame format ────────────────────────────────────────────
//
// Byte offsets and scaling re-implemented from syssi/esphome-jbd-bms (Apache-2.0),
// <https://github.com/syssi/esphome-jbd-bms>. Same protocol under many brands: Overkill Solar,
// ECO-WORTHY, Epoch, LIONTRON, Vatrer, Basen.
//
//   request:   DD A5 <fn> 00 <ck-hi> <ck-lo> 77          (7 bytes)
//   response:  DD <fn> <status> <len> <payload…> <ck-hi> <ck-lo> 77
//
// Everything is BIG-endian — the opposite of LiTime, which is the easy mistake to make here.
// The checksum is the two's complement of the sum of <status>, <len> and the payload.
//
// This header is deliberately free of Arduino and NimBLE, so the decoders can be compiled and
// exercised on a PC: see test/jbd_parse_test.cpp.

static constexpr uint8_t JBD_START   = 0xDD;
static constexpr uint8_t JBD_END     = 0x77;
static constexpr uint8_t JBD_FN_BASIC = 0x03;   // pack voltage, current, capacity, SOC, temps
static constexpr uint8_t JBD_FN_CELLS = 0x04;   // per-cell voltages
static constexpr size_t  JBD_HEAD_LEN = 4;      // start + fn + status + len
static constexpr size_t  JBD_TAIL_LEN = 3;      // checksum(2) + end

static inline uint16_t jbdU16BE(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] << 8 | (uint16_t)p[1]);
}
static inline int16_t jbdS16BE(const uint8_t* p) {
    return (int16_t)jbdU16BE(p);
}

static inline uint16_t jbdChecksum(const uint8_t* p, size_t n) {
    uint16_t sum = 0;
    for (size_t i = 0; i < n; i++) sum += p[i];
    return (uint16_t)(0x10000u - sum);
}

// Length the frame will have once complete, from its header. 0 = header not read yet.
static inline size_t jbdFrameLen(const uint8_t* f, size_t n) {
    if (n < JBD_HEAD_LEN) return 0;
    return JBD_HEAD_LEN + f[3] + JBD_TAIL_LEN;
}

// A frame is usable only if it is well formed AND the BMS reported success (status 0).
static inline bool jbdFrameValid(const uint8_t* f, size_t n) {
    if (n < JBD_HEAD_LEN + JBD_TAIL_LEN) return false;
    if (f[0] != JBD_START) return false;
    const size_t len = jbdFrameLen(f, n);
    if (len != n) return false;
    if (f[n - 1] != JBD_END) return false;
    if (f[2] != 0x00) return false;                       // 0x80 = the BMS refused the request
    const uint16_t want = jbdChecksum(&f[2], (size_t)f[3] + 2);
    return want == jbdU16BE(&f[n - 3]);
}

// ─── Payload decoders ────────────────────────────────────────────────────────
// Both fill only the fields their frame carries: the driver keeps one BmsData across frames,
// because voltage/SOC and the cell voltages arrive in separate answers.

// 0x03 — basic info. Layout (payload-relative):
//   [0..1] pack voltage  u16 /100 → V      [2..3] current  s16 /100 → A (+ charge, − discharge)
//   [4..5] remaining     u16 /100 → Ah     [6..7] nominal  u16 /100 → Ah
//   [8..9] cycles        u16               [19]   SOC %
//   [21]   cell count                      [22]   NTC count, then u16 each at 23 + 2i,
//                                                 °C = (raw − 2731) / 10
static inline bool jbdDecodeBasic(const uint8_t* p, size_t len, BmsData& d) {
    if (len < 23) return false;
    d.voltage        = jbdU16BE(&p[0]) / 100.0f;
    d.current        = jbdS16BE(&p[2]) / 100.0f;
    d.remainingAh    = jbdU16BE(&p[4]) / 100.0f;
    d.fullCapacityAh = jbdU16BE(&p[6]) / 100.0f;
    d.dischargesCount = jbdU16BE(&p[8]);
    d.soc            = p[19];

    // Temperatures: first probe is the pack, a second one (when present) is the MOSFET/board.
    const uint8_t ntc = p[22];
    d.cellTemp = d.mosfetTemp = 0;
    for (uint8_t i = 0; i < ntc && (size_t)(23 + i * 2 + 2) <= len; i++) {
        const int16_t c = (int16_t)((jbdU16BE(&p[23 + i * 2]) - 2731) / 10);
        if (i == 0) d.cellTemp = c;
        else if (i == 1) d.mosfetTemp = c;
    }
    // Not in this protocol: SOH and lifetime discharged Ah. Left at 0, shown as "--".
    return true;
}

// 0x04 — one u16 per cell, in mV.
static inline bool jbdDecodeCells(const uint8_t* p, size_t len, BmsData& d) {
    if (len < 2) return false;
    int n = (int)(len / 2);
    if (n > BMS_MAX_CELLS) n = BMS_MAX_CELLS;   // 48 V packs report more than we display
    for (int i = 0; i < BMS_MAX_CELLS; i++) d.cellVoltages[i] = 0.0f;
    d.cellCount = 0;
    for (int i = 0; i < n; i++) {
        const uint16_t mv = jbdU16BE(&p[i * 2]);
        d.cellVoltages[i] = mv / 1000.0f;
        if (mv > 0) d.cellCount = i + 1;
    }
    return d.cellCount > 0;
}

// Validate a complete frame and decode it into d. Returns the function code handled, or 0.
static inline uint8_t jbdDecodeFrame(const uint8_t* f, size_t n, BmsData& d) {
    if (!jbdFrameValid(f, n)) return 0;
    const uint8_t* payload = &f[JBD_HEAD_LEN];
    const size_t   len     = f[3];
    switch (f[1]) {
        case JBD_FN_BASIC: return jbdDecodeBasic(payload, len, d) ? JBD_FN_BASIC : 0;
        case JBD_FN_CELLS: return jbdDecodeCells(payload, len, d) ? JBD_FN_CELLS : 0;
        default:           return 0;
    }
}

#pragma once
#include <math.h>
#include <stdint.h>
#include <stdio.h>

// Battery snapshot, protocol-independent: every BMS driver fills this and nothing downstream
// (web API, device screens, CSV logger) knows which protocol produced it. Fields a protocol does
// not carry stay 0 — soh and dischargesCount are shown as "--" in that case, see the UI.
//
// Deliberately free of Arduino/NimBLE includes so the protocol decoders that fill it can be
// compiled and tested on a PC (see test/jbd_parse_test.cpp).

// Max cells any supported protocol reports (LiTime reserves 16, JBD up to 32 but 16 covers
// every 48 V pack we care about; extra cells are ignored).
static constexpr int BMS_MAX_CELLS = 16;

struct BmsData {
    float    voltage;          // V  (pack total)
    float    current;          // A  (positive = charging, negative = discharging)
    float    avgCurrent;       // A  smoothed (EMA ~90s) — used for the runtime estimate
    float    power;            // W  = voltage * current
    float    cellVoltages[BMS_MAX_CELLS]; // V each
    int      cellCount;        // how many non-zero cells
    int16_t  cellTemp;         // °C
    int16_t  mosfetTemp;       // °C
    float    remainingAh;      // Ah
    float    fullCapacityAh;   // Ah
    uint16_t soc;              // 0-100 %
    uint32_t soh;              // 0-100 %   (0 = not reported by this protocol)
    uint32_t dischargesCount;  // cycles    (0 = not reported by this protocol)
    float    dischargesAh;
    uint32_t lastSeen;
    bool     valid;
};

// ─── Runtime estimate ────────────────────────────────────────────────────────
// No BMS we support transmits a time-to-empty/time-to-full, but they all give a coulomb-counted
// remainingAh, so we derive it here — one implementation shared by the device UI and the web API.
// Uses the SMOOTHED current: the instantaneous one swings with every compressor/inverter start
// and would make the number jump between hours and days.
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

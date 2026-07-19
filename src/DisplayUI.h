#pragma once
#include <Arduino.h>
#ifdef BOARD_GUITION
  #include <Arduino_GFX_Library.h>
  #include <Wire.h>
#else
  #include <TFT_eSPI.h>
#endif
#include "LitimeBMS.h"
#include "VictronBLE.h"
#include "WitmotionIMU.h"

// ─── RGB565 palette (matches web dashboard dark theme) ───────────────────────
// Conversion: ((R>>3)<<11) | ((G>>2)<<5) | (B>>3)
#define UI_BG       0x0882   // #0f1117  background
#define UI_SURFACE  0x18E4   // #1a1d27  card surface
#define UI_BORDER   0x2967   // #2a2d3e  border
#define UI_TEXT     0xEF5E   // #e8eaf0  primary text
#define UI_MUTED    0x6B90   // #6b7280  secondary text
#define UI_GREEN    0x262B   // #22c55e  positive / online
#define UI_YELLOW   0xF4E1   // #f59e0b  warning
#define UI_RED      0xEA48   // #ef4444  negative / offline
#define UI_BLUE     0x3BDE   // #3b82f6  accent / flow line
#define UI_ORANGE   0xFB82   // #f97316  discharge
#define UI_BLUE_DIM 0x198C   // #183060 dark navy — battery SOC fill

// ─── Layout constants — landscape 480×320 ────────────────────────────────────
//
//   8            146    171          309    334          472
//   ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐  y=8
//   │     SOLAR       │ │    BATTERY      │ │    ORION XS     │
//   │                 │ │                 │ │                 │
//   └─────────────────┘ └────────┬────────┘ └─────────────────┘  y=208
//                       ┌────────▼────────┐ ┌─────────────────┐  y=216
//                       │     LOADS       │ │     LEVEL       │
//                       └─────────────────┘ └─────────────────┘  y=312

#define BOX_Y      8
#define BOX_H      200
#define BOX_W      138

#define SOLAR_X    8
#define BATT_X     171
#define ORION_X    334
#define LOADS_X    171
#define LOADS_Y    216
#define LOADS_H    96

// Connection line endpoints
#define CONN_MID_Y  (BOX_Y + BOX_H / 2)          // vertical midpoint of top boxes
#define BATT_CX     (BATT_X + BOX_W / 2)          // battery horizontal center

class DisplayUI {
public:
    DisplayUI();
    void begin();

    // Call from main loop — updates changed regions and animates flow lines
    void update(const BmsData& b, const SolarData& s, const OrionData& o, const ImuData& imu, uint32_t nowMs);

    // Call from main loop — polls touch and handles taps
    void handleTouch();

    // Update system info shown on Settings screen (call periodically from main)
    void updateSysInfo(const char* apSsid, const char* apIp,
                       const char* staSsid, const char* staIp,
                       const char* ntpTime);

private:
    // ── Screens ──────────────────────────────────────────────────────────────
    enum Screen { SCR_OVERVIEW, SCR_DETAIL_BATT, SCR_DETAIL_SOLAR, SCR_DETAIL_ORION, SCR_LEVEL, SCR_SETTINGS };

    void drawOverview();
    void updateOverviewValues();
    void drawFlowLines(bool solar, bool orion, bool loads);

    void drawDetailBatt();
    void drawDetailSolar();
    void drawDetailOrion();
    void drawDetailHeader(const char* title, bool online);

    void drawLevelScreen();
    void updateLevelValues();
    void drawLevelPanel(int px, bool isPitch);

    void drawSettingsScreen();
    void drawSettingsToggle();
    void drawSettingsInfo();
    void drawSettingsBox();

    // ── Primitives ────────────────────────────────────────────────────────────
    void drawBox(int x, int y, int w, int h, const char* title, bool online, bool showDot = true);
    void drawHFlowLine(int x1, int x2, int y, bool active, bool reverse = false);
    void drawVFlowLine(int x, int y1, int y2, bool active);
    void drawMetric(int x, int y, int w, int h,
                    const char* label, const char* val, const char* unit, uint16_t valCol);
    void drawStateTag(int x, int y, const char* state);
    void drawBigNum(int cx, int y, const char* txt, const char* unit, uint16_t col);
    void fillText(int x, int y, int w, int h, const char* txt, uint8_t font, uint16_t col, uint16_t bg);

    // ── Board-agnostic font/text helpers ─────────────────────────────────────
    void _setFont(uint8_t f);
    int  _textWidth(const char* s);

    // ── Touch ─────────────────────────────────────────────────────────────────
    bool hitTest(int tx, int ty, int bx, int by, int bw, int bh);

    // ── Hardware backend ──────────────────────────────────────────────────────
#ifdef BOARD_GUITION
    Arduino_ESP32QSPI* _bus = nullptr;
    Arduino_GFX*       _gfx = nullptr;
    // Touch state (polled, no IRQ)
    uint32_t _touchPollMs = 0;
    bool     _touchDown   = false;
    int      _touchSX = 0, _touchSY = 0;
    bool _readTouch(int& sx, int& sy);
#else
    TFT_eSPI _tft;
#endif

    // ── State ─────────────────────────────────────────────────────────────────
    Screen   _screen    = SCR_OVERVIEW;
    uint8_t  _flowPhase = 0;
    uint32_t _lastFlowMs = 0;
    bool     _firstDraw  = true;

    // Cached last-drawn values to skip unnecessary redraws
    BmsData   _bd;
    SolarData _sd;
    OrionData _od;
    ImuData   _imu;

    float    _lastPitch  = 99.0f;
    float    _lastRoll   = 99.0f;
    bool     _lvFirstDraw = true;
    bool     _lastImuValid = false;
    float    _lastSoc    = -1.0f;
    bool     _prevTouched = false;
    uint32_t _lastValMs  = 0;

    // Level box arrow colors — 1 = undrawn sentinel, forces first draw
    uint16_t _lvUpC = 1, _lvDnC = 1, _lvLtC = 1, _lvRtC = 1;
    // Level screen bubble positions (-999 = undrawn)
    int _lvPitchBy = -999;
    int _lvRollBx  = -999;

    // Screen timeout
    bool     _screenOn    = true;
    bool     _alwaysOn    = false;
    uint32_t _lastTouchMs = 0;

    // Sysinfo for Settings screen
    char _syApSsid[33]  = "";
    char _syApIp[16]    = "192.168.4.1";
    char _syStaSsid[33] = "";
    char _syStaIp[16]   = "";
    char _syNtpTime[20] = "--";
};

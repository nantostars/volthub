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

// ─── volt·hub palette (RGB565) — from Claude design "Camper Power Monitor 3.5in" ─
// Conversion: ((R>>3)<<11) | ((G>>2)<<5) | (B>>3). Tunable on hardware.
#define C_BG       0x08C4   // #0e1826  screen background
#define C_BODY     0x0041   // #05080d  frame / deepest
#define C_CARD     0x1107   // #17233a  card surface
#define C_INSET    0x08C5   // #0f1a2b  nested inset / track
#define C_BORDER   0x2188   // ~#263143 hairline border
#define C_TEXT     0xEF9F   // #eaf0f8  primary text
#define C_MUTED    0x8CB4   // ~#8a94a6 muted / secondary
#define C_GREEN    0x4670   // #46cf82  ok / charging
#define C_AMBER    0xFD09   // #ffa24d  warning / discharging
#define C_RED      0xFAC9   // #ff5a4d  fault / low
#define C_BLUE     0x5D3E   // #5aa5f5  DC-DC / BLE
#define C_PALE     0xB69E   // #b4d2f0  loads / pale accent
#define C_ORANGE   0xFB40   // #ff6900  brand accent (logo, active tab, solar)
#define C_TABBG    0x0883   // #0b131f  tab bar background
#define C_TABACT   0x28A1   // active-tab tint (dark orange)

// ─── Layout — landscape 480×320 ──────────────────────────────────────────────
//   ┌──────────────────────────────────────────────┐ 0
//   │ status bar (logo · state · BLE · clock)       │ 34
//   ├──────────────────────────────────────────────┤
//   │ content (per-screen)                          │
//   ├──────────────────────────────────────────────┤ 264
//   │ tab bar: Overview Battery Solar DC-DC Level.. │ 320
//   └──────────────────────────────────────────────┘
#define SB_H       34                 // status bar height
#define TAB_H      56                 // bottom tab bar height
#define TAB_Y      (320 - TAB_H)      // = 264
#define CT_Y       SB_H               // content top
#define CT_H       (TAB_Y - SB_H)     // content height = 230
#define TAB_N      6
#define TAB_W      (480 / TAB_N)       // = 80

class DisplayUI {
public:
    DisplayUI();
    void begin();

    // Call from main loop — updates changed regions and animates flow lines
    void update(const BmsData& b, const SolarData& s, const OrionData& o, const ImuData& imu, uint32_t nowMs);

    // Call from main loop — polls touch and handles taps
    void handleTouch();

    // UI language: 0 = English (default), 1 = Italiano. Forces a full redraw on change.
    void setLanguage(uint8_t lang);

    // Update system info shown on the System screen (call periodically from main)
    void updateSysInfo(const char* apSsid, const char* apIp,
                       const char* staSsid, const char* staIp,
                       const char* ntpTime);

private:
    // ── Screens (order == tab order) ──────────────────────────────────────────
    enum Screen { SCR_OVERVIEW, SCR_BATTERY, SCR_SOLAR, SCR_DCDC, SCR_LEVEL, SCR_SYSTEM };

    // ── Chrome ────────────────────────────────────────────────────────────────
    void drawChrome();                 // full status bar + tab bar (on first draw)
    void drawStatusBar(bool full);     // full=true redraws logo/divider too
    void drawTabBar();
    void drawTabIcon(int idx, int cx, int cy, uint16_t col);
    void selectScreen(Screen s);       // switch + full-redraw content + tab bar
    void present();                    // push canvas to panel (Guition); no-op on CYD

    // ── Per-screen full draw + periodic value refresh ─────────────────────────
    void drawOverview();   void updateOverview();
    void drawNodeVal(int slot, int x, int y, float w, uint16_t col);
    void drawBattery();    void updateBattery();
    void drawSolar();      void updateSolar();
    void drawDcdc();       void updateDcdc();
    void drawLevel();      void updateLevel();
    void drawSystem();     void updateSystem();

    void drawFlow(bool solar, bool dcdc, bool loads);   // animated overview flow paths

    // ── Primitives ────────────────────────────────────────────────────────────
    void clearContent();
    void drawCard(int x, int y, int w, int h, uint16_t fill, bool border);
    void drawArc(int cx, int cy, int r, int thick, int a0, int a1, uint16_t col);
    void drawRing(int cx, int cy, int r, int thick, float pct, uint16_t col, uint16_t track);
    void drawPill(int x, int y, int w, int h, const char* txt, uint16_t fg, uint16_t bg);
    void pillCached(int slot, int x, int y, int w, int h, const char* txt, uint16_t fg, uint16_t bg);
    void drawStat(int x, int y, int w, const char* label, const char* val, uint16_t valCol);
    void fillText(int x, int y, int w, int h, const char* txt, uint8_t font, uint16_t col, uint16_t bg);
    void centerText(int cx, int y, const char* txt, uint8_t font, uint16_t col);
    void drawTextL(int x, int y, const char* txt, uint8_t font, uint16_t col);  // left, transparent
    // opaque centered text — single-pass, flicker-free (clears its own w×h via bg)
    void centerFill(int cx, int y, int w, int h, const char* txt, uint8_t font, uint16_t col, uint16_t bg);

    // ── i18n: returns the translation of an English literal for the current language ──
    const char* t(const char* en);

    // ── Board-agnostic font/text helpers ─────────────────────────────────────
    void _setFont(uint8_t f);
    int  _textWidth(const char* s);

    // ── Touch ─────────────────────────────────────────────────────────────────
    bool hitTest(int tx, int ty, int bx, int by, int bw, int bh);

    // ── Hardware backend ──────────────────────────────────────────────────────
#ifdef BOARD_GUITION
    Arduino_ESP32QSPI* _bus   = nullptr;
    Arduino_GFX*       _panel = nullptr;  // native 320x480 portrait panel
    Arduino_GFX*       _gfx   = nullptr;  // software-rotated 480x320 canvas (PSRAM)
    uint16_t*          _shadow = nullptr; // last-flushed framebuffer copy (anti-tearing gate)
    size_t             _fbBytes = 0;
    uint32_t _touchPollMs = 0;
    uint32_t _lastDownMs  = 0;   // last valid finger-down frame (timed-release debounce)
    bool     _propFont    = false;  // current font is a proportional GFXfont (baseline-anchored)
    bool     _touchDown   = false;
    int      _touchSX = 0, _touchSY = 0;
    bool _readTouch(int& sx, int& sy);
#else
    TFT_eSPI _tft;
#endif

    // ── State ─────────────────────────────────────────────────────────────────
    Screen   _screen     = SCR_OVERVIEW;
    uint8_t  _lang       = 0;          // 0 = English, 1 = Italiano
    bool     _firstDraw  = true;       // force full draw of current screen
    uint8_t  _flowPhase  = 0;
    uint32_t _lastFlowMs = 0;
    uint32_t _lastValMs  = 0;

    // Latest snapshots
    BmsData   _bd;
    SolarData _sd;
    OrionData _od;
    ImuData   _imu;

    // Anti-flicker caches
    int      _lastSocBucket = -999;    // overview/battery ring (-999 = undrawn; -1 = offline)
    float    _lastPitch = 99.0f, _lastRoll = 99.0f;
    bool     _lastImuValid = false;
    char     _lastClock[8] = "";
    int      _lastChargeState = -2;    // -1 discharge, 0 idle, 1 charge
    int      _lastDevs = -1;           // status-bar BLE count
    char     _pill[5][14] = {{0}};     // last drawn pill text (0=bms 1=solar 2=dcdc 3=level 4=sys)
    char     _nodeTxt[3][12] = {{0}};  // overview node last value (0=solar 1=dcdc 2=loads) anti-flicker
    uint16_t _nodeCol[3]  = {0, 0, 0}; // overview node last colour
    long     _cellSig = -1;            // battery cell-bars change signature
    int      _lvBx = -999, _lvBy = -999;  // last level-bubble center (erase-in-place)

    // Screen timeout
    bool     _screenOn    = true;
    bool     _alwaysOn    = false;
    uint32_t _lastTouchMs = 0;
    bool     _prevTouched = false;

    // Sysinfo for the System screen
    char _syApSsid[33]  = "";
    char _syApIp[16]    = "192.168.4.1";
    char _syStaSsid[33] = "";
    char _syStaIp[16]   = "";
    char _syNtpTime[20] = "--";
};

#include "DisplayUI.h"
#include "Config.h"
#include "Version.h"
#include "Arduino_AXS15231B_Guition.h"
#include <math.h>


#ifdef BOARD_GUITION
#include "esp_heap_caps.h"
#include "fonts/FreeSansBold18pt7b.h"   // font4 (valori grandi) — Adafruit GFX
#include "fonts/FreeSansBold24pt7b.h"   // font6 (valori grandi)
// Canvas whose framebuffer lives in PSRAM (480x320x2 ~ 300KB won't fit internal RAM).
// The panel only works in native portrait (hw landscape rotation is broken), so we draw
// the landscape UI into this canvas and flush() it rotated onto the 320x480 panel.
class PsramCanvas : public Arduino_Canvas {
public:
    PsramCanvas(int16_t w, int16_t h, Arduino_G *output) : Arduino_Canvas(w, h, output) {}
    bool begin(int32_t speed = GFX_NOT_DEFINED) override {
        if (speed != GFX_SKIP_OUTPUT_BEGIN && _output) {
            if (!_output->begin(speed)) return false;
        }
        if (!_framebuffer) {
            size_t s = (size_t)_width * _height * 2;
            _framebuffer = (uint16_t *)heap_caps_malloc(s, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (!_framebuffer) return false;
        }
        return true;
    }
};
#endif

// Board-agnostic display alias: _D.fillRect(...) works for TFT_eSPI and Arduino_GFX
#ifdef BOARD_GUITION
#define _D (*_gfx)
#else
#define _D _tft
#endif

// ─── Formatting / color helpers ───────────────────────────────────────────────
static void fmtF(char* buf, float v, int dec) {
    if (isnan(v)) { strcpy(buf, "--"); return; }
    dtostrf(v, 0, dec, buf);
}
static uint16_t socColor(float soc) { return soc > 45 ? C_GREEN : soc > 18 ? C_AMBER : C_RED; }
static uint16_t signColor(float v)  { return v > 0.05f ? C_GREEN : v < -0.05f ? C_AMBER : C_TEXT; }

// ─── Constructor ──────────────────────────────────────────────────────────────
DisplayUI::DisplayUI() {}

// ─── Font helpers (board-specific) ───────────────────────────────────────────
void DisplayUI::_setFont(uint8_t f) {
#ifdef BOARD_GUITION
    _gfx->setFont(nullptr);
    _gfx->setTextSize(1);
    _propFont = false;
    switch (f) {
        case 1: _gfx->setTextSize(1); break;  // ~7px  bitmap
        case 2: _gfx->setTextSize(2); break;  // ~14px bitmap
        // Large values → proportional Helvetica-Bold (Adafruit FreeSansBold, baseline-anchored)
        case 4: _gfx->setFont(&FreeSansBold18pt7b); _propFont = true; break;  // ~25px
        case 6: _gfx->setFont(&FreeSansBold24pt7b); _propFont = true; break;  // ~34px
        default: _gfx->setTextSize(1); break;
    }
#else
    // Large values → FreeSansBold (bundled in TFT_eSPI via LOAD_GFXFF). Small text keeps
    // the built-in fonts. TFT_eSPI handles the free-font baseline via the text datum.
    switch (f) {
        case 4: _tft.setFreeFont(&FreeSansBold18pt7b); break;  // ~25px
        case 6: _tft.setFreeFont(&FreeSansBold24pt7b); break;  // ~34px
        default: _tft.setTextFont(f); break;
    }
#endif
}

int DisplayUI::_textWidth(const char* s) {
#ifdef BOARD_GUITION
    int16_t x1, y1; uint16_t w, h;
    _gfx->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
    return (int)w;
#else
    return _tft.textWidth(s);
#endif
}

// ─── i18n ──────────────────────────────────────────────────────────────────────
// English is the key; Italian is looked up here. Missing entries fall back to English.
static const struct { const char* en; const char* it; } LANG_IT[] = {
    // tabs
    {"Overview","Panoramica"}, {"Battery","Batteria"}, {"Solar","Solare"}, {"Level","Livella"},
    {"System","Sistema"},
    // overview / nodes
    {"Loads","Carichi"}, {"offline","offline"}, {"online","online"},
    // states
    {"Idle","Inattivo"}, {"Charging","In carica"}, {"Discharging","In scarica"},
    {"Balanced","Bilanciata"}, {"Balancing","Bilanciam."},
    // battery detail
    {"Voltage","Tensione"}, {"Current","Corrente"}, {"Charge","Carica"}, {"Capacity","Capacita'"},
    {"Cycles","Cicli"}, {"Temp","Temp"},
    {"Runtime","Autonomia"}, {"To full","A pieno"},
    {"Cell voltages   recommended 3.00 - 3.55 V","Tensioni celle   consigliato 3.00 - 3.55 V"},
    // solar detail
    {"To battery","Alla batteria"}, {"Yield today","Resa oggi"},
    {"Production today","Produzione oggi"}, {"energy today","energia oggi"},
    // dc-dc detail
    {"Alternator in","Alternatore"}, {"Output","Uscita"}, {"Charge profile","Profilo di carica"},
    {"Current limit","Limite corrente"}, {"Input range","Range ingresso"}, {"Mode","Modo"},
    {"Engine detect","Rileva motore"}, {"Adaptive","Adattivo"}, {"Auto","Auto"},
    // level
    {"PITCH F-R","BECC. A-P"}, {"ROLL L-R","ROLL. S-D"},
    // system
    {"Connected devices","Dispositivi connessi"}, {"Screen","Schermo"}, {"Network","Rete"},
    {"Client IP","IP client"}, {"NTP time","Ora NTP"}, {"Tilt sensor","Inclinometro"},
    {"Language","Lingua"}, {"ALWAYS","SEMPRE"},
};

const char* DisplayUI::t(const char* en) {
    if (_lang == 0 || en == nullptr) return en;                 // English (default) → identity
    for (auto& p : LANG_IT) if (strcmp(p.en, en) == 0) return p.it;
    return en;                                                  // no translation → English fallback
}

void DisplayUI::setLanguage(uint8_t lang) {
    if (lang == _lang) return;
    _lang = lang;
    _firstDraw = true;   // force a full redraw of the current screen in the new language
}

// ─── begin ────────────────────────────────────────────────────────────────────
void DisplayUI::begin() {
    pinMode(PIN_BL, OUTPUT);
    digitalWrite(PIN_BL, HIGH);

#ifdef BOARD_GUITION
    Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL);
    Wire.setClock(400000);
    _bus = new Arduino_ESP32QSPI(
        45 /* CS */, 47 /* PCLK */,
        21 /* D0 */, 48 /* D1 */, 40 /* D2 */, 39 /* D3 */);
    // Panel in NATIVE PORTRAIT 320x480 (hw landscape rotation via MADCTL MV is broken).
    // Custom driver sets the vendor init + COLMOD 0x55.
    _panel = new Arduino_AXS15231B_Guition(
        _bus, GFX_NOT_DEFINED, 0, false, 320, 480);
    _panel->begin();
    _panel->setRotation(0);
    // force COLMOD = RGB565 (0x55) AFTER init — the init sequence alone doesn't stick
    // (without this: solid fill shows vertical stripes = colour-depth mismatch)
    _bus->beginWrite();
    _bus->writeC8D8(0x3A, 0x55);
    _bus->endWrite();
    // landscape 480x320 drawing surface in PSRAM, flushed rotated onto the portrait panel
    PsramCanvas* cv = new PsramCanvas(320, 480, _panel);
    cv->begin(GFX_SKIP_OUTPUT_BEGIN);
    cv->setRotation(1);
    _gfx = cv;
    // anti-tearing: shadow copy of the framebuffer; flush only when content changes
    _fbBytes = (size_t)320 * 480 * 2;
    _shadow = (uint16_t *)heap_caps_malloc(_fbBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (_shadow) memset(_shadow, 0xA5, _fbBytes);  // != first frame → force initial flush
#else
    pinMode(PIN_TOUCH_IRQ, INPUT);
    _tft.init();
    _tft.setRotation(1);
#endif

    _D.fillScreen(C_BG);
    present();
    _firstDraw   = true;
    _lastTouchMs = millis();
}

// ─── _readTouch (Guition only) ────────────────────────────────────────────────
#ifdef BOARD_GUITION
bool DisplayUI::_readTouch(int& sx, int& sy) {
    uint32_t now = millis();
    if (now - _touchPollMs >= 20) {
        _touchPollMs = now;
        // 11-byte read command (vendor esp_lcd_axs15231b). Sending only 8 bytes makes the
        // controller return garbage (0xNN filler) → the fix for the bogus 3646 coordinate.
        // Bytes [6..7] = payload length = AXS_MAX_TOUCH_NUMBER(1)*6 + 2 = 8.
        static const uint8_t cmd[] = {0xb5, 0xab, 0xa5, 0x5a, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00};
        // The controller intermittently returns filler frames (buf[1] = 0x3E/0xFF) even
        // while a finger is down. A valid frame has buf[0]=0 (gesture) and buf[1] ∈ {0,1}
        // (point count). Retry within the poll until we get one, so a single physical
        // touch doesn't flicker down/up and fire an action repeatedly.
        // The controller intermittently returns filler frames (buf[0]/buf[1] = 0x3E/0x3A/0xFF…)
        // even while a finger is down. Retry within the poll to grab a good frame.
        uint8_t buf[8] = {};
        bool valid = false;
        for (int attempt = 0; attempt < 4 && !valid; attempt++) {
            Wire.beginTransmission(AXS15231B_ADDR);
            Wire.write(cmd, sizeof(cmd));
            Wire.endTransmission();
            Wire.requestFrom((uint8_t)AXS15231B_ADDR, (uint8_t)8);
            for (int i = 0; i < 8; i++) buf[i] = Wire.available() ? Wire.read() : 0xFF;
            valid = (buf[0] == 0x00 && (buf[1] == 0 || buf[1] == 1));
        }
        if (valid) {
            if (buf[1] == 1) {                         // real touch frame
                _touchDown  = true;
                _lastDownMs = now;
                uint16_t tx = ((buf[2] & 0x0F) << 8) | buf[3];
                uint16_t ty = ((buf[4] & 0x0F) << 8) | buf[5];
                _touchSX = constrain((int)ty,       0, 479);
                _touchSY = constrain(319 - (int)tx, 0, 319);
            } else {                                   // clean rest frame → released
                _touchDown = false;
            }
        } else if (_touchDown && (now - _lastDownMs > 120)) {
            // Only garbage read: hold "down" briefly to bridge filler bursts, but release
            // after a timeout so a lifted finger can't stay stuck (that blocked new taps).
            _touchDown = false;
        }
    }
    sx = _touchSX;
    sy = _touchSY;
    return _touchDown;
}
#endif

// ─── update ───────────────────────────────────────────────────────────────────
void DisplayUI::update(const BmsData& b, const SolarData& s, const OrionData& o, const ImuData& imu, uint32_t nowMs) {
    _bd = b; _sd = s; _od = o; _imu = imu;

    if (!_alwaysOn && _screenOn && (nowMs - _lastTouchMs > 60000UL)) {
        _screenOn = false;
        digitalWrite(PIN_BL, LOW);
    }
    if (!_screenOn) return;

    if (_firstDraw) {
        drawChrome();
        switch (_screen) {
            case SCR_OVERVIEW: drawOverview(); break;
            case SCR_BATTERY:  drawBattery();  break;
            case SCR_SOLAR:    drawSolar();    break;
            case SCR_DCDC:     drawDcdc();     break;
            case SCR_LEVEL:    drawLevel();    break;
            case SCR_SYSTEM:   drawSystem();   break;
        }
        _firstDraw = false;
        _lastValMs = nowMs;
        present();
        return;
    }

    bool drew = false;

    // Periodic value refresh (300ms) — status bar + current screen values
    if (nowMs - _lastValMs >= 300) {
        _lastValMs = nowMs;
        drawStatusBar(false);
        switch (_screen) {
            case SCR_OVERVIEW: updateOverview(); break;
            case SCR_BATTERY:  updateBattery();  break;
            case SCR_SOLAR:    updateSolar();    break;
            case SCR_DCDC:     updateDcdc();     break;
            case SCR_LEVEL:    updateLevel();    break;
            case SCR_SYSTEM:   updateSystem();   break;
        }
        drew = true;
    }

    // Overview flow animation (60ms)
    if (_screen == SCR_OVERVIEW && nowMs - _lastFlowMs >= 60) {
        _lastFlowMs = nowMs;
        _flowPhase = (_flowPhase + 12) % 14;   // = -2 mod 14: dashes flow x0->x1 (physical direction)
        float battW  = isnan(_bd.power) ? 0.0f : _bd.power;
        // Battery->Loads active only when the battery is actually discharging (loads pull from it).
        drawFlow(_sd.chargeCurrent > 0.1f, _od.outCurrent > 0.1f, battW < -2.0f);
        drew = true;
    }

    // Level bubble follows tilt in near-real-time — decoupled from the 300ms value throttle
    // (the IMU refreshes every ~100ms, so ~80ms display cadence keeps the bubble responsive).
    if (_screen == SCR_LEVEL && nowMs - _lastFlowMs >= 80) {
        _lastFlowMs = nowMs;
        updateLevel();
        drew = true;
    }

    if (drew) present();
}

// ─── handleTouch ──────────────────────────────────────────────────────────────
void DisplayUI::handleTouch() {
    int sx, sy;

#ifdef BOARD_GUITION
    if (!_screenOn) {
        if (_readTouch(sx, sy)) {
            _screenOn = true; _lastTouchMs = millis();
            digitalWrite(PIN_BL, HIGH); _prevTouched = true;
        } else _prevTouched = false;
        return;
    }
    if (!_readTouch(sx, sy)) { _prevTouched = false; return; }
    if (_prevTouched) return;
    _prevTouched = true;
    _lastTouchMs = millis();
#else
    if (!_screenOn) {
        if (digitalRead(PIN_TOUCH_IRQ) == LOW && _tft.getTouchRawZ() >= 400) {
            _screenOn = true; _lastTouchMs = millis();
            digitalWrite(PIN_BL, HIGH); _prevTouched = true;
        } else _prevTouched = false;
        return;
    }
    if (digitalRead(PIN_TOUCH_IRQ) == HIGH) { _prevTouched = false; return; }
    uint16_t z = _tft.getTouchRawZ();
    if (z < 400) { _prevTouched = false; return; }
    if (_prevTouched) return;
    _prevTouched = true;
    _lastTouchMs = millis();

    uint16_t rx, ry;
    _tft.getTouchRaw(&rx, &ry);
    sx = constrain(479 - (int)map((long)ry, 320, 3860, 0, 479), 0, 479);
    sy = constrain((int)map((long)rx, 480, 3860, 0, 319), 0, 319);
#endif

    // Bottom tab bar — switch screen
    if (sy >= TAB_Y) {
        int idx = sx / TAB_W;
        if (idx >= 0 && idx < TAB_N && idx != (int)_screen) selectScreen((Screen)idx);
        return;
    }

    // System screen toggles: left half = screen timeout, right half = AP (escape hatch).
    if (_screen == SCR_SYSTEM && hitTest(sx, sy, 300, CT_Y + 8, 116, 40)) {
        _alwaysOn = !_alwaysOn;
        drawSystem();
        present();
    } else if (_screen == SCR_SYSTEM && hitTest(sx, sy, 416, CT_Y + 8, 52, 40)) {
        _apTogglePending = true;   // main.cpp owns the WiFi state and will redraw
    }
}

bool DisplayUI::hitTest(int tx, int ty, int bx, int by, int bw, int bh) {
    return tx >= bx && tx < bx + bw && ty >= by && ty < by + bh;
}

// Push the RAM canvas to the panel (Guition software rotation). No-op on CYD.
// Anti-tearing: only flush when the framebuffer actually changed vs the last flush.
void DisplayUI::present() {
#ifdef BOARD_GUITION
    uint16_t* fb = static_cast<Arduino_Canvas*>(_gfx)->getFramebuffer();
    if (_shadow && fb && memcmp(fb, _shadow, _fbBytes) == 0) return;  // unchanged → skip
    _gfx->flush();
    if (_shadow && fb) memcpy(_shadow, fb, _fbBytes);
#endif
}

void DisplayUI::selectScreen(Screen s) {
    _screen = s;
    // reset anti-flicker caches so the new screen fully repaints its values
    _lastSocBucket = -999; _lastPitch = 99.0f; _lastRoll = 99.0f; _lastImuValid = false;
    _cellSig = -1;
    for (int i = 0; i < 5; i++) _pill[i][0] = 0;
    clearContent();
    drawTabBar();
    switch (_screen) {
        case SCR_OVERVIEW: drawOverview(); break;
        case SCR_BATTERY:  drawBattery();  break;
        case SCR_SOLAR:    drawSolar();    break;
        case SCR_DCDC:     drawDcdc();     break;
        case SCR_LEVEL:    drawLevel();    break;
        case SCR_SYSTEM:   drawSystem();   break;
    }
    _lastValMs = millis();
    present();
}

// ─── Primitives ───────────────────────────────────────────────────────────────
void DisplayUI::clearContent() {
    _D.fillRect(0, CT_Y, 480, CT_H, C_BG);
}

void DisplayUI::drawCard(int x, int y, int w, int h, uint16_t fill, bool border) {
    _D.fillRoundRect(x, y, w, h, 10, fill);
    if (border) _D.drawRoundRect(x, y, w, h, 10, C_BORDER);
}

// Thick arc drawn as a run of filled dots. a0/a1 in degrees, 0 = top, clockwise.
void DisplayUI::drawArc(int cx, int cy, int r, int thick, int a0, int a1, uint16_t col) {
    int rad = thick / 2; if (rad < 1) rad = 1;
    for (int a = a0; a <= a1; a++) {
        float t = (a - 90) * 0.01745329f;
        int x = cx + (int)lroundf(cosf(t) * r);
        int y = cy + (int)lroundf(sinf(t) * r);
        _D.fillCircle(x, y, rad, col);
    }
}

void DisplayUI::drawRing(int cx, int cy, int r, int thick, float pct, uint16_t col, uint16_t track) {
    if (pct < 0) pct = 0; if (pct > 1) pct = 1;
    drawArc(cx, cy, r, thick, 0, 360, track);
    int sweep = (int)lroundf(pct * 360.0f);
    if (sweep > 0) drawArc(cx, cy, r, thick, 0, sweep, col);
}

void DisplayUI::drawPill(int x, int y, int w, int h, const char* txt, uint16_t fg, uint16_t bg) {
    _D.fillRoundRect(x, y, w, h, h / 2, bg);
    _setFont(1);
    int tw = _textWidth(txt);
#ifdef BOARD_GUITION
    _gfx->setTextColor(fg);
    _gfx->setCursor(x + (w - tw) / 2, y + (h - 8) / 2);
    _gfx->print(txt);
#else
    _tft.setTextColor(fg, bg);
    _tft.setTextPadding(0);
    _tft.drawString(txt, x + (w - tw) / 2, y + (h - 8) / 2);
#endif
}

// label (small, muted) above value (font2, colored). Clears value rect for anti-flicker.
void DisplayUI::drawStat(int x, int y, int w, const char* label, const char* val, uint16_t valCol) {
    fillText(x, y, w, 12, t(label), 1, C_MUTED, C_CARD);
    fillText(x, y + 13, w, 20, val, 2, valCol, C_CARD);
}

void DisplayUI::fillText(int x, int y, int w, int h, const char* txt, uint8_t font, uint16_t col, uint16_t bg) {
#ifdef BOARD_GUITION
    _gfx->fillRect(x, y, w, h, bg);
    _setFont(font);
    _gfx->setTextColor(col);
    if (_propFont) {
        // proportional GFXfont: cursor is on the baseline → place bbox left-aligned, v-centered
        int16_t x1, y1; uint16_t bw, bh;
        _gfx->getTextBounds(txt, 0, 0, &x1, &y1, &bw, &bh);
        _gfx->setCursor(x + 2 - x1, y + (h - (int)bh) / 2 - y1);
    } else {
        _gfx->setCursor(x + 1, y + 1);
    }
    _gfx->print(txt);
#else
    _setFont(font);
    _tft.setTextColor(col, bg);
    _tft.setTextPadding(w - 1);
    _tft.setTextDatum(TL_DATUM);
    _tft.drawString(txt, x + 1, y + 1);
#endif
}

// Centered text, transparent bg (caller must clear the region first).
void DisplayUI::centerText(int cx, int y, const char* txt, uint8_t font, uint16_t col) {
    _setFont(font);
    int w = _textWidth(txt);
#ifdef BOARD_GUITION
    _gfx->setTextColor(col);
    if (_propFont) {
        int16_t x1, y1; uint16_t bw, bh;
        _gfx->getTextBounds(txt, 0, 0, &x1, &y1, &bw, &bh);
        _gfx->setCursor(cx - bw / 2 - x1, y - y1);   // y treated as top of glyph
    } else {
        _gfx->setCursor(cx - w / 2, y);
    }
    _gfx->print(txt);
#else
    _tft.setTextColor(col);
    _tft.setTextPadding(0);
    _tft.setTextDatum(TL_DATUM);
    _tft.drawString(txt, cx - w / 2, y);
#endif
}

// Left-aligned transparent text (caller ensures a clean background).
void DisplayUI::drawTextL(int x, int y, const char* txt, uint8_t font, uint16_t col) {
    _setFont(font);
#ifdef BOARD_GUITION
    _gfx->setTextColor(col);
    if (_propFont) {
        int16_t x1, y1; uint16_t bw, bh;
        _gfx->getTextBounds(txt, 0, 0, &x1, &y1, &bw, &bh);
        _gfx->setCursor(x - x1, y - y1);   // y treated as top of glyph
    } else {
        _gfx->setCursor(x, y);
    }
    _gfx->print(txt);
#else
    _tft.setTextColor(col); _tft.setTextPadding(0); _tft.setTextDatum(TL_DATUM);
    _tft.drawString(txt, x, y);
#endif
}

// Opaque centered text — single drawString pass clears its own w×h region (no flicker).
void DisplayUI::centerFill(int cx, int y, int w, int h, const char* txt, uint8_t font, uint16_t col, uint16_t bg) {
#ifdef BOARD_GUITION
    _D.fillRect(cx - w / 2, y, w, h, bg);
    _setFont(font);
    _gfx->setTextColor(col);
    if (_propFont) {
        int16_t x1, y1; uint16_t bw, bh;
        _gfx->getTextBounds(txt, 0, 0, &x1, &y1, &bw, &bh);
        _gfx->setCursor(cx - bw / 2 - x1, y + (h - (int)bh) / 2 - y1);
    } else {
        int tw = _textWidth(txt);
        _gfx->setCursor(cx - tw / 2, y + 1);
    }
    _gfx->print(txt);
#else
    _setFont(font);
    _tft.setTextColor(col, bg);
    _tft.setTextPadding(w);
    _tft.setTextDatum(TC_DATUM);
    _tft.drawString(txt, cx, y);
    _tft.setTextDatum(TL_DATUM);
#endif
}

// Pill drawn only when its text changes (rounded bg repaint each cycle would flicker).
void DisplayUI::pillCached(int slot, int x, int y, int w, int h, const char* txt, uint16_t fg, uint16_t bg) {
    if (slot >= 0 && slot < 6) {
        if (strcmp(_pill[slot], txt) == 0) return;
        strncpy(_pill[slot], txt, 13); _pill[slot][13] = 0;
    }
    drawPill(x, y, w, h, txt, fg, bg);
}

// ─── Chrome: status bar + tab bar ─────────────────────────────────────────────
void DisplayUI::drawChrome() {
    drawStatusBar(true);
    drawTabBar();
}

void DisplayUI::drawStatusBar(bool full) {
    if (full) {
        _D.fillRect(0, 0, 480, SB_H, C_BG);
        _D.drawFastHLine(0, SB_H - 1, 480, C_BORDER);
        // wordmark "volthub"
        _setFont(2);
        int x = 12, yb = 9;
#ifdef BOARD_GUITION
        _gfx->setTextColor(C_TEXT); _gfx->setCursor(x, yb); _gfx->print("volt");
        int wv = _textWidth("volt");
        _D.fillCircle(x + wv + 4, yb + 7, 2, C_ORANGE);
        _gfx->setCursor(x + wv + 9, yb); _gfx->print("hub");
#else
        _tft.setTextColor(C_TEXT); _tft.setTextDatum(TL_DATUM); _tft.setTextPadding(0);
        _tft.drawString("volt", x, yb);
        int wv = _tft.textWidth("volt");
        _D.fillCircle(x + wv + 4, yb + 7, 2, C_ORANGE);
        _tft.drawString("hub", x + wv + 9, yb);
#endif
    }

    // Right cluster: charge state · BLE count · clock — each redrawn only on change

    // clock — _syNtpTime is "YYYY-MM-DD HH:MM:SS" (HH:MM at offset 11), or "--" when unsynced
    char hhmm[6] = "--:--";
    int nlen = strlen(_syNtpTime);
    if (nlen >= 16 && _syNtpTime[13] == ':') { memcpy(hhmm, _syNtpTime + 11, 5); hhmm[5] = 0; }
    else if (nlen >= 5 && _syNtpTime[2] == ':') { memcpy(hhmm, _syNtpTime, 5); hhmm[5] = 0; }
    if (full || strcmp(_lastClock, hhmm) != 0) {
        strcpy(_lastClock, hhmm);
        // "HH:MM" in font2. GFX (Guition) font2 is ~12px/char → 60px wide, so shift left and
        // widen to stay inside 480 (was overflowing off-screen and wrapping). TFT (CYD) is narrower.
#ifdef BOARD_GUITION
        fillText(408, 10, 70, 16, hhmm, 2, C_TEXT, C_BG);
#else
        fillText(420, 10, 52, 16, hhmm, 2, C_TEXT, C_BG);
#endif
    }

    // BLE device count
    int devs = (_bd.valid?1:0) + (_sd.valid?1:0) + (_od.valid?1:0) + (_imu.valid?1:0);
    if (full || devs != _lastDevs) {
        _lastDevs = devs;
        char cbuf[8]; snprintf(cbuf, sizeof(cbuf), "BLE %d", devs);
        // Width must not reach the clock's clear rect (Guition clock starts at x=408; CYD at 420).
#ifdef BOARD_GUITION
        fillText(360, 11, 44, 14, cbuf, 1, C_BLUE, C_BG);
#else
        fillText(360, 11, 56, 14, cbuf, 1, C_BLUE, C_BG);
#endif
    }

    // charge state dot + label
    float p = isnan(_bd.power) ? 0 : _bd.power;
    int st = p > 8 ? 1 : (p < -8 ? -1 : 0);
    if (full || st != _lastChargeState) {
        _lastChargeState = st;
        const char* lbl = t(st == 1 ? "Charging" : st == -1 ? "Discharging" : "Idle");
        uint16_t sc = st == 1 ? C_GREEN : st == -1 ? C_AMBER : C_MUTED;
        _D.fillCircle(240, 17, 4, sc);
        fillText(248, 11, 104, 14, lbl, 1, sc, C_BG);
    }
}

void DisplayUI::drawTabIcon(int idx, int cx, int cy, uint16_t col) {
    switch (idx) {
        case SCR_OVERVIEW: // 2x2 grid
            _D.drawRoundRect(cx-8, cy-8, 7, 7, 1, col); _D.drawRoundRect(cx+1, cy-8, 7, 7, 1, col);
            _D.drawRoundRect(cx-8, cy+1, 7, 7, 1, col); _D.drawRoundRect(cx+1, cy+1, 7, 7, 1, col);
            break;
        case SCR_BATTERY: // battery body + terminal
            _D.drawRoundRect(cx-9, cy-5, 15, 10, 2, col); _D.fillRect(cx+6, cy-2, 2, 4, col);
            break;
        case SCR_SOLAR: { // sun
            _D.fillCircle(cx, cy, 3, col);
            for (int a = 0; a < 360; a += 45) {
                float t = a * 0.01745329f;
                _D.drawLine(cx + cosf(t)*6, cy + sinf(t)*6, cx + cosf(t)*8, cy + sinf(t)*8, col);
            }
            break; }
        case SCR_DCDC: // two arrows ⇄
            _D.drawLine(cx-8, cy-3, cx+8, cy-3, col); _D.drawLine(cx+5, cy-6, cx+8, cy-3, col); _D.drawLine(cx+5, cy, cx+8, cy-3, col);
            _D.drawLine(cx-8, cy+3, cx+8, cy+3, col); _D.drawLine(cx-8, cy+3, cx-5, cy+6, col); _D.drawLine(cx-8, cy+3, cx-5, cy, col);
            break;
        case SCR_LEVEL: // target
            _D.drawCircle(cx, cy, 8, col); _D.fillCircle(cx, cy, 2, col);
            _D.drawLine(cx-11, cy, cx-9, cy, col); _D.drawLine(cx+9, cy, cx+11, cy, col);
            _D.drawLine(cx, cy-11, cx, cy-9, col); _D.drawLine(cx, cy+9, cx, cy+11, col);
            break;
        case SCR_SYSTEM: // chip
            _D.drawRoundRect(cx-8, cy-8, 16, 16, 2, col); _D.drawRect(cx-3, cy-3, 6, 6, col);
            break;
    }
}

void DisplayUI::drawTabBar() {
    static const char* labels[TAB_N] = {"Overview","Battery","Solar","DC-DC","Level","System"};
    _D.fillRect(0, TAB_Y, 480, TAB_H, C_TABBG);
    _D.drawFastHLine(0, TAB_Y, 480, C_BORDER);
    for (int i = 0; i < TAB_N; i++) {
        bool on = (i == (int)_screen);
        int x = i * TAB_W;
        uint16_t col = on ? C_ORANGE : C_MUTED;
        if (on) {
            _D.fillRect(x, TAB_Y + 1, TAB_W, TAB_H - 1, C_TABACT);
            _D.fillRect(x, TAB_Y, TAB_W, 2, C_ORANGE);
        }
        drawTabIcon(i, x + TAB_W / 2, TAB_Y + 20, col);
        const char* lbl = t(labels[i]);
        _setFont(1);
        int tw = _textWidth(lbl);
#ifdef BOARD_GUITION
        _gfx->setTextColor(col); _gfx->setCursor(x + (TAB_W - tw) / 2, TAB_Y + 38); _gfx->print(lbl);
#else
        _tft.setTextColor(col); _tft.setTextPadding(0); _tft.setTextDatum(TL_DATUM);
        _tft.drawString(lbl, x + (TAB_W - tw) / 2, TAB_Y + 38);
#endif
    }
}

// ─── Overview: radial flow ────────────────────────────────────────────────────
void DisplayUI::drawOverview() {
    // side nodes (enlarged for font6 value + font4 unit) — labels font2
    drawCard(8, CT_Y + 8, 150, 80, C_INSET, true);
    fillText(20, CT_Y + 16, 130, 16, t("Solar"), 2, C_MUTED, C_INSET);
    drawCard(8, CT_Y + 142, 150, 80, C_INSET, true);
    fillText(20, CT_Y + 150, 130, 16, t("DC-DC"), 2, C_MUTED, C_INSET);
    drawCard(322, CT_Y + 75, 150, 80, C_INSET, true);
    fillText(334, CT_Y + 83, 130, 16, t("Loads"), 2, C_MUTED, C_INSET);
    for (int i = 0; i < 3; i++) _nodeTxt[i][0] = '\x01';  // invalidate node cache → force redraw
    _lastSocBucket = -999;                                  // force ring redraw
    updateOverview();
}

// Big node value: [number][W] as a group centred in the box. The number is right-aligned
// against the W within a fixed 3-digit slot, so the W never moves as digits grow and the
// group stays centred. x = box content-left (boxes are 150 wide, x = boxLeft + 12).
void DisplayUI::drawNodeVal(int slot, int x, int y, float w, uint16_t col) {
    char num[10]; fmtF(num, w, 0);
    if (strcmp(_nodeTxt[slot], num) == 0 && _nodeCol[slot] == col) return;  // unchanged → skip
    strncpy(_nodeTxt[slot], num, sizeof(_nodeTxt[slot]) - 1);
    _nodeTxt[slot][sizeof(_nodeTxt[slot]) - 1] = 0;
    _nodeCol[slot] = col;

    int cx = x + 63;                              // box centre
    _setFont(6); int w3 = _textWidth("888");
    _setFont(4); int wW = _textWidth("W");
    const int gap = 10;
    const int SLOT_H = 40;
    int groupW = w3 + gap + wW;
    int gl = cx - groupW / 2;
    int rightX = gl + w3;                         // number right edge (3-digit slot)
    int wLeftX = gl + w3 + gap;                   // W left edge
    _D.fillRect(gl, y, groupW, 48, C_INSET);      // clear the fixed band (no leftovers)
    // Draw number and W on a common BASELINE (from a fixed digit box, centred in the slot):
    // digits and "--" then sit at the same height, and the W bottom lines up with the number.
#ifdef BOARD_GUITION
    _setFont(6);
    int16_t dx, dy; uint16_t dbw, dbh; _gfx->getTextBounds("8", 0, 0, &dx, &dy, &dbw, &dbh);
    int baseline = y + (SLOT_H + (int)dbh) / 2;
    int16_t x1, y1; uint16_t bw, bh; _gfx->getTextBounds(num, 0, 0, &x1, &y1, &bw, &bh);
    _gfx->setTextColor(col);
    _gfx->setCursor(rightX - x1 - (int)bw, baseline);   // number right-aligned, on baseline
    _gfx->print(num);
    _setFont(4);
    int16_t wx, wy; uint16_t wbw, wbh; _gfx->getTextBounds("W", 0, 0, &wx, &wy, &wbw, &wbh);
    _gfx->setTextColor(C_MUTED);
    _gfx->setCursor(wLeftX - wx, baseline);             // W bottom on the same baseline
    _gfx->print("W");
#else
    _setFont(6);
    int baseline = y + (SLOT_H + _tft.fontHeight()) / 2;
    _tft.setTextColor(col, C_INSET); _tft.setTextPadding(0);
    _tft.setTextDatum(BR_DATUM); _tft.drawString(num, rightX, baseline);
    _setFont(4);
    _tft.setTextColor(C_MUTED, C_INSET);
    _tft.setTextDatum(BL_DATUM); _tft.drawString("W", wLeftX, baseline);
    _tft.setTextDatum(TL_DATUM);
#endif
}

void DisplayUI::updateOverview() {
    char buf[16];
    // Solar W
    float sw = (!isnan(_sd.solarPower) && _sd.valid) ? _sd.solarPower : NAN;
    drawNodeVal(0, 20, CT_Y + 38, sw, sw > 0.5f ? C_GREEN : C_MUTED);   // green when producing
    // DC-DC W
    float ow = (_od.valid && !isnan(_od.outVoltage) && !isnan(_od.outCurrent)) ? _od.outVoltage * _od.outCurrent : NAN;
    drawNodeVal(1, 20, CT_Y + 172, ow, ow > 0.5f ? C_GREEN : C_MUTED);  // green when producing
    // Loads W = battery discharge only, straight from the BMS: what the loads pull FROM the
    // battery. power>0 = charging, power<0 = discharging, so discharge power = max(0,-power).
    // Charging or idle -> 0 (Loads must never show a value that is charging the battery).
    float loadsW = NAN;
    bool battDischarging = false;
    if (_bd.valid) {
        float batW = isnan(_bd.power) ? 0 : _bd.power;
        loadsW = fmaxf(0.0f, -batW);
        battDischarging = batW < -2.0f;
    }
    uint16_t loadsCol = battDischarging ? C_AMBER : (loadsW > 0.5f ? C_PALE : C_MUTED);
    drawNodeVal(2, 334, CT_Y + 105, loadsW, loadsCol);

    // Battery ring (center) — redraw only when SOC bucket changes
    int cx = 240, cy = CT_Y + 115, r = 50, th = 10;
    float soc = _bd.valid ? _bd.soc : NAN;
    int bucket = _bd.valid ? (int)soc : -1;
    if (bucket != _lastSocBucket) {
        _lastSocBucket = bucket;
        _D.fillRect(cx - r - th, cy - r - th, (r + th) * 2, (r + th) * 2, C_BG);
        uint16_t rc = _bd.valid ? socColor(soc) : C_MUTED;
        // offline: full grey ring so the footprint is visible (state colour = grey)
        drawRing(cx, cy, r, th, _bd.valid ? soc / 100.0f : 1.0f, rc, C_INSET);
        if (_bd.valid) {
            char num[6]; snprintf(num, sizeof(num), "%d", (int)soc);
            const int gap = 3;
            // At 100% the third digit makes [num %] ~88px wide, wider than the ring's usable
            // inner span (~81px at digit height) → it would clip the stroke. Drop one font step
            // for 3-digit values only; 0..99 keep the large font.
            const uint8_t nf = ((int)soc >= 100) ? 4 : 6;
            _setFont(nf); int nw = _textWidth(num);
            _setFont(2); int uw = _textWidth("%");
            int gl = cx - (nw + gap + uw) / 2;         // [num %] group centred at cx
#ifdef BOARD_GUITION
            _setFont(nf);
            int16_t x1, y1; uint16_t bw, bh; _gfx->getTextBounds(num, 0, 0, &x1, &y1, &bw, &bh);
            int topN = cy - (int)bh / 2;               // number vertically centred at cy
            drawTextL(gl, topN, num, nf, rc);
            drawTextL(gl + nw + gap, topN + (int)bh - 14, "%", 2, C_MUTED);
#else
            int hn = (nf == 6) ? 34 : 25;              // TFT free-font height (approx)
            int topN = cy - hn / 2;
            drawTextL(gl, topN, num, nf, rc);
            drawTextL(gl + nw + gap, topN + hn - 16, "%", 2, C_MUTED);
#endif
        } else {
#ifdef BOARD_GUITION
            _setFont(4); int16_t ex, ey; uint16_t ebw, ebh; _gfx->getTextBounds("--", 0, 0, &ex, &ey, &ebw, &ebh);
            _gfx->setTextColor(C_MUTED);
            _gfx->setCursor(cx - (int)ebw / 2 - ex, cy - (int)ebh / 2 - ey);   // "--" centred in ring
            _gfx->print("--");
#else
            _setFont(4);
            _tft.setTextColor(C_MUTED, C_BG); _tft.setTextPadding(0);
            _tft.setTextDatum(MC_DATUM); _tft.drawString("--", cx, cy);
            _tft.setTextDatum(TL_DATUM);
#endif
        }
    }
    // V / A / runtime stacked and centred below the ring. Three 16px lines from cy+r+12 end at
    // 259, still clear of the tab bar (264).
    if (_bd.valid) {
        char vb[8], ab[8]; fmtF(vb, _bd.voltage, 1); fmtF(ab, _bd.current, 1);
        char l1[10], l2[10], l3[12];
        snprintf(l1, sizeof(l1), "%s V", vb);
        snprintf(l2, sizeof(l2), "%s A", ab);
        bool etaFull = false;
        bmsFmtEta(l3, sizeof(l3), bmsEtaMinutes(_bd, etaFull));
        centerFill(cx, cy + r + 12, 120, 16, l1, 2, C_MUTED, C_BG);
        centerFill(cx, cy + r + 28, 120, 16, l2, 2, C_MUTED, C_BG);
        centerFill(cx, cy + r + 44, 120, 16, l3, 2, etaFull ? C_GREEN : C_MUTED, C_BG);
    } else {
        centerFill(cx, cy + r + 12, 120, 16, t("offline"), 2, C_MUTED, C_BG);
        _D.fillRect(cx - 60, cy + r + 28, 120, 32, C_BG);
    }
}

void DisplayUI::drawFlow(bool solar, bool dcdc, bool loads) {
    auto dash = [&](int x0, int y0, int x1, int y1, bool active, uint16_t col) {
        float dx = x1 - x0, dy = y1 - y0;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 1) return;
        float ux = dx / len, uy = dy / len;
        // active: coloured dashes that flow (phase-shifted). inactive: faint static grey dashes.
        int period = 14;
        for (int i = -period; i < (int)len + period; i += period) {
            for (int j = 0; j < period; j++) {
                int t = i + j - (active ? _flowPhase : 0);
                if (t < 0 || t >= (int)len) continue;
                int x = x0 + (int)(ux * t), y = y0 + (int)(uy * t);
                uint16_t dc = (j < 6) ? (active ? col : C_BORDER) : C_BG;
                _D.fillCircle(x, y, 1, dc);
            }
        }
    };
    // Battery-side endpoints kept ~60px from the ring centre (ring outer edge = 55) so the
    // dashes stop just outside the ring instead of drawing over its stroke each frame.
    dash(158, CT_Y + 48,  186, CT_Y + 89,  solar, C_ORANGE);  // solar → battery
    dash(158, CT_Y + 182, 186, CT_Y + 141, dcdc,  C_BLUE);    // dc-dc → battery
    dash(300, CT_Y + 115, 322, CT_Y + 115, loads, C_PALE);    // battery → loads
}

// ─── Battery ──────────────────────────────────────────────────────────────────
void DisplayUI::drawBattery() {
    drawCard(12, CT_Y + 6, 456, 110, C_CARD, true);     // card A: header + ring + grid
    fillText(26, CT_Y + 30, 260, 12, "LiFePO4 - BMS - Bluetooth", 1, C_MUTED, C_CARD);
    drawCard(12, CT_Y + 126, 456, 98, C_CARD, true);    // card B: stats + cells
    fillText(26, CT_Y + 168, 430, 12, "Cell voltages   recommended 3.00 - 3.55 V", 1, C_MUTED, C_CARD);
    updateBattery();
}

void DisplayUI::updateBattery() {
    char buf[16], vb[8], ab[8];
    bool on = _bd.valid;
    // dynamic model title from BLE data (generic when offline)
    char btit[24];
    if (on && _bd.cellCount > 0) {
        int nomV = _bd.cellCount <= 4 ? 12 : _bd.cellCount <= 8 ? 24 : 48;
        if (_bd.fullCapacityAh > 0) snprintf(btit, sizeof(btit), "%dV %.0fAh %dS", nomV, _bd.fullCapacityAh, _bd.cellCount);
        else                        snprintf(btit, sizeof(btit), "%dV %dS", nomV, _bd.cellCount);
    } else strcpy(btit, t("Battery"));
    fillText(26, CT_Y + 12, 300, 16, btit, 2, C_TEXT, C_CARD);
    // cell delta / balance
    float mn = 9, mx = 0; for (int i = 0; i < _bd.cellCount; i++) { if (_bd.cellVoltages[i] < mn) mn = _bd.cellVoltages[i]; if (_bd.cellVoltages[i] > mx) mx = _bd.cellVoltages[i]; }
    int delta = (_bd.cellCount > 0) ? (int)lroundf((mx - mn) * 1000) : 0;
    const char* bmsTxt = !on ? t("offline") : (delta <= 30 ? t("Balanced") : t("Balancing"));
    uint16_t bmsFg = !on ? C_MUTED : (delta <= 30 ? C_GREEN : C_AMBER);
    pillCached(0, 372, CT_Y + 12, 84, 20, bmsTxt, bmsFg, C_INSET);   // card A header row

    // ring (card A)
    int cx = 58, cy = CT_Y + 78, r = 24, th = 6;
    int bucket = on ? (int)_bd.soc : -1;
    if (bucket != _lastSocBucket) {
        _lastSocBucket = bucket;
        _D.fillRect(cx - r - th, cy - r - th, (r + th) * 2, (r + th) * 2, C_CARD);
        uint16_t rc = on ? socColor(_bd.soc) : C_MUTED;
        drawRing(cx, cy, r, th, on ? _bd.soc / 100.0f : 0, rc, C_INSET);
        if (on) snprintf(buf, sizeof(buf), "%d%%", (int)_bd.soc); else strcpy(buf, "--");
        // "100%" is 4 chars (~48px) and does not fit this small ring (42px inner) at font2 →
        // drop one step for 3-digit values only.
        const uint8_t sf = (on && (int)_bd.soc >= 100) ? 1 : 2;
        centerText(cx, cy - (sf == 2 ? 8 : 4), buf, sf, on ? rc : C_MUTED);
    }
    // 2x2 grid (card A, beside ring)
    fmtF(vb, on ? _bd.voltage : NAN, 2); snprintf(buf, sizeof(buf), "%s V", vb);
    drawStat(120, CT_Y + 50, 120, "Voltage", on ? buf : "--", C_TEXT);
    fmtF(ab, on ? _bd.current : NAN, 1); snprintf(buf, sizeof(buf), "%s A", ab);
    drawStat(250, CT_Y + 50, 120, "Current", on ? buf : "--", on ? signColor(_bd.current) : C_MUTED);
    fmtF(vb, on ? _bd.remainingAh : NAN, 0); snprintf(buf, sizeof(buf), "%s Ah", vb);
    drawStat(120, CT_Y + 80, 120, "Charge", on ? buf : "--", C_TEXT);
    fmtF(ab, on ? _bd.fullCapacityAh : NAN, 0); snprintf(buf, sizeof(buf), "%s Ah", ab);
    drawStat(250, CT_Y + 80, 120, "Capacity", on ? buf : "--", C_MUTED);
    // Runtime estimate — third column, in the free space under the BMS pill
    bool etaFull = false;
    char eb[12]; bmsFmtEta(eb, sizeof(eb), on ? bmsEtaMinutes(_bd, etaFull) : 0);
    drawStat(376, CT_Y + 50, 88, etaFull ? "To full" : "Runtime", on ? eb : "--",
             on ? (etaFull ? C_GREEN : C_TEXT) : C_MUTED);

    // 4 stats (card B)
    snprintf(buf, sizeof(buf), "%d%%", on ? (int)_bd.soh : 0);
    drawStat(26, CT_Y + 134, 100, "SOH", on ? buf : "--", C_TEXT);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)_bd.dischargesCount);
    drawStat(140, CT_Y + 134, 100, "Cycles", on ? buf : "--", C_TEXT);
    snprintf(buf, sizeof(buf), "%d C", on ? _bd.cellTemp : 0);
    drawStat(254, CT_Y + 134, 100, "Temp", on ? buf : "--", on ? (_bd.cellTemp > 45 ? C_RED : C_TEXT) : C_MUTED);
    snprintf(buf, sizeof(buf), "%d mV", delta);
    // Delta cell imbalance alarm: white ≤50mV, amber 50–100mV, red >100mV.
    uint16_t deltaCol = !on ? C_MUTED : (delta > 100 ? C_RED : delta > 50 ? C_AMBER : C_TEXT);
    drawStat(368, CT_Y + 134, 90, "Delta", on ? buf : "--", deltaCol);

    // cell voltage boxes — 4-col grid, red when out of range; redraw only on change
    int cells = _bd.cellCount; if (cells > 4) cells = 4;
    long sig = on ? cells : -1;
    for (int i = 0; i < cells; i++) sig = sig * 4099 + (long)lroundf(_bd.cellVoltages[i] * 1000);
    if (sig != _cellSig) {
        _cellSig = sig;
        const int bw = 101, bh = 38, boxY = CT_Y + 182;
        for (int i = 0; i < 4; i++) {
            int bx = 26 + i * (bw + 8);
            if (i < cells) {
                float v = _bd.cellVoltages[i];
                bool bad = (v < 3.00f || v > 3.55f);
                uint16_t bgc = bad ? 0x38A2 /* dark red tint */ : C_INSET;
                uint16_t bdc = bad ? C_RED : C_BORDER;
                uint16_t vc  = bad ? C_RED : C_TEXT;
                _D.fillRoundRect(bx, boxY, bw, bh, 8, bgc);
                _D.drawRoundRect(bx, boxY, bw, bh, 8, bdc);
                char lbl[8]; snprintf(lbl, sizeof(lbl), "C%d", i + 1);
                char cvb[10]; fmtF(cvb, v, 2);
#ifdef BOARD_GUITION
                // GFX fonts are taller — a separate "V" line overflows the box. Put value+unit
                // on one compact line ("3.28V") so the V no longer wraps below.
                centerFill(bx + bw / 2, boxY + 6,  bw - 8, 10, lbl, 1, C_MUTED, bgc);
                char cvu[12]; snprintf(cvu, sizeof(cvu), "%sV", cvb);
                centerFill(bx + bw / 2, boxY + 20, bw - 8, 16, cvu, 2, vc, bgc);
#else
                centerFill(bx + bw / 2, boxY + 5, bw - 8, 10, lbl, 1, C_MUTED, bgc);
                centerFill(bx + bw / 2, boxY + 16, bw - 8, 18, cvb, 2, vc, bgc);
                centerFill(bx + bw / 2, boxY + 30, bw - 8, 9, "V", 1, vc, bgc);
#endif
            } else {
                _D.fillRect(bx, boxY, bw, bh, C_CARD);  // clear unused box
            }
        }
    }
}

// ─── Solar (degraded: no PV V/A, no hourly history) ───────────────────────────
void DisplayUI::drawSolar() {
    drawCard(12, CT_Y + 8, 456, 118, C_CARD, true);
    // "W" bottom-aligned to the font4 value. GFX (Guition) metrics differ from TFT (CYD).
#ifdef BOARD_GUITION
    fillText(150, CT_Y + 49, 40, 16, "W", 2, C_MUTED, C_CARD);
#else
    fillText(150, CT_Y + 52, 40, 16, "W", 2, C_MUTED, C_CARD);
#endif
    // inset backgrounds + static labels (drawn once)
    int iy = CT_Y + 82, iw = 142;
    _D.fillRoundRect(26, iy, iw, 34, 8, C_INSET);
    _D.fillRoundRect(26 + iw + 6, iy, iw, 34, 8, C_INSET);
    _D.fillRoundRect(26 + 2*(iw+6), iy, iw, 34, 8, C_INSET);
    fillText(34, iy + 3, iw - 12, 11, t("Battery"), 1, C_MUTED, C_INSET);
    fillText(34 + iw + 6, iy + 3, iw - 12, 11, t("To battery"), 1, C_MUTED, C_INSET);
    fillText(34 + 2*(iw+6), iy + 3, iw - 12, 11, t("Yield today"), 1, C_MUTED, C_INSET);
    drawCard(12, CT_Y + 134, 456, 88, C_CARD, true);
    fillText(26, CT_Y + 144, 300, 14, t("Production today"), 1, C_MUTED, C_CARD);
    fillText(300, CT_Y + 170, 160, 14, t("energy today"), 1, C_MUTED, C_CARD);
    _solarSig = -1;   // force value redraw on screen entry
    updateSolar();
}

void DisplayUI::updateSolar() {
    char buf[16], tb[8];
    bool on = _sd.valid;
    // change-guard: redraw only when a shown value changes (CYD draws direct → repeated
    // redraws of the free-font values flicker; Guition uses a canvas so it's harmless there).
    auto ri = [](float v, float m){ return isnan(v) ? 0L : (long)lroundf(v * m); };
    long sig = on ? 1 : 0;
    if (on) {
        sig = sig*8191 + ri(_sd.solarPower, 1)  + _sd.chargeState;
        sig = sig*8191 + ri(_sd.battVoltage, 100);
        sig = sig*8191 + ri(_sd.chargeCurrent, 10);
        sig = sig*8191 + ri(_sd.yieldToday, 1);
    }
    if (sig == _solarSig) return;
    _solarSig = sig;
    fillText(26, CT_Y + 18, 320, 16, on ? victronModelName(_sd.productId, false) : t("Solar"), 2, C_TEXT, C_CARD);
    const char* state = on ? victronStateName(_sd.chargeState) : t("offline");
    pillCached(1, 360, CT_Y + 16, 96, 20, state, on ? C_ORANGE : C_MUTED, C_INSET);
    // big W (opaque, single pass)
    fmtF(buf, on ? _sd.solarPower : NAN, 0);
    centerFill(86, CT_Y + 42, 120, 28, buf, 4, on ? C_ORANGE : C_MUTED, C_CARD);
    // inset values (backgrounds/labels already drawn in drawSolar)
    int iy = CT_Y + 82, iw = 142;
    fmtF(tb, on ? _sd.battVoltage : NAN, 2); snprintf(buf, sizeof(buf), "%s V", tb);
    fillText(34, iy + 15, iw - 12, 16, on ? buf : "--", 2, C_TEXT, C_INSET);
    fmtF(tb, on ? _sd.chargeCurrent : NAN, 1); snprintf(buf, sizeof(buf), "%s A", tb);
    fillText(34 + iw + 6, iy + 15, iw - 12, 16, on ? buf : "--", 2, C_TEXT, C_INSET);
    fmtF(tb, on ? _sd.yieldToday : NAN, 0); snprintf(buf, sizeof(buf), "%s Wh", tb);
    fillText(34 + 2*(iw+6), iy + 15, iw - 12, 16, on ? buf : "--", 2, C_TEXT, C_INSET);
    // card B: yield today big
    fmtF(tb, on ? _sd.yieldToday : NAN, 0); snprintf(buf, sizeof(buf), "%s Wh", tb);
    centerFill(120, CT_Y + 162, 200, 28, buf, 4, on ? C_TEXT : C_MUTED, C_CARD);
}

// ─── DC-DC (degraded: no converter temp) ──────────────────────────────────────
void DisplayUI::drawDcdc() {
    drawCard(12, CT_Y + 8, 456, 118, C_CARD, true);
    drawCard(12, CT_Y + 134, 456, 88, C_CARD, true);
    fillText(26, CT_Y + 144, 300, 14, t("Charge profile"), 1, C_MUTED, C_CARD);
    // static charge profile
    fillText(26, CT_Y + 164, 210, 14, t("Current limit"), 1, C_MUTED, C_CARD);  fillText(180, CT_Y + 164, 56, 14, "50 A", 1, C_TEXT, C_CARD);
    fillText(26, CT_Y + 182, 210, 14, t("Input range"), 1, C_MUTED, C_CARD);    fillText(180, CT_Y + 182, 70, 14, "9-17 V", 1, C_TEXT, C_CARD);
    fillText(250, CT_Y + 164, 150, 14, t("Mode"), 1, C_MUTED, C_CARD);          fillText(360, CT_Y + 164, 90, 14, t("Adaptive"), 1, C_TEXT, C_CARD);
    fillText(250, CT_Y + 182, 150, 14, t("Engine detect"), 1, C_MUTED, C_CARD); fillText(360, CT_Y + 182, 90, 14, t("Auto"), 1, C_TEXT, C_CARD);
    // "W" bottom-aligned to the font4 value. GFX (Guition) metrics differ from TFT (CYD).
#ifdef BOARD_GUITION
    fillText(150, CT_Y + 49, 40, 16, "W", 2, C_MUTED, C_CARD);
#else
    fillText(150, CT_Y + 52, 40, 16, "W", 2, C_MUTED, C_CARD);
#endif
    // inset backgrounds + static labels (drawn once)
    int iy = CT_Y + 82, iw = 142;
    _D.fillRoundRect(26, iy, iw, 34, 8, C_INSET);
    _D.fillRoundRect(26 + iw + 6, iy, iw, 34, 8, C_INSET);
    _D.fillRoundRect(26 + 2*(iw+6), iy, iw, 34, 8, C_INSET);
    fillText(34, iy + 3, iw - 12, 11, t("Alternator in"), 1, C_MUTED, C_INSET);
    fillText(34 + iw + 6, iy + 3, iw - 12, 11, t("To battery"), 1, C_MUTED, C_INSET);
    fillText(34 + 2*(iw+6), iy + 3, iw - 12, 11, t("Output"), 1, C_MUTED, C_INSET);
    _dcdcSig = -1;   // force value redraw on screen entry
    updateDcdc();
}

void DisplayUI::updateDcdc() {
    char buf[16], tb[8];
    bool on = _od.valid;
    // change-guard (anti-flicker on CYD direct draw) — redraw only when a shown value changes
    auto ri = [](float v, float m){ return isnan(v) ? 0L : (long)lroundf(v * m); };
    long sig = on ? 1 : 0;
    if (on) {
        sig = sig*8191 + ri(_od.outVoltage, 100) + _od.deviceState;
        sig = sig*8191 + ri(_od.outCurrent, 10);
        sig = sig*8191 + ri(_od.inVoltage, 100);
    }
    if (sig == _dcdcSig) return;
    _dcdcSig = sig;
    fillText(26, CT_Y + 18, 320, 16, on ? victronModelName(_od.productId, true) : t("DC-DC"), 2, C_TEXT, C_CARD);
    float w = (on && !isnan(_od.outVoltage) && !isnan(_od.outCurrent)) ? _od.outVoltage * _od.outCurrent : NAN;
    // Real charge state from the advert (BULK/ABSORPTION/FLOAT/...), same as Solar — not a
    // Charging/Standby guess derived from the output current.
    const char* state = on ? victronStateName(_od.deviceState) : t("offline");
    pillCached(2, 360, CT_Y + 16, 96, 20, state, on ? C_BLUE : C_MUTED, C_INSET);
    fmtF(buf, w, 0);
    centerFill(86, CT_Y + 42, 120, 28, buf, 4, on ? C_BLUE : C_MUTED, C_CARD);
    int iy = CT_Y + 82, iw = 142;
    fmtF(tb, on ? _od.inVoltage : NAN, 1); snprintf(buf, sizeof(buf), "%s V", tb);
    fillText(34, iy + 15, iw - 12, 16, on ? buf : "--", 2, C_TEXT, C_INSET);
    fmtF(tb, on ? _od.outCurrent : NAN, 1); snprintf(buf, sizeof(buf), "%s A", tb);
    fillText(34 + iw + 6, iy + 15, iw - 12, 16, on ? buf : "--", 2, C_TEXT, C_INSET);
    fmtF(tb, on ? _od.outVoltage : NAN, 1); snprintf(buf, sizeof(buf), "%s V", tb);
    fillText(34 + 2*(iw+6), iy + 15, iw - 12, 16, on ? buf : "--", 2, C_TEXT, C_INSET);
}

// ─── Level ────────────────────────────────────────────────────────────────────
void DisplayUI::drawLevel() {
    drawCard(12, CT_Y + 8, 176, 214, C_CARD, true);
    fillText(24, CT_Y + 16, 150, 14, "Bubble level", 1, C_MUTED, C_CARD);
    // bubble base circle
    int cx = 100, cy = CT_Y + 116, R = 72;
    _D.fillCircle(cx, cy, R, C_INSET);
    _D.drawCircle(cx, cy, R, C_BORDER);
    _D.drawCircle(cx, cy, 30, C_BORDER);
    _D.drawFastHLine(cx - R + 8, cy, 2 * (R - 8), C_BORDER);
    _D.drawFastVLine(cx, cy - R + 8, 2 * (R - 8), C_BORDER);

    drawCard(200, CT_Y + 8, 128, 66, C_CARD, true);
    fillText(210, CT_Y + 16, 110, 12, t("PITCH F-R"), 1, C_MUTED, C_CARD);
    drawCard(336, CT_Y + 8, 132, 66, C_CARD, true);
    fillText(346, CT_Y + 16, 110, 12, t("ROLL L-R"), 1, C_MUTED, C_CARD);
    drawCard(200, CT_Y + 82, 268, 140, C_CARD, true);
    fillText(210, CT_Y + 90, 240, 14, "Ramp / chock guidance", 1, C_MUTED, C_CARD);
    _lvBx = -999;                     // fresh screen — no old bubble to erase
    _lastImuValid = !_imu.valid;      // force updateLevel to draw once on entry
    updateLevel();
}

void DisplayUI::updateLevel() {
    bool on = _imu.valid;
    float pitch = on ? _imu.pitch : 0, roll = on ? _imu.roll : 0;
    // Skip redraw when nothing shown changed: same online-state AND either offline (values are
    // static "--") or the angle moved less than the 0.3° deadband (IMU noise). Fixes the CYD
    // flicker — including the offline case, where the old guard never returned (on == false).
    if (on == _lastImuValid &&
        (!on || (fabsf(pitch - _lastPitch) < 0.3f && fabsf(roll - _lastRoll) < 0.3f)))
        return;
    _lastPitch = pitch; _lastRoll = roll; _lastImuValid = on;

    const float tol = 0.5f;
    bool okP = fabsf(pitch) <= tol, okR = fabsf(roll) <= tol, level = okP && okR;
    auto axCol = [&](float v){ return fabsf(v) <= tol ? C_GREEN : (fabsf(v) <= 2 ? C_AMBER : C_RED); };
    char buf[16];

    // bubble — erase ONLY the previous bubble spot, then restore the thin guides
    // it covered, then draw the new bubble. No full-disc fill (that was the flicker).
    int cx = 100, cy = CT_Y + 116, R = 72;
    int bx = cx + (int)constrain(roll * 6.0f, -54.0f, 54.0f);
    int by = cy + (int)constrain(-pitch * 6.0f, -54.0f, 54.0f);
    if (_lvBx > -900 && (_lvBx != bx || _lvBy != by))
        _D.fillCircle(_lvBx, _lvBy, 13, C_INSET);      // wipe old bubble
    // restore guides (thin — redraw in place, no visible flicker)
    _D.drawCircle(cx, cy, 30, C_BORDER);
    _D.drawFastHLine(cx - R + 8, cy, 2 * (R - 8), C_BORDER);
    _D.drawFastVLine(cx, cy - R + 8, 2 * (R - 8), C_BORDER);
    _D.fillCircle(bx, by, 12, on ? (level ? C_GREEN : C_AMBER) : C_MUTED);
    _lvBx = bx; _lvBy = by;

    // status pill
    pillCached(3, 52, CT_Y + 196, 96, 20, on ? (level ? "Levelled" : "Adjust") : "no sensor", on ? (level ? C_GREEN : C_AMBER) : C_MUTED, C_INSET);

    // pitch / roll values
    snprintf(buf, sizeof(buf), "%+.1f", pitch);
    fillText(210, CT_Y + 32, 110, 26, on ? buf : "--", 4, on ? axCol(pitch) : C_MUTED, C_CARD);
    snprintf(buf, sizeof(buf), "%+.1f", roll);
    fillText(346, CT_Y + 32, 110, 26, on ? buf : "--", 4, on ? axCol(roll) : C_MUTED, C_CARD);

    // ramp guidance (2 rows) — opaque in-place, no block clear
    const int mmPerDeg = 42;
    // roll row
    const char* rw = roll > tol ? "Left wheels" : (roll < -tol ? "Right wheels" : "Side axle");
    _D.fillCircle(218, CT_Y + 120, 4, okR ? C_GREEN : C_AMBER);
    fillText(230, CT_Y + 114, 150, 14, rw, 1, C_TEXT, C_CARD);
    if (okR) strcpy(buf, "OK"); else snprintf(buf, sizeof(buf), "raise %d mm", (int)(fabsf(roll) * mmPerDeg));
    fillText(360, CT_Y + 114, 96, 14, on ? buf : "--", 1, okR ? C_GREEN : C_AMBER, C_CARD);
    // pitch row
    const char* pw = pitch < -tol ? "Front wheels" : (pitch > tol ? "Rear wheels" : "Front-rear");
    _D.fillCircle(218, CT_Y + 146, 4, okP ? C_GREEN : C_AMBER);
    fillText(230, CT_Y + 140, 150, 14, pw, 1, C_TEXT, C_CARD);
    if (okP) strcpy(buf, "OK"); else snprintf(buf, sizeof(buf), "raise %d mm", (int)(fabsf(pitch) * mmPerDeg));
    fillText(360, CT_Y + 140, 96, 14, on ? buf : "--", 1, okP ? C_GREEN : C_AMBER, C_CARD);
}

// ─── System (degraded: no RSSI / no wifi scan) ────────────────────────────────
void DisplayUI::drawSystem() {
    drawCard(12, CT_Y + 8, 280, 214, C_CARD, true);
    fillText(24, CT_Y + 16, 200, 14, t("Connected devices"), 1, C_MUTED, C_CARD);
    drawCard(300, CT_Y + 8, 168, 40, C_INSET, true);   // toggles: screen timeout | AP
    fillText(312, CT_Y + 14, 48, 14, t("Screen"), 1, C_MUTED, C_INSET);
    fillText(418, CT_Y + 14, 18, 14, "AP", 1, C_MUTED, C_INSET);
    drawCard(300, CT_Y + 56, 168, 166, C_CARD, true);
    fillText(312, CT_Y + 64, 150, 14, t("Network"), 1, C_MUTED, C_CARD);
    updateSystem();
}

void DisplayUI::updateSystem() {
    struct { const char* name; bool on; } devs[4] = {
        { "Solar",       _sd.valid },
        { "DC-DC",       _od.valid },
        { "Battery",     _bd.valid },
        { "Tilt sensor", _imu.valid },
    };
    for (int i = 0; i < 4; i++) {
        int y = CT_Y + 40 + i * 44;
        _D.fillCircle(32, y + 12, 4, devs[i].on ? C_GREEN : C_MUTED);
        fillText(44, y + 4, 236, 16, t(devs[i].name), 2, C_TEXT, C_CARD);
        fillText(44, y + 22, 236, 12, t(devs[i].on ? "online" : "offline"), 1, devs[i].on ? C_GREEN : C_MUTED, C_CARD);
    }

    // screen-timeout toggle + AP state/escape hatch (labels drawn once in drawSystem)
    pillCached(4, 362, CT_Y + 16, 52, 22, _alwaysOn ? t("ALWAYS") : t("AUTO"), _alwaysOn ? C_GREEN : C_MUTED, _alwaysOn ? C_INSET : C_BG);
    pillCached(5, 438, CT_Y + 16, 28, 22, _syApOn ? "ON" : "OFF", _syApOn ? C_GREEN : C_MUTED, _syApOn ? C_INSET : C_BG);

    // network info
    int ny = CT_Y + 82;
    auto row = [&](const char* label, const char* val) {          // stacked (long values)
        fillText(312, ny, 150, 12, t(label), 1, C_MUTED, C_CARD);
        fillText(312, ny + 12, 150, 14, (val && val[0]) ? val : "--", 1, C_TEXT, C_CARD);
        ny += 24;
    };
    row("AP SSID", _syApSsid);
    row("AP IP", _syApIp);
    row("Client IP", _syStaIp[0] ? _syStaIp : nullptr);
    row("NTP time", _syNtpTime);
    auto rowc = [&](const char* label, const char* val, uint16_t vc) {   // compact single line
        fillText(312, ny, 66, 14, t(label), 1, C_MUTED, C_CARD);
        fillText(380, ny, 84, 14, val, 1, vc, C_CARD);
        ny += 13;
    };
    rowc("Language", _lang == 1 ? "Italiano" : "English", C_TEXT);
    rowc("OTA", _syOta ? "ON" : "OFF", _syOta ? C_GREEN : C_MUTED);
    rowc("Firmware", "v" FW_VERSION, C_TEXT);
}

// ─── updateSysInfo ────────────────────────────────────────────────────────────
void DisplayUI::updateSysInfo(const char* apSsid, const char* apIp,
                              const char* staSsid, const char* staIp,
                              const char* ntpTime, bool otaOn, bool apOn) {
    if (apSsid)  strncpy(_syApSsid,  apSsid,  sizeof(_syApSsid) - 1);
    if (apIp)    strncpy(_syApIp,    apIp,    sizeof(_syApIp) - 1);
    if (staSsid) strncpy(_syStaSsid, staSsid, sizeof(_syStaSsid) - 1);
    if (staIp)   strncpy(_syStaIp,   staIp,   sizeof(_syStaIp) - 1);
    if (ntpTime) strncpy(_syNtpTime, ntpTime, sizeof(_syNtpTime) - 1);
    _syOta  = otaOn;
    _syApOn = apOn;
    if (_screen == SCR_SYSTEM && _screenOn && !_firstDraw) { updateSystem(); present(); }
}

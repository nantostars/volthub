#include "DisplayUI.h"
#include "Config.h"
#include <math.h>

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
    switch (f) {
        case 1: _gfx->setTextSize(1); break;  // ~7px
        case 2: _gfx->setTextSize(2); break;  // ~14px
        case 4: _gfx->setTextSize(3); break;  // ~21px
        case 6: _gfx->setTextSize(5); break;  // ~35px
        default: _gfx->setTextSize(1); break;
    }
#else
    _tft.setTextFont(f);
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
    _gfx = new Arduino_AXS15231B(
        _bus, GFX_NOT_DEFINED, 0, false, 320, 480, 0, 0, 0, 0);
    _gfx->begin();
    _gfx->setRotation(1);   // landscape 480×320
#else
    pinMode(PIN_TOUCH_IRQ, INPUT);
    _tft.init();
    _tft.setRotation(1);
#endif

    _D.fillScreen(C_BG);
    _firstDraw   = true;
    _lastTouchMs = millis();
}

// ─── _readTouch (Guition only) ────────────────────────────────────────────────
#ifdef BOARD_GUITION
bool DisplayUI::_readTouch(int& sx, int& sy) {
    uint32_t now = millis();
    if (now - _touchPollMs >= 20) {
        _touchPollMs = now;
        static const uint8_t cmd[] = {0xb5, 0xab, 0xa5, 0x5a, 0x00, 0x00, 0x00, 0x08};
        Wire.beginTransmission(AXS15231B_ADDR);
        Wire.write(cmd, sizeof(cmd));
        Wire.endTransmission();
        Wire.requestFrom((uint8_t)AXS15231B_ADDR, (uint8_t)8);
        uint8_t buf[8] = {};
        for (int i = 0; i < 8 && Wire.available(); i++) buf[i] = Wire.read();
        _touchDown = (buf[1] > 0);
        if (_touchDown) {
            uint16_t tx = ((buf[2] & 0x0F) << 8) | buf[3];
            uint16_t ty = ((buf[4] & 0x0F) << 8) | buf[5];
            _touchSX = constrain((int)ty,       0, 479);
            _touchSY = constrain(319 - (int)tx, 0, 319);
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
        return;
    }

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
    }

    // Overview flow animation (60ms)
    if (_screen == SCR_OVERVIEW && nowMs - _lastFlowMs >= 60) {
        _lastFlowMs = nowMs;
        _flowPhase = (_flowPhase + 2) % 14;
        float solarW = (!isnan(_sd.solarPower) && _sd.chargeCurrent > 0.1f) ? _sd.solarPower : 0.0f;
        float orionW = (!isnan(_od.outCurrent) && _od.outCurrent > 0.1f)
                       ? _od.outCurrent * (_od.outVoltage > 0 ? _od.outVoltage : 13.0f) : 0.0f;
        float battW  = isnan(_bd.power) ? 0.0f : _bd.power;
        drawFlow(_sd.chargeCurrent > 0.1f, _od.outCurrent > 0.1f, fmaxf(0.0f, solarW + orionW - battW) > 2.0f);
    }
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

    // System screen: tap the "screen timeout" pill toggles always-on
    if (_screen == SCR_SYSTEM && hitTest(sx, sy, 300, CT_Y + 8, 168, 40)) {
        _alwaysOn = !_alwaysOn;
        drawSystem();
    }
}

bool DisplayUI::hitTest(int tx, int ty, int bx, int by, int bw, int bh) {
    return tx >= bx && tx < bx + bw && ty >= by && ty < by + bh;
}

void DisplayUI::selectScreen(Screen s) {
    _screen = s;
    // reset anti-flicker caches so the new screen fully repaints its values
    _lastSocBucket = -1; _lastPitch = 99.0f; _lastRoll = 99.0f; _lastImuValid = false;
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
    fillText(x, y, w, 12, label, 1, C_MUTED, C_CARD);
    fillText(x, y + 13, w, 20, val, 2, valCol, C_CARD);
}

void DisplayUI::fillText(int x, int y, int w, int h, const char* txt, uint8_t font, uint16_t col, uint16_t bg) {
#ifdef BOARD_GUITION
    _gfx->fillRect(x, y, w, h, bg);
    _setFont(font);
    _gfx->setTextColor(col);
    _gfx->setCursor(x + 1, y + 1);
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
    _gfx->setCursor(cx - w / 2, y);
    _gfx->print(txt);
#else
    _tft.setTextColor(col);
    _tft.setTextPadding(0);
    _tft.setTextDatum(TL_DATUM);
    _tft.drawString(txt, cx - w / 2, y);
#endif
}

// Opaque centered text — single drawString pass clears its own w×h region (no flicker).
void DisplayUI::centerFill(int cx, int y, int w, int h, const char* txt, uint8_t font, uint16_t col, uint16_t bg) {
#ifdef BOARD_GUITION
    _D.fillRect(cx - w / 2, y, w, h, bg);
    _setFont(font);
    _gfx->setTextColor(col);
    int tw = _textWidth(txt);
    _gfx->setCursor(cx - tw / 2, y + 1);
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
    if (slot >= 0 && slot < 5) {
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
        // wordmark "volt·hub"
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

    // clock
    char hhmm[6] = "--:--";
    if (strlen(_syNtpTime) >= 5) { hhmm[0]=_syNtpTime[0]; hhmm[1]=_syNtpTime[1]; hhmm[2]=':'; hhmm[3]=_syNtpTime[3]; hhmm[4]=_syNtpTime[4]; hhmm[5]=0; }
    if (full || strcmp(_lastClock, hhmm) != 0) {
        strcpy(_lastClock, hhmm);
        fillText(420, 10, 52, 16, hhmm, 2, C_TEXT, C_BG);
    }

    // BLE device count
    int devs = (_bd.valid?1:0) + (_sd.valid?1:0) + (_od.valid?1:0) + (_imu.valid?1:0);
    if (full || devs != _lastDevs) {
        _lastDevs = devs;
        char cbuf[8]; snprintf(cbuf, sizeof(cbuf), "BLE %d", devs);
        fillText(360, 11, 56, 14, cbuf, 1, C_BLUE, C_BG);
    }

    // charge state dot + label
    float p = isnan(_bd.power) ? 0 : _bd.power;
    int st = p > 8 ? 1 : (p < -8 ? -1 : 0);
    if (full || st != _lastChargeState) {
        _lastChargeState = st;
        const char* lbl = st == 1 ? "Charging" : st == -1 ? "Discharging" : "Idle";
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
        int tw = _textWidth(labels[i]); _setFont(1);
        tw = _textWidth(labels[i]);
#ifdef BOARD_GUITION
        _gfx->setTextColor(col); _gfx->setCursor(x + (TAB_W - tw) / 2, TAB_Y + 38); _gfx->print(labels[i]);
#else
        _tft.setTextColor(col); _tft.setTextPadding(0); _tft.setTextDatum(TL_DATUM);
        _tft.drawString(labels[i], x + (TAB_W - tw) / 2, TAB_Y + 38);
#endif
    }
}

// ─── Overview: radial flow ────────────────────────────────────────────────────
void DisplayUI::drawOverview() {
    // side nodes
    drawCard(12, CT_Y + 16, 120, 60, C_INSET, true);
    fillText(22, CT_Y + 22, 100, 12, "Solar", 1, C_MUTED, C_INSET);
    drawCard(12, CT_Y + 150, 120, 60, C_INSET, true);
    fillText(22, CT_Y + 156, 100, 12, "DC-DC", 1, C_MUTED, C_INSET);
    drawCard(348, CT_Y + 84, 120, 60, C_INSET, true);
    fillText(358, CT_Y + 90, 100, 12, "Loads", 1, C_MUTED, C_INSET);
    updateOverview();
}

void DisplayUI::updateOverview() {
    char buf[16];
    // Solar W
    float sw = (!isnan(_sd.solarPower) && _sd.valid) ? _sd.solarPower : NAN;
    fmtF(buf, sw, 0); strcat(buf, " W");
    fillText(22, CT_Y + 40, 100, 22, buf, 4, sw > 0.5f ? C_ORANGE : C_MUTED, C_INSET);
    // DC-DC W
    float ow = (_od.valid && !isnan(_od.outVoltage) && !isnan(_od.outCurrent)) ? _od.outVoltage * _od.outCurrent : NAN;
    fmtF(buf, ow, 0); strcat(buf, " W");
    fillText(22, CT_Y + 174, 100, 22, buf, 4, ow > 0.5f ? C_BLUE : C_MUTED, C_INSET);
    // Loads W (aggregate only — degraded)
    float loadsW = NAN;
    if (_bd.valid) {
        float solW = (!isnan(_sd.solarPower)) ? _sd.solarPower : 0;
        float dcW  = (!isnan(ow)) ? ow : 0;
        float batW = isnan(_bd.power) ? 0 : _bd.power;
        loadsW = fmaxf(0.0f, solW + dcW - batW);
    }
    fmtF(buf, loadsW, 0); strcat(buf, " W");
    fillText(358, CT_Y + 108, 100, 22, buf, 4, loadsW > 0.5f ? C_PALE : C_MUTED, C_INSET);

    // Battery ring (center) — redraw only when SOC bucket changes
    int cx = 240, cy = CT_Y + 96, r = 52, th = 11;
    float soc = _bd.valid ? _bd.soc : NAN;
    int bucket = _bd.valid ? (int)soc : -1;
    if (bucket != _lastSocBucket) {
        _lastSocBucket = bucket;
        _D.fillRect(cx - r - th, cy - r - th, (r + th) * 2, (r + th) * 2, C_BG);
        uint16_t rc = _bd.valid ? socColor(soc) : C_MUTED;
        drawRing(cx, cy, r, th, _bd.valid ? soc / 100.0f : 0.0f, rc, C_INSET);
        // center text
        if (_bd.valid) { snprintf(buf, sizeof(buf), "%d%%", (int)soc); }
        else strcpy(buf, "--");
        centerText(cx, cy - 14, buf, 4, _bd.valid ? rc : C_MUTED);
    }
    // sub line V·A (always refresh)
    if (_bd.valid) {
        char vb[8], ab[8]; fmtF(vb, _bd.voltage, 1); fmtF(ab, _bd.current, 1);
        snprintf(buf, sizeof(buf), "%sV %sA", vb, ab);
    } else strcpy(buf, "offline");
    centerFill(cx, cy + 15, 110, 14, buf, 1, C_MUTED, C_BG);
}

void DisplayUI::drawFlow(bool solar, bool dcdc, bool loads) {
    auto dash = [&](int x0, int y0, int x1, int y1, bool active, uint16_t col) {
        float dx = x1 - x0, dy = y1 - y0;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 1) return;
        float ux = dx / len, uy = dy / len;
        uint16_t c = active ? col : C_BORDER;
        // clear the corridor lightly by redrawing background dots first would flicker;
        // instead draw gap dots in bg then segment dots in colour
        int period = 14;
        for (int i = -period; i < (int)len + period; i += period) {
            for (int j = 0; j < period; j++) {
                int t = i + j - _flowPhase;
                if (t < 0 || t >= (int)len) continue;
                int x = x0 + (int)(ux * t), y = y0 + (int)(uy * t);
                uint16_t dc = (j < 6 && active) ? c : C_BG;
                _D.fillCircle(x, y, 1, dc);
            }
        }
    };
    dash(132, CT_Y + 46, 186, CT_Y + 84,  solar, C_ORANGE);   // solar → battery
    dash(132, CT_Y + 180, 186, CT_Y + 108, dcdc,  C_BLUE);    // dc-dc → battery
    dash(294, CT_Y + 96, 348, CT_Y + 114,  loads, C_PALE);    // battery → loads
}

// ─── Battery ──────────────────────────────────────────────────────────────────
void DisplayUI::drawBattery() {
    drawCard(12, CT_Y + 8, 456, 84, C_CARD, true);
    drawCard(12, CT_Y + 100, 456, 122, C_CARD, true);
    // header of card B
    fillText(26, CT_Y + 110, 300, 16, "LiTime 12V bank · LiFePO4", 2, C_TEXT, C_CARD);
    updateBattery();
}

void DisplayUI::updateBattery() {
    char buf[16], vb[8], ab[8];
    bool on = _bd.valid;
    // ring in card A
    int cx = 58, cy = CT_Y + 50, r = 30, th = 8;
    int bucket = on ? (int)_bd.soc : -1;
    if (bucket != _lastSocBucket) {
        _lastSocBucket = bucket;
        _D.fillRect(cx - r - th, cy - r - th, (r + th) * 2, (r + th) * 2, C_CARD);
        uint16_t rc = on ? socColor(_bd.soc) : C_MUTED;
        drawRing(cx, cy, r, th, on ? _bd.soc / 100.0f : 0, rc, C_INSET);
        if (on) snprintf(buf, sizeof(buf), "%d%%", (int)_bd.soc); else strcpy(buf, "--");
        centerText(cx, cy - 10, buf, 2, on ? rc : C_MUTED);
    }
    // 2x2 grid in card A
    fmtF(vb, on ? _bd.voltage : NAN, 2); snprintf(buf, sizeof(buf), "%s V", vb);
    drawStat(120, CT_Y + 16, 120, "Voltage", on ? buf : "--", C_TEXT);
    fmtF(ab, on ? _bd.current : NAN, 1); snprintf(buf, sizeof(buf), "%s A", ab);
    drawStat(250, CT_Y + 16, 120, "Current", on ? buf : "--", on ? signColor(_bd.current) : C_MUTED);
    fmtF(vb, on ? _bd.remainingAh : NAN, 0); snprintf(buf, sizeof(buf), "%s Ah", vb);
    drawStat(120, CT_Y + 50, 120, "Charge", on ? buf : "--", C_TEXT);
    fmtF(ab, on ? _bd.fullCapacityAh : NAN, 0); snprintf(buf, sizeof(buf), "%s Ah", ab);
    drawStat(250, CT_Y + 50, 120, "Capacity", on ? buf : "--", C_MUTED);

    // card B: BMS pill + 4 stats + cell bars
    // cell delta
    float mn = 9, mx = 0; for (int i = 0; i < _bd.cellCount; i++) { if (_bd.cellVoltages[i] < mn) mn = _bd.cellVoltages[i]; if (_bd.cellVoltages[i] > mx) mx = _bd.cellVoltages[i]; }
    int delta = (_bd.cellCount > 0) ? (int)lroundf((mx - mn) * 1000) : 0;
    const char* bmsTxt = !on ? "offline" : (delta <= 30 ? "Balanced" : "Balancing");
    uint16_t bmsFg = !on ? C_MUTED : (delta <= 30 ? C_GREEN : C_AMBER);
    pillCached(0, 392, CT_Y + 108, 64, 20, bmsTxt, bmsFg, C_INSET);

    snprintf(buf, sizeof(buf), "%d%%", on ? (int)_bd.soh : 0);
    drawStat(26, CT_Y + 132, 100, "SOH", on ? buf : "--", C_TEXT);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)_bd.dischargesCount);
    drawStat(140, CT_Y + 132, 100, "Cycles", on ? buf : "--", C_TEXT);
    snprintf(buf, sizeof(buf), "%d C", on ? _bd.cellTemp : 0);
    drawStat(254, CT_Y + 132, 100, "Temp", on ? buf : "--", on ? (_bd.cellTemp > 45 ? C_RED : C_TEXT) : C_MUTED);
    snprintf(buf, sizeof(buf), "%d mV", delta);
    drawStat(368, CT_Y + 132, 90, "Delta", on ? buf : "--", C_TEXT);

    // cell bars — redraw only when the cell data actually changes (BMS polls ~2s)
    int cells = _bd.cellCount; if (cells > 4) cells = 4;
    long sig = on ? cells : -1;
    for (int i = 0; i < cells; i++) sig = sig * 4099 + (long)lroundf(_bd.cellVoltages[i] * 1000);
    if (sig != _cellSig) {
        _cellSig = sig;
        int barY = CT_Y + 170;
        for (int i = 0; i < 4; i++) {
            int y = barY + i * 13;
            if (i < cells) {
                char lbl[10]; snprintf(lbl, sizeof(lbl), "C%d", i + 1);
                fillText(26, y, 26, 11, lbl, 1, C_MUTED, C_CARD);
                int bx = 56, bw = 300, bh = 9;
                float pct = (_bd.cellVoltages[i] - 2.9f) / (3.65f - 2.9f);
                if (pct < 0) pct = 0; if (pct > 1) pct = 1;
                int fw = (int)(bw * pct);
                // fill + remainder tile the track exactly (no full-row blank frame)
                if (fw > 0) _D.fillRoundRect(bx, y + 1, fw, bh, 3, C_BLUE);
                if (fw < bw) _D.fillRect(bx + fw, y + 1, bw - fw, bh, C_INSET);
                char cvb[10]; fmtF(cvb, _bd.cellVoltages[i], 2); strcat(cvb, "V");
                fillText(bx + bw + 8, y, 60, 11, cvb, 1, C_TEXT, C_CARD);
            } else {
                _D.fillRect(26, y, 432, 12, C_CARD);  // clear unused rows once
            }
        }
    }
}

// ─── Solar (degraded: no PV V/A, no hourly history) ───────────────────────────
void DisplayUI::drawSolar() {
    drawCard(12, CT_Y + 8, 456, 118, C_CARD, true);
    fillText(26, CT_Y + 18, 300, 16, "Victron SmartSolar MPPT", 2, C_TEXT, C_CARD);
    fillText(150, CT_Y + 52, 40, 16, "W", 2, C_MUTED, C_CARD);
    // inset backgrounds + static labels (drawn once)
    int iy = CT_Y + 82, iw = 142;
    _D.fillRoundRect(26, iy, iw, 34, 8, C_INSET);
    _D.fillRoundRect(26 + iw + 6, iy, iw, 34, 8, C_INSET);
    _D.fillRoundRect(26 + 2*(iw+6), iy, iw, 34, 8, C_INSET);
    fillText(34, iy + 3, iw - 12, 11, "Battery", 1, C_MUTED, C_INSET);
    fillText(34 + iw + 6, iy + 3, iw - 12, 11, "To battery", 1, C_MUTED, C_INSET);
    fillText(34 + 2*(iw+6), iy + 3, iw - 12, 11, "Yield today", 1, C_MUTED, C_INSET);
    drawCard(12, CT_Y + 134, 456, 88, C_CARD, true);
    fillText(26, CT_Y + 144, 300, 14, "Production today", 1, C_MUTED, C_CARD);
    fillText(300, CT_Y + 170, 160, 14, "energia oggi", 1, C_MUTED, C_CARD);
    updateSolar();
}

void DisplayUI::updateSolar() {
    char buf[16], t[8];
    bool on = _sd.valid;
    const char* state = on ? victronStateName(_sd.chargeState) : "offline";
    pillCached(1, 360, CT_Y + 16, 96, 20, state, on ? C_ORANGE : C_MUTED, C_INSET);
    // big W (opaque, single pass)
    fmtF(buf, on ? _sd.solarPower : NAN, 0);
    centerFill(86, CT_Y + 42, 120, 28, buf, 4, on ? C_ORANGE : C_MUTED, C_CARD);
    // inset values (backgrounds/labels already drawn in drawSolar)
    int iy = CT_Y + 82, iw = 142;
    fmtF(t, on ? _sd.battVoltage : NAN, 2); snprintf(buf, sizeof(buf), "%s V", t);
    fillText(34, iy + 15, iw - 12, 16, on ? buf : "--", 2, C_TEXT, C_INSET);
    fmtF(t, on ? _sd.chargeCurrent : NAN, 1); snprintf(buf, sizeof(buf), "%s A", t);
    fillText(34 + iw + 6, iy + 15, iw - 12, 16, on ? buf : "--", 2, C_TEXT, C_INSET);
    fmtF(t, on ? _sd.yieldToday : NAN, 0); snprintf(buf, sizeof(buf), "%s Wh", t);
    fillText(34 + 2*(iw+6), iy + 15, iw - 12, 16, on ? buf : "--", 2, C_ORANGE, C_INSET);
    // card B: yield today big
    fmtF(t, on ? _sd.yieldToday : NAN, 0); snprintf(buf, sizeof(buf), "%s Wh", t);
    centerFill(120, CT_Y + 162, 200, 28, buf, 4, on ? C_TEXT : C_MUTED, C_CARD);
}

// ─── DC-DC (degraded: no converter temp) ──────────────────────────────────────
void DisplayUI::drawDcdc() {
    drawCard(12, CT_Y + 8, 456, 118, C_CARD, true);
    fillText(26, CT_Y + 18, 300, 16, "Victron Orion-XS DC-DC", 2, C_TEXT, C_CARD);
    drawCard(12, CT_Y + 134, 456, 88, C_CARD, true);
    fillText(26, CT_Y + 144, 300, 14, "Charge profile", 1, C_MUTED, C_CARD);
    // static charge profile
    fillText(26, CT_Y + 164, 210, 14, "Current limit", 1, C_MUTED, C_CARD);  fillText(180, CT_Y + 164, 56, 14, "50 A", 1, C_TEXT, C_CARD);
    fillText(26, CT_Y + 182, 210, 14, "Input range", 1, C_MUTED, C_CARD);    fillText(180, CT_Y + 182, 70, 14, "9-17 V", 1, C_TEXT, C_CARD);
    fillText(250, CT_Y + 164, 150, 14, "Mode", 1, C_MUTED, C_CARD);          fillText(360, CT_Y + 164, 90, 14, "Adaptive", 1, C_TEXT, C_CARD);
    fillText(250, CT_Y + 182, 150, 14, "Engine detect", 1, C_MUTED, C_CARD); fillText(360, CT_Y + 182, 90, 14, "Auto", 1, C_TEXT, C_CARD);
    fillText(150, CT_Y + 52, 40, 16, "W", 2, C_MUTED, C_CARD);
    // inset backgrounds + static labels (drawn once)
    int iy = CT_Y + 82, iw = 142;
    _D.fillRoundRect(26, iy, iw, 34, 8, C_INSET);
    _D.fillRoundRect(26 + iw + 6, iy, iw, 34, 8, C_INSET);
    _D.fillRoundRect(26 + 2*(iw+6), iy, iw, 34, 8, C_INSET);
    fillText(34, iy + 3, iw - 12, 11, "Alternator in", 1, C_MUTED, C_INSET);
    fillText(34 + iw + 6, iy + 3, iw - 12, 11, "To battery", 1, C_MUTED, C_INSET);
    fillText(34 + 2*(iw+6), iy + 3, iw - 12, 11, "Output", 1, C_MUTED, C_INSET);
    updateDcdc();
}

void DisplayUI::updateDcdc() {
    char buf[16], t[8];
    bool on = _od.valid;
    float w = (on && !isnan(_od.outVoltage) && !isnan(_od.outCurrent)) ? _od.outVoltage * _od.outCurrent : NAN;
    const char* state = !on ? "offline" : (_od.outCurrent > 0.1f ? "Charging" : "Standby");
    pillCached(2, 360, CT_Y + 16, 96, 20, state, on ? C_BLUE : C_MUTED, C_INSET);
    fmtF(buf, w, 0);
    centerFill(86, CT_Y + 42, 120, 28, buf, 4, on ? C_BLUE : C_MUTED, C_CARD);
    int iy = CT_Y + 82, iw = 142;
    fmtF(t, on ? _od.inVoltage : NAN, 1); snprintf(buf, sizeof(buf), "%s V", t);
    fillText(34, iy + 15, iw - 12, 16, on ? buf : "--", 2, C_TEXT, C_INSET);
    fmtF(t, on ? _od.outCurrent : NAN, 1); snprintf(buf, sizeof(buf), "%s A", t);
    fillText(34 + iw + 6, iy + 15, iw - 12, 16, on ? buf : "--", 2, C_TEXT, C_INSET);
    fmtF(t, on ? _od.outVoltage : NAN, 1); snprintf(buf, sizeof(buf), "%s V", t);
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
    fillText(210, CT_Y + 16, 110, 12, "PITCH F-R", 1, C_MUTED, C_CARD);
    drawCard(336, CT_Y + 8, 132, 66, C_CARD, true);
    fillText(346, CT_Y + 16, 110, 12, "ROLL L-R", 1, C_MUTED, C_CARD);
    drawCard(200, CT_Y + 82, 268, 140, C_CARD, true);
    fillText(210, CT_Y + 90, 240, 14, "Ramp / chock guidance", 1, C_MUTED, C_CARD);
    _lvBx = -999;   // fresh screen — no old bubble to erase
    updateLevel();
}

void DisplayUI::updateLevel() {
    bool on = _imu.valid;
    float pitch = on ? _imu.pitch : 0, roll = on ? _imu.roll : 0;
    if (on && fabsf(pitch - _lastPitch) < 0.2f && fabsf(roll - _lastRoll) < 0.2f && on == _lastImuValid) return;
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
    fillText(24, CT_Y + 16, 200, 14, "Connected devices", 1, C_MUTED, C_CARD);
    drawCard(300, CT_Y + 8, 168, 40, C_INSET, true);   // screen-timeout toggle pill area
    fillText(312, CT_Y + 14, 90, 14, "Screen", 1, C_MUTED, C_INSET);
    drawCard(300, CT_Y + 56, 168, 166, C_CARD, true);
    fillText(312, CT_Y + 64, 150, 14, "Network", 1, C_MUTED, C_CARD);
    updateSystem();
}

void DisplayUI::updateSystem() {
    struct { const char* name; bool on; } devs[4] = {
        { "SmartSolar MPPT", _sd.valid },
        { "Orion-XS DC-DC",  _od.valid },
        { "LiTime BMS",      _bd.valid },
        { "WitMotion IMU",   _imu.valid },
    };
    for (int i = 0; i < 4; i++) {
        int y = CT_Y + 40 + i * 44;
        _D.fillCircle(32, y + 12, 4, devs[i].on ? C_GREEN : C_MUTED);
        fillText(44, y + 4, 236, 16, devs[i].name, 2, C_TEXT, C_CARD);
        fillText(44, y + 22, 236, 12, devs[i].on ? "online" : "offline", 1, devs[i].on ? C_GREEN : C_MUTED, C_CARD);
    }

    // screen-timeout toggle (label drawn once in drawSystem)
    pillCached(4, 400, CT_Y + 16, 60, 22, _alwaysOn ? "ALWAYS" : "AUTO", _alwaysOn ? C_GREEN : C_MUTED, _alwaysOn ? C_INSET : C_BG);

    // network info
    int ny = CT_Y + 82;
    auto row = [&](const char* label, const char* val) {
        fillText(312, ny, 150, 12, label, 1, C_MUTED, C_CARD);
        fillText(312, ny + 12, 150, 14, (val && val[0]) ? val : "--", 1, C_TEXT, C_CARD);
        ny += 32;
    };
    row("AP SSID", _syApSsid);
    row("AP IP", _syApIp);
    row("Client IP", _syStaIp[0] ? _syStaIp : nullptr);
    row("NTP time", _syNtpTime);
}

// ─── updateSysInfo ────────────────────────────────────────────────────────────
void DisplayUI::updateSysInfo(const char* apSsid, const char* apIp,
                              const char* staSsid, const char* staIp,
                              const char* ntpTime) {
    if (apSsid)  strncpy(_syApSsid,  apSsid,  sizeof(_syApSsid) - 1);
    if (apIp)    strncpy(_syApIp,    apIp,    sizeof(_syApIp) - 1);
    if (staSsid) strncpy(_syStaSsid, staSsid, sizeof(_syStaSsid) - 1);
    if (staIp)   strncpy(_syStaIp,   staIp,   sizeof(_syStaIp) - 1);
    if (ntpTime) strncpy(_syNtpTime, ntpTime, sizeof(_syNtpTime) - 1);
    if (_screen == SCR_SYSTEM && _screenOn && !_firstDraw) updateSystem();
}

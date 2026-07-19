#include "DisplayUI.h"
#include "Config.h"
#include <math.h>

// Board-agnostic display alias: _D.fillRect(...) works for both TFT_eSPI and Arduino_GFX
#ifdef BOARD_GUITION
#define _D (*_gfx)
#else
#define _D _tft
#endif

// ─── Float formatting ─────────────────────────────────────────────────────────
static void fmtF(char* buf, float v, int dec) {
    if (isnan(v) || v == 0.0f / 0.0f) { strcpy(buf, "--"); return; }
    dtostrf(v, 0, dec, buf);
}

// ─── Color helpers ────────────────────────────────────────────────────────────
static uint16_t signCol(float v)  { return v >  0.05f ? UI_GREEN : v < -0.05f ? UI_ORANGE : UI_TEXT; }
static uint16_t socCol(float soc) { return soc > 50 ? UI_GREEN : soc > 20 ? UI_YELLOW : UI_RED; }

static uint16_t stateColor(const char* s) {
    if (!s) return UI_MUTED;
    String n = String(s); n.toLowerCase();
    if (n == "float")       return UI_GREEN;
    if (n == "bulk")        return UI_YELLOW;
    if (n == "absorption")  return UI_BLUE;
    if (n == "fault")       return UI_RED;
    return UI_MUTED;
}

// ─── Constructor ──────────────────────────────────────────────────────────────

DisplayUI::DisplayUI() {}

// ─── Font helpers (board-specific) ───────────────────────────────────────────

void DisplayUI::_setFont(uint8_t f) {
#ifdef BOARD_GUITION
    _gfx->setFont(nullptr);   // built-in 5×7 bitmap font
    switch (f) {
        case 1: _gfx->setTextSize(1); break;  // ~7px  — label
        case 2: _gfx->setTextSize(2); break;  // ~14px — state/header
        case 4: _gfx->setTextSize(3); break;  // ~21px — values (≈ font4 26px; TODO: swap to GFX font after hw tuning)
        case 6: _gfx->setTextSize(6); break;  // ~42px — level screen (≈ font6 48px)
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
    // Touch I2C — AXS15231B integrated capacitive touch
    Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL);
    Wire.setClock(400000);

    // Display — AXS15231B QSPI (SPI2_HOST)
    // Pins confirmed from JC3248W535EN/1-Demo/Demo_Arduino/DEMO_LVGL/esp_bsp.h
    _bus = new Arduino_ESP32QSPI(
        45 /* CS */, 47 /* PCLK */,
        21 /* D0 */, 48 /* D1 */, 40 /* D2 */, 39 /* D3 */);
    // GFX Library 1.4.9 constructor: (bus, rst, rotation, ips, w, h, col_off1, row_off1, col_off2, row_off2)
    // The built-in init sequence targets a 360×640 panel, but CASET/RASET handle the 320×480 window.
    // If colors look wrong on hardware, upgrade to GFX Library ≥1.5 which ships 320×480 init sequences,
    // or provide a custom sequence from the Guition demo (esp_lcd_axs15231b.c vendor_specific_init_default[]).
    _gfx = new Arduino_AXS15231B(
        _bus,
        GFX_NOT_DEFINED,  // RST = not connected
        0,                // initial rotation (overridden by setRotation below)
        false,            // IPS
        320, 480,         // native portrait resolution
        0, 0, 0, 0);      // no column/row offsets
    _gfx->begin();
    _gfx->setRotation(1);   // landscape 480×320

#else
    pinMode(PIN_TOUCH_IRQ, INPUT);
    _tft.init();
    _tft.setRotation(1);
#endif

    _D.fillScreen(UI_BG);
    _firstDraw   = true;
    _lastTouchMs = millis();
}

// ─── _readTouch (Guition only) — poll AXS15231B I2C, apply landscape transform ─

#ifdef BOARD_GUITION
bool DisplayUI::_readTouch(int& sx, int& sy) {
    uint32_t now = millis();
    if (now - _touchPollMs >= 20) {
        _touchPollMs = now;
        // AXS15231B touch read protocol (8-byte command, 8-byte response)
        static const uint8_t cmd[] = {0xb5, 0xab, 0xa5, 0x5a, 0x00, 0x00, 0x00, 0x08};
        Wire.beginTransmission(AXS15231B_ADDR);
        Wire.write(cmd, sizeof(cmd));
        Wire.endTransmission();
        Wire.requestFrom((uint8_t)AXS15231B_ADDR, (uint8_t)8);
        uint8_t buf[8] = {};
        for (int i = 0; i < 8 && Wire.available(); i++) buf[i] = Wire.read();
        _touchDown = (buf[1] > 0);
        if (_touchDown) {
            uint16_t tx = ((buf[2] & 0x0F) << 8) | buf[3]; // portrait X [0,319]
            uint16_t ty = ((buf[4] & 0x0F) << 8) | buf[5]; // portrait Y [0,479]
            // Landscape transform for setRotation(1): portrait-Y → screen-X, portrait-X (inverted) → screen-Y
            // TODO: verify axis mapping on hardware; adjust mirror flags if coordinates are inverted
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

    // Screen timeout: turn off backlight after 1 minute of inactivity
    if (!_alwaysOn && _screenOn && (nowMs - _lastTouchMs > 60000UL)) {
        _screenOn = false;
        digitalWrite(PIN_BL, LOW);
    }
    if (!_screenOn) return;

    if (_screen == SCR_OVERVIEW) {
        if (_firstDraw) {
            drawOverview();
            _firstDraw = false;
            _lastValMs = nowMs;
        } else if (nowMs - _lastValMs >= 300) {
            updateOverviewValues();
            _lastValMs = nowMs;
        }
        if (nowMs - _lastFlowMs >= 60) {
            _flowPhase = (_flowPhase + 12) % 14;
            _lastFlowMs = nowMs;
            float solarW = (!isnan(_sd.solarPower) && _sd.chargeCurrent > 0.1f) ? _sd.solarPower : 0.0f;
            float orionW = (!isnan(_od.outCurrent) && _od.outCurrent   > 0.1f) ? _od.outCurrent * (_od.outVoltage > 0 ? _od.outVoltage : 13.0f) : 0.0f;
            float battW  = isnan(_bd.power) ? 0.0f : _bd.power;
            drawFlowLines(_sd.chargeCurrent > 0.1f, _od.outCurrent > 0.1f, fmaxf(0.0f, solarW + orionW - battW) > 2.0f);
        }
    } else if (_screen == SCR_LEVEL) {
        if (_lvFirstDraw) {
            drawLevelScreen();
            _lvFirstDraw = false;
        } else if (_imu.valid != _lastImuValid ||
                   fabsf(imu.pitch - _lastPitch) > 0.3f || fabsf(imu.roll - _lastRoll) > 0.3f) {
            updateLevelValues();
        }
    } else if (_screen == SCR_SETTINGS) {
        if (_firstDraw) {
            drawSettingsScreen();
            _firstDraw = false;
        }
    }
}

// ─── handleTouch ──────────────────────────────────────────────────────────────

void DisplayUI::handleTouch() {
    int sx, sy;

#ifdef BOARD_GUITION
    // Wake from sleep: any touch turns backlight back on without processing the tap
    if (!_screenOn) {
        if (_readTouch(sx, sy)) {
            _screenOn    = true;
            _lastTouchMs = millis();
            digitalWrite(PIN_BL, HIGH);
            _prevTouched = true;
        } else {
            _prevTouched = false;
        }
        return;
    }

    if (!_readTouch(sx, sy)) {
        _prevTouched = false;
        return;
    }
    if (_prevTouched) return;
    _prevTouched = true;
    _lastTouchMs = millis();
    Serial.printf("[TOUCH-G] screen=(%d,%d)\n", sx, sy);

#else
    // CYD: XPT2046 resistive touch — wake from sleep
    if (!_screenOn) {
        if (digitalRead(PIN_TOUCH_IRQ) == LOW && _tft.getTouchRawZ() >= 400) {
            _screenOn    = true;
            _lastTouchMs = millis();
            digitalWrite(PIN_BL, HIGH);
            _prevTouched = true;
        } else {
            _prevTouched = false;
        }
        return;
    }

    // PENIRQ (GPIO36) goes LOW when screen is touched — cheap pre-filter
    if (digitalRead(PIN_TOUCH_IRQ) == HIGH) {
        _prevTouched = false;
        return;
    }
    uint16_t z = _tft.getTouchRawZ();
    if (z < 400) {
        _prevTouched = false;
        return;
    }
    if (_prevTouched) return;
    _prevTouched = true;
    _lastTouchMs = millis();

    uint16_t rx, ry;
    _tft.getTouchRaw(&rx, &ry);
    // Map raw ADC → screen pixels (landscape 480×320, rotation=1)
    sx = constrain(479 - (int)map((long)ry, 320, 3860, 0, 479), 0, 479);
    sy = constrain((int)map((long)rx, 480, 3860, 0, 319), 0, 319);
    Serial.printf("[TOUCH] raw=(%u,%u) z=%u  screen=(%d,%d)\n", rx, ry, z, sx, sy);
#endif

    // Navigation — identical for both boards
    if (_screen == SCR_OVERVIEW) {
        if (hitTest(sx, sy, SOLAR_X, BOX_Y, BOX_W, BOX_H)) {
            _screen = SCR_DETAIL_SOLAR;  drawDetailSolar();
        } else if (hitTest(sx, sy, BATT_X, BOX_Y, BOX_W, BOX_H)) {
            _screen = SCR_DETAIL_BATT;   drawDetailBatt();
        } else if (hitTest(sx, sy, ORION_X, BOX_Y, BOX_W, BOX_H)) {
            _screen = SCR_DETAIL_ORION;  drawDetailOrion();
        } else if (hitTest(sx, sy, ORION_X, LOADS_Y, BOX_W, LOADS_H)) {
            _screen = SCR_LEVEL;
            _lvFirstDraw = true;
        } else if (hitTest(sx, sy, SOLAR_X, LOADS_Y, BOX_W, LOADS_H)) {
            _screen = SCR_SETTINGS;
            _firstDraw = true;
        }
    } else if (_screen == SCR_SETTINGS) {
        if (hitTest(sx, sy, 4, 4, 58, 30)) {
            _screen = SCR_OVERVIEW;
            _firstDraw = true;
        } else if (hitTest(sx, sy, 0, 48, 480, 48)) {
            _alwaysOn = !_alwaysOn;
            drawSettingsToggle();
        }
    } else {
        // SCR_DETAIL_* and SCR_LEVEL: tap "< BACK" button (x=4,y=4,w=58,h=30)
        if (hitTest(sx, sy, 4, 4, 58, 30)) {
            _screen = SCR_OVERVIEW;
            _firstDraw = true;
        }
    }
}

bool DisplayUI::hitTest(int tx, int ty, int bx, int by, int bw, int bh) {
    return tx >= bx && tx < bx + bw && ty >= by && ty < by + bh;
}

// ─── drawBox ──────────────────────────────────────────────────────────────────

void DisplayUI::drawBox(int x, int y, int w, int h, const char* title, bool online, bool showDot) {
    _D.fillRoundRect(x, y, w, h, 6, UI_SURFACE);
    _D.drawRoundRect(x, y, w, h, 6, UI_BORDER);
    _setFont(1);
    _D.setTextColor(UI_MUTED, UI_SURFACE);
    _D.setCursor(x + 5, y + 5);
    _D.print(title);
    if (showDot)
        _D.fillCircle(x + w - 10, y + 10, 5, online ? UI_GREEN : UI_RED);
}

// ─── Flow lines ───────────────────────────────────────────────────────────────

void DisplayUI::drawHFlowLine(int x1, int x2, int y, bool active, bool reverse) {
    int len = x2 - x1;
    if (len <= 0) return;
    if (!active) {
        for (int row = -1; row <= 1; row++)
            _D.drawFastHLine(x1, y + row, len, UI_BORDER);
        return;
    }
    for (int row = -1; row <= 1; row++) {
        int i = 0;
        while (i < len) {
            int p = (i + _flowPhase) % 14;
            int runMax;
            uint16_t col;
            if (p < 6) { runMax = 6 - p; col = UI_BLUE; }
            else       { runMax = 14 - p; col = UI_BG; }
            int runLen = min(runMax, len - i);
            int drawX = reverse ? (x2 - runLen - i) : (x1 + i);
            _D.drawFastHLine(drawX, y + row, runLen, col);
            i += runLen;
        }
    }
}

void DisplayUI::drawVFlowLine(int x, int y1, int y2, bool active) {
    int len = y2 - y1;
    if (len <= 0) return;
    if (!active) {
        for (int c = -1; c <= 1; c++)
            _D.drawFastVLine(x + c, y1, len, UI_BORDER);
        return;
    }
    for (int col = -1; col <= 1; col++) {
        int i = 0;
        while (i < len) {
            int p = (i + _flowPhase) % 14;
            int runMax;
            uint16_t c;
            if (p < 6) { runMax = 6 - p; c = UI_BLUE; }
            else       { runMax = 14 - p; c = UI_BG; }
            int runLen = min(runMax, len - i);
            _D.drawFastVLine(x + col, y1 + i, runLen, c);
            i += runLen;
        }
    }
}

void DisplayUI::drawFlowLines(bool solar, bool orion, bool loads) {
    // Solar → Battery (left to right)
    drawHFlowLine(SOLAR_X + BOX_W + 1, BATT_X - 1, CONN_MID_Y, solar);
    // Orion → Battery (right to left: reverse=true so dashes flow toward Battery)
    drawHFlowLine(BATT_X + BOX_W + 1, ORION_X - 1, CONN_MID_Y, orion, true);
    // Battery → Loads (top to bottom)
    drawVFlowLine(BATT_CX, BOX_Y + BOX_H + 1, LOADS_Y - 1, loads);
}

// ─── fillText ─────────────────────────────────────────────────────────────────

void DisplayUI::fillText(int x, int y, int w, int h, const char* txt, uint8_t font, uint16_t col, uint16_t bg) {
#ifdef BOARD_GUITION
    // Anti-flicker: clear area first, then draw — at QSPI speeds the clear is imperceptible
    _gfx->fillRect(x, y, w, h, bg);
    _setFont(font);
    _gfx->setTextColor(col);
    _gfx->setCursor(x + 2, y + 2);
    _gfx->print(txt);
#else
    _tft.setTextFont(font);
    _tft.setTextColor(col, bg);
    _tft.setTextPadding(w - 2);
    _tft.drawString(txt, x + 2, y + 2);
#endif
}

// ─── drawSettingsBox (overview bottom-left entry button) ──────────────────────

void DisplayUI::drawSettingsBox() {
    _D.fillRoundRect(SOLAR_X, LOADS_Y, BOX_W, LOADS_H, 6, UI_SURFACE);
    _D.drawRoundRect(SOLAR_X, LOADS_Y, BOX_W, LOADS_H, 6, UI_BORDER);
    _setFont(1);
    _D.setTextColor(UI_MUTED, UI_SURFACE);
    _D.setCursor(SOLAR_X + 5, LOADS_Y + 5);
    _D.print("SETTINGS");
    // Gear icon centered in box below title
    int cx = SOLAR_X + BOX_W / 2;
    int cy = LOADS_Y + 57;
    _D.fillCircle(cx, cy, 18, UI_MUTED);
    for (int k = 0; k < 8; k++) {
        float a = k * 3.14159f / 4.0f;
        _D.fillCircle(cx + (int)(18.f * cosf(a) + 0.5f),
                      cy + (int)(18.f * sinf(a) + 0.5f), 5, UI_MUTED);
    }
    _D.fillCircle(cx, cy, 10, UI_SURFACE);
}

// ─── drawSettingsToggle (toggle row in the Settings screen) ───────────────────

void DisplayUI::drawSettingsToggle() {
    _D.fillRect(0, 48, 480, 48, UI_BG);
    _setFont(2);
    _D.setTextColor(UI_TEXT, UI_BG);
    _D.setCursor(16, 55);
    _D.print("SCHERMO");
    const char* lbl = _alwaysOn ? "SEMPRE ON" : "AUTO OFF";
    uint16_t lblCol = _alwaysOn ? UI_GREEN : UI_MUTED;
    fillText(16, 76, 90, 12, lbl, 1, lblCol, UI_BG);
    // Pill 52×22 right-aligned
    const int pillX = 362, pillY = 57;
    uint16_t pillBg = _alwaysOn ? UI_GREEN : UI_BORDER;
    _D.fillRoundRect(pillX, pillY, 52, 22, 11, pillBg);
    int circX = _alwaysOn ? pillX + 41 : pillX + 11;
    _D.fillCircle(circX, pillY + 11, 9, UI_TEXT);
}

// ─── drawSettingsInfo (info rows in the Settings screen) ──────────────────────

void DisplayUI::drawSettingsInfo() {
    // 5 rows at y=106,142,178,214,250 (step 36px).
    struct InfoRow { const char* label; const char* value; uint16_t col; };
    bool ntpOk  = strcmp(_syNtpTime, "--") != 0;
    InfoRow rows[5] = {
        { "AP SSID",     _syApSsid[0]  ? _syApSsid  : "--",              (uint16_t)UI_TEXT  },
        { "AP IP",       _syApIp[0]    ? _syApIp    : "--",              (uint16_t)UI_TEXT  },
        { "CLIENT SSID", _syStaSsid[0] ? _syStaSsid : "non configurato", (uint16_t)(_syStaSsid[0] ? UI_TEXT : UI_MUTED) },
        { "CLIENT IP",   _syStaIp[0]   ? _syStaIp   : "non connesso",    (uint16_t)(_syStaIp[0]   ? UI_TEXT : UI_MUTED) },
        { "ORARIO NTP",  ntpOk         ? _syNtpTime : "in attesa...",    (uint16_t)(ntpOk          ? UI_TEXT : UI_MUTED) },
    };
    const int startY = 106, step = 36, valueX = 160, valueW = 308;
    for (int i = 0; i < 5; i++) {
        int y = startY + i * step;
        _D.drawFastHLine(0, y - 4, 480, UI_BORDER);
        _setFont(1);
        _D.setTextColor(UI_MUTED, UI_BG);
        _D.setCursor(16, y + 3);
        _D.print(rows[i].label);
        // fillText handles anti-flicker for the value (variable-length strings)
        fillText(valueX, y, valueW, 18, rows[i].value, 2, rows[i].col, UI_BG);
    }
}

// ─── drawSettingsScreen ───────────────────────────────────────────────────────

void DisplayUI::drawSettingsScreen() {
    drawDetailHeader("SETTINGS", true);
    drawSettingsToggle();
    drawSettingsInfo();
}

// ─── updateSysInfo ────────────────────────────────────────────────────────────

void DisplayUI::updateSysInfo(const char* apSsid, const char* apIp,
                               const char* staSsid, const char* staIp,
                               const char* ntpTime) {
    strlcpy(_syApSsid,  apSsid  ? apSsid  : "", sizeof(_syApSsid));
    strlcpy(_syApIp,    apIp    ? apIp    : "", sizeof(_syApIp));
    strlcpy(_syStaSsid, staSsid ? staSsid : "", sizeof(_syStaSsid));
    strlcpy(_syStaIp,   staIp   ? staIp   : "", sizeof(_syStaIp));
    strlcpy(_syNtpTime, ntpTime ? ntpTime : "--", sizeof(_syNtpTime));
    if (_screen == SCR_SETTINGS && !_firstDraw) {
        drawSettingsInfo();
    }
}

// ─── drawOverview ─────────────────────────────────────────────────────────────

void DisplayUI::drawOverview() {
    _D.fillScreen(UI_BG);

    drawBox(SOLAR_X, BOX_Y,  BOX_W, BOX_H,  "SOLAR",    _sd.valid);
    drawBox(BATT_X,  BOX_Y,  BOX_W, BOX_H,  "BATTERY",  _bd.valid);
    drawBox(ORION_X, BOX_Y,  BOX_W, BOX_H,  "DC-DC",    _od.valid);
    drawBox(LOADS_X, LOADS_Y,BOX_W, LOADS_H, "LOADS",   false, false);

    // Level screen button (right of LOADS)
    _D.fillRoundRect(ORION_X, LOADS_Y, BOX_W, LOADS_H, 6, UI_SURFACE);
    _D.drawRoundRect(ORION_X, LOADS_Y, BOX_W, LOADS_H, 6, UI_BORDER);
    _setFont(1);
    _D.setTextColor(UI_MUTED, UI_SURFACE);
    _D.setCursor(ORION_X + 5, LOADS_Y + 5);
    _D.print("LEVEL");
    _D.drawFastVLine(ORION_X + 69, LOADS_Y + 14, LOADS_H - 18, UI_BORDER);

    _lvUpC = _lvDnC = _lvLtC = _lvRtC = 1;  // force arrow redraw on first updateOverviewValues()
    drawSettingsBox();
    updateOverviewValues();
    drawFlowLines(false, false, false);  // draw static lines, animation starts in update()
}

// ─── updateOverviewValues ─────────────────────────────────────────────────────

void DisplayUI::updateOverviewValues() {
    char buf[16];
    uint16_t col;

    // ── Solar ─────────────────────────────────────────────────────────────────
    {
        float pw = _sd.valid ? _sd.solarPower : 0.0f;
        col = (pw > 0.5f) ? UI_GREEN : UI_MUTED;
        fmtF(buf, pw, 0); strcat(buf, "W");
        fillText(SOLAR_X+4, BOX_Y+16, BOX_W-8, 28, buf, 4, col, UI_SURFACE);

        const char* stStr = (_sd.valid && _sd.chargeState != 0xFF) ? victronStateName(_sd.chargeState) : "--";
        fillText(SOLAR_X+4, BOX_Y+50, BOX_W-8, 18, stStr, 2, UI_MUTED, UI_SURFACE);

        fmtF(buf, _sd.valid ? _sd.chargeCurrent : NAN, 1); strcat(buf, "A");
        fillText(SOLAR_X+4, BOX_Y+72,  BOX_W-8, 28, buf, 4, UI_TEXT, UI_SURFACE);

        fmtF(buf, _sd.valid ? _sd.battVoltage : NAN, 1); strcat(buf, "V");
        fillText(SOLAR_X+4, BOX_Y+104, BOX_W-8, 28, buf, 4, UI_TEXT, UI_SURFACE);

        fmtF(buf, _sd.valid ? _sd.yieldToday : NAN, 0); strcat(buf, "Wh");
        fillText(SOLAR_X+4, BOX_Y+136, BOX_W-8, 28, buf, 4, UI_MUTED, UI_SURFACE);
    }

    // ── Battery ───────────────────────────────────────────────────────────────
    {
        float soc = (_bd.valid && !isnan((float)_bd.soc)) ? (float)_bd.soc : 0.0f;
        fmtF(buf, soc, 0); strcat(buf, "%");
        col = socCol(soc);
        fillText(BATT_X+4, BOX_Y+16, BOX_W-8, 28, buf, 4, col, UI_SURFACE);

        float curr = _bd.valid ? _bd.current : 0.0f;
        const char* stStr = !_bd.valid ? "--" : curr > 0.1f ? "CHARGING" : curr < -0.1f ? "DISCHARGING" : "IDLE";
        col = !_bd.valid ? UI_MUTED : curr > 0.1f ? UI_GREEN : curr < -0.1f ? UI_ORANGE : UI_MUTED;
        fillText(BATT_X+4, BOX_Y+50, BOX_W-8, 18, stStr, 2, col, UI_SURFACE);

        fmtF(buf, _bd.valid ? _bd.current : NAN, 1); strcat(buf, "A");
        fillText(BATT_X+4, BOX_Y+72, BOX_W-8, 28, buf, 4, signCol(_bd.valid ? _bd.current : 0), UI_SURFACE);

        fmtF(buf, _bd.valid ? _bd.voltage : NAN, 1); strcat(buf, "V");
        fillText(BATT_X+4, BOX_Y+104, BOX_W-8, 28, buf, 4, UI_TEXT, UI_SURFACE);

        fmtF(buf, _bd.valid ? _bd.power : NAN, 0); strcat(buf, "W");
        fillText(BATT_X+4, BOX_Y+136, BOX_W-8, 28, buf, 4, signCol(_bd.valid ? _bd.power : 0), UI_SURFACE);
    }

    // ── Orion — OUT only ──────────────────────────────────────────────────────
    {
        float outW = (_od.valid && !isnan(_od.outCurrent) && !isnan(_od.outVoltage))
                     ? _od.outCurrent * _od.outVoltage : 0.0f;
        col = (outW > 0.5f) ? UI_GREEN : UI_MUTED;
        fmtF(buf, outW, 0); strcat(buf, "W");
        fillText(ORION_X+4, BOX_Y+16, BOX_W-8, 28, buf, 4, col, UI_SURFACE);

        const char* stStr = (_od.valid && _od.deviceState != 0xFF) ? victronStateName(_od.deviceState) : "--";
        fillText(ORION_X+4, BOX_Y+50, BOX_W-8, 18, stStr, 2, UI_MUTED, UI_SURFACE);

        char tmp[10];
        fmtF(tmp, _od.valid ? _od.outCurrent : NAN, 1); strncpy(buf, tmp, 10); strcat(buf, "A");
        fillText(ORION_X+4, BOX_Y+72, BOX_W-8, 28, buf, 4, UI_TEXT, UI_SURFACE);

        fmtF(tmp, _od.valid ? _od.outVoltage : NAN, 1); strncpy(buf, tmp, 10); strcat(buf, "V");
        fillText(ORION_X+4, BOX_Y+104, BOX_W-8, 28, buf, 4, UI_TEXT, UI_SURFACE);
    }

    // ── Loads ─────────────────────────────────────────────────────────────────
    {
        float solarW = (_sd.valid && !isnan(_sd.solarPower))  ? _sd.solarPower  : 0.0f;
        float orionW = (_od.valid && !isnan(_od.outCurrent) && !isnan(_od.outVoltage))
                       ? _od.outCurrent * _od.outVoltage : 0.0f;
        float battW  = (_bd.valid && !isnan(_bd.power)) ? _bd.power : 0.0f;
        float loadsW = fmaxf(0.0f, solarW + orionW - battW);
        float battV  = (_bd.valid && !isnan(_bd.voltage) && _bd.voltage > 1.0f) ? _bd.voltage : 12.0f;
        float loadsA = loadsW / battV;

        fmtF(buf, loadsA, 1); strcat(buf, "A");
        fillText(LOADS_X+4, LOADS_Y+27, BOX_W-8, 28, buf, 4, UI_MUTED, UI_SURFACE);

        fmtF(buf, loadsW, 0); strcat(buf, "W");
        fillText(LOADS_X+4, LOADS_Y+61, BOX_W-8, 28, buf, 4, UI_TEXT, UI_SURFACE);
    }

    // ── Online dots — update every tick based on staleness ────────────────────
    uint32_t _now = millis();
    _D.fillCircle(SOLAR_X + BOX_W - 10, BOX_Y + 10, 5,
        (_sd.valid && _now - _sd.lastSeen < DEVICE_STALE_MS) ? UI_GREEN : UI_RED);
    _D.fillCircle(BATT_X  + BOX_W - 10, BOX_Y + 10, 5,
        (_bd.valid && _now - _bd.lastSeen < DEVICE_STALE_MS) ? UI_GREEN : UI_RED);
    _D.fillCircle(ORION_X + BOX_W - 10, BOX_Y + 10, 5,
        (_od.valid && _now - _od.lastSeen < DEVICE_STALE_MS) ? UI_GREEN : UI_RED);
    _D.fillCircle(ORION_X + BOX_W - 10, LOADS_Y + 10, 5,
        (_imu.valid && _now - _imu.lastSeen < DEVICE_STALE_MS) ? UI_GREEN : UI_RED);

    // ── LEVEL box: arrows (top) → values → labels (bottom) ───────────────────
    {
        bool imuOk = _imu.valid && (_now - _imu.lastSeen < DEVICE_STALE_MS);
        const int cx1 = ORION_X + 34;    // pitch column center (x=368)
        const int cx2 = ORION_X + 103;   // roll column center  (x=437)
        const int ay  = LOADS_Y + 41;    // arrow vertical center

        // ── Value + label colors ──────────────────────────────────────────────
        uint16_t pCol = imuOk ? (fabsf(_imu.pitch) < 2.f ? UI_GREEN : fabsf(_imu.pitch) < 5.f ? UI_YELLOW : UI_RED) : UI_MUTED;
        uint16_t rCol = imuOk ? (fabsf(_imu.roll)  < 2.f ? UI_GREEN : fabsf(_imu.roll)  < 5.f ? UI_YELLOW : UI_RED) : UI_MUTED;

        // ── Arrow colors: both arrows on each axis show the threshold color ──────
        uint16_t upCol, dnCol, ltCol, rtCol;
        if (!imuOk) {
            upCol = dnCol = ltCol = rtCol = UI_BORDER;
        } else {
            upCol = dnCol = pCol;
            ltCol = rtCol = rCol;
        }

        // ── Arrows: only redraw when colors change (eliminates flicker) ───────
        if (upCol != _lvUpC || dnCol != _lvDnC || ltCol != _lvLtC || rtCol != _lvRtC) {
            _lvUpC = upCol; _lvDnC = dnCol; _lvLtC = ltCol; _lvRtC = rtCol;

            // Erase arrow area only (below "LEVEL" title, above values)
            _D.fillRect(ORION_X+2, LOADS_Y+20, 130, 44, UI_SURFACE);
            // Restore divider for erased portion
            _D.drawFastVLine(ORION_X+69, LOADS_Y+20, 44, UI_BORDER);

            // UP arrow: tip at top (LOADS_Y+20), head 8px, shaft 10px
            _D.fillTriangle(cx1, LOADS_Y+20, cx1-5, LOADS_Y+28, cx1+5, LOADS_Y+28, upCol);
            _D.fillRect(cx1-2, LOADS_Y+28, 5, 10, upCol);
            // DOWN arrow: tip at bottom (LOADS_Y+63), head 8px, shaft 10px
            _D.fillTriangle(cx1, LOADS_Y+63, cx1-5, LOADS_Y+55, cx1+5, LOADS_Y+55, dnCol);
            _D.fillRect(cx1-2, LOADS_Y+45, 5, 10, dnCol);

            // LEFT arrow: tip at left
            _D.fillTriangle(cx2-17, ay, cx2-9, ay-5, cx2-9, ay+5, ltCol);
            _D.fillRect(cx2-9, ay-2, 8, 5, ltCol);
            // RIGHT arrow: tip at right
            _D.fillTriangle(cx2+17, ay, cx2+9, ay-5, cx2+9, ay+5, rtCol);
            _D.fillRect(cx2+1,  ay-2, 8, 5, rtCol);
        }

        // ── Values (font2) — below arrows, flicker-free via fillText ──────────
        char lvBuf[10];
        if (imuOk) snprintf(lvBuf, sizeof(lvBuf), "%+.1f\xB0", _imu.pitch);
        else strcpy(lvBuf, "--");
        fillText(ORION_X+2,  LOADS_Y+66, 63, 18, lvBuf, 2, pCol, UI_SURFACE);

        if (imuOk) snprintf(lvBuf, sizeof(lvBuf), "%+.1f\xB0", _imu.roll);
        else strcpy(lvBuf, "--");
        fillText(ORION_X+71, LOADS_Y+66, 63, 18, lvBuf, 2, rCol, UI_SURFACE);

        // ── Labels (font1) — very bottom ──────────────────────────────────────
        fillText(ORION_X+2,  LOADS_Y+84, 63, 10, "PITCH", 1, UI_MUTED, UI_SURFACE);
        fillText(ORION_X+71, LOADS_Y+84, 63, 10, "ROLL",  1, UI_MUTED, UI_SURFACE);
    }
}

// ─── Detail header ────────────────────────────────────────────────────────────

void DisplayUI::drawDetailHeader(const char* title, bool online) {
    _D.fillScreen(UI_BG);
    _D.fillRoundRect(4, 4, 58, 30, 6, UI_SURFACE);
    _D.drawRoundRect(4, 4, 58, 30, 6, UI_BORDER);
    _setFont(1);
    _D.setTextColor(UI_TEXT, UI_SURFACE);
    _D.setCursor(10, 13);
    _D.print("< BACK");

    _setFont(2);
    _D.setTextColor(UI_TEXT, UI_BG);
    _D.setCursor(70, 12);
    _D.print(title);

    _D.fillCircle(472, 20, 5, online ? UI_GREEN : UI_RED);
    _D.drawFastHLine(0, 40, 480, UI_BORDER);
}

// ─── Metric card ─────────────────────────────────────────────────────────────

void DisplayUI::drawMetric(int x, int y, int w, int h,
                            const char* label, const char* val, const char* unit, uint16_t valCol) {
    _D.fillRoundRect(x, y, w, h, 6, UI_SURFACE);
    // Value (font4)
    _setFont(4);
    _D.setTextColor(valCol, UI_SURFACE);
    _D.setCursor(x + 6, y + 8);
    _D.print(val);
    // Unit (font1)
    _setFont(1);
    _D.setTextColor(UI_MUTED, UI_SURFACE);
    _D.print(unit);
    // Label at bottom
    _D.setCursor(x + 6, y + h - 14);
    _D.print(label);
}

void DisplayUI::drawStateTag(int x, int y, const char* state) {
    uint16_t col = stateColor(state);
    _D.fillRoundRect(x, y, 160, 28, 4, UI_SURFACE);
    _D.drawRoundRect(x, y, 160, 28, 4, col);
    _setFont(2);
    _D.setTextColor(col, UI_SURFACE);
    _D.setCursor(x + 8, y + 7);
    _D.print(state ? state : "--");
}

// ─── Detail: Battery ──────────────────────────────────────────────────────────

void DisplayUI::drawDetailBatt() {
    bool on = _bd.valid;
    drawDetailHeader("LITIME 140AH", on);
    if (!on) { _setFont(2); _D.setTextColor(UI_MUTED,UI_BG); _D.setCursor(80,110); _D.print("OFFLINE"); return; }

    char v[10];
    // Row 1: y=48
    fmtF(v, _bd.voltage, 2);     drawMetric(6,   48, 148, 70, "VOLTAGE",  v, "V",  UI_TEXT);
    fmtF(v, _bd.current, 2);     drawMetric(162, 48, 148, 70, "CURRENT",  v, "A",  signCol(_bd.current));
    fmtF(v, _bd.power,   0);     drawMetric(318, 48, 148, 70, "POWER",    v, "W",  signCol(_bd.power));
    // Row 2: y=124
    fmtF(v, (float)_bd.soc, 0);  drawMetric(6,   124, 148, 70, "SOC",     v, "%",  socCol(_bd.soc));
    fmtF(v, (float)_bd.soh, 0);  drawMetric(162, 124, 148, 70, "SOH",     v, "%",  UI_TEXT);
    fmtF(v, _bd.remainingAh, 1); drawMetric(318, 124, 148, 70, "REM. Ah", v, "Ah", UI_TEXT);
    // Row 3: y=200
    fmtF(v, (float)_bd.cellTemp,  0); drawMetric(6,   200, 148, 70, "CELL C",  v, "\xB0", UI_TEXT);
    fmtF(v, (float)_bd.mosfetTemp,0); drawMetric(162, 200, 148, 70, "FET C",   v, "\xB0", UI_TEXT);
    fmtF(v, _bd.fullCapacityAh,1);    drawMetric(318, 200, 148, 70, "FULL Ah", v, "Ah",   UI_MUTED);

    // Cell voltages (compact)
    if (_bd.cellCount > 0) {
        _setFont(1);
        _D.setTextColor(UI_MUTED, UI_BG);
        int cx = 6;
        for (int i = 0; i < _bd.cellCount && i < 8; i++) {
            char cell[12];
            snprintf(cell, sizeof(cell), "C%d:%.3f", i+1, _bd.cellVoltages[i]);
            _D.setCursor(cx, 278);
            _D.print(cell);
            cx += 74;
            if (cx > 400) break;
        }
    }
}

// ─── Detail: Solar ────────────────────────────────────────────────────────────

void DisplayUI::drawDetailSolar() {
    bool on = _sd.valid;
    drawDetailHeader("SMARTSOLAR 75/15", on);
    if (!on) { _setFont(2); _D.setTextColor(UI_MUTED,UI_BG); _D.setCursor(80,110); _D.print("OFFLINE"); return; }

    char v[10];
    fmtF(v, _sd.solarPower, 0);     drawMetric(6,   48, 228, 72, "POWER",    v, "W",  UI_GREEN);
    fmtF(v, _sd.chargeCurrent, 2);  drawMetric(246, 48, 228, 72, "CHARGE A", v, "A",  UI_GREEN);
    fmtF(v, _sd.battVoltage, 2);    drawMetric(6,   128, 228, 72, "BATT V",  v, "V",  UI_TEXT);
    fmtF(v, _sd.yieldToday, 0);     drawMetric(246, 128, 228, 72, "YIELD",   v, "Wh", UI_TEXT);

    if (!isnan(_sd.loadCurrent)) {
        fmtF(v, _sd.loadCurrent, 2);
        drawMetric(6, 208, 228, 72, "LOAD A", v, "A", UI_MUTED);
    }

    const char* stStr = (_sd.chargeState != 0xFF) ? victronStateName(_sd.chargeState) : "--";
    drawStateTag(246, 216, stStr);
}

// ─── Detail: Orion ────────────────────────────────────────────────────────────

void DisplayUI::drawDetailOrion() {
    bool on = _od.valid;
    drawDetailHeader("DC-DC 50A", on);
    if (!on) { _setFont(2); _D.setTextColor(UI_MUTED,UI_BG); _D.setCursor(80,110); _D.print("OFFLINE"); return; }

    // Column headers
    _setFont(2);
    _D.setTextColor(UI_MUTED, UI_BG); _D.setCursor(50,  48); _D.print("INPUT");
    _D.setTextColor(UI_BLUE,  UI_BG); _D.setCursor(310, 48); _D.print("OUTPUT");
    _D.drawFastVLine(240, 48, 240, UI_BORDER);

    char v[10];
    float inW  = (!isnan(_od.inVoltage)  && !isnan(_od.inCurrent))  ? _od.inVoltage  * _od.inCurrent  : 0.0f;
    float outW = (!isnan(_od.outVoltage) && !isnan(_od.outCurrent)) ? _od.outVoltage * _od.outCurrent : 0.0f;

    // Voltage row y=70
    fmtF(v, _od.inVoltage, 2);  drawMetric(6,   70, 228, 66, "VOLTAGE", v, "V", UI_TEXT);
    fmtF(v, _od.outVoltage,2);  drawMetric(246, 70, 228, 66, "VOLTAGE", v, "V", UI_GREEN);
    // Current row y=144
    fmtF(v, _od.inCurrent, 2);  drawMetric(6,   144, 228, 66, "CURRENT", v, "A", UI_TEXT);
    fmtF(v, _od.outCurrent,2);  drawMetric(246, 144, 228, 66, "CURRENT", v, "A", UI_GREEN);
    // Power row y=218
    fmtF(v, inW,  0);            drawMetric(6,   218, 228, 60, "POWER",  v, "W", UI_TEXT);
    fmtF(v, outW, 0);            drawMetric(246, 218, 228, 60, "POWER",  v, "W", UI_GREEN);

    // Efficiency
    if (inW > 1.0f) {
        float eff = fminf(100.0f, outW / inW * 100.0f);
        char effBuf[8]; dtostrf(eff, 0, 1, effBuf); strcat(effBuf, "%");
        _setFont(1); _D.setTextColor(eff > 85 ? UI_GREEN : UI_YELLOW, UI_BG);
        _D.setCursor(6, 290); _D.print("EFF: "); _D.print(effBuf);
    }
}

// ─── Level screen ─────────────────────────────────────────────────────────────

// Draws only the static colored bar (no bubble, no text).
void DisplayUI::drawLevelPanel(int px, bool isPitch) {
    if (isPitch) {
        // Vertical pitch bar: BAR_X=102, y=70-240 (170px), ±10° range
        // RED42|YEL26|GRN34|YEL26|RED42, center at y=155
        const int BAR_X = px + 102;
        _D.fillRect(BAR_X, 70,  36, 42, UI_RED);
        _D.fillRect(BAR_X, 112, 36, 26, UI_YELLOW);
        _D.fillRect(BAR_X, 138, 36, 34, UI_GREEN);
        _D.fillRect(BAR_X, 172, 36, 26, UI_YELLOW);
        _D.fillRect(BAR_X, 198, 36, 42, UI_RED);
        _D.drawFastHLine(BAR_X - 3, 155, 42, UI_BG);   // center tick
    } else {
        // Horizontal roll bar: BAR_X=px+20, y=110, h=36, ±10° range
        // RED50|YEL30|GRN40|YEL30|RED50, center at BAR_X+100
        const int BAR_X = px + 20;
        _D.fillRect(BAR_X,        110, 50, 36, UI_RED);
        _D.fillRect(BAR_X + 50,   110, 30, 36, UI_YELLOW);
        _D.fillRect(BAR_X + 80,   110, 40, 36, UI_GREEN);
        _D.fillRect(BAR_X + 120,  110, 30, 36, UI_YELLOW);
        _D.fillRect(BAR_X + 150,  110, 50, 36, UI_RED);
        _D.drawFastVLine(BAR_X + 100, 107, 42, UI_BG); // center tick
    }
}

void DisplayUI::drawLevelScreen() {
    _D.fillScreen(UI_BG);

    _D.fillRoundRect(4, 4, 58, 30, 6, UI_SURFACE);
    _D.drawRoundRect(4, 4, 58, 30, 6, UI_BORDER);
    _setFont(1);
    _D.setTextColor(UI_TEXT, UI_SURFACE);
    _D.setCursor(10, 13);
    _D.print("< BACK");

    _setFont(2);
    _D.setTextColor(UI_TEXT, UI_BG);
    _D.setCursor(70, 12);
    _D.print("LIVELLO");

    _D.drawFastHLine(0, 40, 480, UI_BORDER);
    _D.drawFastVLine(240, 40, 280, UI_BORDER);

    _setFont(2);
    _D.setTextColor(UI_MUTED, UI_BG);
    _D.setCursor(20,  44); _D.print("PITCH");
    _D.setCursor(260, 44); _D.print("ROLL");

    // Draw static bars, reset bubble trackers, draw initial bubble+text
    drawLevelPanel(0,   true);
    drawLevelPanel(240, false);
    _lvPitchBy = -999;
    _lvRollBx  = -999;
    updateLevelValues();
}

void DisplayUI::updateLevelValues() {
    float pitch = _imu.valid ? _imu.pitch : 0.0f;
    float roll  = _imu.valid ? _imu.roll  : 0.0f;
    bool imuOk  = _imu.valid;

    // Zone tables for bar restoration (compile-time constants)
    static const int  pZY[] = {70, 112, 138, 172, 198};
    static const int  pZH[] = {42, 26,  34,  26,  42};
    static const uint16_t pZC[] = {UI_RED, UI_YELLOW, UI_GREEN, UI_YELLOW, UI_RED};

    static const int  rZX[] = {260, 310, 340, 380, 410};
    static const int  rZW[] = {50,  30,  40,  30,  50};
    static const uint16_t rZC[] = {UI_RED, UI_YELLOW, UI_GREEN, UI_YELLOW, UI_RED};

    // ── PITCH (vertical bar, cx=120, BAR_X=102, BAR_CY=155) ─────────────────
    {
        const int cx = 120, BAR_X = 102, BAR_CY = 155;
        float cl = imuOk ? fmaxf(-10.f, fminf(10.f, pitch)) : 0.f;
        int by = BAR_CY - (int)(cl * 85.f / 10.f);

        if (_lvPitchBy != by) {
            // Restore bar colors under old bubble
            if (_lvPitchBy >= 0) {
                int y0 = max(70, _lvPitchBy - 15);
                int y1 = min(240, _lvPitchBy + 15);
                for (int i = 0; i < 5; i++) {
                    int zy = max(y0, pZY[i]), zb = min(y1, pZY[i] + pZH[i]);
                    if (zy < zb) _D.fillRect(BAR_X, zy, 36, zb - zy, pZC[i]);
                }
                _D.drawFastHLine(BAR_X - 3, BAR_CY, 42, UI_BG); // restore center tick if erased
            }
            _D.fillCircle(cx, by, 14, UI_BG);
            _D.drawCircle(cx, by, 14, UI_TEXT);
            _D.drawCircle(cx, by, 13, UI_TEXT);
            _lvPitchBy = by;
        }

        // Angle text (font4): fillRect erase then centered print
        uint16_t col = imuOk ? (fabsf(pitch) < 2.f ? UI_GREEN : fabsf(pitch) < 5.f ? UI_YELLOW : UI_RED) : UI_MUTED;
        char buf[14];
        if (imuOk) snprintf(buf, sizeof(buf), "%+.1f\xB0", pitch);
        else strncpy(buf, "--", sizeof(buf));
        _D.fillRect(70, 248, 100, 26, UI_BG);
        _setFont(4);
        _D.setTextColor(col, UI_BG);
        _D.setCursor(cx - _textWidth(buf) / 2, 248);
        _D.print(buf);

        // Status text (font1)
        const char* st; uint16_t sc;
        if (!imuOk) { st = "NO IMU DATA"; sc = UI_MUTED; }
        else {
            float ab = fabsf(pitch);
            sc = ab < 0.3f ? UI_GREEN : ab < 2.f ? UI_GREEN : ab < 5.f ? UI_YELLOW : UI_RED;
            st = ab < 0.3f ? "LIVELLATO" : (pitch > 0 ? "REAR LOW" : "FRONT LOW");
        }
        _D.fillRect(60, 276, 120, 10, UI_BG);
        _setFont(1);
        _D.setTextColor(sc, UI_BG);
        _D.setCursor(cx - _textWidth(st) / 2, 276);
        _D.print(st);
    }

    // ── ROLL (horizontal bar, px=240, BAR_X=260, BAR_CY=128, cx=360) ────────
    {
        const int cx = 360, BAR_X = 260, BAR_CY = 128;
        float cl = imuOk ? fmaxf(-10.f, fminf(10.f, roll)) : 0.f;
        int bx = BAR_X + 100 + (int)(cl * 100.f / 10.f);

        if (_lvRollBx != bx) {
            // Restore bar colors under old bubble
            if (_lvRollBx >= 0) {
                int x0 = max(BAR_X, _lvRollBx - 15);
                int x1 = min(BAR_X + 200, _lvRollBx + 15);
                for (int i = 0; i < 5; i++) {
                    int zx = max(x0, rZX[i]), ze = min(x1, rZX[i] + rZW[i]);
                    if (zx < ze) _D.fillRect(zx, 110, ze - zx, 36, rZC[i]);
                }
                _D.drawFastVLine(BAR_X + 100, 107, 42, UI_BG); // restore center tick if erased
            }
            _D.fillCircle(bx, BAR_CY, 14, UI_BG);
            _D.drawCircle(bx, BAR_CY, 14, UI_TEXT);
            _D.drawCircle(bx, BAR_CY, 13, UI_TEXT);
            _lvRollBx = bx;
        }

        // Angle text
        uint16_t col = imuOk ? (fabsf(roll) < 2.f ? UI_GREEN : fabsf(roll) < 5.f ? UI_YELLOW : UI_RED) : UI_MUTED;
        char buf[14];
        if (imuOk) snprintf(buf, sizeof(buf), "%+.1f\xB0", roll);
        else strncpy(buf, "--", sizeof(buf));
        _D.fillRect(310, 64, 100, 26, UI_BG);
        _setFont(4);
        _D.setTextColor(col, UI_BG);
        _D.setCursor(cx - _textWidth(buf) / 2, 64);
        _D.print(buf);

        // Status text
        const char* st; uint16_t sc;
        if (!imuOk) { st = "NO IMU DATA"; sc = UI_MUTED; }
        else {
            float ab = fabsf(roll);
            sc = ab < 0.3f ? UI_GREEN : ab < 2.f ? UI_GREEN : ab < 5.f ? UI_YELLOW : UI_RED;
            st = ab < 0.3f ? "LIVELLATO" : (roll > 0 ? "DRIVER SIDE LOW" : "PASS. SIDE LOW");
        }
        _D.fillRect(250, 96, 220, 10, UI_BG);
        _setFont(1);
        _D.setTextColor(sc, UI_BG);
        _D.setCursor(cx - _textWidth(st) / 2, 96);
        _D.print(st);
    }

    _lastPitch    = pitch;
    _lastRoll     = roll;
    _lastImuValid = _imu.valid;
}

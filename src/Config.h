#pragma once

// ─── WiFi ────────────────────────────────────────────────────────────────────
#define WIFI_SSID     "CamperEnergy"
#define WIFI_PASSWORD "camper1234"
#define WEB_PORT      80

// ─── WiFi Client / STA (opzionale) ───────────────────────────────────────────
#define STA_SSID        ""
#define STA_PASSWORD    ""

// ─── NTP ─────────────────────────────────────────────────────────────────────
#define NTP_SERVER      "pool.ntp.org"
#define NTP_TZ          "CET-1CEST,M3.5.0,M10.5.0/3"

// ─── Device defaults (overridden by NVS after first Settings save) ────────────
#define LITIME_BMS_MAC     "AA:BB:CC:DD:EE:FF"
#define VICTRON_SOLAR_MAC  ""
#define VICTRON_SOLAR_KEY  "00000000000000000000000000000000"
#define VICTRON_ORION_MAC  ""
#define VICTRON_ORION_KEY  "00000000000000000000000000000000"
#define WITMOTION_MAC      "AA:BB:CC:DD:EE:FF"

// ─── Timing ──────────────────────────────────────────────────────────────────
#define BMS_QUERY_INTERVAL_MS     2000
#define BMS_RECONNECT_DELAY_MS   30000
#define DEVICE_STALE_MS          10000

// ─── OTA update credentials (change these before deploying) ─────────────────
#define OTA_USERNAME  "ota"
#define OTA_PASSWORD  "camper-ota"

// ─── Display hardware ────────────────────────────────────────────────────────
#ifdef BOARD_GUITION
  // Guition JC3248W535C — ESP32-S3, AXS15231B QSPI display + I2C touch
  #define PIN_BL           1    // backlight (active HIGH)
  #define PIN_TOUCH_SDA    4    // AXS15231B I2C SDA
  #define PIN_TOUCH_SCL    8    // AXS15231B I2C SCL (multiplexed with display DC — OK)
  #define AXS15231B_ADDR   0x3B // I2C touch address
#else
  // ESP32-2432S035R CYD — ILI9488 SPI + XPT2046 resistive touch
  #define PIN_BL           27
  #define PIN_TOUCH_IRQ    36   // XPT2046 PENIRQ — LOW when touched
#endif

#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"

// Persists BMS MAC and Victron keys in NVS (survives reboots).
// Falls back to Config.h compile-time defaults when NVS is empty (first boot).
class SettingsManager {
public:
    void begin() {
        _prefs.begin("camper", false);
    }

    String getBmsMac()        { return _prefs.getString("bms_mac",   LITIME_BMS_MAC);    }
    String getSolarKey()      { return _prefs.getString("solar_key", VICTRON_SOLAR_KEY); }
    String getOrionKey()      { return _prefs.getString("orion_key", VICTRON_ORION_KEY); }
    String getWifiSsid()      { return _prefs.getString("wifi_ssid", WIFI_SSID);         }
    String getWifiPassword()  { return _prefs.getString("wifi_pass", WIFI_PASSWORD);     }
    String getStaSsid()       { return _prefs.getString("sta_ssid",  STA_SSID);          }
    String getStaPass()       { return _prefs.getString("sta_pass",  STA_PASSWORD);      }
    String getNtpServer()     { return _prefs.getString("ntp_srv",   NTP_SERVER);        }
    String getNtpTZ()         { return _prefs.getString("ntp_tz",    NTP_TZ);            }
    String getWitmotionMac()  { return _prefs.getString("imu_mac",   WITMOTION_MAC);     }
    int    getLang()          { return _prefs.getInt("lang", 0); }   // 0 = English, 1 = Italiano
    // OTA — disabled by default, no credentials on first boot (nothing hardcoded/in git)
    // Turn the AP off while the WiFi client (STA) is connected, so the phone stops
    // auto-joining CamperEnergy and losing internet. Default OFF: with it on there are
    // situations the device cannot detect (client isolation, captive portal) where the AP
    // is the only way in — see the escape hatches in main.cpp (boot grace + device toggle).
    bool   getApOffWhenSta()  { return _prefs.getBool("ap_off_sta", false); }
    void   setApOffWhenSta(bool v) { _prefs.putBool("ap_off_sta", v); }

    bool   getOtaEnabled()    { return _prefs.getBool("ota_en", false); }
    String getOtaUser()       { return _prefs.getString("ota_user", ""); }
    String getOtaPass()       { return _prefs.getString("ota_pass", ""); }
    // OTA is usable only when enabled AND both credentials are set
    bool   otaActive()        { return getOtaEnabled() && getOtaUser().length() > 0 && getOtaPass().length() > 0; }

    void setBmsMac(const String& v)        { _prefs.putString("bms_mac",   v); }
    void setSolarKey(const String& v)      { _prefs.putString("solar_key", v); }
    void setOrionKey(const String& v)      { _prefs.putString("orion_key", v); }
    void setWifiSsid(const String& v)      { _prefs.putString("wifi_ssid", v); }
    void setWifiPassword(const String& v)  { _prefs.putString("wifi_pass", v); }
    void setStaSsid(const String& v)       { _prefs.putString("sta_ssid",  v); }
    void setStaPass(const String& v)       { _prefs.putString("sta_pass",  v); }
    void setNtpServer(const String& v)     { _prefs.putString("ntp_srv",   v); }
    void setNtpTZ(const String& v)         { _prefs.putString("ntp_tz",    v); }
    void setWitmotionMac(const String& v)  { _prefs.putString("imu_mac",   v); }
    void setLang(int v)                    { _prefs.putInt("lang", v); }
    void setOtaEnabled(bool v)             { _prefs.putBool("ota_en", v); }
    void setOtaUser(const String& v)       { _prefs.putString("ota_user", v); }
    void setOtaPass(const String& v)       { _prefs.putString("ota_pass", v); }

private:
    Preferences _prefs;
};

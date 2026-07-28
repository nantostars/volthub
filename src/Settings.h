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

private:
    Preferences _prefs;
};

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include <time.h>
#include "esp_sntp.h"

#include "Config.h"
#include "Settings.h"
#include "VictronBLE.h"
#include "LitimeBMS.h"
#include "WitmotionIMU.h"
#include "DisplayUI.h"
#include "Dashboard.h"   // kept for web access from phone (settings etc.)

// ─── Globals ──────────────────────────────────────────────────────────────────

WebServer       server(WEB_PORT);
SettingsManager settings;
VictronBLE      victron;
LitimeBMS       bms;
WitmotionIMU    imu;
DisplayUI       display;

// ─── NTP ──────────────────────────────────────────────────────────────────────

static void onNtpSync(struct timeval*) {
    struct tm ti; time_t now = time(nullptr);
    localtime_r(&now, &ti);
    char buf[20]; strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &ti);
    Serial.printf("[NTP] Synced: %s\n", buf);
}

static void ntpSetup() {
    String srv = settings.getNtpServer();
    String tz  = settings.getNtpTZ();
    if (srv.isEmpty()) srv = NTP_SERVER;
    if (tz.isEmpty())  tz  = NTP_TZ;
    sntp_set_time_sync_notification_cb(onNtpSync);
    // Three servers: configured + Google (name + IP fallback to bypass DNS issues)
    configTime(0, 0, srv.c_str(), "time.google.com", "216.239.35.0");
    setenv("TZ", tz.c_str(), 1);
    tzset();
    Serial.printf("[NTP] server=%s  TZ=%s\n", srv.c_str(), tz.c_str());
}

// ─── WiFi ─────────────────────────────────────────────────────────────────────

static void wifiSetup() {
    String apSsid  = settings.getWifiSsid();
    String apPass  = settings.getWifiPassword();
    String staSsid = settings.getStaSsid();

    WiFi.mode(staSsid.length() > 0 ? WIFI_AP_STA : WIFI_AP);
    bool ok = WiFi.softAP(apSsid.c_str(), apPass.c_str());
    Serial.printf("[WiFi] AP %s – SSID \"%s\"  http://%s\n",
                  ok ? "OK" : "FAILED", apSsid.c_str(),
                  WiFi.softAPIP().toString().c_str());

    if (staSsid.length() > 0) {
        WiFi.begin(staSsid.c_str(), settings.getStaPass().c_str());
        Serial.printf("[WiFi] STA connecting to \"%s\"...\n", staSsid.c_str());
        uint32_t t = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t < 10000) delay(200);
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[WiFi] STA OK – http://%s\n", WiFi.localIP().toString().c_str());
            ntpSetup();
        } else {
            Serial.println("[WiFi] STA failed – AP only mode");
        }
    }
}

// ─── HTTP helpers ─────────────────────────────────────────────────────────────

static void addFloat(JsonObject obj, const char* key, float v) {
    if (isnan(v)) obj[key] = nullptr; else obj[key] = v;
}

static void handleRoot() {
    server.sendHeader("Cache-Control", "no-cache");
    server.send_P(200, "text/html", DASHBOARD_HTML);
}

static void handleApiData() {
    BmsData   b  = bms.getData();
    SolarData s  = victron.getSolar();
    OrionData o  = victron.getOrion();
    ImuData   im = imu.getData();
    uint32_t  now = millis();
    bool imOnline = im.valid && (now - im.lastSeen < DEVICE_STALE_MS);

    JsonDocument doc;
    JsonObject jb = doc["battery"].to<JsonObject>();
    jb["online"] = b.valid && (now - b.lastSeen < DEVICE_STALE_MS);
    if (jb["online"].as<bool>()) {
        jb["voltage"]=b.voltage; jb["current"]=b.current; jb["power"]=b.power;
        jb["soc"]=b.soc; jb["soh"]=b.soh; jb["cycles"]=b.dischargesCount;
        jb["remainingAh"]=b.remainingAh; jb["fullAh"]=b.fullCapacityAh;
        jb["cellTemp"]=b.cellTemp; jb["mosfetTemp"]=b.mosfetTemp;
        JsonArray cells = jb["cells"].to<JsonArray>();
        for (int i=0;i<b.cellCount;i++) cells.add(b.cellVoltages[i]);
        if (b.cellCount > 0) {
            int nomV = b.cellCount <= 4 ? 12 : b.cellCount <= 8 ? 24 : 48;
            char bmodel[32];
            if (b.fullCapacityAh > 0) snprintf(bmodel, sizeof(bmodel), "%dV %.0fAh %dS", nomV, b.fullCapacityAh, b.cellCount);
            else                      snprintf(bmodel, sizeof(bmodel), "%dV %dS", nomV, b.cellCount);
            jb["model"] = bmodel;
        }
    }
    JsonObject js = doc["solar"].to<JsonObject>();
    js["online"] = s.valid && (now - s.lastSeen < DEVICE_STALE_MS);
    if (js["online"].as<bool>()) {
        js["stateCode"]=s.chargeState; js["state"]=victronStateName(s.chargeState);
        addFloat(js,"battVoltage",s.battVoltage); addFloat(js,"chargeCurrent",s.chargeCurrent);
        addFloat(js,"solarPower",s.solarPower);   addFloat(js,"yieldToday",s.yieldToday);
        addFloat(js,"loadCurrent",s.loadCurrent);
        js["model"]=victronModelName(s.productId,false); js["pid"]=s.productId;
    }
    JsonObject jo = doc["orion"].to<JsonObject>();
    jo["online"] = o.valid && (now - o.lastSeen < DEVICE_STALE_MS);
    if (jo["online"].as<bool>()) {
        jo["stateCode"]=o.deviceState; jo["state"]=victronStateName(o.deviceState);
        addFloat(jo,"outVoltage",o.outVoltage); addFloat(jo,"outCurrent",o.outCurrent);
        addFloat(jo,"inVoltage",o.inVoltage);   addFloat(jo,"inCurrent",o.inCurrent);
        jo["model"]=victronModelName(o.productId,true); jo["pid"]=o.productId;
    }
    JsonObject jim = doc["imu"].to<JsonObject>();
    jim["online"] = imOnline;
    if (imOnline) {
        jim["pitch"] = im.pitch;
        jim["roll"]  = im.roll;
        jim["yaw"]   = im.yaw;
        jim["temp"]  = im.temp;
    }

    JsonObject jsys = doc["sys"].to<JsonObject>();
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 50)) {
        char tbuf[9], dbuf[11];
        strftime(tbuf, sizeof(tbuf), "%H:%M:%S", &timeinfo);
        strftime(dbuf, sizeof(dbuf), "%Y-%m-%d", &timeinfo);
        jsys["time"] = tbuf;
        jsys["date"] = dbuf;
    } else {
        jsys["time"] = nullptr;
        jsys["date"] = nullptr;
    }
    jsys["apIp"]  = WiFi.softAPIP().toString();
    jsys["staIp"] = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "";

    String json; serializeJson(doc,json);
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Cache-Control","no-cache");
    server.send(200,"application/json",json);
}

static void handleGetSettings() {
    JsonDocument doc;
    doc["bmsMac"]     = settings.getBmsMac();
    doc["solarKey"]   = settings.getSolarKey();
    doc["orionKey"]   = settings.getOrionKey();
    doc["wifiSsid"]   = settings.getWifiSsid();
    doc["wifiPass"]   = settings.getWifiPassword();
    doc["staSsid"]    = settings.getStaSsid();
    doc["staPass"]    = settings.getStaPass();
    doc["ntpServer"]  = settings.getNtpServer();
    doc["ntpTZ"]      = settings.getNtpTZ();
    doc["imuMac"]     = settings.getWitmotionMac();
    String json; serializeJson(doc,json);
    server.send(200,"application/json",json);
}

static void handlePostSettings() {
    if (!server.hasArg("plain")) { server.send(400,"application/json","{\"error\":\"no body\"}"); return; }
    JsonDocument doc;
    if (deserializeJson(doc,server.arg("plain"))) { server.send(400,"application/json","{\"error\":\"invalid json\"}"); return; }
    if (doc["bmsMac"].is<const char*>()   && strlen(doc["bmsMac"])>0)    settings.setBmsMac(doc["bmsMac"].as<String>());
    if (doc["solarKey"].is<const char*>() && strlen(doc["solarKey"])>0)  settings.setSolarKey(doc["solarKey"].as<String>());
    if (doc["orionKey"].is<const char*>() && strlen(doc["orionKey"])>0)  settings.setOrionKey(doc["orionKey"].as<String>());
    if (doc["wifiSsid"].is<const char*>() && strlen(doc["wifiSsid"])>0)  settings.setWifiSsid(doc["wifiSsid"].as<String>());
    if (doc["wifiPass"].is<const char*>() && strlen(doc["wifiPass"])>=8) settings.setWifiPassword(doc["wifiPass"].as<String>());
    if (doc["staSsid"].is<const char*>())  settings.setStaSsid(doc["staSsid"].as<String>());
    if (doc["staPass"].is<const char*>())  settings.setStaPass(doc["staPass"].as<String>());
    if (doc["ntpServer"].is<const char*>() && strlen(doc["ntpServer"])>0) settings.setNtpServer(doc["ntpServer"].as<String>());
    if (doc["ntpTZ"].is<const char*>()     && strlen(doc["ntpTZ"])>0)     settings.setNtpTZ(doc["ntpTZ"].as<String>());
    if (doc["imuMac"].is<const char*>()    && strlen(doc["imuMac"])>0)    settings.setWitmotionMac(doc["imuMac"].as<String>());
    server.send(200,"application/json","{\"status\":\"ok\",\"rebooting\":true}");
    delay(300); ESP.restart();
}

// ─── OTA update ───────────────────────────────────────────────────────────────

static const char OTA_PAGE[] =
R"(<!DOCTYPE html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>OTA Update</title>
<style>
body{font-family:sans-serif;background:#111;color:#eee;max-width:440px;margin:40px auto;padding:0 20px}
h2{margin-bottom:24px}
input[type=file]{display:block;margin:16px 0;color:#eee}
button{padding:10px 24px;background:#22c55e;color:#fff;border:none;border-radius:6px;cursor:pointer;font-size:1em}
button:disabled{background:#444;cursor:default}
#st{margin-top:20px;padding:12px;border-radius:6px;display:none}
.info{background:#1e3a5f;color:#bfdbfe}
.ok{background:#14532d;color:#bbf7d0}
.err{background:#7f1d1d;color:#fecaca}
</style></head><body>
<h2>Firmware Update</h2>
<form id="f">
<input type="file" id="bin" name="firmware" accept=".bin" required>
<button type="submit" id="btn">Upload &amp; Flash</button>
</form>
<div id="st"></div>
<script>
document.getElementById('f').addEventListener('submit',function(e){
  e.preventDefault();
  var file=document.getElementById('bin').files[0];
  if(!file)return;
  var st=document.getElementById('st'),btn=document.getElementById('btn');
  btn.disabled=true;
  st.className='info';st.style.display='block';
  st.textContent='Uploading '+file.name+' ('+Math.round(file.size/1024)+' KB)...';
  var fd=new FormData();fd.append('firmware',file);
  fetch('/update',{method:'POST',body:fd})
    .then(r=>r.text()).then(t=>{st.className=t.includes('OK')?'ok':'err';st.textContent=t;})
    .catch(function(){st.className='ok';st.textContent='Upload completato — il dispositivo si sta riavviando.';});
});
</script></body></html>)";

static void handleOtaPage() {
    if (!server.authenticate(OTA_USERNAME, OTA_PASSWORD))
        return server.requestAuthentication();
    server.send(200, "text/html", OTA_PAGE);
}

static void handleOtaDone() {
    if (!server.authenticate(OTA_USERNAME, OTA_PASSWORD))
        return server.requestAuthentication();
    bool ok = !Update.hasError();
    server.send(200, "text/plain", ok ? "Update OK — rebooting." : "Update FAILED.");
    Serial.printf("[OTA] %s\n", ok ? "Success" : "Failed");
    delay(500);
    ESP.restart();
}

static void handleOtaChunk() {
    if (!server.authenticate(OTA_USERNAME, OTA_PASSWORD)) return;
    HTTPUpload& up = server.upload();
    if (up.status == UPLOAD_FILE_START) {
        Serial.printf("[OTA] Start: %s\n", up.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (Update.write(up.buf, up.currentSize) != up.currentSize)
            Update.printError(Serial);
    } else if (up.status == UPLOAD_FILE_END) {
        if (Update.end(true)) Serial.printf("[OTA] Done: %u bytes\n", up.totalSize);
        else Update.printError(Serial);
    }
}

static void handle204()     { server.send(204,"text/plain",""); }
static void handleHotspot() { server.send(200,"text/html","<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>"); }
static void handleNotFound(){ server.send(404,"text/plain","Not found"); }

// ─── BLE tasks ────────────────────────────────────────────────────────────────

static void bmsTask(void*) {
    for (;;) { bms.update(); vTaskDelay(pdMS_TO_TICKS(100)); }
}

static void imuTask(void*) {
    for (;;) { imu.update(); vTaskDelay(pdMS_TO_TICKS(100)); }
}

static void scanWatchdogTask(void*) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(3000));
        if (!NimBLEDevice::getScan()->isScanning()) {
            Serial.println("[BLE] Scan watchdog: restarting");
            victron.startScan();
        }
    }
}

// ─── setup / loop ─────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[boot] Camper Energy Monitor CYD");

    display.begin();
    settings.begin();
    Serial.printf("[Settings] BMS MAC: %s\n", settings.getBmsMac().c_str());

    wifiSetup();

    NimBLEDevice::init("ESP32-CamperEnergy");
    victron.begin(settings.getSolarKey().c_str(), settings.getOrionKey().c_str(),
                  VICTRON_SOLAR_MAC, VICTRON_ORION_MAC);
    victron.startScan();
    bms.begin(settings.getBmsMac().c_str());
    imu.begin(settings.getWitmotionMac().c_str());
    Serial.printf("[BLE] IMU target: %s\n", settings.getWitmotionMac().c_str());

    xTaskCreate(bmsTask,          "BMS",    8192, nullptr, 1, nullptr);
    xTaskCreate(imuTask,          "IMU",    4096, nullptr, 1, nullptr);
    xTaskCreate(scanWatchdogTask, "ScanWD", 2048, nullptr, 1, nullptr);

    server.on("/",                    handleRoot);
    server.on("/api/data",            handleApiData);
    server.on("/api/settings",        HTTP_GET,  handleGetSettings);
    server.on("/api/settings",        HTTP_POST, handlePostSettings);
    server.on("/update",              HTTP_GET,  handleOtaPage);
    server.on("/update",              HTTP_POST, handleOtaDone, handleOtaChunk);
    server.on("/generate_204",        handle204);
    server.on("/hotspot-detect.html", handleHotspot);
    server.onNotFound(handleNotFound);
    server.begin();

    Serial.println("[boot] Setup complete");
}

void loop() {
    server.handleClient();

    // Re-init NTP only if SNTP daemon is not running at all (e.g. STA connected
    // after boot). SNTP_SYNC_STATUS_RESET is NOT a reliable "not running" check:
    // in IMMED mode the status stays RESET while waiting for a response, so using
    // it would restart SNTP every 60s and prevent sync. Use esp_sntp_enabled().
    static uint32_t ntpRetryAt = 60000;
    if (WiFi.status() == WL_CONNECTED && millis() > ntpRetryAt) {
        if (!esp_sntp_enabled()) {
            Serial.println("[NTP] SNTP daemon not running, initializing");
            ntpSetup();
        }
        ntpRetryAt = millis() + 60000;
    }

    // Update sysinfo on Settings screen every 5s
    static uint32_t sysInfoAt = 0;
    if (millis() - sysInfoAt > 5000) {
        sysInfoAt = millis();
        String apSsid  = settings.getWifiSsid();
        String apIp    = WiFi.softAPIP().toString();
        String staSsid = settings.getStaSsid();
        String staIp   = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "";
        char ntpBuf[20] = "--";
        time_t now = time(nullptr);
        if (now > 1000000000UL) {
            struct tm ti; localtime_r(&now, &ti);
            strftime(ntpBuf, sizeof(ntpBuf), "%Y-%m-%d %H:%M:%S", &ti);
        }
        display.updateSysInfo(apSsid.c_str(), apIp.c_str(),
                              staSsid.c_str(), staIp.c_str(), ntpBuf);
    }

    BmsData   b = bms.getData();
    SolarData s = victron.getSolar();
    OrionData o = victron.getOrion();

    display.update(b, s, o, imu.getData(), millis());
    display.handleTouch();
}

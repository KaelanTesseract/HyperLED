/*
 * HyperLED - Open Source LED Controller
 * 
 * Copyright (c) 2026 Dennis Guse
 * 
 * Licensed under the EUPL, Version 1.2 or – as soon they will be approved by 
 * the European Commission - subsequent versions of the EUPL (the "Licence");
 * You may not use this work except in compliance with the Licence.
 * You may obtain a copy of the Licence at:
 * 
 * https://joinup.ec.europa.eu/software/page/eupl
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the Licence is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the Licence for the specific language governing permissions and
 * limitations under the Licence.
 */
#include "AppWebServer.h"
#include "esp_system.h"
#include <LittleFS.h>
#include <time.h>
#include "Config.h"
#include "LEDManager.h"
#include "WeatherManager.h"
#include <ArduinoJson.h>
#include "UpdateManager.h"
#include "SlaveManager.h"
#include <AsyncJson.h>
#include <ESPmDNS.h>
#include "MqttManager.h"
#include "PresetManager.h"
#include "ScheduleManager.h"
#include "StatusLedManager.h"

WebServerManagerClass WebServerManager;
AsyncWebServer server(80);

void WebServerManagerClass::begin() {
    if (!LittleFS.begin(true)) {
        Serial.println("An Error has occurred while mounting LittleFS");
    }

    setupRoutes();
    setupWLEDJsonAPI();
    
    if (WiFiManager.isAPMode()) {
        setupCaptivePortal();
    } else {
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char hostname[32];
        sprintf(hostname, "HyperLED-%02x%02x%02x", mac[3], mac[4], mac[5]);
        
        char macStr[13];
        sprintf(macStr, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        
        if (!MDNS.begin(hostname)) {
            Serial.println("Error setting up MDNS responder!");
        } else {
            MDNS.addService("wled", "tcp", 80);
            MDNS.addServiceTxt(String("wled"), String("tcp"), String("mac"), String(macStr));
        }
    }
    setupOTA();

    server.begin();
    Serial.println("HTTP server started");
}

void WebServerManagerClass::setupRoutes() {
    // Serve static files from LittleFS
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html").setCacheControl("no-store, no-cache, must-revalidate, max-age=0");

    // API Routes
    server.on("/api/scan", HTTP_GET, [](AsyncWebServerRequest *request){
        WiFiManager.startScan();
        request->send(200, "text/plain", "Scanning...");
    });

    server.on("/api/scan_results", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = WiFiManager.getScanResultsJSON();
        request->send(200, "application/json", json);
    });

    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{\"ap_mode\": " + String(WiFiManager.isAPMode() ? "true" : "false") + "}";
        request->send(200, "application/json", json);
    });

    server.on("/api/version", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{\"version\": \"" + String(SOFTWARE_VERSION) + "\"}";
        request->send(200, "application/json", json);
    });

    server.on("/api/save_wifi", HTTP_POST, [](AsyncWebServerRequest *request){
        String ssid = "";
        String pass = "";
        if(request->hasParam("ssid", true)) ssid = request->getParam("ssid", true)->value();
        if(request->hasParam("password", true)) pass = request->getParam("password", true)->value();

        if (WiFiManager.isAPMode()) {
            // During AP-mode setup, test the credentials first and keep the AP up, so the
            // client can be told the new IP (or the failure) instead of silently losing the
            // connection to a rebooting device. Poll /api/wifi/setup_status for the result.
            WiFiManager.startSetupConnect(ssid, pass);
            request->send(200, "application/json", "{\"status\":\"connecting\"}");
        } else {
            request->send(200, "application/json", "{\"status\":\"rebooting\"}");
            WiFiManager.saveCredentials(ssid, pass);
        }
    });

    server.on("/api/wifi/setup_status", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        switch (WiFiManager.getSetupState()) {
            case WIFI_SETUP_CONNECTING: doc["state"] = "connecting"; break;
            case WIFI_SETUP_SUCCESS:    doc["state"] = "success";    break;
            case WIFI_SETUP_FAILED:     doc["state"] = "failed";     break;
            default:                    doc["state"] = "idle";       break;
        }
        if (WiFiManager.getSetupState() == WIFI_SETUP_SUCCESS) {
            doc["ip"] = WiFiManager.getSetupIp();
            doc["ssid"] = WiFi.SSID();
        }
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    server.on("/api/wifi/setup_finish", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200, "application/json", "{\"status\":\"restarting\"}");
        // Give the response a moment to reach the client before the radio goes down.
        WiFiManager.requestRestart(1000);
    });

    // --- State API ---
    server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        // The UI restores its sync checkbox from this: without it a reloaded page always came
        // up unticked, no matter what the device was actually doing.
        doc["sync"] = LEDManager.getSync();
        JsonArray segArr = doc["seg"].to<JsonArray>();
        LEDManager.getSegmentsJson(segArr);
        
        String json;
        serializeJson(doc, json);
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        request->send(response);
    });

    // Diagnostics for wireless Slave discovery: ESP-NOW only works when both sides sit on
    // the same channel, and the Master simply inherits whatever channel its router put it
    // on - so this is the first thing to check when a Slave isn't found.
    server.on("/api/espnow_status", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        doc["channel"] = WiFi.channel();
        doc["rssi"] = WiFi.RSSI();
        doc["ssid"] = WiFi.SSID();
        doc["mode"] = (int)WiFi.getMode();
        doc["mac"] = WiFi.macAddress();
        EspNowBusClass* bus = SlaveManager.getEspBus();
        if (bus) {
            doc["rxPackets"] = bus->getPacketsReceived();
            doc["lastSender"] = bus->getLastSenderId();
            doc["lastCommand"] = bus->getLastCommand();
            doc["pongs"] = bus->getPongsReceived();
            doc["dropIncomplete"] = bus->getDroppedIncomplete();
            doc["dropNoCallback"] = bus->getDroppedNoCallback();
            doc["dropForeign"] = bus->getDroppedForeign();
            doc["sendErrors"] = bus->getSendErrors();
        }
        {
            doc["ledPackets"] = SlaveManager.getLedPacketsSent();
            doc["ledFrames"] = SlaveManager.getLedFramesSent();
        }
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    server.on("/api/wifi/status", HTTP_GET, [](AsyncWebServerRequest *request){
        if (WiFi.status() == WL_CONNECTED) {
            String json = "{\"ssid\":\"" + WiFi.SSID() + "\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
            request->send(200, "application/json", json);
        } else {
            request->send(404, "application/json", "{\"error\":\"not connected\"}");
        }
    });

    server.on("/api/info", HTTP_GET, [](AsyncWebServerRequest *request){
        // Heap figures included on purpose: the large uploads (image widgets, OTA) are the
        // operations most likely to run the device out of memory, and without a number to look
        // at that can only be guessed from crashes.
        String json = "{\"version\":\"" + String(HYPERLED_VERSION) + "\"";
        json += ",\"heapFree\":" + String((unsigned)ESP.getFreeHeap());
        json += ",\"heapMinFree\":" + String((unsigned)ESP.getMinFreeHeap());
        json += ",\"heapLargestBlock\":" + String((unsigned)ESP.getMaxAllocHeap());
        // Uptime and reset reason together answer the one question that outside observation
        // cannot: whether an outage was the link going away or the board restarting under it.
        // They look identical from the network, and on this hardware they look identical on the
        // serial port too, because a restart re-enumerates the USB device and the monitor simply
        // goes quiet.
        json += ",\"uptime\":" + String((unsigned long)(millis() / 1000));
        json += ",\"resetReason\":" + String((int)esp_reset_reason());
        json += ",\"wifiRssi\":" + String((int)WiFi.RSSI());
        json += ",\"wifiChannel\":" + String((int)WiFi.channel());
        json += ",\"wifiDisconnects\":" + String((unsigned long)WiFiManager.getDisconnectCount());
        json += ",\"wifiReconnects\":" + String((unsigned long)WiFiManager.getReconnectCount());
        json += ",\"wifiLastReason\":" + String((unsigned)WiFiManager.getLastDisconnectReason());
        json += ",\"wifiOfflineMs\":" + String((unsigned long)WiFiManager.getOfflineMs());
        json += "}";
        request->send(200, "application/json", json);
    });

    AsyncCallbackJsonWebHandler* onlineUpdateHandler = new AsyncCallbackJsonWebHandler("/api/update_online", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
        if (!jsonObj["version"].isNull()) {
            String ver = jsonObj["version"].as<String>();
            UpdateManager.startOnlineUpdate(ver);
            request->send(200, "application/json", "{\"status\":\"started\"}");
        } else {
            request->send(400, "application/json", "{\"error\":\"version missing\"}");
        }
    });
    server.addHandler(onlineUpdateHandler);

    server.on("/api/update_progress", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{\"progress\":" + String(UpdateManager.getProgress()) + ",\"status\":\"" + UpdateManager.getStatus() + "\"}";
        request->send(200, "application/json", json);
    });

    AsyncCallbackJsonWebHandler* stateHandler = new AsyncCallbackJsonWebHandler("/api/state", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
        
        if (!jsonObj["sync"].isNull()) {
            LEDManager.setSync(jsonObj["sync"].as<bool>());
        }
        
        if (!jsonObj["seg"].isNull() && jsonObj["seg"].is<JsonArray>()) {
            JsonArray arr = jsonObj["seg"].as<JsonArray>();
            for (JsonObject s : arr) {
                uint8_t id = s["id"] | 0;
                if (id < LEDManager.getNumSegments()) {
                    if (!s["on"].isNull()) LEDManager.setPower(id, s["on"].as<bool>());
                    if (!s["bri"].isNull()) LEDManager.setBrightness(id, s["bri"].as<uint8_t>());
                    if (!s["effect"].isNull()) LEDManager.setEffect(id, s["effect"].as<uint8_t>());
                    if (!s["speed"].isNull()) LEDManager.setSpeed(id, s["speed"].as<uint8_t>());
                    if (!s["color"].isNull()) {
                        String c = s["color"].as<String>();
                        if (c.startsWith("#")) c.remove(0, 1);
                        uint32_t newRgb = strtol(c.c_str(), NULL, 16) & 0x00FFFFFF;
                        uint32_t oldW = LEDManager.getColor(id) & 0xFF000000;
                        LEDManager.setColor(id, oldW | newRgb);
                    }
                    if (!s["white"].isNull()) {
                          uint8_t w = s["white"].as<uint8_t>();
                          uint32_t c = LEDManager.getColor(id);
                          LEDManager.setColor(id, (c & 0x00FFFFFF) | ((uint32_t)w << 24));
                      }
                      if (!s["whiteOnly"].isNull()) LEDManager.setWhiteOnly(id, s["whiteOnly"].as<bool>());
                      if (!s["cct"].isNull()) LEDManager.setCct(id, s["cct"].as<uint8_t>());
                      if (!s["palette"].isNull()) LEDManager.setPalette(id, s["palette"].as<uint8_t>());
                      if (!s["intensity"].isNull()) LEDManager.setIntensity(id, s["intensity"].as<uint8_t>());
                      if (!s["color2"].isNull()) {
                          String c2 = s["color2"].as<String>();
                          if (c2.startsWith("#")) c2.remove(0, 1);
                          uint32_t newRgb2 = strtol(c2.c_str(), NULL, 16) & 0x00FFFFFF;
                          LEDManager.setColor2(id, newRgb2);
                      }
                      if (!s["color2Enabled"].isNull()) LEDManager.setColor2Enabled(id, s["color2Enabled"].as<bool>());
                }
            }
        }
        
        MqttManager.publishState();
        request->send(200, "text/plain", "OK");
    });
    server.addHandler(stateHandler);
    
    server.on("/api/slaves", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        auto slaves = SlaveManager.getDiscoveredSlaves();
        for (const auto& s : slaves) {
            JsonObject obj = arr.add<JsonObject>();
            obj["id"] = s.currentId;
            obj["ledCount"] = s.ledCount;
            obj["name"] = s.name;
            obj["version"] = s.version;
            obj["isWireless"] = s.isWireless;
            obj["lastSeenAge"] = millis() - s.lastSeen;
            obj["configPending"] = SlaveManager.isConfigPending(s.currentId);
            // Only present once the Slave has reported it (firmware 0.2.1+). The UI leaves its
            // fields alone when it is missing rather than showing a default it would then save.
            if (s.ledType != 255) {
                obj["ledType"] = s.ledType;
                obj["matrixWidth"] = s.matrixWidth;
                obj["matrixHeight"] = s.matrixHeight;
                obj["hub75ShiftDriver"] = s.hub75ShiftDriver;
            }
        }
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    AsyncCallbackJsonWebHandler* slaveConfigHandler = new AsyncCallbackJsonWebHandler("/api/slaves/config", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
        if (!jsonObj["currentId"].isNull() && !jsonObj["newId"].isNull()) {
            uint8_t currentId = jsonObj["currentId"].as<uint8_t>();
            uint8_t newId = jsonObj["newId"].as<uint8_t>();
            uint8_t pin = jsonObj["pin"] | 4; // Default to pin 4
            uint8_t pin2 = jsonObj["pin2"] | 255;
            uint16_t count = jsonObj["ledCount"] | 0;
            uint8_t type = jsonObj["type"] | 22; // Default to WS281x
            String name = jsonObj["name"] | "New Slave";
            uint16_t matrixWidth = jsonObj["matrixWidth"] | 16;
            uint16_t matrixHeight = jsonObj["matrixHeight"] | 16;
            uint8_t hub75ShiftDriver = jsonObj["hub75ShiftDriver"] | 0;

            SlaveManager.configureSlave(currentId, newId, pin, pin2, count, type, name, matrixWidth, matrixHeight, hub75ShiftDriver);
            
            // Also create or update a dynamic segment in LEDManager
            if (newId != 254 && count > 0) {
                LEDManager.addSlaveSegment(newId, count, name);
            }
            
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Missing fields");
        }
    });
    server.addHandler(slaveConfigHandler);
    
    AsyncCallbackJsonWebHandler* slaveStatusLedHandler = new AsyncCallbackJsonWebHandler("/api/slaves/statusled", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
        if (jsonObj["id"].isNull()) {
            request->send(400, "text/plain", "Missing id");
            return;
        }
        uint8_t id = jsonObj["id"].as<uint8_t>();
        bool on = jsonObj["on"] | true;
        uint32_t color = jsonObj["color"] | 0x00FF00;
        uint8_t bri = jsonObj["bri"] | 40;
        SlaveManager.setSlaveStatusLed(id, on, color, bri);
        request->send(200, "text/plain", "OK");
    });
    server.addHandler(slaveStatusLedHandler);

    AsyncCallbackJsonWebHandler* slaveUpdateHandler = new AsyncCallbackJsonWebHandler("/api/slaves/update", [](AsyncWebServerRequest *request, JsonVariant &json) {
        String url = "";
        JsonObject jsonObj = json.as<JsonObject>();
        if (!jsonObj.isNull() && jsonObj["url"].is<String>()) {
            url = jsonObj["url"].as<String>();
        } else {
            url = "https://raw.githubusercontent.com/KaelanTesseract/HyperLED-Slave/main/.pio/build/esp32-s3/firmware.bin";
        }
        
        // We need the current WiFi credentials
        Preferences prefs;
        prefs.begin("wled_clone", true);
        String ssid = prefs.getString(PREF_WIFI_SSID, "");
        String pass = prefs.getString(PREF_WIFI_PASS, "");
        prefs.end();
        
        SlaveManager.triggerSlaveUpdate(HYPERBUS_BROADCAST_ID, ssid, pass, url);
        request->send(200, "text/plain", "OK");
    });
    server.addHandler(slaveUpdateHandler);
    
    AsyncCallbackJsonWebHandler* matrixHandler = new AsyncCallbackJsonWebHandler("/api/matrix", [](AsyncWebServerRequest *request, JsonVariant &json) {
        if (json.is<JsonArray>()) {
            JsonArray arr = json.as<JsonArray>();
            uint16_t x = 0;
            uint16_t y = 0;
            uint16_t width = LEDManager.getMatrixWidth();
            for (uint32_t c : arr) {
                uint8_t r = (c >> 16) & 0xFF;
                uint8_t g = (c >> 8) & 0xFF;
                uint8_t b = c & 0xFF;
                LEDManager.setPixelColorXY(x, y, r, g, b, 0);
                x++;
                if (x >= width) {
                    x = 0;
                    y++;
                }
            }

            // Switch any local segment covering the matrix into the "Bild" (Image)
            // hold effect, so the streamed frame isn't immediately overwritten by
            // whatever effect was previously running - then push it out right away.
            uint32_t matrixPixelCount = (uint32_t)LEDManager.getMatrixWidth() * LEDManager.getMatrixHeight();
            for (uint8_t i = 0; i < LEDManager.getNumSegments(); i++) {
                const Segment* seg = LEDManager.getSegment(i);
                if (seg && !seg->isSlave && seg->start < matrixPixelCount && seg->stop > 0) {
                    LEDManager.setEffect(i, 25);
                }
            }
            LEDManager.showNow();

            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Bad Request");
        }
    });
    matrixHandler->setMaxContentLength(100000); // room for up to ~64x64 matrix uploads (default 16KB was far too small)
    server.addHandler(matrixHandler);

    // Live preview: current pixel colors of the Master's own matrix (whatever
    // effect is driving it), so the WebUI can mirror the panel without a camera.
    server.on("/api/matrix_preview", HTTP_GET, [](AsyncWebServerRequest *request){
        // With ?seg=N the live pixels of that segment are returned instead of the Master's own
        // matrix. That is the only way to see a panel driven by a Slave: the Master renders it
        // and streams it onward without ever displaying it.
        if (request->hasParam("seg")) {
            uint8_t segId = (uint8_t)request->getParam("seg")->value().toInt();
            uint16_t start = 0, count = 0;
            const uint8_t* buf = nullptr;
            if (!LEDManager.getSegmentPixels(segId, start, count, buf)) {
                request->send(200, "application/json", "[]");
                return;
            }
            // Built in one string with the space reserved up front, NOT through an
            // AsyncResponseStream. That class reads its buffer back one character at a time, and
            // each read shifts the whole remainder down - quadratic work that took seconds for a
            // 4096-pixel panel and tripped the task watchdog into rebooting the device.
            String json;
            json.reserve((size_t)count * 9 + 2);
            json += '[';
            for (uint16_t i = 0; i < count; i++) {
                if (i) json += ',';
                const uint8_t* px = &buf[(size_t)(start + i) * 5];
                uint32_t rgb = ((uint32_t)px[0] << 16) | ((uint32_t)px[1] << 8) | px[2];
                json += rgb;
            }
            json += ']';
            AsyncWebServerResponse* response = request->beginResponse(200, "application/json", json);
            response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
            request->send(response);
            return;
        }

        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        LEDManager.getMatrixPreviewJson(arr);
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    AsyncCallbackJsonWebHandler* matrixConfigHandler = new AsyncCallbackJsonWebHandler("/api/matrix_config", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
        bool isMat = jsonObj["isMatrix"] | false;
        uint16_t w = jsonObj["width"] | 16;
        uint16_t h = jsonObj["height"] | 16;
        uint8_t l = jsonObj["layout"] | 0;
        LEDManager.setMatrixConfig(isMat, w, h, l);
        request->send(200, "text/plain", "OK");
    });
    server.addHandler(matrixConfigHandler);

    // --- Multi-Panel Canvas API ---
    server.on("/api/canvas_panels", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        LEDManager.getCanvasPanelsJson(arr);
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    AsyncCallbackJsonWebHandler* canvasPanelsHandler = new AsyncCallbackJsonWebHandler("/api/canvas_panels", [](AsyncWebServerRequest *request, JsonVariant &json) {
        if (json.is<JsonArray>()) {
            LEDManager.setCanvasPanels(json.as<JsonArray>());
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Bad Request");
        }
    });
    server.addHandler(canvasPanelsHandler);

    // --- Segments API ---
    server.on("/api/segments", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        JsonArray segArr = doc.to<JsonArray>();
        LEDManager.getSegmentsJson(segArr);
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    AsyncCallbackJsonWebHandler* segmentsHandler = new AsyncCallbackJsonWebHandler("/api/segments", [](AsyncWebServerRequest *request, JsonVariant &json) {
        if (json.is<JsonArray>()) {
            LEDManager.setSegmentsFromJson(json.as<JsonArray>());
            LEDManager.recalculateSegments(); 
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Expected JSON Array");
        }
    });
    server.addHandler(segmentsHandler);

    // --- "Uhr / Text" widgets API (see TextWidget in LEDManager.h) ---
    AsyncCallbackJsonWebHandler* textWidgetsHandler = new AsyncCallbackJsonWebHandler("/api/text_widgets", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
        uint8_t segId = jsonObj["seg"] | 0;
        if (!jsonObj["widgets"].isNull() && jsonObj["widgets"].is<JsonArray>()) {
            LEDManager.setTextWidgets(segId, jsonObj["widgets"].as<JsonArray>());
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Expected {seg, widgets:[]}");
        }
    });
    server.addHandler(textWidgetsHandler);

    // Uploads one image widget's pixel data: { seg, widget, w, h, pixels:[0xRRGGBB,...] }
    // (same flat-array pixel convention as /api/matrix).
    AsyncCallbackJsonWebHandler* textWidgetImageHandler = new AsyncCallbackJsonWebHandler("/api/text_widget_image", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
        uint8_t segId = jsonObj["seg"] | 0;
        uint8_t widgetId = jsonObj["widget"] | 0;
        uint8_t w = jsonObj["w"] | 0;
        uint8_t h = jsonObj["h"] | 0;
        if (widgetId == 0 || w == 0 || h == 0 || !jsonObj["pixels"].is<JsonArray>()) {
            request->send(400, "text/plain", "Bad Request");
            return;
        }
        JsonArray arr = jsonObj["pixels"].as<JsonArray>();
        std::vector<uint8_t> rgb;
        rgb.reserve(arr.size() * 3);
        for (uint32_t c : arr) {
            rgb.push_back((c >> 16) & 0xFF);
            rgb.push_back((c >> 8) & 0xFF);
            rgb.push_back(c & 0xFF);
        }
        LEDManager.setTextWidgetImage(segId, widgetId, w, h, rgb);
        request->send(200, "text/plain", "OK");
    });
    textWidgetImageHandler->setMaxContentLength(100000); // room for up to ~64x64 image widgets (matches /api/matrix's cap)
    server.addHandler(textWidgetImageHandler);

    // --- Weather API (see WeatherManager.h - used by the "Wetter" widget) ---
    server.on("/api/weather_status", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        doc["city"] = WeatherManager.getCity();
        doc["hasLocation"] = WeatherManager.hasLocation();
        doc["hasData"] = WeatherManager.hasData();
        doc["temperature"] = WeatherManager.getTemperature();
        doc["icon"] = WeatherManager.getWeatherIcon();
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    AsyncCallbackJsonWebHandler* weatherLocationHandler = new AsyncCallbackJsonWebHandler("/api/weather_location", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
        String city = jsonObj["city"] | "";
        if (city.length() == 0) {
            request->send(400, "text/plain", "Missing city");
            return;
        }
        bool ok = WeatherManager.setLocation(city);
        if (ok) {
            request->send(200, "text/plain", "OK");
        } else {
            request->send(422, "text/plain", "Standort nicht gefunden oder kein WLAN");
        }
    });
    server.addHandler(weatherLocationHandler);

    // --- MQTT API ---
    server.on("/api/mqtt", HTTP_GET, [](AsyncWebServerRequest *request){
        Preferences prefs;
        prefs.begin(PREF_NAMESPACE, true);
        JsonDocument doc;
        doc["enabled"] = prefs.getBool(PREF_MQTT_ENABLE, false);
        doc["server"] = prefs.getString(PREF_MQTT_SERVER, "");
        doc["port"] = prefs.getUShort(PREF_MQTT_PORT, 1883);
        doc["user"] = prefs.getString(PREF_MQTT_USER, "");
        doc["pass"] = prefs.getString(PREF_MQTT_PASS, "");
        doc["topic"] = prefs.getString(PREF_MQTT_TOPIC, "hyperled/device");
        prefs.end();
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/api/mqtt", HTTP_POST, [](AsyncWebServerRequest *request){
        Preferences prefs;
        prefs.begin(PREF_NAMESPACE, false);
        
        if (request->hasParam("enabled", true)) {
            prefs.putBool(PREF_MQTT_ENABLE, request->getParam("enabled", true)->value() == "true");
        }
        
        if (request->hasParam("server", true)) {
            String srv = request->getParam("server", true)->value();
            if (srv.length() > 0) prefs.putString(PREF_MQTT_SERVER, srv);
            else prefs.remove(PREF_MQTT_SERVER);
        }
        
        if (request->hasParam("port", true)) {
            prefs.putUShort(PREF_MQTT_PORT, request->getParam("port", true)->value().toInt());
        }
        
        if (request->hasParam("user", true)) {
            String usr = request->getParam("user", true)->value();
            if (usr.length() > 0) prefs.putString(PREF_MQTT_USER, usr);
            else prefs.remove(PREF_MQTT_USER);
        }
        
        if (request->hasParam("pass", true)) {
            String pwd = request->getParam("pass", true)->value();
            if (pwd.length() > 0) prefs.putString(PREF_MQTT_PASS, pwd);
            else prefs.remove(PREF_MQTT_PASS);
        }
        
        if (request->hasParam("topic", true)) {
            String top = request->getParam("topic", true)->value();
            if (top.length() > 0) prefs.putString(PREF_MQTT_TOPIC, top);
            else prefs.remove(PREF_MQTT_TOPIC);
        }
        
        prefs.end();
        
        request->send(200, "application/json", "{\"status\":\"ok\"}");
        LEDManager.recalculateSegments();
    });



    // --- Config API ---
    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        JsonArray pins = doc["pins"].to<JsonArray>();
        for(int i=0; i<5; i++) pins.add(LEDManager.getPin(i));
        doc["count"] = LEDManager.getCount();
    doc["ledsPerIC"] = LEDManager.getLedsPerIC();
        doc["type"] = LEDManager.getType();
        doc["abl_en"] = LEDManager.getAblEnabled();
        doc["abl_ma"] = LEDManager.getAblMaxmA();
        
        doc["matrix_en"] = LEDManager.isMatrix();
        doc["matrix_w"] = LEDManager.getMatrixWidth();
        doc["matrix_h"] = LEDManager.getMatrixHeight();
        // Assume Matrix Layout has a getter or we just don't return it strictly since it's write-only or we can skip it.
        // I will add getMatrixLayout to LEDManager.h
        doc["matrix_l"] = LEDManager.getMatrixLayout();
        doc["hub75_shift_driver"] = LEDManager.getHub75ShiftDriver();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    AsyncCallbackJsonWebHandler* configHandler = new AsyncCallbackJsonWebHandler("/api/config", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
        
        if (!jsonObj["pins"].isNull() && !jsonObj["count"].isNull() && !jsonObj["type"].isNull()) {
            JsonArray pins = jsonObj["pins"].as<JsonArray>();
            uint8_t p0 = pins.size() > 0 ? pins[0].as<uint8_t>() : 255;
            uint8_t p1 = pins.size() > 1 ? pins[1].as<uint8_t>() : 255;
            uint8_t p2 = pins.size() > 2 ? pins[2].as<uint8_t>() : 255;
            uint8_t p3 = pins.size() > 3 ? pins[3].as<uint8_t>() : 255;
            uint8_t p4 = pins.size() > 4 ? pins[4].as<uint8_t>() : 255;
            
            bool ablEn = jsonObj["abl_en"].isNull() ? true : jsonObj["abl_en"].as<bool>();
            uint16_t ablMa = jsonObj["abl_ma"].isNull() ? 850 : jsonObj["abl_ma"].as<uint16_t>();
            
            uint8_t ledsPerIC = jsonObj["ledsPerIC"].isNull() ? 1 : jsonObj["ledsPerIC"].as<uint8_t>();
              LEDManager.setConfig(p0, p1, p2, p3, p4, jsonObj["count"].as<uint16_t>(), jsonObj["type"].as<uint8_t>(), ablEn, ablMa, ledsPerIC);

            if (!jsonObj["hub75_shift_driver"].isNull()) {
                LEDManager.setHub75ShiftDriver(jsonObj["hub75_shift_driver"].as<uint8_t>());
            }
        }

        request->send(200, "text/plain", "OK");
        LEDManager.recalculateSegments();
    });
    server.addHandler(configHandler);

    // --- Buttons API ---
    // --- Onboard status LED (independent of the configured LED strips) ---
    server.on("/api/statusled", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        doc["on"] = StatusLedManager.isOn();
        doc["color"] = StatusLedManager.getColor();
        doc["bri"] = StatusLedManager.getBrightness();
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    AsyncCallbackJsonWebHandler* statusLedHandler = new AsyncCallbackJsonWebHandler("/api/statusled", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
        bool on = jsonObj["on"] | StatusLedManager.isOn();
        uint32_t color = jsonObj["color"] | StatusLedManager.getColor();
        uint8_t bri = jsonObj["bri"] | StatusLedManager.getBrightness();
        StatusLedManager.setState(on, color, bri);
        request->send(200, "text/plain", "OK");
    });
    server.addHandler(statusLedHandler);

    server.on("/api/buttons", HTTP_GET, [](AsyncWebServerRequest *request){
        Preferences prefs;
        prefs.begin("wled_clone", true);
        JsonDocument doc;
        
        JsonObject b1 = doc["btn1"].to<JsonObject>();
        b1["active"] = prefs.getBool("btn1_en", false);
        b1["type"] = prefs.getString("btn1_type", "push");
        
        JsonObject b2 = doc["btn2"].to<JsonObject>();
        b2["active"] = prefs.getBool("btn2_en", false);
        b2["type"] = prefs.getString("btn2_type", "switch");
        
        prefs.end();
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    AsyncCallbackJsonWebHandler* buttonsHandler = new AsyncCallbackJsonWebHandler("/api/buttons", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
        Preferences prefs;
        prefs.begin("wled_clone", false);
        
        if (!jsonObj["btn1"].isNull()) {
            prefs.putBool("btn1_en", jsonObj["btn1"]["active"].as<bool>());
            prefs.putString("btn1_type", jsonObj["btn1"]["type"].as<String>());
        }
        if (!jsonObj["btn2"].isNull()) {
            prefs.putBool("btn2_en", jsonObj["btn2"]["active"].as<bool>());
            prefs.putString("btn2_type", jsonObj["btn2"]["type"].as<String>());
        }
        prefs.end();
        
        request->send(200, "text/plain", "OK");
        LEDManager.recalculateSegments(); // Buttons need restart to init pins
    });
    server.addHandler(buttonsHandler);
    
    server.on("/api/factory_reset", HTTP_POST, [](AsyncWebServerRequest *request){
        Preferences prefs;
        
        // Tasten auf Standard
        prefs.begin("wled_clone", false);
        prefs.putBool("btn1_en", false);
        prefs.putString("btn1_type", "push");
        prefs.putBool("btn2_en", false);
        prefs.putString("btn2_type", "switch");
        prefs.end();
        
        // System auf Standard
        prefs.begin(PREF_NAMESPACE, false);
        // LEDs
        prefs.putUChar("led_p0", 4);
        prefs.putUChar("led_p1", 255);
        prefs.putUChar("led_p2", 255);
        prefs.putUChar("led_p3", 255);
        prefs.putUChar("led_p4", 255);
        prefs.putUShort("led_count", 30);
        prefs.putUChar("led_type", 22);
        prefs.putBool("led_abl_en", true);
        prefs.putUShort("led_abl_ma", 850);
        
        // Matrix
        prefs.putBool("mat_en", false);
        prefs.putUShort("mat_w", 16);
        prefs.putUShort("mat_h", 16);
        prefs.putUChar("mat_l", 0);
        
        // Segmente zurücksetzen
        prefs.remove("segments");
        
        // MQTT
        prefs.putBool(PREF_MQTT_ENABLE, false);
        prefs.remove(PREF_MQTT_SERVER);
        prefs.putUShort(PREF_MQTT_PORT, 1883);
        prefs.remove(PREF_MQTT_USER);
        prefs.remove(PREF_MQTT_PASS);
        prefs.putString(PREF_MQTT_TOPIC, "hyperled/device");
        
        // WLAN löschen
        prefs.remove(PREF_WIFI_SSID);
        prefs.remove(PREF_WIFI_PASS);
        
        prefs.end();
        
        request->send(200, "text/plain", "OK");
        LEDManager.recalculateSegments();
    });
}

void WebServerManagerClass::setupCaptivePortal() {
    server.onNotFound([](AsyncWebServerRequest *request){
        if (request->host().indexOf(WiFi.softAPIP().toString()) == -1) {
            request->redirect("http://" + WiFi.softAPIP().toString() + "/");
        } else {
            request->send(LittleFS, "/index.html", "text/html");
        }
    });
}

void WebServerManagerClass::setupOTA() {
    server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
        bool shouldReboot = !Update.hasError();
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot ? "OK" : "FAIL");
        response->addHeader("Connection", "close");
        request->send(response);
        if (shouldReboot) {
            LEDManager.recalculateSegments();
        }
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
        if(!index){
            Serial.printf("Update Start: %s\n", filename.c_str());
            int cmd = (filename.indexOf("littlefs") > -1 || filename.indexOf("spiffs") > -1) ? U_SPIFFS : U_FLASH;
            if(!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)){
                Update.printError(Serial);
            }
        }
        if(!Update.hasError()){
            if(Update.write(data, len) != len){
                Update.printError(Serial);
            }
        }
        if(final){
            if(Update.end(true)){
                Serial.printf("Update Success: %uB\n", index+len);
            } else {
                Update.printError(Serial);
            }
        }
    });
}

void WebServerManagerClass::loop() {
    if (_triggerRestart) {
        _triggerRestart = false;
        delay(500);
        ESP.restart();
    }
}

void WebServerManagerClass::setupWLEDJsonAPI() {
    auto buildState = [](JsonVariant doc) {
        JsonObject state = doc.to<JsonObject>();
        state["on"] = LEDManager.getPower(0);
        state["bri"] = LEDManager.getBrightness(0);
        state["transition"] = 7;
        state["ps"] = -1;
        state["pl"] = -1;
        
        JsonObject nl = state["nl"].to<JsonObject>();
        nl["on"] = false;
        nl["dur"] = 60;
        nl["mode"] = 1;
        nl["tbri"] = 0;
        nl["rem"] = -1;

        JsonObject udpn = state["udpn"].to<JsonObject>();
        udpn["send"] = false;
        udpn["recv"] = true;

        state["lor"] = 0;
        state["mainseg"] = 0;
        
        JsonArray seg = state["seg"].to<JsonArray>();
        LEDManager.getSegmentsJson(seg);
    };

    auto buildInfo = [](JsonVariant doc) {
        JsonObject info = doc.to<JsonObject>();
        info["ver"] = "0.14.0";
        info["vid"] = 2401010;
        
        JsonObject leds = info["leds"].to<JsonObject>();
        leds["count"] = LEDManager.getCount();
        leds["pwr"] = 0;
        leds["cct"] = false;
        leds["lco"] = 1;
        leds["lc"] = 1;
        leds["fps"] = 30;
        leds["maxpwr"] = 0;
        leds["maxseg"] = 1;
        JsonArray seglc = leds["seglc"].to<JsonArray>();
        seglc.add(1);

        JsonObject fs = info["fs"].to<JsonObject>();
        fs["u"] = 120;
        fs["t"] = 1500;
        fs["pmt"] = 1700000000;

        info["str"] = true;

        uint8_t mac[6];
        WiFi.macAddress(mac);
        char hostname[32];
        sprintf(hostname, "HyperLED-%02x%02x%02x", mac[3], mac[4], mac[5]);
        
        char macStr[13];
        sprintf(macStr, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        
        info["name"] = hostname;
        info["udpport"] = 21324;
        info["live"] = false;
        info["lm"] = "";
        info["lip"] = "";
        info["ws"] = -1;
        info["eq"] = 1;
        info["ndc"] = 1;
        info["arch"] = "esp32";
        info["core"] = "2_0_14";
        info["lwip"] = 1;
        info["freeheap"] = ESP.getFreeHeap();
        info["uptime"] = millis() / 1000;
        info["opt"] = 15;
        info["brand"] = "WLED";
        info["product"] = "HyperLED";
        info["mac"] = macStr;
        info["ip"] = WiFi.localIP().toString();
    };

    server.on("/json", HTTP_GET, [buildState, buildInfo](AsyncWebServerRequest *request){
        JsonDocument doc;

        buildState(doc["state"].to<JsonVariant>());
        buildInfo(doc["info"].to<JsonVariant>());

        JsonArray eff = doc["effects"].to<JsonArray>();
        for (uint8_t i = 0; i < EFFECT_COUNT; i++) eff.add(EFFECT_NAMES[i]);

        JsonArray pal = doc["palettes"].to<JsonArray>();
        for (uint8_t i = 0; i < PALETTE_COUNT; i++) pal.add(PALETTE_NAMES[i]);

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/json/eff", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (uint8_t i = 0; i < EFFECT_COUNT; i++) arr.add(EFFECT_NAMES[i]);
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/json/pal", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (uint8_t i = 0; i < PALETTE_COUNT; i++) arr.add(PALETTE_NAMES[i]);
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/presets.json", HTTP_GET, [](AsyncWebServerRequest *request){
        if (LittleFS.exists("/presets.json")) {
            request->send(LittleFS, "/presets.json", "application/json");
        } else {
            request->send(200, "application/json", "{}");
        }
    });

    AsyncCallbackJsonWebHandler* presetSaveHandler = new AsyncCallbackJsonWebHandler("/api/presets/save", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
        uint8_t id = jsonObj["id"] | 0;
        String name = jsonObj["name"] | "Preset";
        if (id == 0) id = PresetManager.nextFreePresetId();

        if (PresetManager.savePreset(id, name)) {
            request->send(200, "application/json", "{\"status\":\"ok\",\"id\":" + String(id) + "}");
        } else {
            request->send(500, "application/json", "{\"error\":\"save failed\"}");
        }
    });
    server.addHandler(presetSaveHandler);

    AsyncCallbackJsonWebHandler* presetApplyHandler = new AsyncCallbackJsonWebHandler("/api/presets/apply", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
        uint8_t id = jsonObj["id"] | 0;
        if (PresetManager.applyPreset(id)) {
            request->send(200, "text/plain", "OK");
        } else {
            request->send(404, "application/json", "{\"error\":\"preset not found\"}");
        }
    });
    server.addHandler(presetApplyHandler);

    AsyncCallbackJsonWebHandler* presetDeleteHandler = new AsyncCallbackJsonWebHandler("/api/presets/delete", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
        uint8_t id = jsonObj["id"] | 0;
        if (PresetManager.deletePreset(id)) {
            request->send(200, "text/plain", "OK");
        } else {
            request->send(404, "application/json", "{\"error\":\"preset not found\"}");
        }
    });
    server.addHandler(presetDeleteHandler);

    // --- Playlist API ---
    server.on("/api/playlist", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        PresetManager.getPlaylistJson(doc);
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    AsyncCallbackJsonWebHandler* playlistHandler = new AsyncCallbackJsonWebHandler("/api/playlist", [](AsyncWebServerRequest *request, JsonVariant &json) {
        PresetManager.setPlaylistFromJson(json);
        request->send(200, "text/plain", "OK");
    });
    server.addHandler(playlistHandler);

    // --- Schedules API (NTP-based time schedules) ---
    server.on("/api/schedules", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        ScheduleManager.getSchedulesJson(doc);
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    AsyncCallbackJsonWebHandler* schedulesHandler = new AsyncCallbackJsonWebHandler("/api/schedules", [](AsyncWebServerRequest *request, JsonVariant &json) {
        ScheduleManager.setSchedulesFromJson(json);
        request->send(200, "text/plain", "OK");
    });
    server.addHandler(schedulesHandler);

    server.on("/api/time", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        doc["synced"] = ScheduleManager.isTimeSynced();
        if (ScheduleManager.isTimeSynced()) {
            time_t now = time(nullptr);
            struct tm ti;
            localtime_r(&now, &ti);
            char buf[32];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &ti);
            doc["localTime"] = buf;
        }
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    server.on("/json/state", HTTP_GET, [buildState](AsyncWebServerRequest *request){
        JsonDocument doc;
        buildState(doc.to<JsonVariant>());
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/json/info", HTTP_GET, [buildInfo](AsyncWebServerRequest *request){
        JsonDocument doc;
        buildInfo(doc.to<JsonVariant>());
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    auto handlePost = [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
        
        if (!jsonObj["on"].isNull()) {
            if (jsonObj["on"].is<bool>()) {
                bool on = jsonObj["on"].as<bool>();
                for(uint8_t i=0; i<LEDManager.getNumSegments(); i++) LEDManager.setPower(i, on);
            } else if (jsonObj["on"].is<String>() && jsonObj["on"].as<String>() == "t") {
                for(uint8_t i=0; i<LEDManager.getNumSegments(); i++) LEDManager.setPower(i, !LEDManager.getPower(i));
            }
        }
        
        if (!jsonObj["bri"].isNull()) {
            uint8_t bri = jsonObj["bri"].as<uint8_t>();
            for(uint8_t i=0; i<LEDManager.getNumSegments(); i++) LEDManager.setBrightness(i, bri);
        }
        
        if (!jsonObj["seg"].isNull()) {
            JsonArray segs = jsonObj["seg"].as<JsonArray>();
            for (JsonObject seg : segs) {
                uint8_t id = seg["id"] | 0;
                if (id >= LEDManager.getNumSegments()) continue;
                
                if (!seg["on"].isNull()) {
                    LEDManager.setPower(id, seg["on"].as<bool>());
                }
                if (!seg["bri"].isNull()) {
                    LEDManager.setBrightness(id, seg["bri"].as<uint8_t>());
                }
                if (!seg["col"].isNull()) {
                    JsonArray cols = seg["col"].as<JsonArray>();
                    if (cols.size() > 0) {
                        JsonArray c0 = cols[0].as<JsonArray>();
                        if (c0.size() >= 3) {
                            uint32_t color = ((uint32_t)c0[0].as<uint8_t>() << 16) | 
                                             ((uint32_t)c0[1].as<uint8_t>() << 8) | 
                                             c0[2].as<uint8_t>();
                            // preserve white channel if present
                            uint32_t oldW = LEDManager.getColor(id) & 0xFF000000;
                            LEDManager.setColor(id, oldW | color);
                        }
                    }
                }
                if (!seg["fx"].isNull()) {
                    uint8_t fx = seg["fx"].as<uint8_t>();
                    if (fx == 0) LEDManager.setEffect(id, 0);
                    else if (fx == 2 || fx == 1) LEDManager.setEffect(id, 1);
                    else if (fx == 8 || fx == 9 || fx == 11) LEDManager.setEffect(id, 2);
                    else LEDManager.setEffect(id, 3);
                }
            }
        }
        
        MqttManager.publishState();
        request->send(200, "application/json", "{\"success\":true}");
    };

    AsyncCallbackJsonWebHandler* wledJsonHandler = new AsyncCallbackJsonWebHandler("/json", handlePost);
    server.addHandler(wledJsonHandler);
    
    AsyncCallbackJsonWebHandler* wledStateJsonHandler = new AsyncCallbackJsonWebHandler("/json/state", handlePost);
    server.addHandler(wledStateJsonHandler);
}

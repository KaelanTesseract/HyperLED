/*
 * HyperLED - Open Source LED Controller
 * 
 * Copyright (c) 2026 Dennis Guse
 * 
 * Licensed under the EUPL, Version 1.2 or � as soon they will be approved by 
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
#include <LittleFS.h>
#include "Config.h"
#include "LEDManager.h"
#include <ArduinoJson.h>
#include "UpdateManager.h"
#include "SlaveManager.h"
#include <AsyncJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include <ESPmDNS.h>
#include "MqttManager.h"

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

    server.on("/api/internet_update", HTTP_POST, [](AsyncWebServerRequest *request){
        // Trigger the update in the main loop to avoid blocking AsyncWebServer
        WebServerManager._triggerInternetUpdate = true;
        request->send(200, "application/json", "{\"status\": \"started\", \"message\": \"Update-Prozess gestartet...\"}");
    });

    server.on("/api/save_wifi", HTTP_POST, [](AsyncWebServerRequest *request){
        String ssid = "";
        String pass = "";
        if(request->hasParam("ssid", true)) ssid = request->getParam("ssid", true)->value();
        if(request->hasParam("password", true)) pass = request->getParam("password", true)->value();
        
        request->send(200, "text/plain", "Saved. Rebooting...");
        WiFiManager.saveCredentials(ssid, pass);
    });

    // --- State API ---
    server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        JsonArray segArr = doc["seg"].to<JsonArray>();
        LEDManager.getSegmentsJson(segArr);
        
        String json;
        serializeJson(doc, json);
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        request->send(response);
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
        String json = "{\"version\":\"" + String(HYPERLED_VERSION) + "\"}";
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
            
            SlaveManager.configureSlave(currentId, newId, pin, pin2, count, type, name);
            
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
    
    AsyncCallbackJsonWebHandler* slaveUpdateHandler = new AsyncCallbackJsonWebHandler("/api/slaves/update", [](AsyncWebServerRequest *request, JsonVariant &json) {
        String url = "";
        JsonObject jsonObj = json.as<JsonObject>();
        if (!jsonObj.isNull() && jsonObj["url"].is<String>()) {
            url = jsonObj["url"].as<String>();
        } else {
            url = "https://raw.githubusercontent.com/KaelanTesseract/HyperLED-Slave/main/.pio/build/esp32-c6/firmware.bin";
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
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Bad Request");
        }
    });
    server.addHandler(matrixHandler);

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
            WebServerManager._triggerRestart = true; 
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Expected JSON Array");
        }
    });
    server.addHandler(segmentsHandler);

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
        WebServerManager._triggerRestart = true;
    });



    // --- Config API ---
    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument doc;
        JsonArray pins = doc["pins"].to<JsonArray>();
        for(int i=0; i<5; i++) pins.add(LEDManager.getPin(i));
        doc["count"] = LEDManager.getCount();
        doc["type"] = LEDManager.getType();
        doc["abl_en"] = LEDManager.getAblEnabled();
        doc["abl_ma"] = LEDManager.getAblMaxmA();
        
        doc["matrix_en"] = LEDManager.isMatrix();
        doc["matrix_w"] = LEDManager.getMatrixWidth();
        doc["matrix_h"] = LEDManager.getMatrixHeight();
        // Assume Matrix Layout has a getter or we just don't return it strictly since it's write-only or we can skip it.
        // I will add getMatrixLayout to LEDManager.h
        doc["matrix_l"] = LEDManager.getMatrixLayout();
        
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
            
            LEDManager.setConfig(p0, p1, p2, p3, p4, jsonObj["count"].as<uint16_t>(), jsonObj["type"].as<uint8_t>(), ablEn, ablMa);
        }
        
        request->send(200, "text/plain", "OK");
        WebServerManager._triggerRestart = true;
    });
    server.addHandler(configHandler);

    // --- Buttons API ---
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
        WebServerManager._triggerRestart = true; // Buttons need restart to init pins
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
        WebServerManager._triggerRestart = true;
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
            WebServerManager._triggerRestart = true;
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
    
    if (_triggerInternetUpdate) {
        _triggerInternetUpdate = false;
        Serial.println("Internet Update triggered...");
        
        WiFiClientSecure client;
        client.setInsecure(); // For GitHub or self-signed
        
        HTTPClient http;
        http.begin(client, UPDATE_JSON_URL);
        
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);
            
            if (!error) {
                String latestVersion = doc["version"].as<String>();
                String binUrl = doc["bin_url"].as<String>();
                
                if (latestVersion != String(SOFTWARE_VERSION) && binUrl.length() > 0) {
                    Serial.println("New version found: " + latestVersion);
                    Serial.println("Downloading from: " + binUrl);
                    
                    t_httpUpdate_return ret = httpUpdate.update(client, binUrl);
                    switch (ret) {
                        case HTTP_UPDATE_FAILED:
                            Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
                            break;
                        case HTTP_UPDATE_NO_UPDATES:
                            Serial.println("HTTP_UPDATE_NO_UPDATES");
                            break;
                        case HTTP_UPDATE_OK:
                            Serial.println("HTTP_UPDATE_OK");
                            break;
                    }
                } else {
                    Serial.println("Already up to date.");
                }
            } else {
                Serial.println("Failed to parse update JSON");
            }
        } else {
            Serial.printf("Failed to fetch update JSON, HTTP Code: %d\n", httpCode);
        }
        http.end();
    }
}

void WebServerManagerClass::setupWLEDJsonAPI() {
    auto buildState = [](JsonDocument& doc) {
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

    auto buildInfo = [](JsonDocument& doc) {
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
        JsonDocument stateDoc;
        JsonDocument infoDoc;
        
        buildState(stateDoc);
        buildInfo(infoDoc);
        
        doc["state"] = stateDoc;
        doc["info"] = infoDoc;
        
        JsonArray eff = doc["effects"].to<JsonArray>();
        const char* effects[] = {"Solid", "Blink", "Breathe", "Wipe", "Wipe Random", "Random Colors", "Sweep", "Dynamic", "Colorloop", "Rainbow", "Scan", "Scan Dual"};
        for(int i=0; i<12; i++) eff.add(effects[i]);
        
        JsonArray pal = doc["palettes"].to<JsonArray>();
        const char* palettes[] = {"Default", "Random Cycle", "Color 1", "Colors 1&2", "Color Gradient"};
        for(int i=0; i<5; i++) pal.add(palettes[i]);
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/json/eff", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "application/json", "[\"Solid\",\"Blink\",\"Breathe\",\"Wipe\",\"Wipe Random\",\"Random Colors\",\"Sweep\",\"Dynamic\",\"Colorloop\",\"Rainbow\",\"Scan\",\"Scan Dual\"]");
    });
    
    server.on("/json/pal", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "application/json", "[\"Default\",\"Random Cycle\",\"Color 1\",\"Colors 1&2\",\"Color Gradient\"]");
    });

    server.on("/presets.json", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "application/json", "{\"0\":{}}");
    });

    server.on("/json/state", HTTP_GET, [buildState](AsyncWebServerRequest *request){
        JsonDocument doc;
        buildState(doc);
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/json/info", HTTP_GET, [buildInfo](AsyncWebServerRequest *request){
        JsonDocument doc;
        buildInfo(doc);
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

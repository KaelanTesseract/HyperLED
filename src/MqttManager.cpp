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
#include "MqttManager.h"
#include "Config.h"
#include "LEDManager.h"
#include <WiFi.h>

MqttManagerClass MqttManager;

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String t = String(topic);
    String p = "";
    for (int i = 0; i < length; i++) {
        p += (char)payload[i];
    }
    
    // Check if the topic ends with /ha/set
    if (t.endsWith("/ha/set")) {
        // Extract segment ID from something like: hyperled/AABBCCDDEEFF/seg0/ha/set
        int segIndex = t.indexOf("/seg");
        if (segIndex != -1) {
            int haIndex = t.indexOf("/ha/set");
            if (haIndex > segIndex + 4) {
                String idStr = t.substring(segIndex + 4, haIndex);
                uint8_t segId = idStr.toInt();
                
                if (segId < LEDManager.getNumSegments()) {
                    JsonDocument doc;
                    DeserializationError error = deserializeJson(doc, p);
                    if (!error) {
                        if (!doc["state"].isNull()) {
                            String state = doc["state"].as<String>();
                            if (state == "ON") LEDManager.setPower(segId, true);
                            else if (state == "OFF") LEDManager.setPower(segId, false);
                        }
                        if (!doc["brightness"].isNull()) {
                            LEDManager.setBrightness(segId, doc["brightness"].as<uint8_t>());
                        }
                        if (!doc["color"].isNull()) {
                            JsonObject col = doc["color"];
                            uint8_t r = col["r"].as<uint8_t>();
                            uint8_t g = col["g"].as<uint8_t>();
                            uint8_t b = col["b"].as<uint8_t>();
                            uint8_t w = !col["w"].isNull() ? col["w"].as<uint8_t>() : (LEDManager.getColor(segId) >> 24) & 0xFF;
                            LEDManager.setColor(segId, ((uint32_t)w << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b);
                        }
                        if (!doc["effect"].isNull()) {
                            String eff = doc["effect"].as<String>();
                            for (uint8_t e = 0; e < EFFECT_COUNT; e++) {
                                if (eff == EFFECT_NAMES[e]) {
                                    LEDManager.setEffect(segId, e);
                                    break;
                                }
                            }
                        }
                        MqttManager.triggerStatePublish();
                    }
                }
            }
        } else if (t.indexOf("/seg") == -1) {
            // Global command
            int haIndex = t.indexOf("/ha/set");
            if (haIndex != -1) {
                if (p == "ON" || p == "OFF") {
                    bool power = (p == "ON");
                    uint8_t numSegs = LEDManager.getNumSegments();
                    for (uint8_t segId = 0; segId < numSegs; segId++) {
                        LEDManager.setPower(segId, power);
                    }
                    MqttManager.triggerStatePublish();
                } else {
                    JsonDocument doc;
                    DeserializationError error = deserializeJson(doc, p);
                    if (!error) {
                    uint8_t numSegs = LEDManager.getNumSegments();
                    for (uint8_t segId = 0; segId < numSegs; segId++) {
                        if (!doc["state"].isNull()) {
                            String state = doc["state"].as<String>();
                            if (state == "ON") LEDManager.setPower(segId, true);
                            else if (state == "OFF") LEDManager.setPower(segId, false);
                        }
                        if (!doc["brightness"].isNull()) {
                            LEDManager.setBrightness(segId, doc["brightness"].as<uint8_t>());
                        }
                        if (!doc["color"].isNull()) {
                            JsonObject col = doc["color"];
                            uint8_t r = col["r"].as<uint8_t>();
                            uint8_t g = col["g"].as<uint8_t>();
                            uint8_t b = col["b"].as<uint8_t>();
                            uint8_t w = !col["w"].isNull() ? col["w"].as<uint8_t>() : (LEDManager.getColor(segId) >> 24) & 0xFF;
                            LEDManager.setColor(segId, ((uint32_t)w << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b);
                        }
                        if (!doc["effect"].isNull()) {
                            String eff = doc["effect"].as<String>();
                            for (uint8_t e = 0; e < EFFECT_COUNT; e++) {
                                if (eff == EFFECT_NAMES[e]) {
                                    LEDManager.setEffect(segId, e);
                                    break;
                                }
                            }
                        }
                    }
                    }
                    MqttManager.triggerStatePublish();
                }
            }
        }
    }
}

void MqttManagerClass::begin() {
    loadConfig();
    _macAddress = WiFi.macAddress();
    _macAddress.replace(":", "");
    
    if (!_enabled || _server.isEmpty()) return;
    
    _client.setClient(_wifiClient);
    
    IPAddress ip;
    if (ip.fromString(_server)) {
        _client.setServer(ip, _port);
    } else {
        _client.setServer(_server.c_str(), _port);
    }
    
    _client.setBufferSize(2048);
    _client.setCallback(mqttCallback);
}

void MqttManagerClass::loadConfig() {
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, true);
    _enabled = prefs.getBool(PREF_MQTT_ENABLE, false);
    _server = prefs.getString(PREF_MQTT_SERVER, "");
    _port = prefs.getUShort(PREF_MQTT_PORT, 1883);
    _user = prefs.getString(PREF_MQTT_USER, "");
    _pass = prefs.getString(PREF_MQTT_PASS, "");
    _topic = prefs.getString(PREF_MQTT_TOPIC, "hyperled/device");
    prefs.end();
}

void MqttManagerClass::reconnect() {
    if (_client.connected() || !_enabled || _server.isEmpty() || WiFi.status() != WL_CONNECTED) return;
    
    unsigned long now = millis();
    if (now - _lastReconnectAttempt > 5000) {
        _lastReconnectAttempt = now;
        String clientId = "HyperLED-";
        clientId += String(random(0xffff), HEX);
        
        bool success;
        if (_user.isEmpty()) {
            success = _client.connect(clientId.c_str());
        } else {
            success = _client.connect(clientId.c_str(), _user.c_str(), _pass.c_str());
        }
        
        if (success) {
            // Subscribe to all segment topics
            uint8_t numSegs = LEDManager.getNumSegments();
            for (uint8_t i = 0; i < numSegs; i++) {
                String haSetTopic = "hyperled/" + _macAddress + "/seg" + String(i) + "/ha/set";
                _client.subscribe(haSetTopic.c_str());
            }
            
            // Subscribe to global set topic
            String globalSetTopic = "hyperled/" + _macAddress + "/ha/set";
            _client.subscribe(globalSetTopic.c_str());
            
            publishHomeAssistantDiscovery();
            publishState();
        }
    }
}

void MqttManagerClass::loop() {
    if (!_enabled || _server.isEmpty()) return;
    
    if (!_client.connected()) {
        reconnect();
    } else {
        _client.loop();
        if (_needsStatePublish && millis() - _lastStatePublish > 100) {
            _needsStatePublish = false;
            _lastStatePublish = millis();
            publishState();
        }
    }
}

void MqttManagerClass::triggerStatePublish() {
    _needsStatePublish = true;
}

void MqttManagerClass::publishState() {
    if (!_client.connected()) return;
    
    uint8_t numSegs = LEDManager.getNumSegments();
    for (uint8_t i = 0; i < numSegs; i++) {
        JsonDocument haDoc;
        haDoc["state"] = LEDManager.getPower(i) ? "ON" : "OFF";
        haDoc["brightness"] = LEDManager.getBrightness(i);
        
        JsonObject color = haDoc["color"].to<JsonObject>();
        uint32_t haColor = LEDManager.getColor(i);
        color["r"] = (haColor >> 16) & 0xFF;
        color["g"] = (haColor >> 8) & 0xFF;
        color["b"] = haColor & 0xFF;
        if (LEDManager.hasWhiteChannel()) {
            color["w"] = (haColor >> 24) & 0xFF;
            haDoc["color_mode"] = "rgbw";
        } else {
            haDoc["color_mode"] = "rgb";
        }
        
        uint8_t eff = LEDManager.getEffect(i);
        if (eff < EFFECT_COUNT) {
            haDoc["effect"] = EFFECT_NAMES[eff];
        } else {
            haDoc["effect"] = "Solid";
        }
        
        String haPayload;
        serializeJson(haDoc, haPayload);
        String haStateTopic = "hyperled/" + _macAddress + "/seg" + String(i) + "/ha/state";
        _client.publish(haStateTopic.c_str(), haPayload.c_str(), true);
    }
    
    // Publish global state
    JsonDocument globalHaDoc;
    bool anyOn = false;
    for (uint8_t i = 0; i < numSegs; i++) {
        if (LEDManager.getPower(i)) {
            anyOn = true;
            break;
        }
    }
    
    globalHaDoc["state"] = anyOn ? "ON" : "OFF";
    
    String globalStatePayload;
    serializeJson(globalHaDoc, globalStatePayload);
    String globalStateTopic = "hyperled/" + _macAddress + "/ha/state";
    _client.publish(globalStateTopic.c_str(), globalStatePayload.c_str(), true);
}

void MqttManagerClass::publishHomeAssistantDiscovery() {
    if (!_client.connected()) return;
    
    // Publish Global Master Switch
    JsonDocument globalDoc;
    String globalHaTopic = "homeassistant/switch/hyperled_" + _macAddress + "/config";
    
    // Cleanup old light entity
    String oldGlobalHaTopic = "homeassistant/light/hyperled_" + _macAddress + "/config";
    _client.publish(oldGlobalHaTopic.c_str(), "", true);
    
    globalDoc["~"] = "hyperled/" + _macAddress;
    globalDoc["name"] = "HyperLED Master";
    globalDoc["unique_id"] = "hyperled_" + _macAddress + "_master";
    globalDoc["cmd_t"] = "~/ha/set";
    globalDoc["stat_t"] = "~/ha/state";
    globalDoc["val_tpl"] = "{{ value_json.state }}";
    
    JsonObject globalDev = globalDoc["device"].to<JsonObject>();
    JsonArray globalIds = globalDev["identifiers"].to<JsonArray>();
    globalIds.add("hyperled_" + _macAddress);
    globalDev["name"] = "HyperLED";
    globalDev["manufacturer"] = "HyperLED";
    globalDev["model"] = "ESP32-C6";
    globalDev["sw_version"] = "1.0.0";
    
    String globalPayload;
    serializeJson(globalDoc, globalPayload);
    _client.publish(globalHaTopic.c_str(), globalPayload.c_str(), true);
    
    
    uint8_t numSegs = LEDManager.getNumSegments();
    for (uint8_t i = 0; i < numSegs; i++) {
        JsonDocument doc;
        String haTopic = "homeassistant/light/hyperled_" + _macAddress + "_seg" + String(i) + "/config";
        
        doc["~"] = "hyperled/" + _macAddress + "/seg" + String(i);
        const Segment* seg = LEDManager.getSegment(i);
        doc["name"] = seg ? seg->name : ("Segment " + String(i));
        doc["unique_id"] = "hyperled_" + _macAddress + "_seg" + String(i);
        doc["cmd_t"] = "~/ha/set";
        doc["stat_t"] = "~/ha/state";
        doc["schema"] = "json";
        doc["brightness"] = true;
        doc["color_mode"] = true;
        
        JsonArray modes = doc["supported_color_modes"].to<JsonArray>();
        if (LEDManager.hasWhiteChannel()) {
            modes.add("rgbw");
        } else {
            modes.add("rgb");
        }
        
        doc["effect"] = true;
        JsonArray effects = doc["effect_list"].to<JsonArray>();
        for (uint8_t e = 0; e < EFFECT_COUNT; e++) {
            effects.add(EFFECT_NAMES[e]);
        }
        
        JsonObject dev = doc["device"].to<JsonObject>();
        JsonArray ids = dev["identifiers"].to<JsonArray>();
        ids.add("hyperled_" + _macAddress);
        dev["name"] = "HyperLED";
        dev["manufacturer"] = "HyperLED";
        dev["model"] = "ESP32-C6";
        dev["sw_version"] = "1.0.0";
        
        String payload;
        serializeJson(doc, payload);
        
        _client.publish(haTopic.c_str(), payload.c_str(), true);
    }
    
    // Clean up potentially deleted segments (up to 16 to be safe)
    for (uint8_t i = numSegs; i < 16; i++) {
        String haTopic = "homeassistant/light/hyperled_" + _macAddress + "_seg" + String(i) + "/config";
        _client.publish(haTopic.c_str(), "", true);
    }
}

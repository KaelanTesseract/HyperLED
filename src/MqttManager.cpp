/*
 * HyperLED - Open Source LED Controller
 * 
 * Copyright (c) 2026 Dennis Guse
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
                            state.toUpperCase();
                            if (state == "ON" || state == "TRUE" || state == "1") LEDManager.setPower(segId, true);
                            else if (state == "OFF" || state == "FALSE" || state == "0") LEDManager.setPower(segId, false);
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
                        if (!doc["color_temp"].isNull()) {
                            uint16_t mired = doc["color_temp"].as<uint16_t>();
                            if (mired > 500) mired = 500;
                            if (mired < 153) mired = 153;
                            uint8_t cct = (500 - mired) * 255 / 347;
                            LEDManager.setCct(segId, cct);
                        }
                        if (!doc["color_mode"].isNull()) {
                            String mode = doc["color_mode"].as<String>();
                            bool wo = (mode == "color_temp");
                            LEDManager.setWhiteOnly(segId, wo);
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
                String u = p;
                u.toUpperCase();
                if (u == "ON" || u == "TRUE" || u == "1" || u == "OFF" || u == "FALSE" || u == "0") {
                    bool power = (u == "ON" || u == "TRUE" || u == "1");
                    uint8_t numSegs = LEDManager.getNumSegments();
                    for (uint8_t segId = 0; segId < numSegs; segId++) {
                        LEDManager.setPower(segId, power);
                    }
                    MqttManager.triggerStatePublish();
                } else {
                    JsonDocument doc;
                    DeserializationError error = deserializeJson(doc, p);
                    if (!error) {
                        if (!doc["state"].isNull()) {
                            String state = doc["state"].as<String>();
                            state.toUpperCase();
                            if (state == "ON" || state == "TRUE" || state == "1") {
                                uint8_t numSegs = LEDManager.getNumSegments();
                                for (uint8_t segId = 0; segId < numSegs; segId++) LEDManager.setPower(segId, true);
                            } else if (state == "OFF" || state == "FALSE" || state == "0") {
                                uint8_t numSegs = LEDManager.getNumSegments();
                                for (uint8_t segId = 0; segId < numSegs; segId++) LEDManager.setPower(segId, false);
                            }
                        }
                        if (!doc["brightness"].isNull()) {
                            uint8_t numSegs = LEDManager.getNumSegments();
                            for (uint8_t segId = 0; segId < numSegs; segId++) LEDManager.setBrightness(segId, doc["brightness"].as<uint8_t>());
                        }
                        if (!doc["color"].isNull()) {
                            JsonObject col = doc["color"];
                            uint8_t r = col["r"].as<uint8_t>();
                            uint8_t g = col["g"].as<uint8_t>();
                            uint8_t b = col["b"].as<uint8_t>();
                            uint8_t numSegs = LEDManager.getNumSegments();
                            for (uint8_t segId = 0; segId < numSegs; segId++) {
                                uint8_t w = !col["w"].isNull() ? col["w"].as<uint8_t>() : (LEDManager.getColor(segId) >> 24) & 0xFF;
                                LEDManager.setColor(segId, ((uint32_t)w << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b);
                            }
                        }
                        if (!doc["color_temp"].isNull()) {
                            uint16_t mired = doc["color_temp"].as<uint16_t>();
                            if (mired > 500) mired = 500;
                            if (mired < 153) mired = 153;
                            uint8_t cct = (500 - mired) * 255 / 347;
                            uint8_t numSegs = LEDManager.getNumSegments();
                            for (uint8_t segId = 0; segId < numSegs; segId++) {
                                LEDManager.setCct(segId, cct);
                            }
                        }
                        if (!doc["color_mode"].isNull()) {
                            String mode = doc["color_mode"].as<String>();
                            bool wo = (mode == "color_temp");
                            uint8_t numSegs = LEDManager.getNumSegments();
                            for (uint8_t segId = 0; segId < numSegs; segId++) {
                                LEDManager.setWhiteOnly(segId, wo);
                            }
                        }
                        if (!doc["effect"].isNull()) {
                            String eff = doc["effect"].as<String>();
                            for (uint8_t e = 0; e < EFFECT_COUNT; e++) {
                                if (eff == EFFECT_NAMES[e]) {
                                    uint8_t numSegs = LEDManager.getNumSegments();
                                    for (uint8_t segId = 0; segId < numSegs; segId++) LEDManager.setEffect(segId, e);
                                    break;
                                }
                            }
                        }
                        MqttManager.triggerStatePublish();
                    }
                }
            }
        }
    }
}

void MqttManagerClass::begin() {
    loadConfig();
    _macAddress = WiFi.macAddress();
    _macAddress.replace(":", "");
    _client.setClient(_wifiClient);
    _wifiClient.setTimeout(2); // Short timeout to avoid blocking main loop
    _client.setBufferSize(2048); // Large enough for HA discovery payloads
    _client.setServer(_server.c_str(), _port);
    _client.setCallback(mqttCallback);
}

void MqttManagerClass::loop() {
    if (!_enabled) return;
    
    if (!_client.connected()) {
        reconnect();
    }
    
    if (_client.connected()) {
        _client.loop();
        
        if (_needsStatePublish && millis() - _lastStatePublish > 200) {
            publishState();
            _needsStatePublish = false;
        }
    }
}

void MqttManagerClass::reconnect() {
    if (_client.connected() || !_enabled || _server.isEmpty() || WiFi.status() != WL_CONNECTED) return;
    
    unsigned long now = millis();
    if (now - _lastReconnectAttempt > 5000) {
        _lastReconnectAttempt = now;
        
        String clientId = "HyperLED-" + _macAddress;
        bool success = false;
        
        if (_user.isEmpty()) {
            success = _client.connect(clientId.c_str());
        } else {
            success = _client.connect(clientId.c_str(), _user.c_str(), _pass.c_str());
        }
        
        if (success) {
            uint8_t numSegs = LEDManager.getNumSegments();
            for (uint8_t i = 0; i < numSegs; i++) {
                String haSetTopic = "hyperled/" + _macAddress + "/seg" + String(i) + "/ha/set";
                _client.subscribe(haSetTopic.c_str());
            }
            
            String globalSetTopic = "hyperled/" + _macAddress + "/ha/set";
            _client.subscribe(globalSetTopic.c_str());
            
            publishHomeAssistantDiscovery();
            publishState();
        }
    }
}

void MqttManagerClass::publishState() {
    if (!_client.connected() || !_enabled) return;
    
    uint8_t numSegs = LEDManager.getNumSegments();
    
    // Publish global state
    if (numSegs > 0) {
        String globalTopic = "hyperled/" + _macAddress + "/ha/state";
        JsonDocument globalDoc;
        bool anyOn = false;
        uint8_t firstBri = 0;
        uint32_t firstColor = 0;
        uint8_t firstEffect = 0;
        bool firstWhiteOnly = false;
        uint8_t firstCct = 128;
        
        for (uint8_t i = 0; i < numSegs; i++) {
            const Segment* seg = LEDManager.getSegment(i);
            if (seg && !seg->isSlave) {
                if (seg->isOn) anyOn = true;
                if (i == 0) {
                    firstBri = seg->brightness;
                    firstColor = seg->color;
                    firstEffect = seg->effect;
                    firstWhiteOnly = seg->whiteOnly;
                    firstCct = seg->cct;
                }
            }
        }
        
        globalDoc["state"] = anyOn ? "ON" : "OFF";
        globalDoc["brightness"] = firstBri;
        globalDoc["color_mode"] = firstWhiteOnly ? "color_temp" : "rgbw";
        globalDoc["color_temp"] = 500 - (firstCct * 347 / 255);
        
        JsonObject colorObj = globalDoc["color"].to<JsonObject>();
        colorObj["r"] = (firstColor >> 16) & 0xFF;
        colorObj["g"] = (firstColor >> 8) & 0xFF;
        colorObj["b"] = firstColor & 0xFF;
        colorObj["w"] = (firstColor >> 24) & 0xFF;
        
        if (firstEffect < EFFECT_COUNT) {
            globalDoc["effect"] = EFFECT_NAMES[firstEffect];
        }
        
        String payload;
        serializeJson(globalDoc, payload);
        _client.publish(globalTopic.c_str(), payload.c_str(), true);
    }
    
    // Publish individual segments
    for (uint8_t i = 0; i < numSegs; i++) {
        const Segment* seg = LEDManager.getSegment(i);
        if (seg && !seg->isSlave) {
            String topic = "hyperled/" + _macAddress + "/seg" + String(i) + "/ha/state";
            JsonDocument doc;
            doc["state"] = seg->isOn ? "ON" : "OFF";
            doc["brightness"] = seg->brightness;
            doc["color_mode"] = seg->whiteOnly ? "color_temp" : "rgbw";
            doc["color_temp"] = 500 - (seg->cct * 347 / 255);
            
            JsonObject colorObj = doc["color"].to<JsonObject>();
            colorObj["r"] = (seg->color >> 16) & 0xFF;
            colorObj["g"] = (seg->color >> 8) & 0xFF;
            colorObj["b"] = seg->color & 0xFF;
            colorObj["w"] = (seg->color >> 24) & 0xFF;
            
            if (seg->effect < EFFECT_COUNT) {
                doc["effect"] = EFFECT_NAMES[seg->effect];
            }
            
            String payload;
            serializeJson(doc, payload);
            _client.publish(topic.c_str(), payload.c_str(), true);
        }
    }
}

void MqttManagerClass::triggerStatePublish() {
    _needsStatePublish = true;
    _lastStatePublish = millis();
}

void MqttManagerClass::loadConfig() {
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, true);
    _enabled = prefs.getBool(PREF_MQTT_ENABLE, false);
    _server = prefs.getString(PREF_MQTT_SERVER, "");
    _port = prefs.getUShort(PREF_MQTT_PORT, 1883);
    _user = prefs.getString(PREF_MQTT_USER, "");
    _pass = prefs.getString(PREF_MQTT_PASS, "");
    prefs.end();
}

void MqttManagerClass::publishHomeAssistantDiscovery() {
    if (!_client.connected() || !_enabled) return;
    
    uint8_t numSegs = LEDManager.getNumSegments();
    
    // Global Master Light
    if (numSegs > 0) {
        String globalTopic = "homeassistant/light/hyperled_" + _macAddress + "/config";
        JsonDocument globalDoc;
        globalDoc["name"] = "Master";
        globalDoc["~"] = "hyperled/" + _macAddress;
        globalDoc["stat_t"] = "~/ha/state";
        globalDoc["cmd_t"] = "~/ha/set";
        globalDoc["schema"] = "json";
        globalDoc["brightness"] = true;
        globalDoc["color_mode"] = true;
        globalDoc["supported_color_modes"][0] = "rgbw";
        globalDoc["supported_color_modes"][1] = "color_temp";
        globalDoc["max_mireds"] = 500;
        globalDoc["min_mireds"] = 153;
        globalDoc["effect"] = true;
        
        JsonArray fxList = globalDoc["effect_list"].to<JsonArray>();
        for (uint8_t e = 0; e < EFFECT_COUNT; e++) fxList.add(EFFECT_NAMES[e]);
        
        globalDoc["val_tpl"] = "{{ value_json.state }}";
        globalDoc["unique_id"] = "hyperled_" + _macAddress;
        
        JsonObject dev = globalDoc["device"].to<JsonObject>();
        dev["identifiers"][0] = _macAddress;
        dev["name"] = "HyperLED";
        dev["manufacturer"] = "HyperLED";
        dev["model"] = "ESP32-S3 Controller";
        
        String globalPayload;
        serializeJson(globalDoc, globalPayload);
        _client.publish(globalTopic.c_str(), globalPayload.c_str(), true);
    }
    
    // Individual Segments
    for (uint8_t i = 0; i < numSegs; i++) {
        const Segment* seg = LEDManager.getSegment(i);
        if (seg && !seg->isSlave) {
            String topic = "homeassistant/light/hyperled_" + _macAddress + "_seg" + String(i) + "/config";
            JsonDocument doc;
            doc["name"] = seg->name;
            doc["~"] = "hyperled/" + _macAddress + "/seg" + String(i);
            doc["stat_t"] = "~/ha/state";
            doc["cmd_t"] = "~/ha/set";
            doc["schema"] = "json";
            doc["brightness"] = true;
            doc["color_mode"] = true;
            doc["supported_color_modes"][0] = "rgbw";
            doc["supported_color_modes"][1] = "color_temp";
            doc["max_mireds"] = 500;
            doc["min_mireds"] = 153;
            doc["effect"] = true;
            
            JsonArray fxList = doc["effect_list"].to<JsonArray>();
            for (uint8_t e = 0; e < EFFECT_COUNT; e++) fxList.add(EFFECT_NAMES[e]);
            
            doc["val_tpl"] = "{{ value_json.state }}";
            doc["unique_id"] = "hyperled_" + _macAddress + "_seg" + String(i);
            
            JsonObject dev = doc["device"].to<JsonObject>();
            dev["identifiers"][0] = _macAddress;
            dev["name"] = "HyperLED";
            dev["manufacturer"] = "HyperLED";
            dev["model"] = "ESP32-S3 Controller";
            
            String payload;
            serializeJson(doc, payload);
            _client.publish(topic.c_str(), payload.c_str(), true);
        }
    }
}

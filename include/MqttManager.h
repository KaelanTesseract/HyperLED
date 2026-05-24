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
#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>

class MqttManagerClass {
public:
    void begin();
    void loop();
    void publishState();
    void triggerStatePublish();
    void publishHomeAssistantDiscovery();
    
    bool isEnabled() const { return _enabled; }
    bool isConnected() { return _client.connected(); }

private:
    void reconnect();
    void loadConfig();
    
    WiFiClient _wifiClient;
    PubSubClient _client;
    
    bool _enabled;
    String _server;
    uint16_t _port;
    String _user;
    String _pass;
    String _topic;
    String _macAddress;
    unsigned long _lastReconnectAttempt = 0;
    bool _needsStatePublish = false;
    unsigned long _lastStatePublish = 0;
};

extern MqttManagerClass MqttManager;

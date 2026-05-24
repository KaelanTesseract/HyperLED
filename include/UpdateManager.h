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
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <ArduinoJson.h>

class UpdateManagerClass {
public:
    void begin();
    void loop();
    
    void startOnlineUpdate(String version);
    uint8_t getProgress();
    String getStatus();

private:
    bool _updatePending = false;
    String _targetVersion;
    
    uint8_t _progress = 0;
    String _status = "idle";
    
    void performUpdate();
    bool downloadAndFlash(String url, int command, int startProgress, int endProgress);
};

extern UpdateManagerClass UpdateManager;

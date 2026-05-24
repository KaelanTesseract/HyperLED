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
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <LittleFS.h>
#include "WiFiManager.h"
#include "Config.h"

class WebServerManagerClass {
public:
    void begin();
    void loop();
    
private:
    bool _triggerInternetUpdate = false;
    bool _triggerRestart = false;
    void setupRoutes();
    void setupCaptivePortal();
    void setupOTA();
    void setupWLEDJsonAPI();
};

extern WebServerManagerClass WebServerManager;

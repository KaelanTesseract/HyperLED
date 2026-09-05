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
#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "Config.h"

// Result of a credential test started from the AP-mode setup screen.
enum WifiSetupState : uint8_t {
    WIFI_SETUP_IDLE = 0,
    WIFI_SETUP_CONNECTING,
    WIFI_SETUP_SUCCESS,
    WIFI_SETUP_FAILED
};

class WiFiManagerClass {
public:
    void begin();
    void loop();

    bool isAPMode() const { return _isAPMode; }
    String getSSID() const { return _ssid; }

    // Scan networks asynchronously
    void startScan();
    String getScanResultsJSON();

    // Save new credentials
    void saveCredentials(const String& ssid, const String& password);

    // AP-mode setup flow: try the credentials while the AP stays up, so the WebUI can
    // report the new IP (or a failure) to a client that is still connected to the AP.
    // The actual connection attempt runs in loop() - starting it from an async web
    // handler would block the web server for the whole timeout.
    void startSetupConnect(const String& ssid, const String& password);
    WifiSetupState getSetupState() const { return _setupState; }
    String getSetupIp() const { return _setupIp; }
    void requestRestart(uint32_t delayMs = 500);

private:
    bool _isAPMode = false;
    bool _triggerScan = false;
    DNSServer _dnsServer;
    String _ssid;
    String _password;

    WifiSetupState _setupState = WIFI_SETUP_IDLE;
    bool _triggerSetupConnect = false;
    String _setupSsid;
    String _setupPassword;
    String _setupIp;
    unsigned long _restartAt = 0;

    void connectSTA();
    void startAP();
    void runSetupConnect();
};

extern WiFiManagerClass WiFiManager;

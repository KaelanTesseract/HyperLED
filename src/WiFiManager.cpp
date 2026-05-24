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
#include "WiFiManager.h"
#include <ESPmDNS.h>

WiFiManagerClass WiFiManager;

void WiFiManagerClass::begin() {
    Preferences preferences;
    preferences.begin(PREF_NAMESPACE, true);
    _ssid = preferences.getString(PREF_WIFI_SSID, "");
    _password = preferences.getString(PREF_WIFI_PASS, "");
    preferences.end();

    // Connect or start AP
    if (_ssid.length() > 0) {
        connectSTA();
    } else {
        startAP();
    }
}

void WiFiManagerClass::connectSTA() {
    Serial.println("Attempting to connect to STA: " + _ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(_ssid.c_str(), _password.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        _isAPMode = false;
        Serial.println("Connected! IP: " + WiFi.localIP().toString());
        
        if (!MDNS.begin("hyperled")) {
            Serial.println("Error setting up MDNS responder!");
        } else {
            MDNS.addService("http", "tcp", 80);
        }
    } else {
        Serial.println("Failed to connect. Starting AP...");
        startAP();
    }
}

void WiFiManagerClass::startAP() {
    _isAPMode = true;
    WiFi.mode(WIFI_AP_STA);
    WiFi.disconnect(); // Required to prevent crash when scanning if STA is unconfigured
    WiFi.softAP(DEFAULT_AP_SSID);
    Serial.println("AP Mode started. SSID: " + String(DEFAULT_AP_SSID));
    Serial.println("IP: " + WiFi.softAPIP().toString());
    
    if (!MDNS.begin("hyperled")) {
        Serial.println("Error setting up MDNS responder!");
    } else {
        MDNS.addService("http", "tcp", 80);
    }
    
    // Start DNS Server for captive portal
    _dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    _dnsServer.start(53, "*", WiFi.softAPIP());
}

void WiFiManagerClass::loop() {
    if (_isAPMode) {
        _dnsServer.processNextRequest();
    }
    if (_triggerScan) {
        _triggerScan = false;
        Serial.println("Starting Wi-Fi Scan...");
        WiFi.scanDelete(); // Clean up any old scan in memory
        delay(10); // Small delay to let radio settle
        WiFi.scanNetworks(true, true); // true = async, true = show hidden
        Serial.println("Scan triggered async");
    }
}

void WiFiManagerClass::startScan() {
    _triggerScan = true;
}

String WiFiManagerClass::getScanResultsJSON() {
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING || n == WIFI_SCAN_FAILED) {
        return "[]";
    }

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    
    // Limit to 15 networks to prevent Out of Memory / heap fragmentation
    int limit = (n > 15) ? 15 : n;
    
    for (int i = 0; i < limit; ++i) {
        JsonObject net = arr.add<JsonObject>();
        net["ssid"] = WiFi.SSID(i);
        net["rssi"] = WiFi.RSSI(i);
        net["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
    
    String json;
    serializeJson(doc, json);
    WiFi.scanDelete();
    return json;
}

void WiFiManagerClass::saveCredentials(const String& ssid, const String& password) {
    Preferences preferences;
    preferences.begin(PREF_NAMESPACE, false);
    preferences.putString(PREF_WIFI_SSID, ssid);
    preferences.putString(PREF_WIFI_PASS, password);
    preferences.end();
    
    Serial.println("Saved new WiFi credentials. Rebooting...");
    delay(1000);
    ESP.restart();
}

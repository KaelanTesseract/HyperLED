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
#include <NetBIOS.h>
#include "ScheduleManager.h"

WiFiManagerClass WiFiManager;

// Static, because the SDK calls it from the Wi-Fi task. It only records - reconnecting is left
// to superviseLink() on the main loop, where blocking is safe and the retry can be paced.
void WiFiManagerClass::onWiFiEvent(arduino_event_id_t event, arduino_event_info_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            WiFiManager._disconnectCount++;
            WiFiManager._lastDisconnectReason = info.wifi_sta_disconnected.reason;
            WiFiManager._lastDisconnectAt = millis();
            if (WiFiManager._offlineSince == 0) WiFiManager._offlineSince = millis();
            Serial.printf("WiFi: link lost (reason %u, %lu so far)\n",
                          (unsigned)info.wifi_sta_disconnected.reason,
                          (unsigned long)WiFiManager._disconnectCount);
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            WiFiManager._offlineSince = 0;
            Serial.printf("WiFi: back on %s, channel %d, RSSI %d\n",
                          WiFi.localIP().toString().c_str(), WiFi.channel(), (int)WiFi.RSSI());
            break;
        default:
            break;
    }
}

unsigned long WiFiManagerClass::getOfflineMs() const {
    if (_offlineSince == 0) return 0;
    return millis() - _offlineSince;
}

void WiFiManagerClass::begin() {
    WiFi.onEvent(WiFiManagerClass::onWiFiEvent);
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
        _wasConnected = true;
        _offlineSince = 0;
        Serial.println("Connected! IP: " + WiFi.localIP().toString());
        
        if (!MDNS.begin("hyperled")) {
            Serial.println("Error setting up MDNS responder!");
        } else {
            MDNS.addService("http", "tcp", 80);
        }
        // NBNS lets Windows resolve http://hyperled/ natively (no Bonjour needed),
        // complementing mDNS (hyperled.local), which Windows browsers often can't resolve.
        NBNS.begin("hyperled");

        ScheduleManager.applyTimezone();
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
    NBNS.begin("hyperled");

    // Start DNS Server for captive portal
    _dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    _dnsServer.start(53, "*", WiFi.softAPIP());
}

void WiFiManagerClass::loop() {
    superviseLink();

    if (_isAPMode) {
        _dnsServer.processNextRequest();
    }
    if (_restartAt > 0 && millis() >= _restartAt) {
        Serial.println("Restarting to apply the new Wi-Fi configuration...");
        ESP.restart();
    }
    if (_triggerSetupConnect) {
        _triggerSetupConnect = false;
        runSetupConnect();
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


// --- Reachability probe -------------------------------------------------------------------
// The callbacks run on the ping session's own task and only tally the result; everything that
// acts on it happens in superviseLink() on the main loop.

static void hyperledPingEnd(esp_ping_handle_t hdl, void* args) {
    uint32_t received = 0;
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    WiFiManager.probeFinished(received);
}

void WiFiManagerClass::probeFinished(uint32_t received) {
    _probeRunning = false;
    if (received > 0) {
        _lastProbeOk = millis();
        _probeFailures = 0;
        return;
    }
    _probeFailures++;
    Serial.printf("WiFi: gateway did not answer (%lu in a row)\n",
                  (unsigned long)_probeFailures);
}

void WiFiManagerClass::startLinkProbe() {
    if (_probeRunning) return;

    IPAddress gw = WiFi.gatewayIP();
    if ((uint32_t)gw == 0) return;

    // One session, reused. Creating and deleting one per probe leaks sockets over days.
    if (_pingHandle == nullptr) {
        ip_addr_t target;
        memset(&target, 0, sizeof(target));
        target.type = IPADDR_TYPE_V4;
        target.u_addr.ip4.addr = (uint32_t)gw;

        esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
        cfg.target_addr = target;
        cfg.count = 2;
        cfg.timeout_ms = 2000;
        cfg.interval_ms = 500;
        cfg.task_stack_size = 3072;
        cfg.task_prio = 1;

        esp_ping_callbacks_t cbs;
        memset(&cbs, 0, sizeof(cbs));
        cbs.on_ping_end = hyperledPingEnd;

        if (esp_ping_new_session(&cfg, &cbs, &_pingHandle) != ESP_OK) {
            _pingHandle = nullptr;
            return;
        }
    }

    _probeRunning = true;
    _lastProbeStart = millis();
    if (esp_ping_start(_pingHandle) != ESP_OK) _probeRunning = false;
}

void WiFiManagerClass::superviseLink() {
    // Only meaningful for a station with credentials. In AP mode there is nothing to supervise,
    // and during the setup flow a reconnect here would fight with the attempt being tested.
    if (_isAPMode || _ssid.isEmpty() || _triggerSetupConnect ||
        _setupState == WIFI_SETUP_CONNECTING) return;

    unsigned long now = millis();
    if (now - _lastLinkCheck < WIFI_CHECK_INTERVAL_MS) return;
    _lastLinkCheck = now;

    if (WiFi.status() == WL_CONNECTED) {
        if (!_wasConnected) {
            _wasConnected = true;
            _reconnectCount++;
            _probeFailures = 0;
            _lastProbeOk = now;
            // mDNS and NetBIOS bind to the address the device had when they started, so after a
            // new lease they answer for one that no longer exists - http://hyperled/ then leads
            // nowhere even though the controller is back.
            MDNS.end();
            if (MDNS.begin("hyperled")) MDNS.addService("http", "tcp", 80);
            NBNS.begin("hyperled");
        }

        // Associated as far as the driver is concerned - now find out whether that is true.
        if (now - _lastProbeStart >= LINK_PROBE_INTERVAL_MS) startLinkProbe();

        if (_probeFailures >= LINK_PROBE_FAILURES_BEFORE_RECONNECT) {
            if (_offlineSince == 0) _offlineSince = now;

            if (getOfflineMs() >= LINK_DEAD_RESTART_MS) {
                Serial.println("WiFi: unreachable for five minutes despite being associated, restarting");
                Serial.flush();
                ESP.restart();
            }

            if (now - _lastReconnectAttempt >= WIFI_RETRY_INTERVAL_MS) {
                _lastReconnectAttempt = now;
                _forcedReconnects++;
                Serial.println("WiFi: associated but unreachable, forcing re-association");
                // Full teardown: a plain begin() on a zombie association is accepted by the
                // driver and changes nothing, because as far as it is concerned there is
                // nothing wrong.
                WiFi.disconnect(true, false);
                WiFi.begin(_ssid.c_str(), _password.c_str());
            }
            return;
        }

        _offlineSince = 0;
        return;
    }

    if (_wasConnected) {
        _wasConnected = false;
        if (_offlineSince == 0) _offlineSince = now;
    }

    // Give the SDK's own retry a chance first - it usually wins - and only then force the issue.
    // WiFi.begin() while it is already trying would restart that attempt each time and could keep
    // the link down indefinitely, so this is deliberately slow.
    if (now - _lastReconnectAttempt < WIFI_RETRY_INTERVAL_MS) return;
    _lastReconnectAttempt = now;

    Serial.printf("WiFi: still down after %lus, reconnecting\n", getOfflineMs() / 1000);
    WiFi.disconnect();
    WiFi.begin(_ssid.c_str(), _password.c_str());
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

void WiFiManagerClass::startSetupConnect(const String& ssid, const String& password) {
    _setupSsid = ssid;
    _setupPassword = password;
    _setupIp = "";
    _setupState = WIFI_SETUP_CONNECTING;
    _triggerSetupConnect = true;
}

void WiFiManagerClass::requestRestart(uint32_t delayMs) {
    _restartAt = millis() + delayMs;
}

// Runs from loop(), never from a web handler: the connection attempt below blocks for up
// to ~10s, which would stall the async web server (and with it the very request that is
// polling for the result). The SoftAP stays up throughout because startAP() puts the radio
// in WIFI_AP_STA - that is what lets the client read the new IP before anything reboots.
void WiFiManagerClass::runSetupConnect() {
    Serial.println("Setup: testing Wi-Fi credentials for SSID '" + _setupSsid + "'");
    WiFi.begin(_setupSsid.c_str(), _setupPassword.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        _setupIp = WiFi.localIP().toString();
        _setupState = WIFI_SETUP_SUCCESS;
        Serial.println("Setup: connected, IP is " + _setupIp);

        // Only persist once the credentials are proven to work, so a typo can be corrected
        // without the device rebooting into a broken configuration.
        Preferences preferences;
        preferences.begin(PREF_NAMESPACE, false);
        preferences.putString(PREF_WIFI_SSID, _setupSsid);
        preferences.putString(PREF_WIFI_PASS, _setupPassword);
        preferences.end();

        _ssid = _setupSsid;
        _password = _setupPassword;

        // Fallback in case the user never confirms (closed the tab, walked away): the setup
        // AP is open/passwordless, so it must not stay up indefinitely. 5 minutes is enough
        // to read and note the address down.
        requestRestart(5 * 60 * 1000);
    } else {
        _setupState = WIFI_SETUP_FAILED;
        Serial.println("Setup: could not connect with the given credentials");
        // Drop the half-open attempt so the AP keeps working and the user can retry.
        WiFi.disconnect();
    }
}

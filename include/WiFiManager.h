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
#include "ping/ping_sock.h"

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

    // Link history, so an outage leaves a trace instead of having to be caught in the act.
    // Without these a dropped Wi-Fi link and a silent reboot look exactly alike from outside:
    // both show up only as "not reachable for a while, then back".
    uint32_t getDisconnectCount() const { return _disconnectCount; }
    uint8_t getLastDisconnectReason() const { return _lastDisconnectReason; }
    unsigned long getLastDisconnectAt() const { return _lastDisconnectAt; }
    uint32_t getReconnectCount() const { return _reconnectCount; }
    unsigned long getOfflineMs() const;

    // Reachability, as opposed to what the driver claims. See superviseLink().
    uint32_t getProbeFailures() const { return _probeFailures; }
    uint32_t getForcedReconnects() const { return _forcedReconnects; }
    unsigned long getLastProbeOkAgoMs() const {
        return _lastProbeOk == 0 ? 0 : millis() - _lastProbeOk;
    }
    // Called from the ping session's task when a probe finishes. Public only because that
    // callback is a plain C function pointer and cannot be a member.
    void probeFinished(uint32_t received);

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

    // Wi-Fi supervision. connectSTA() used to run once from begin() and that was the whole of
    // it: nothing ever looked at the link again. A router reboot, an access point moving the
    // Slaves' channel, or any ordinary roam therefore took the Master off the network for good,
    // or for as long as the SDK's own retry happened to take - silently, because nothing printed
    // and nothing counted it. Over days that is the difference between a controller that stays
    // up and one that has to be power-cycled.
    static const unsigned long WIFI_CHECK_INTERVAL_MS = 2000;
    static const unsigned long WIFI_RETRY_INTERVAL_MS = 15000;
    unsigned long _lastLinkCheck = 0;
    unsigned long _lastReconnectAttempt = 0;
    bool _wasConnected = false;
    uint32_t _disconnectCount = 0;
    uint32_t _reconnectCount = 0;
    uint8_t _lastDisconnectReason = 0;
    unsigned long _lastDisconnectAt = 0;
    unsigned long _offlineSince = 0;
    void superviseLink();
    static void onWiFiEvent(arduino_event_id_t event, arduino_event_info_t info);

    // WiFi.status() is not a statement about reachability. An access point can drop a client
    // without the station noticing: the driver keeps reporting WL_CONNECTED, no disconnect event
    // is raised, and the device sits there believing it is online while nothing reaches it - not
    // even ARP. Every check built on WiFi.status() stayed silent through exactly that, which is
    // why the outages left no trace at all. So the link is tested by using it: a ping to the
    // gateway every half minute, and a link that stops answering is treated as down no matter
    // what the driver says.
    static const unsigned long LINK_PROBE_INTERVAL_MS = 30000;
    // Ten minutes of a gateway that will not answer before anything is done about it.
    //
    // This used to force a re-association after ninety seconds, and that was a bad trade in both
    // directions. The probe produces false alarms - "ping_sock: send error" means the probe could
    // not be transmitted, which says nothing about the link - and the cure was worse than the
    // disease: tearing down an association and immediately rebuilding it is exactly what makes an
    // access point fail the WPA2 four-way handshake, because it still holds the old session. The
    // reason-15 storms that looked like the underlying fault were traced to this very code.
    //
    // So there is no re-association any more. If the link is genuinely, persistently dead the
    // only action is a restart, which leaves the access point a clean association to accept
    // rather than a half-torn-down one to argue with.
    static const unsigned long LINK_DEAD_RESTART_MS = 600000;

    // The other half of the problem, and the one that was left without any escalation at all:
    // not associated and unable to get back. Observed in the field as reason 15 - the WPA2
    // four-way handshake timing out - repeating every 2.1 seconds for over half an hour and more
    // than a thousand attempts. The station never recovered on its own.
    //
    // The retry storm is itself destructive: every attempt moves the radio to another channel,
    // and ESP-NOW rides on that channel, so the Slaves could not settle anywhere and lost the
    // Master entirely. Hence the SDK's own auto-reconnect is turned off and retries are paced
    // from here, leaving the radio parked in between so the LEDs keep working while the web
    // interface is unreachable.
    static const unsigned long STA_DEAD_RESTART_MS = 180000;
    // A rebooting controller cannot serve the Slaves either, so it is only worth doing while
    // there is reason to think it helps. If several reboots in a row fail to get us associated,
    // the access point is simply not available and we settle into slow retries instead.
    static const uint32_t STA_MAX_RESTART_STREAK = 3;



    void startLinkProbe();

    esp_ping_handle_t _pingHandle = nullptr;
    bool _probeRunning = false;
    unsigned long _lastProbeStart = 0;
    unsigned long _lastProbeOk = 0;
    // When the link was last known to work. Cleared ONLY by a probe that actually succeeds -
    // never by an attempt to fix things. Reconnecting used to reset it, so the escalation timer
    // restarted on every attempt and the "give up and restart" branch could never be reached:
    // the recovery was erasing the evidence that it was not working.
    unsigned long _linkBadSince = 0;
    uint32_t _probeFailures = 0;
    uint32_t _forcedReconnects = 0;

    void connectSTA();
    void startAP();
    void runSetupConnect();
};

extern WiFiManagerClass WiFiManager;

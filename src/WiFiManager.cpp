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
#include "SlaveManager.h"
#include "EspNowBus.h"
#include <esp_heap_caps.h>

WiFiManagerClass WiFiManager;

// Survives a restart, so a reboot loop against an access point that is simply gone can be
// recognised and stopped. RTC_NOINIT_ATTR is deliberately not cleared on a warm boot.
#define WIFI_STREAK_MAGIC 0x48594657u  // "HYFW"
static RTC_NOINIT_ATTR uint32_t g_staStreakMagic;
static RTC_NOINIT_ATTR uint32_t g_staRestartStreak;

static uint32_t staRestartStreak() {
    if (g_staStreakMagic != WIFI_STREAK_MAGIC) {
        g_staStreakMagic = WIFI_STREAK_MAGIC;
        g_staRestartStreak = 0;
    }
    return g_staRestartStreak;
}

static void setStaRestartStreak(uint32_t v) {
    g_staStreakMagic = WIFI_STREAK_MAGIC;
    g_staRestartStreak = v;
}

// The link-failure snapshot, same survival rules as the streak above.
// Bumped whenever the layout changes, so a record written by older firmware is never misread.
#define LINK_SNAPSHOT_MAGIC 0x484C4632u  // "HLF2"
struct LinkSnapshotRecord {
    uint32_t magic;
    WiFiManagerClass::LinkFailureSnapshot data;
};
static RTC_NOINIT_ATTR LinkSnapshotRecord g_linkSnapshot;

void WiFiManagerClass::recordLinkFailure(bool restarting, uint32_t reason) {
    LinkFailureSnapshot& s = g_linkSnapshot.data;
    // A fresh picture at the first failed probe; later calls keep it and only add to it, so the
    // numbers describe the moment the link died, not the hour after.
    if (_probeFailures <= 1 || g_linkSnapshot.magic != LINK_SNAPSHOT_MAGIC) {
        s = LinkFailureSnapshot{};
        s.valid = 1;
        s.uptimeAtFailure = millis() / 1000;
        s.heapFree = ESP.getFreeHeap();
        s.heapMinFree = ESP.getMinFreeHeap();
        s.heapLargestBlock = ESP.getMaxAllocHeap();
        s.internalFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        s.rssi = WiFi.RSSI();
        s.channel = WiFi.channel();
        s.wifiStatus = (uint32_t)WiFi.status();
        EspNowBusClass* bus = SlaveManager.getEspBus();
        if (bus) {
            s.espNowSendErrors = bus->getSendErrors();
            s.espNowReceived = bus->getPacketsReceived();
        }
        s.ledPackets = SlaveManager.getLedPacketsSent();
        g_linkSnapshot.magic = LINK_SNAPSHOT_MAGIC;
    }
    s.probeFailures = _probeFailures;
    if (restarting) {
        s.uptimeAtRestart = millis() / 1000;
        s.reason = reason;
    }
    // The ESP-NOW side is refreshed every time: its state is the evidence, whenever it is taken.
    EspNowBusClass* nowBus = SlaveManager.getEspBus();
    if (nowBus) {
        s.espNowFirstError = nowBus->getFailRunFirstError();
        s.espNowLastError = nowBus->getLastSendError();
        s.espNowFailStreak = nowBus->getSendFailStreak();
        s.espNowRxAgoMs = nowBus->getLastRxAgoMs();
    }

    // On the serial port too, with the numbers now - if someone is watching, they see the
    // state of the device at the moment it went deaf, not only after.
    Serial.printf("WiFi: link dead since %lus - heap %lu (min %lu, block %lu, internal %lu), "
                  "rssi %ld ch %lu status %lu, espnow sendErr %lu rx %lu, led pkts %lu, "
                  "espnow refused %lu in a row (first 0x%x, last 0x%x), nothing heard for %lums\n",
                  (unsigned long)s.uptimeAtFailure, (unsigned long)ESP.getFreeHeap(),
                  (unsigned long)ESP.getMinFreeHeap(), (unsigned long)ESP.getMaxAllocHeap(),
                  (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                  (long)WiFi.RSSI(), (unsigned long)WiFi.channel(), (unsigned long)WiFi.status(),
                  (unsigned long)(SlaveManager.getEspBus() ? SlaveManager.getEspBus()->getSendErrors() : 0),
                  (unsigned long)(SlaveManager.getEspBus() ? SlaveManager.getEspBus()->getPacketsReceived() : 0),
                  (unsigned long)SlaveManager.getLedPacketsSent(),
                  (unsigned long)s.espNowFailStreak, (unsigned)s.espNowFirstError,
                  (unsigned)s.espNowLastError, (unsigned long)s.espNowRxAgoMs);
}

bool WiFiManagerClass::radioLooksDead() const {
    EspNowBusClass* bus = SlaveManager.getEspBus();
    if (!bus) return false;
    return bus->getSendFailStreak() >= RADIO_DEAD_MIN_REFUSED &&
           bus->getSendFailingForMs() >= RADIO_DEAD_MS &&
           bus->getLastRxAgoMs() >= RADIO_DEAD_MS;
}

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
    // Take over what the previous life recorded about its dead link, then clear it so a later,
    // unrelated restart does not report the same event again.
    if (g_linkSnapshot.magic == LINK_SNAPSHOT_MAGIC) {
        _prevLinkFailure = g_linkSnapshot.data;
        g_linkSnapshot.magic = 0;
        Serial.printf("WiFi: previous run lost its link at %lus and restarted at %lus, reason %lu "
                      "(heap %lu, block %lu, espnow sendErr %lu, first refusal 0x%x, last 0x%x)\n",
                      (unsigned long)_prevLinkFailure.uptimeAtFailure,
                      (unsigned long)_prevLinkFailure.uptimeAtRestart,
                      (unsigned long)_prevLinkFailure.reason,
                      (unsigned long)_prevLinkFailure.heapFree,
                      (unsigned long)_prevLinkFailure.heapLargestBlock,
                      (unsigned long)_prevLinkFailure.espNowSendErrors,
                      (unsigned)_prevLinkFailure.espNowFirstError,
                      (unsigned)_prevLinkFailure.espNowLastError);
    }
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
    // Retries are paced by superviseLink() instead. Left to itself the SDK retries every two
    // seconds, and since each attempt moves the radio to another channel, that storm is what
    // takes ESP-NOW - and with it the Slaves - down alongside the web interface.
    WiFi.setAutoReconnect(false);
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
        _linkBadSince = 0;   // the only thing that may clear this
        // The gateway answering is the one reliable sign that whatever was tried last time
        // worked, so this - and only this - refills the restart budget. It used to be refilled on
        // association instead, which never happens after a restart (connectSTA() associates
        // before superviseLink() ever sees a disconnected station). The budget therefore only
        // ever went down, and after three dead-link events - however far apart - the device
        // stopped restarting for good and stayed offline until it lost power.
        setStaRestartStreak(0);
        return;
    }
    _probeFailures++;
    Serial.printf("WiFi: gateway did not answer (%lu in a row)\n",
                  (unsigned long)_probeFailures);
    if (_probeFailures == 1 || _probeFailures % 10 == 0) recordLinkFailure(false);
}

void WiFiManagerClass::startLinkProbe() {
    if (_probeRunning) return;

    IPAddress gw = WiFi.gatewayIP();
#ifdef HYPERLED_TEST_DEAD_LINK
    // Test build only: 192.0.2.0/24 is TEST-NET-1 and is guaranteed never to be routed, so the
    // probe fails exactly as it would against a gateway that has stopped answering.
    gw = IPAddress(192, 0, 2, 1);
#endif
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
            _lastProbeOk = now;
            // The restart budget is NOT refilled here: associating says nothing about whether
            // the link carries anything. See probeFinished().
            // mDNS and NetBIOS bind to the address the device had when they started, so after a
            // new lease they answer for one that no longer exists - http://hyperled/ then leads
            // nowhere even though the controller is back.
            MDNS.end();
            if (MDNS.begin("hyperled")) MDNS.addService("http", "tcp", 80);
            NBNS.begin("hyperled");
        }

        // Associated as far as the driver is concerned - now find out whether that is true.
        if (now - _lastProbeStart >= LINK_PROBE_INTERVAL_MS) startLinkProbe();

        if (_probeFailures > 0) {
            if (_linkBadSince == 0) _linkBadSince = now;

            // Nothing is touched before this point. The station stays associated, ESP-NOW keeps
            // its channel, and the Slaves keep being served - a probe that is merely wrong costs
            // nothing at all now, which is the whole point of the change.
            uint32_t streak = staRestartStreak();
            // Within budget: restart after ten minutes - or after one, if ESP-NOW confirms that
            // the radio itself is dead. Budget spent: still restart, but only once an hour, so a
            // device that cannot recover does not reboot-loop the Slaves.
            bool radioDead = radioLooksDead();
            unsigned long wait = streak >= STA_MAX_RESTART_STREAK ? LINK_DEAD_BACKOFF_MS
                               : radioDead                        ? RADIO_DEAD_MS
                                                                  : LINK_DEAD_RESTART_MS;
            if (now - _linkBadSince >= wait) {
                if (streak < STA_MAX_RESTART_STREAK + 1) setStaRestartStreak(streak + 1);
                _forcedReconnects++;
                Serial.printf("WiFi: associated but unreachable for %lus, restarting (%lu)\n",
                              (now - _linkBadSince) / 1000, (unsigned long)(streak + 1));
                recordLinkFailure(true, radioDead ? 2 : 1);
                Serial.flush();
                ESP.restart();
            }
            return;
        }

        _linkBadSince = 0;
        _offlineSince = 0;
        return;
    }

    if (_wasConnected) {
        _wasConnected = false;
        if (_offlineSince == 0) _offlineSince = now;
    }

    unsigned long downMs = getOfflineMs();

    // Second escalation: reboot. A restart rebuilds the whole Wi-Fi stack, which is the only
    // thing observed to clear a station stuck failing the four-way handshake - it retried for
    // over half an hour and a thousand attempts without ever getting through.
    uint32_t streak = staRestartStreak();
    unsigned long restartAfter = streak < STA_MAX_RESTART_STREAK ? STA_DEAD_RESTART_MS
                                                                 : LINK_DEAD_BACKOFF_MS;
    // Once the budget is spent the access point is probably not there at all: retries continue
    // quietly below with the radio left alone (so ESP-NOW keeps serving the Slaves), and a
    // restart is only tried once an hour instead of never again.
    if (downMs >= restartAfter) {
        {
            if (streak < STA_MAX_RESTART_STREAK + 1) setStaRestartStreak(streak + 1);
            Serial.printf("WiFi: cannot associate after %lus (last reason %u), restarting (%lu)\n",
                          downMs / 1000, (unsigned)_lastDisconnectReason,
                          (unsigned long)(streak + 1));
            Serial.flush();
            ESP.restart();
        }
    }

    // There used to be a "reset the radio" step here - WIFI_OFF then WIFI_STA - as a cheaper
    // alternative to rebooting. It was a mistake twice over. ESP-NOW is built on the Wi-Fi
    // driver, so powering the radio down deinitialises it, and nothing put it back: the Master
    // went on hearing Slaves without ever registering one again. It also panicked, which is
    // exactly what the recorded reset reason 4 in WiFiManager after 457s was. Paced retries
    // until the reboot threshold is both safer and enough.

    if (now - _lastReconnectAttempt < WIFI_RETRY_INTERVAL_MS) return;
    _lastReconnectAttempt = now;

    Serial.printf("WiFi: still down after %lus (reason %u), retrying\n",
                  downMs / 1000, (unsigned)_lastDisconnectReason);
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

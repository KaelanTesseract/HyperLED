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
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <map>
#include <vector>
#include "PresetManager.h"

struct Segment;

// Home Assistant integration over MQTT (JSON light schema with discovery).
//
// Topics, all below the base topic (the configured one, or hyperled/<mac> when none is set):
//   <base>/status            "online" / "offline" (retained, "offline" is the last will)
//   <base>/ha/state|set      "Alle Segmente" - on/off and brightness for every segment at once
//   <base>/seg<N>/ha/state|set  one light per segment, Slave segments included
//
// Everything runs in the main loop: state changes are found by comparing each segment with
// what was last published, so a change from the web interface, a button, a schedule, a scene
// or the playlist reaches Home Assistant without those places having to know about MQTT.
class MqttManagerClass {
public:
    void begin();
    void loop();
    // Publishes discovery and the complete state again on the next loop pass.
    void requestResync();
    // MQTT is being switched off: on the next loop pass the controller takes its lights out of
    // Home Assistant (so they do not linger there as unavailable) and stops. Safe from any task.
    void requestRemoval() { _removalRequested = true; }

    bool isEnabled() const { return _enabled; }
    bool isConnected() { return _connectState == CONNECT_IDLE && _client.connected(); }
    const String& baseTopic() const { return _base; }

    // For the web interface. Safe to call from the web server's task: they only read values the
    // main loop keeps up to date, or set a flag for it.
    //   error: "none", "disabled", "no_server", "no_wifi", "unreachable", "unreachable_tls",
    //   "no_answer", "protocol", "client_id", "unavailable", "credentials", "unauthorized",
    //   "lost", "stalled", "failed"
    void statusJson(JsonObject out) const;
    void requestResyncFromWeb() { _resyncRequested = true; }
    // Tries the given settings with a connection of its own (in a task) without saving them or
    // disturbing the running connection. testResult() is "running" until it is done, then "ok"
    // or one of the errors above.
    bool startTest(const String& server, uint16_t port, const String& user, const String& pass, bool tls);
    const char* testResult() const { return _testResult; }

    // A physical button was used (ButtonManager, main loop). Home Assistant gets it as an event
    // of that button's event entity; the button keeps doing what it does on the controller.
    enum ButtonEvent : uint8_t { BUTTON_SHORT, BUTTON_LONG, BUTTON_FLIPPED };
    void buttonEvent(uint8_t index, ButtonEvent event);

private:
    // --- Everything besides the lights (MqttEntities.cpp) ------------------------------------
    // Scenes, playlist, sync, effect settings, texts, panel background, diagnostics, Slaves,
    // buttons, firmware update, restart and the status LED. Each is announced with
    // "hyperled_<mac>_<suffix>" as its object id; the set announced last is kept in
    // PREF_MQTT_EXTRAS, so entities that disappear are removed from Home Assistant too.
    struct SlaveCache {
        String name;
        String version;
        bool wireless = true;
    };
    String componentTopic(const char* component, const String& suffix) const;
    void beginEntity(JsonDocument& doc, const String& suffix, const String& name) const;
    void announce(const char* component, const String& suffix, JsonDocument& doc, std::vector<String>& seen);
    uint32_t extrasSignature();
    void startExtraDiscovery();
    void publishExtraStage();
    uint8_t _extraStage = 0;  // 0 = nothing to announce
    std::vector<String> _extraSeen;
    void loadAnnouncedExtras();
    void handleExtraCommand(const String& rest, const uint8_t* payload, unsigned int length);
    void loopExtras(bool force);
    void publishSegmentExtras(bool force);
    void publishSysState(bool force);
    void publishDiagnostics();
    void publishSlaves(bool force);
    void publishUpdateState(bool force);
    void publishStatusLed(bool force);
    void flushButtonEvents();
    void removeExtras();
    void relevantSlaves(std::vector<uint8_t>& ids);
    void refreshPresets();
    String sceneOption(uint8_t presetId) const;

    std::vector<String> _announcedExtras;  // "component:suffix"
    bool _extrasLoaded = false;
    std::vector<uint32_t> _extSigs;
    uint32_t _sysSig = 0;
    uint32_t _slaveSig = 0;
    uint32_t _updateSig = 0;
    uint32_t _statusLedSig = 0;
    unsigned long _lastDiag = 0;
    unsigned long _connectedAt = 0;
    unsigned long _lastLatestCheck = 0;
    bool _latestChecked = false;
    std::vector<PresetManagerClass::PresetInfo> _presets;
    uint32_t _presetRevision = 0xFFFFFFFF;
    uint8_t _sceneId = 0;          // shown as the current scene until something changes
    uint32_t _sceneSig = 0;
    uint32_t _seenApplyCount = 0;
    std::map<uint8_t, SlaveCache> _slaveCache;
    uint8_t _buttonQueue[8];
    uint8_t _buttonQueueLen = 0;

    // What a segment's hardware can show, mapped onto Home Assistant's colour modes.
    enum Caps : uint8_t { CAP_ONOFF, CAP_BRIGHTNESS, CAP_COLOR_TEMP, CAP_RGB, CAP_RGBW, CAP_RGB_CT };
    struct SegInfo {
        Caps caps;
        bool hub75;   // clock/text and the panel showcase effects
        bool matrix;  // any 2D layout - the image effect
    };

    // Connecting happens in two halves: connectTask() does the name lookup and the TCP handshake
    // in its own task, finishConnect() the short MQTT handshake in the main loop.
    enum ConnectState : uint8_t { CONNECT_IDLE, CONNECT_RUNNING, CONNECT_OK, CONNECT_FAILED };
    static const int32_t CONNECT_TIMEOUT_MS = 3000;
    volatile ConnectState _connectState = CONNECT_IDLE;
    static void connectTask(void* arg);
    void finishConnect(bool tcpOk);

    volatile bool _removalRequested = false;
    void removeFromHomeAssistant();

    void loopStep();
    static const unsigned long SLOW_PASS_MS = 200;
    unsigned long _lastSlowLog = 0;

    void reconnect();
    void loadConfig();
    void onConnected();
    void handleMessage(char* topic, uint8_t* payload, unsigned int length);
    void applyToSegment(uint8_t segId, JsonDocument& doc);
    void applyToAll(JsonDocument& doc);

    void refreshSlaveTypes();
    SegInfo segmentInfo(const Segment& seg) const;
    static Caps capsForType(uint8_t type);
    static bool effectOffered(uint8_t effect, const SegInfo& info);

    uint32_t discoverySignature();
    void publishDiscovery();
    void clearSegmentDiscovery(uint8_t from, uint8_t to);
    void publishChangedStates(bool force);
    void fillSegmentState(JsonDocument& doc, const Segment& seg, const SegInfo& info) const;

    bool publish(const String& topic, const String& payload, bool retained = true);
    bool socketWritable();
    void dropConnection(const char* reason);
    String segTopic(uint8_t segId) const;
    String discoveryTopic(const String& objectId) const;
    void addDevice(JsonDocument& doc) const;

    // The connection underneath: plain TCP, or TLS for MQTTS. TLS encrypts but does not check the
    // broker's certificate - home brokers mostly use self-signed ones.
    WiFiClient _plainClient;
    NetworkClientSecure _tlsClient;
    NetworkClient* _net = &_plainClient;
    bool _tls = false;
    PubSubClient _client;

    // Status for the web interface (see statusJson()).
    volatile bool _connectedFlag = false;
    const char* volatile _lastError = "none";
    volatile unsigned long _statusSince = 0;  // when the connection came up or the error happened
    void setError(const char* error);
    static const char* errorForState(int state);
    volatile bool _resyncRequested = false;
    struct TestParams {
        String server;
        uint16_t port;
        String user;
        String pass;
        bool tls;
    };
    static void testTask(void* arg);
    const char* volatile _testResult = "none";

    bool _enabled = false;
    String _server;
    uint16_t _port = 1883;
    String _user;
    String _pass;
    String _base;
    String _macAddress;   // without colons, for ids
    String _macColons;    // for Home Assistant's device connections

    unsigned long _lastReconnectAttempt = 0;
    uint32_t _reconnectDelay = 5000;
    bool _everAttempted = false;

    unsigned long _lastStateCheck = 0;
    unsigned long _lastDiscoveryCheck = 0;
    unsigned long _discoveryDueAt = 0;
    bool _discoveryDue = false;
    bool _forceState = false;
    uint32_t _discoverySig = 0;
    uint8_t _announcedSegs = 0;  // as stored in PREF_MQTT_ANNOUNCED
    std::vector<uint32_t> _stateSigs;
    uint32_t _groupSig = 0;

    // Last LED type each Slave reported, by Slave id. Kept when a Slave drops out for a moment,
    // so its light does not flip to plain RGB in Home Assistant and back.
    uint8_t _slaveTypes[256];

    uint32_t _publishFailures = 0;
    unsigned long _lastFailureLog = 0;
};

extern MqttManagerClass MqttManager;

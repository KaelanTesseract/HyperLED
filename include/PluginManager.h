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
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <memory>
#include <vector>
#include "LEDManager.h"
#include "PluginDef.h"
#include "PluginExpr.h"
#include "PluginRun.h"
#include "ScriptHost.h"
#include "ScriptWire.h"
#include "TextVariables.h"

// Plugins: JSON files that read a source on the network and show the result on one segment.
// See docs/en/10_Plugins_entwickeln.md.
//
// Who does what: the plugin task (see taskLoop) is the only one that touches the network; the main
// loop (loop) evaluates the rules and is the only one that talks to LEDManager; the web server's
// task calls the public setters. All of them meet in `_lock`.

enum class PluginState : uint8_t {
    Off,           // switched off by the user
    Waiting,       // switched on, no answer from the source yet
    Running,       // answers arrive and the rules are applied
    NoConnection,  // the source failed repeatedly; on_error applies
    Incompatible,  // needs another plugin interface than this firmware offers
    Invalid        // the file cannot be read (damaged, or not valid for this firmware)
};

enum class PluginResult : uint8_t { Ok, NotFound, Rejected };

struct PluginInstance {
    String id;
    PluginDef::Definition def;  // empty when the file is Invalid
    bool valid = false;
    bool incompatible = false;  // see compatibility(); a forced plugin runs although this is true
    String compatNote;          // why it is incompatible

    // What the user set; stored in /plugins/<id>.set.json.
    bool enabled = false;
    bool force = false;       // run although incompatible, on the user's responsibility
    bool allowPower = false;  // the plugin may switch the segment on or off
    std::vector<PluginRun::SettingValue> values;

    PluginState state = PluginState::Off;
    String reason;  // for NoConnection, Incompatible, Invalid, and an automatic switch-off

    // Runtime, never stored.
    uint32_t nextFetchMs = 0;
    uint8_t failures = 0;    // fetches in a row that failed
    uint8_t mismatches = 0;  // answers in a row that held none of the values the plugin reads
    bool haveData = false;
    std::vector<PluginExpr::Value> results;  // in the order of def.values
    int16_t appliedSegment = -1;             // the segment this plugin has an overlay on, -1 none
    SegmentOverlay applied;

    // The plugin's script (see driveScript), runtime only. The segment it runs on, on the Master or on
    // a Slave; the checksum of its text; what it was last given; why it is not in charge; its state.
    int16_t scriptSegment = -1;
    bool scriptOnSlave = false;
    uint8_t scriptSlaveId = 0;
    uint32_t scriptCrc = 0;
    uint32_t scriptRetryAt = 0;    // not before this time (millis) is the script handed over again
    uint32_t scriptStartedAt = 0;  // when it was handed to a Slave
    std::vector<Script::Item> scriptSettingsSent, scriptValuesSent;
    String scriptNote;             // why the script is not in charge (the rules apply instead)
    bool scriptInCharge = false;
    Script::Wire::Status scriptStatus;
    bool scriptHaveStatus = false;

    // The start of the last answer from the source (at most 1 KB), for the live view in the interface.
    String lastRaw;
};

class PluginManagerClass {
public:
    static constexpr size_t MAX_PLUGINS = 8;
    static const char* const DIR;  // "/plugins"

    void begin();
    void loop();  // main loop: rules -> overlay

    // For the web interface; they may run on the web server's task.
    PluginResult install(const String& json, String& error, String& idOut);
    // Everything install checks, without saving anything: what the person is told before they say yes.
    PluginResult preview(const String& json, JsonObject out, String& error);
    // The settings of a plugin as the web interface needs them to build a form: type, labels in every
    // language, hint, default, range, options. False when there is no such plugin.
    bool definitionJson(const String& id, JsonObject out);
    size_t pluginCount();
    // The values read, the rule that applies and the start of the last answer, for the live view.
    bool valuesJson(const String& id, JsonObject out);

    // Loading a plugin file from an address (the controller does it, the browser could not follow
    // GitHub's redirects). One at a time, in a task of its own; the browser asks how it is going.
    static bool validFetchUrl(const String& url, String& error);
    bool startFetch(const String& url, String& error);
    void fetchJson(JsonObject out);
    PluginResult remove(const String& id, String& error);
    PluginResult setEnabled(const String& id, bool enabled, String& error);
    PluginResult setOptions(const String& id, int8_t force, int8_t allowPower, String& error);
    PluginResult setSettings(const String& id, JsonObjectConst values, String& error);
    void listJson(JsonArray out);
    // How much of the plugin task's stack has never been used, in bytes - a diagnostic for tests and
    // for a controller in the field: a plugin that needs more shows up here before it crashes.
    uint32_t taskStackFree() const;

    // Pure helpers; also checked by the on-device tests.
    static bool versionAtLeast(const String& have, const String& need);
    static bool compatibility(const PluginDef::Definition& def, String& reason);
    static bool validateValue(const PluginDef::Setting& setting, const String& value, String& error);

private:
    struct Guard;  // holds _lock for a scope

    SemaphoreHandle_t _lock = nullptr;
    volatile bool _ready = false;
    std::vector<std::shared_ptr<PluginInstance>> _plugins;
    std::vector<int16_t> _pendingRelease;  // segments whose overlay must be cleared by the main loop
    struct ScriptStop {
        int16_t segment;
        bool onSlave;
        uint8_t slaveId;
    };
    std::vector<ScriptStop> _pendingScriptStop;  // scripts the main loop must end (plugin removed or replaced)
    TaskHandle_t _task = nullptr;
    uint32_t _nextLoopMs = 0;

    // loading from an address
    enum class FetchState : uint8_t { Idle, Running, Done, Failed };
    FetchState _fetchState = FetchState::Idle;
    String _fetchUrl, _fetchText, _fetchError;
    static void fetchTaskEntry(void* self);
    void fetchTask();

    // storage and model (Task 8)
    void loadAll();
    std::shared_ptr<PluginInstance> makeInstance(const String& id, const PluginDef::Definition& def);
    std::shared_ptr<PluginInstance> makeInvalid(const String& id, const String& why);
    void loadSettings(PluginInstance& p);
    bool saveSettings(const PluginInstance& p);
    void refreshState(PluginInstance& p);
    std::shared_ptr<PluginInstance> find(const String& id);
    String segmentOwner(const String& segment, const String& exceptId);
    String problemWith(const PluginInstance& p, const std::vector<PluginRun::SettingValue>& values);

    // Makes the values of the running plugins available as text variables "<id>.<value>" (see
    // TextVariables.h), so a Text or Lauftext element can show them as {<id>.<value>}. Main loop, under
    // _lock. Only a plugin that is switched on, running and has data counts, and only a value that is
    // known: anything else shows "--" - a stale value would pretend to be fresh.
    void publishTextVariables();

    // running (Task 9)
    static void taskEntry(void* self);
    void taskLoop();
    void ensureTask();
    std::shared_ptr<PluginInstance> pickDue();
    void fetchOne(const std::shared_ptr<PluginInstance>& p);
    void recordFailure(PluginInstance& p, const String& why);
    void setState(PluginInstance& p, PluginState state, const String& reason);
    void apply(PluginInstance& p);

    // scripts (main loop, under _lock)
    bool driveScript(PluginInstance& p, int segment);  // true: the script is in charge (running, or about to)
    void stopScript(PluginInstance& p);
    void queueScriptStop(const PluginInstance& p);
};

extern PluginManagerClass PluginManager;

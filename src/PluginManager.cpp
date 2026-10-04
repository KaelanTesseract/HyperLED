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
#include "PluginManager.h"

#include <LittleFS.h>
#include <WiFi.h>
#include "Config.h"
#include "PluginHttp.h"
#include "PluginJson.h"
#include "MasterScripts.h"
#include "SlaveManager.h"
#include <esp_rom_crc.h>


PluginManagerClass PluginManager;

const char* const PluginManagerClass::DIR = "/plugins";

struct PluginManagerClass::Guard {
    SemaphoreHandle_t handle;
    explicit Guard(SemaphoreHandle_t h) : handle(h) { xSemaphoreTakeRecursive(handle, portMAX_DELAY); }
    ~Guard() { xSemaphoreGiveRecursive(handle); }
};

namespace {

String definitionPath(const String& id) { return String(PluginManagerClass::DIR) + "/" + id + ".json"; }
String settingsPath(const String& id) { return String(PluginManagerClass::DIR) + "/" + id + ".set.json"; }

// Written to a temporary file first and renamed over the old one, so a power cut leaves either the
// old file or the new one - never half of each.
bool writeAtomic(const String& path, const String& content) {
    String temp = path + ".tmp";
    File f = LittleFS.open(temp, "w");
    if (!f) return false;
    size_t written = f.print(content);
    f.close();
    if (written != content.length()) {
        LittleFS.remove(temp);
        return false;
    }
    return LittleFS.rename(temp, path);
}

bool readAll(const String& path, String& out) {
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    out = f.readString();
    f.close();
    return true;
}

// The script text of a plugin file, without keeping the rest of the file around.
bool readScript(const String& json, String& script) {
    JsonDocument filter;
    filter["script"] = true;
    JsonDocument doc;
    if (deserializeJson(doc, json, DeserializationOption::Filter(filter))) return false;
    if (!doc["script"].is<const char*>()) return false;
    script = doc["script"].as<const char*>();
    return true;
}

bool readScriptFromFile(const String& id, String& script) {
    String json;
    if (!readAll(definitionPath(id), json)) return false;
    return readScript(json, script);
}

// Compiles a script without running it, in a task of its own: the parser nests as deep as Lua allows
// (about 11 KB of stack for a script made to do that), which does not belong on the web server's stack.
struct CheckJob {
    const String* script;
    String error;
    Script::Result result = Script::Result::Ok;
    SemaphoreHandle_t done;
};

void checkTaskEntry(void* arg) {
    CheckJob* job = static_cast<CheckJob*>(arg);
    job->result = Script::Host::check(job->script->c_str(), job->script->length(), job->error);
    xSemaphoreGive(job->done);  // the caller owns the job from here on
    vTaskDelete(nullptr);
}

Script::Result checkScript(const String& script, String& error) {
    CheckJob job;
    job.script = &script;
    job.done = xSemaphoreCreateBinary();
    if (!job.done || xTaskCreatePinnedToCore(&checkTaskEntry, "scriptcheck", 16384, &job, 1, nullptr, 0) != pdPASS) {
        if (job.done) vSemaphoreDelete(job.done);
        error = "Kein Speicher für die Prüfung";
        return Script::Result::OutOfMemory;
    }
    xSemaphoreTake(job.done, portMAX_DELAY);
    vSemaphoreDelete(job.done);
    error = job.error;
    return job.result;
}

// What install and preview both check: the file, and a script in it. Nothing is stored.
bool checkPluginFile(const String& json, PluginDef::Definition& def, String& error) {
    if (!PluginDef::parse(json, def, error)) return false;
    if (def.hasScript) {
        // Compiled once, not run: a script with a syntax error is refused here, with its line, and not
        // found out one day on a Slave.
        String script, scriptError;
        if (!readScript(json, script)) {
            error = "Das Skript konnte nicht gelesen werden";
            return false;
        }
        if (checkScript(script, scriptError) != Script::Result::Ok) {
            error = "Skript: " + scriptError;
            return false;
        }
    }
    return true;
}

constexpr uint8_t FAIL_LIMIT = 3;          // failed fetches in a row before the source counts as gone
constexpr uint8_t MISMATCH_LIMIT = 5;      // answers in a row that hold nothing the plugin reads
constexpr uint32_t MIN_FETCH_HEAP = 30000; // largest free block needed for a TLS connection
constexpr uint16_t MIN_RETRY_SECONDS = 5;  // after a failure, ask again no sooner than this

// An address or header value goes out as one line: nothing a user typed may add lines to it.
String oneLine(const String& s) {
    String out;
    for (size_t i = 0; i < s.length(); i++) {
        if (s[i] != '\r' && s[i] != '\n') out += s[i];
    }
    out.trim();
    return out;
}

// What went wrong, for the person who set the plugin up.
String describeError(const String& code, int status) {
    if (code == "no_connection") return "Keine Verbindung zur Quelle";
    if (code == "bad_url") return "Die Adresse der Quelle ist ungültig";
    if (code == "too_large") return "Die Antwort der Quelle ist größer als 8 KB";
    if (code == "read_failed") return "Die Antwort der Quelle konnte nicht gelesen werden";
    if (code.startsWith("http_")) return "Die Quelle antwortet mit Fehler " + String(status);
    return "Die Quelle konnte nicht abgefragt werden";
}

const char* stateName(PluginState s) {
    switch (s) {
        case PluginState::Off: return "off";
        case PluginState::Waiting: return "waiting";
        case PluginState::Running: return "running";
        case PluginState::NoConnection: return "no_connection";
        case PluginState::Incompatible: return "incompatible";
        case PluginState::Invalid: return "invalid";
    }
    return "off";
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// Pure helpers

bool PluginManagerClass::versionAtLeast(const String& have, const String& need) {
    size_t h = 0, n = 0;
    for (int part = 0; part < 4; part++) {
        long a = 0, b = 0;
        while (h < have.length() && isDigit(have[h])) a = a * 10 + (have[h++] - '0');
        while (n < need.length() && isDigit(need[n])) b = b * 10 + (need[n++] - '0');
        if (a != b) return a > b;
        if (h < have.length() && have[h] == '.') h++;
        if (n < need.length() && need[n] == '.') n++;
    }
    return true;
}

bool PluginManagerClass::compatibility(const PluginDef::Definition& def, String& reason) {
    if (def.api > PLUGIN_API_VERSION) {
        reason = "Das Plugin braucht die Plugin-Schnittstelle " + String(def.api) + ", diese Firmware bietet " +
                 String(PLUGIN_API_VERSION) + ". Bitte die Firmware aktualisieren.";
        return false;
    }
    if (def.api < PLUGIN_API_MIN) {
        reason = "Das Plugin ist für eine ältere Plugin-Schnittstelle (" + String(def.api) +
                 ") geschrieben; diese Firmware versteht ab " + String(PLUGIN_API_MIN) +
                 ". Bitte eine neuere Fassung des Plugins installieren.";
        return false;
    }
    if (def.minFirmware.length() > 0 && !versionAtLeast(HYPERLED_VERSION, def.minFirmware)) {
        reason = "Das Plugin braucht mindestens die Firmware " + def.minFirmware + ", installiert ist " +
                 String(HYPERLED_VERSION) + ".";
        return false;
    }
    if (def.scriptLevel > PLUGIN_SCRIPT_LEVEL) {
        reason = PLUGIN_SCRIPT_LEVEL == 0
                     ? String("Das Plugin braucht Skript-Unterstützung (Stufe ") + String(def.scriptLevel) +
                           "); diese Firmware kann noch keine Skripte ausführen. Das Plugin bleibt gespeichert und "
                           "läuft nach einem Firmware-Update, das Skripte kann."
                     : String("Das Plugin braucht Skript-Stufe ") + String(def.scriptLevel) +
                           ", diese Firmware bietet Stufe " + String(PLUGIN_SCRIPT_LEVEL) + ". Bitte die Firmware aktualisieren.";
        return false;
    }
    return true;
}

bool PluginManagerClass::validateValue(const PluginDef::Setting& s, const String& v, String& error) {
    if (v.length() == 0) return true;  // not set; whether it must be set is checked when switching on
    if (s.type == "text" || s.type == "password") {
        if (v.length() > 120) {
            error = "höchstens 120 Zeichen";
            return false;
        }
        return true;
    }
    if (s.type == "number") {
        char* end = nullptr;
        double d = strtod(v.c_str(), &end);
        if (end == v.c_str() || *end != '\0') {
            error = "keine Zahl";
            return false;
        }
        if (s.hasRange && (d < s.minValue || d > s.maxValue)) {
            error = "muss zwischen " + PluginExpr::Value::number(s.minValue).toText() + " und " +
                    PluginExpr::Value::number(s.maxValue).toText() + " liegen";
            return false;
        }
        return true;
    }
    if (s.type == "switch") {
        if (v == "true" || v == "false") return true;
        error = "muss true oder false sein";
        return false;
    }
    if (s.type == "list") {
        for (const String& option : s.options) {
            if (option == v) return true;
        }
        error = "keine der angebotenen Auswahlmöglichkeiten";
        return false;
    }
    if (s.type == "color") {
        if (PluginRun::parseColor(v) >= 0) return true;
        error = "keine Farbe (#rrggbb)";
        return false;
    }
    if (s.type == "effect") {
        int idx = PluginDef::effectIndex(v);
        if (idx < 0) {
            error = "unbekannter Effekt";
            return false;
        }
        if (PluginDef::effectBlocked(idx)) {
            error = "dieser Effekt ist für Plugins gesperrt";
            return false;
        }
        return true;
    }
    if (s.type == "segment") {
        for (size_t i = 0; i < v.length(); i++) {
            if (!isDigit(v[i])) {
                error = "keine Segment-Nummer";
                return false;
            }
        }
        if (v.length() > 3 || (int)v.toInt() >= (int)LEDManager.getNumSegments()) {
            error = "dieses Segment gibt es nicht";
            return false;
        }
        return true;
    }
    error = "unbekannter Typ";
    return false;
}

// ---------------------------------------------------------------------------------------------
// Model

std::shared_ptr<PluginInstance> PluginManagerClass::makeInstance(const String& id, const PluginDef::Definition& def) {
    auto p = std::make_shared<PluginInstance>();
    p->id = id;
    p->def = def;
    p->valid = true;
    p->incompatible = !compatibility(p->def, p->compatNote);
    for (const PluginDef::Setting& s : p->def.settings) {
        PluginRun::SettingValue sv;
        sv.key = s.key;
        sv.value = (s.type == "switch" && s.defaultValue.length() == 0) ? String("false") : s.defaultValue;
        p->values.push_back(sv);
    }
    return p;
}

std::shared_ptr<PluginInstance> PluginManagerClass::makeInvalid(const String& id, const String& why) {
    auto p = std::make_shared<PluginInstance>();
    p->id = id;
    p->valid = false;
    p->state = PluginState::Invalid;
    p->reason = why;
    return p;
}

std::shared_ptr<PluginInstance> PluginManagerClass::find(const String& id) {
    for (auto& p : _plugins) {
        if (p->id == id) return p;
    }
    return nullptr;
}

// Back to the state the settings call for, with nothing remembered from the source.
void PluginManagerClass::refreshState(PluginInstance& p) {
    p.nextFetchMs = 0;
    p.failures = 0;
    p.mismatches = 0;
    p.haveData = false;
    p.results.clear();
    if (!p.valid) {
        p.state = PluginState::Invalid;
        return;
    }
    if (p.incompatible && !p.force) {
        p.state = PluginState::Incompatible;
        p.reason = p.compatNote;
        return;
    }
    if (!p.enabled) {
        p.state = PluginState::Off;
        p.reason = "";
        return;
    }
    p.state = PluginState::Waiting;
    p.reason = "";
}

// The name of the plugin that has switched on the segment, "" when none has.
String PluginManagerClass::segmentOwner(const String& segment, const String& exceptId) {
    for (auto& p : _plugins) {
        if (!p->valid || !p->enabled || p->id == exceptId || p->def.segmentKey.length() == 0) continue;
        if (PluginRun::settingText(p->values, p->def.segmentKey) == segment) return p->def.name;
    }
    return String();
}

// Why a plugin cannot be switched on with these values; "" when it can.
String PluginManagerClass::problemWith(const PluginInstance& p, const std::vector<PluginRun::SettingValue>& values) {
    for (const PluginDef::Setting& s : p.def.settings) {
        if (PluginRun::settingText(values, s.key).length() == 0 && !s.optional && s.type != "switch") {
            return "Die Einstellung '" + s.key + "' ist noch leer";
        }
    }
    if (p.def.segmentKey.length() > 0) {
        String segment = PluginRun::settingText(values, p.def.segmentKey);
        String owner = segmentOwner(segment, p.id);
        if (owner.length() > 0) {
            return "Das Segment " + segment + " wird schon vom Plugin '" + owner + "' gesteuert (ein Segment, ein Plugin)";
        }
    }
    return String();
}

// ---------------------------------------------------------------------------------------------
// Storage

void PluginManagerClass::loadSettings(PluginInstance& p) {
    String text;
    if (!readAll(settingsPath(p.id), text)) return;
    JsonDocument doc;
    if (deserializeJson(doc, text)) return;
    p.enabled = doc["enabled"] | false;
    p.force = doc["force"] | false;
    p.allowPower = doc["allow_power"] | false;
    JsonObjectConst saved = doc["values"];
    for (PluginRun::SettingValue& sv : p.values) {
        if (!saved[sv.key].is<const char*>()) continue;
        const PluginDef::Setting* s = PluginDef::findSetting(p.def, sv.key);
        String v = saved[sv.key].as<const char*>();
        String ignored;
        if (s && validateValue(*s, v, ignored)) sv.value = v;
    }
}

bool PluginManagerClass::saveSettings(const PluginInstance& p) {
    JsonDocument doc;
    doc["enabled"] = p.enabled;
    doc["force"] = p.force;
    doc["allow_power"] = p.allowPower;
    JsonObject values = doc["values"].to<JsonObject>();
    for (const PluginRun::SettingValue& sv : p.values) values[sv.key] = sv.value;
    String out;
    serializeJson(doc, out);
    return writeAtomic(settingsPath(p.id), out);
}

void PluginManagerClass::loadAll() {
    std::vector<String> names;
    File dir = LittleFS.open(DIR);
    if (dir && dir.isDirectory()) {
        for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
            names.push_back(String(f.name()));
            f.close();
        }
        dir.close();
    }
    for (const String& name : names) {
        // A temporary file left by a write that did not finish: the real file is still whole.
        if (name.endsWith(".tmp")) {
            LittleFS.remove(String(DIR) + "/" + name);
            continue;
        }
        if (!name.endsWith(".json") || name.endsWith(".set.json")) continue;
        if (_plugins.size() >= MAX_PLUGINS) {
            Serial.printf("Plugins: more than %u files, ignoring %s\n", (unsigned)MAX_PLUGINS, name.c_str());
            continue;
        }
        String id = name.substring(0, name.length() - 5);
        String json, error;
        std::shared_ptr<PluginInstance> p;
        PluginDef::Definition def;
        if (!readAll(String(DIR) + "/" + name, json)) {
            p = makeInvalid(id, "Die Datei kann nicht gelesen werden");
        } else if (!PluginDef::parse(json, def, error)) {
            p = makeInvalid(id, error);
        } else if (def.id != id) {
            p = makeInvalid(id, "Der Dateiname passt nicht zur id '" + def.id + "'");
        } else {
            p = makeInstance(id, def);
            loadSettings(*p);
        }
        refreshState(*p);
        _plugins.push_back(p);
    }
}

void PluginManagerClass::begin() {
    _lock = xSemaphoreCreateRecursiveMutex();
    if (!LittleFS.exists(DIR)) LittleFS.mkdir(DIR);
    loadAll();
    unsigned invalid = 0, incompatible = 0;
    for (auto& p : _plugins) {
        if (p->state == PluginState::Invalid) invalid++;
        if (p->state == PluginState::Incompatible) incompatible++;
    }
    Serial.printf("Plugins: %u installed (%u invalid, %u incompatible)\n", (unsigned)_plugins.size(), invalid, incompatible);
    _ready = true;
    for (auto& p : _plugins) {
        if (p->valid && p->enabled && !(p->incompatible && !p->force)) {
            ensureTask();
            break;
        }
    }
}

void PluginManagerClass::loop() {
    if (!_ready) return;
    uint32_t now = millis();
    if ((int32_t)(now - _nextLoopMs) < 0) return;
    // Never wait for the lock: while the web interface is writing a plugin file the main loop has
    // other work to do (a Slave counts a pause of a few seconds as lost). Try again on the next pass.
    if (xSemaphoreTakeRecursive(_lock, 0) != pdTRUE) return;
    _nextLoopMs = now + 200;
    for (int16_t segment : _pendingRelease) LEDManager.clearPluginOverlay((uint8_t)segment);
    _pendingRelease.clear();
    for (const ScriptStop& stop : _pendingScriptStop) {
        if (stop.onSlave) SlaveManager.releaseScript(stop.slaveId);
        else MasterScripts.stop((uint8_t)stop.segment);
    }
    _pendingScriptStop.clear();
    for (auto& p : _plugins) apply(*p);
    xSemaphoreGiveRecursive(_lock);
}

// ---------------------------------------------------------------------------------------------
// What the web interface does

PluginResult PluginManagerClass::install(const String& json, String& error, String& idOut) {
    if (!_ready) {
        error = "Die Plugins werden noch gestartet";
        return PluginResult::Rejected;
    }
    PluginDef::Definition def;
    if (!checkPluginFile(json, def, error)) return PluginResult::Rejected;
    Guard guard(_lock);
    auto old = find(def.id);
    if (!old && _plugins.size() >= MAX_PLUGINS) {
        error = "Es sind schon " + String((unsigned)MAX_PLUGINS) + " Plugins installiert - eines muss zuerst entfernt werden";
        return PluginResult::Rejected;
    }
    if (!writeAtomic(definitionPath(def.id), json)) {
        error = "Das Plugin konnte nicht gespeichert werden (Speicher voll?)";
        return PluginResult::Rejected;
    }
    auto fresh = makeInstance(def.id, def);
    if (old) {
        // An update keeps what the user set, as far as it still fits.
        for (PluginRun::SettingValue& sv : fresh->values) {
            const PluginDef::Setting* before = old->valid ? PluginDef::findSetting(old->def, sv.key) : nullptr;
            const PluginDef::Setting* now = PluginDef::findSetting(fresh->def, sv.key);
            if (!before || !now || before->type != now->type) continue;
            String kept = PluginRun::settingText(old->values, sv.key);
            String ignored;
            if (validateValue(*now, kept, ignored)) sv.value = kept;
        }
        fresh->force = old->force;
        fresh->allowPower = old->allowPower;
        fresh->enabled = old->enabled && problemWith(*fresh, fresh->values).length() == 0;
        if (old->appliedSegment >= 0) _pendingRelease.push_back(old->appliedSegment);
        queueScriptStop(*old);
    }
    refreshState(*fresh);
    saveSettings(*fresh);
    if (fresh->enabled) ensureTask();
    if (old) {
        for (auto& slot : _plugins) {
            if (slot->id == def.id) slot = fresh;
        }
    } else {
        _plugins.push_back(fresh);
    }
    idOut = def.id;
    Serial.printf("Plugins: %s %s (%s)\n", old ? "updated" : "installed", def.id.c_str(), stateName(fresh->state));
    return PluginResult::Ok;
}

bool PluginManagerClass::valuesJson(const String& id, JsonObject out) {
    if (!_ready) return false;
    Guard guard(_lock);
    auto p = find(id);
    if (!p) return false;
    out["id"] = p->id;
    out["state"] = stateName(p->state);
    out["reason"] = p->reason;
    out["enabled"] = p->enabled;
    if (!p->valid) return true;
    JsonObject values = out["values"].to<JsonObject>();
    for (size_t i = 0; i < p->def.values.size() && i < p->results.size(); i++) {
        const PluginExpr::Value& v = p->results[i];
        const String& name = p->def.values[i].name;
        if (!v.known()) values[name] = nullptr;
        else if (v.type == PluginExpr::Type::Number) values[name] = v.num;
        else if (v.type == PluginExpr::Type::Bool) values[name] = v.num != 0;
        else values[name] = v.text;
    }
    // Who is drawing: the script, a rule (which one and why), on_error, or nobody.
    String source = "none";
    JsonVariant rule = out["rule"].to<JsonVariant>();
    rule.set(nullptr);
    if (p->scriptInCharge) {
        source = "script";
    } else if (p->enabled && (p->state == PluginState::Running || p->state == PluginState::NoConnection)) {
        PluginRun::InstanceScope scope(p->def, p->results, p->values);
        int index = PluginRun::chooseIndex(p->def, scope, p->state == PluginState::NoConnection);
        if (index == PluginRun::RULE_ON_ERROR) {
            source = "on_error";
        } else if (index >= 0) {
            source = "rules";
            JsonObject r = out["rule"].to<JsonObject>();
            r["index"] = index + 1;
            r["when"] = p->def.rules[index].whenText;
        }
    }
    out["source"] = source;
    if (p->def.hasScript) {
        JsonObject script = out["script"].to<JsonObject>();
        script["mode"] = p->scriptInCharge ? "script" : "rules";
        script["note"] = p->scriptNote;
        if (p->scriptHaveStatus) {
            script["state"] = p->scriptStatus.state;
            script["message"] = p->scriptStatus.message;
            script["frame_ms"] = p->scriptStatus.frameUs10 / 10.0;
        }
    }
    out["raw"] = p->lastRaw;
    return true;
}

bool PluginManagerClass::validFetchUrl(const String& url, String& error) {
    if (url.length() == 0) {
        error = "Bitte eine Adresse eingeben";
        return false;
    }
    if (url.length() > 300) {
        error = "Die Adresse ist länger als 300 Zeichen";
        return false;
    }
    String rest;
    if (url.startsWith("https://")) rest = url.substring(8);
    else if (url.startsWith("http://")) rest = url.substring(7);
    else {
        error = "Die Adresse muss mit http:// oder https:// beginnen";
        return false;
    }
    if (rest.length() == 0 || rest[0] == '/' || rest[0] == ':') {
        error = "In der Adresse fehlt der Rechnername";
        return false;
    }
    for (size_t i = 0; i < url.length(); i++) {
        unsigned char ch = (unsigned char)url[i];
        if (ch <= 32 || ch == 127) {
            error = "Die Adresse darf keine Leer- oder Steuerzeichen enthalten";
            return false;
        }
    }
    return true;
}

bool PluginManagerClass::startFetch(const String& url, String& error) {
    if (!_ready) {
        error = "Die Plugins werden noch gestartet";
        return false;
    }
    if (!validFetchUrl(url, error)) return false;
    Guard guard(_lock);
    if (_fetchState == FetchState::Running) {
        error = "Es wird gerade schon eine Datei geladen";
        return false;
    }
    _fetchState = FetchState::Running;
    _fetchUrl = url;
    _fetchText = String();
    _fetchError = String();
    // One task for the one transfer: it ends itself. Core 1 below the main loop, like the plugin task.
    if (xTaskCreatePinnedToCore(&PluginManagerClass::fetchTaskEntry, "plugin-load", 10240, this, 1, nullptr, 1) != pdPASS) {
        _fetchState = FetchState::Idle;
        error = "Kein Speicher zum Laden";
        return false;
    }
    return true;
}

void PluginManagerClass::fetchTaskEntry(void* self) {
    static_cast<PluginManagerClass*>(self)->fetchTask();
    vTaskDelete(nullptr);
}

void PluginManagerClass::fetchTask() {
    String url;
    {
        Guard guard(_lock);
        url = _fetchUrl;
    }
    // A TLS connection needs a big block of memory; wait a little for one rather than fail at once.
    for (int i = 0; i < 40 && ESP.getMaxAllocHeap() < MIN_FETCH_HEAP; i++) vTaskDelay(pdMS_TO_TICKS(250));
    PluginHttp::Request request;
    request.url = url;
    request.timeoutMs = 10000;
    request.maxBytes = PluginDef::MAX_FILE;
    request.followRedirects = true;
    PluginHttp::Response response;
    bool ok = ESP.getMaxAllocHeap() >= MIN_FETCH_HEAP && PluginHttp::get(request, response);
    String error;
    if (ESP.getMaxAllocHeap() < MIN_FETCH_HEAP && !ok && response.error.length() == 0) {
        error = "Zu wenig Speicher, bitte später noch einmal versuchen";
    } else if (!ok) {
        if (response.error == "too_large") error = "Die Datei ist größer als 16 KB";
        else if (response.error == "no_connection") error = "Keine Verbindung zur Adresse";
        else if (response.error == "bad_url") error = "Die Adresse ist ungültig";
        else if (response.error == "read_failed") error = "Die Datei konnte nicht gelesen werden";
        else if (response.error.startsWith("http_")) error = "Die Adresse antwortet mit Fehler " + String(response.status);
        else error = "Die Datei konnte nicht geladen werden";
    }
    Guard guard(_lock);
    if (ok) {
        _fetchText = response.body;
        _fetchState = FetchState::Done;
    } else {
        _fetchError = error;
        _fetchState = FetchState::Failed;
    }
}

void PluginManagerClass::fetchJson(JsonObject out) {
    Guard guard(_lock);
    switch (_fetchState) {
        case FetchState::Idle: out["state"] = "idle"; break;
        case FetchState::Running: out["state"] = "running"; break;
        case FetchState::Done:
            // Handed over once; then the memory is free again.
            out["state"] = "done";
            out["text"] = _fetchText;
            _fetchText = String();
            _fetchState = FetchState::Idle;
            break;
        case FetchState::Failed:
            out["state"] = "error";
            out["error"] = _fetchError;
            _fetchError = String();
            _fetchState = FetchState::Idle;
            break;
    }
}

size_t PluginManagerClass::pluginCount() {
    if (!_ready) return 0;
    Guard guard(_lock);
    return _plugins.size();
}

PluginResult PluginManagerClass::preview(const String& json, JsonObject out, String& error) {
    if (!_ready) {
        error = "Die Plugins werden noch gestartet";
        return PluginResult::Rejected;
    }
    PluginDef::Definition def;
    if (!checkPluginFile(json, def, error)) return PluginResult::Rejected;
    out["id"] = def.id;
    out["name"] = def.name;
    out["version"] = def.version;
    out["author"] = def.author;
    out["license"] = def.license;
    out["description"] = def.description;
    // The address as the plugin has it, with {setting} placeholders: what the person is agreeing to
    // is where the plugin may look, and the placeholders show what they will type in later.
    out["source_url"] = def.source.url;
    out["segment_setting"] = def.segmentKey;
    out["wants_power"] = def.wantsPower;
    out["has_script"] = def.hasScript;
    out["script_level"] = def.scriptLevel;
    String note;
    bool compatible = compatibility(def, note);
    out["compatible"] = compatible;
    out["compat_note"] = compatible ? String() : note;
    Guard guard(_lock);
    auto old = find(def.id);
    out["replaces"] = old != nullptr;
    out["installed_version"] = (old && old->valid) ? old->def.version : String();
    out["limit_reached"] = !old && _plugins.size() >= MAX_PLUGINS;
    return PluginResult::Ok;
}

namespace {
void putText(JsonObject parent, const char* key, const PluginDef::Text& text) {
    if (text.empty()) return;
    JsonObject o = parent[key].to<JsonObject>();
    if (text.de.length()) o["de"] = text.de;
    if (text.en.length()) o["en"] = text.en;
    if (text.ru.length()) o["ru"] = text.ru;
}
}  // namespace

bool PluginManagerClass::definitionJson(const String& id, JsonObject out) {
    if (!_ready) return false;
    Guard guard(_lock);
    auto p = find(id);
    if (!p) return false;
    out["id"] = p->id;
    out["valid"] = p->valid;
    if (!p->valid) return true;
    const PluginDef::Definition& def = p->def;
    out["name"] = def.name;
    out["segment_setting"] = def.segmentKey;
    out["wants_power"] = def.wantsPower;
    out["has_script"] = def.hasScript;
    out["source_url"] = def.source.url;
    JsonArray settings = out["settings"].to<JsonArray>();
    for (const PluginDef::Setting& s : def.settings) {
        JsonObject o = settings.add<JsonObject>();
        o["key"] = s.key;
        o["type"] = s.type;
        putText(o, "label", s.label);
        putText(o, "hint", s.hint);
        if (s.defaultValue.length()) o["default"] = s.defaultValue;
        o["optional"] = s.optional;
        if (s.hasRange) {
            o["min"] = s.minValue;
            o["max"] = s.maxValue;
        }
        if (s.type == "list") {
            JsonArray options = o["options"].to<JsonArray>();
            for (size_t i = 0; i < s.options.size(); i++) {
                JsonObject opt = options.add<JsonObject>();
                opt["value"] = s.options[i];
                if (i < s.optionLabels.size()) putText(opt, "label", s.optionLabels[i]);
            }
        }
    }
    // The effects a plugin may use (the ones named in an `effect` setting), by name as stored.
    JsonArray effects = out["effects"].to<JsonArray>();
    for (int e = 0; e < EFFECT_COUNT; e++) {
        if (!PluginDef::effectBlocked(e)) effects.add(EFFECT_NAMES[e]);
    }
    return true;
}

PluginResult PluginManagerClass::remove(const String& id, String& error) {
    if (!_ready) {
        error = "Die Plugins werden noch gestartet";
        return PluginResult::Rejected;
    }
    Guard guard(_lock);
    for (size_t i = 0; i < _plugins.size(); i++) {
        if (_plugins[i]->id != id) continue;
        if (_plugins[i]->appliedSegment >= 0) _pendingRelease.push_back(_plugins[i]->appliedSegment);
        queueScriptStop(*_plugins[i]);
        LittleFS.remove(definitionPath(id));
        LittleFS.remove(settingsPath(id));
        _plugins.erase(_plugins.begin() + i);
        Serial.printf("Plugins: removed %s\n", id.c_str());
        return PluginResult::Ok;
    }
    error = "Kein Plugin mit der id '" + id + "'";
    return PluginResult::NotFound;
}

PluginResult PluginManagerClass::setEnabled(const String& id, bool enabled, String& error) {
    if (!_ready) {
        error = "Die Plugins werden noch gestartet";
        return PluginResult::Rejected;
    }
    Guard guard(_lock);
    auto p = find(id);
    if (!p) {
        error = "Kein Plugin mit der id '" + id + "'";
        return PluginResult::NotFound;
    }
    if (enabled) {
        if (!p->valid) {
            error = "Das Plugin ist ungültig: " + p->reason;
            return PluginResult::Rejected;
        }
        if (p->incompatible && !p->force) {
            error = p->compatNote + " Wer es trotzdem versuchen will, schaltet \"Trotzdem ausführen\" ein.";
            return PluginResult::Rejected;
        }
        String problem = problemWith(*p, p->values);
        if (problem.length() > 0) {
            error = problem;
            return PluginResult::Rejected;
        }
    }
    p->enabled = enabled;
    refreshState(*p);
    if (!saveSettings(*p)) {
        error = "Die Einstellung konnte nicht gespeichert werden";
        return PluginResult::Rejected;
    }
    if (enabled) ensureTask();
    Serial.printf("Plugins: %s %s\n", id.c_str(), enabled ? "switched on" : "switched off");
    return PluginResult::Ok;
}

PluginResult PluginManagerClass::setOptions(const String& id, int8_t force, int8_t allowPower, String& error) {
    if (!_ready) {
        error = "Die Plugins werden noch gestartet";
        return PluginResult::Rejected;
    }
    Guard guard(_lock);
    auto p = find(id);
    if (!p) {
        error = "Kein Plugin mit der id '" + id + "'";
        return PluginResult::NotFound;
    }
    if (force >= 0) p->force = force == 1;
    if (allowPower >= 0) p->allowPower = allowPower == 1;
    refreshState(*p);
    if (p->enabled && p->valid) ensureTask();
    if (!saveSettings(*p)) {
        error = "Die Einstellung konnte nicht gespeichert werden";
        return PluginResult::Rejected;
    }
    return PluginResult::Ok;
}

PluginResult PluginManagerClass::setSettings(const String& id, JsonObjectConst values, String& error) {
    if (!_ready) {
        error = "Die Plugins werden noch gestartet";
        return PluginResult::Rejected;
    }
    Guard guard(_lock);
    auto p = find(id);
    if (!p) {
        error = "Kein Plugin mit der id '" + id + "'";
        return PluginResult::NotFound;
    }
    if (!p->valid) {
        error = "Das Plugin ist ungültig und hat keine Einstellungen";
        return PluginResult::Rejected;
    }
    std::vector<PluginRun::SettingValue> next = p->values;
    for (JsonPairConst kv : values) {
        const PluginDef::Setting* s = PluginDef::findSetting(p->def, kv.key().c_str());
        if (!s) {
            error = String("Das Plugin hat keine Einstellung '") + kv.key().c_str() + "'";
            return PluginResult::Rejected;
        }
        JsonVariantConst v = kv.value();
        String text;
        if (v.is<bool>()) text = v.as<bool>() ? "true" : "false";
        else if (v.is<const char*>()) text = v.as<const char*>();
        else if (v.is<double>()) text = PluginExpr::Value::number(v.as<double>()).toText();
        else if (!v.isNull()) {
            error = "Einstellung '" + s->key + "': Text, Zahl oder true/false erwartet";
            return PluginResult::Rejected;
        }
        // An empty password field means "leave the stored one", so it can be left blank when
        // other settings change.
        if (s->type == "password" && text.length() == 0) continue;
        String why;
        if (!validateValue(*s, text, why)) {
            error = "Einstellung '" + s->key + "': " + why;
            return PluginResult::Rejected;
        }
        for (PluginRun::SettingValue& sv : next) {
            if (sv.key == s->key) sv.value = text;
        }
    }
    if (p->enabled) {
        String problem = problemWith(*p, next);
        if (problem.length() > 0) {
            error = problem;
            return PluginResult::Rejected;
        }
    }
    p->values = next;
    refreshState(*p);
    if (!saveSettings(*p)) {
        error = "Die Einstellungen konnten nicht gespeichert werden";
        return PluginResult::Rejected;
    }
    return PluginResult::Ok;
}

uint32_t PluginManagerClass::taskStackFree() const {
    return _task ? (uint32_t)uxTaskGetStackHighWaterMark(_task) : 0;
}

void PluginManagerClass::listJson(JsonArray out) {
    if (!_ready) return;
    Guard guard(_lock);
    for (auto& p : _plugins) {
        JsonObject o = out.add<JsonObject>();
        o["id"] = p->id;
        o["state"] = stateName(p->state);
        o["reason"] = p->reason;
        o["enabled"] = p->enabled;
        if (!p->valid) continue;
        o["name"] = p->def.name;
        o["version"] = p->def.version;
        o["author"] = p->def.author;
        o["license"] = p->def.license;
        o["description"] = p->def.description;
        o["force"] = p->force;
        o["allow_power"] = p->allowPower;
        o["wants_power"] = p->def.wantsPower;
        o["script_level"] = p->def.scriptLevel;
        if (p->def.hasScript) {
            JsonObject script = o["script"].to<JsonObject>();
            script["mode"] = p->scriptInCharge ? "script" : "rules";
            script["note"] = p->scriptNote;
            if (p->scriptHaveStatus) {
                script["state"] = p->scriptStatus.state;
                script["message"] = p->scriptStatus.message;
                script["fps"] = p->scriptStatus.fps;
                script["frame_ms"] = p->scriptStatus.frameUs10 / 10.0;
                script["memory_kb"] = p->scriptStatus.memoryKb;
            }
        }
        o["warning"] = (p->incompatible && p->force) ? p->compatNote : String();
        JsonObject settings = o["settings"].to<JsonObject>();
        for (const PluginRun::SettingValue& sv : p->values) {
            const PluginDef::Setting* s = PluginDef::findSetting(p->def, sv.key);
            // A password never leaves the controller; the interface only learns whether there is one.
            if (s && s->type == "password") settings[sv.key] = sv.value.length() > 0 ? "***" : "";
            else settings[sv.key] = sv.value;
        }
        JsonObject data = o["data"].to<JsonObject>();
        for (size_t i = 0; i < p->def.values.size() && i < p->results.size(); i++) {
            const PluginExpr::Value& v = p->results[i];
            const String& name = p->def.values[i].name;
            if (!v.known()) data[name] = nullptr;
            else if (v.type == PluginExpr::Type::Number) data[name] = v.num;
            else if (v.type == PluginExpr::Type::Bool) data[name] = v.num != 0;
            else data[name] = v.text;
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Rules -> overlay (main loop, under _lock)

// Says on the serial port only when something changes, never on every pass.
void PluginManagerClass::setState(PluginInstance& p, PluginState state, const String& reason) {
    bool changed = p.state != state;
    p.state = state;
    p.reason = reason;
    if (changed) Serial.printf("Plugins: %s -> %s%s%s\n", p.id.c_str(), stateName(state), reason.length() ? ": " : "", reason.c_str());
}

void PluginManagerClass::recordFailure(PluginInstance& p, const String& why) {
    if (p.failures < 255) p.failures++;
    if (p.failures >= FAIL_LIMIT) setState(p, PluginState::NoConnection, why);
}

// Brings the segment in line with what the plugin wants to show right now: lays the overlay on,
// changes it, or lets go. The overlay is re-applied when LEDManager has lost it (segments rebuilt).
void PluginManagerClass::apply(PluginInstance& p) {
    int segment = -1;
    if (p.valid && p.def.segmentKey.length() > 0) {
        String text = PluginRun::settingText(p.values, p.def.segmentKey);
        if (text.length() > 0) segment = (int)text.toInt();
    }
    bool runnable = p.valid && p.enabled && segment >= 0 && segment < (int)LEDManager.getNumSegments() &&
                    (p.state == PluginState::Running || p.state == PluginState::NoConnection);

    SegmentOverlay want;
    bool wantControl = false;
    // A script is in charge while the answers arrive. Without answers (no connection) the plugin's
    // on_error applies, and the script is let go - it would only draw stale values.
    bool scriptWanted = runnable && p.def.hasScript && p.state == PluginState::Running;
    bool scriptInCharge = scriptWanted && driveScript(p, segment);
    p.scriptInCharge = scriptInCharge;
    if (!scriptWanted) {
        stopScript(p);
        if (!p.def.hasScript) p.scriptNote = "";
    }
    if (scriptInCharge) {
        want.script = true;
        wantControl = true;
    } else if (runnable) {
        PluginRun::InstanceScope scope(p.def, p.results, p.values);
        const PluginDef::Show* show = PluginRun::choose(p.def, scope, p.state == PluginState::NoConnection);
        if (show) {
            want = PluginRun::toOverlay(*show, p.values, scope, p.allowPower);
            wantControl = !PluginRun::overlayEmpty(want);
        }
    }
    // One segment, one plugin - also after a restored backup that put two on the same segment.
    if (wantControl) {
        for (const auto& other : _plugins) {
            if (other.get() != &p && other->appliedSegment == segment) {
                wantControl = false;
                break;
            }
        }
    }

    if (p.appliedSegment >= 0 && (!wantControl || p.appliedSegment != segment)) {
        LEDManager.clearPluginOverlay((uint8_t)p.appliedSegment);
        p.appliedSegment = -1;
        p.applied = SegmentOverlay();
    }
    if (wantControl && (p.appliedSegment < 0 || !LEDManager.isPluginControlled((uint8_t)segment) ||
                        !PluginRun::sameOverlay(want, p.applied))) {
        want.pluginId = p.id;
        want.pluginName = p.def.name;
        LEDManager.setPluginOverlay((uint8_t)segment, want);
        p.applied = want;
        p.appliedSegment = (int16_t)segment;
    }
}

// ---------------------------------------------------------------------------------------------
// Scripts (main loop, under _lock)

// Ends what the plugin's script uses on its segment.
void PluginManagerClass::stopScript(PluginInstance& p) {
    if (p.scriptSegment >= 0) {
        if (p.scriptOnSlave) SlaveManager.releaseScript(p.scriptSlaveId);
        else MasterScripts.stop((uint8_t)p.scriptSegment);
    }
    p.scriptSegment = -1;
    p.scriptCrc = 0;
    p.scriptRetryAt = 0;
    p.scriptSettingsSent.clear();
    p.scriptValuesSent.clear();
    p.scriptHaveStatus = false;
    p.scriptInCharge = false;
}

// For a plugin that is removed or replaced from the web interface's task: the script cannot be ended
// from there (it talks to the radio), so the main loop does it.
void PluginManagerClass::queueScriptStop(const PluginInstance& p) {
    if (p.scriptSegment < 0) return;
    ScriptStop stop;
    stop.segment = p.scriptSegment;
    stop.onSlave = p.scriptOnSlave;
    stop.slaveId = p.scriptSlaveId;
    _pendingScriptStop.push_back(stop);
}

// Makes the plugin's script run on the segment and keeps it supplied with the plugin's settings and
// values. True when the script is in charge: running, or about to start. False when it cannot run
// (an old Slave, an error in the script, no answer from the Slave): the plugin's rules take over for
// as long as that lasts, and p.scriptNote says why.
bool PluginManagerClass::driveScript(PluginInstance& p, int segment) {
    uint32_t now = millis();
    uint8_t slaveId = 0;
    bool onSlave = LEDManager.segmentIsSlave((uint8_t)segment, slaveId);
    if (p.scriptSegment >= 0 && (p.scriptSegment != segment || p.scriptOnSlave != onSlave || p.scriptSlaveId != slaveId)) {
        stopScript(p);  // the plugin moved to another segment
    }

    std::vector<Script::Item> settings = PluginRun::scriptSettings(p.def, p.values);
    std::vector<Script::Item> values = PluginRun::scriptValues(p.def, p.results);
    bool settingsChanged = !PluginRun::sameItems(settings, p.scriptSettingsSent);
    bool valuesChanged = !PluginRun::sameItems(values, p.scriptValuesSent);
    bool on = LEDManager.getPower((uint8_t)segment);
    uint8_t brightness = LEDManager.effectiveBrightness((uint8_t)segment);

    if (onSlave) {
        // The job lives in the Master's memory: after a restart it is gone, and the text is read again
        // and sent. While it stands, the checksum is all that is asked.
        uint32_t jobCrc = SlaveManager.scriptCrc(slaveId);
        if (jobCrc == 0 || jobCrc != p.scriptCrc || p.scriptSegment < 0) {
            if ((int32_t)(now - p.scriptRetryAt) < 0) return false;  // tried a moment ago; the note says why
            String script;
            if (!readScriptFromFile(p.id, script)) {
                p.scriptNote = "Das Skript konnte nicht gelesen werden";
                p.scriptRetryAt = now + 10000;
                return false;
            }
            uint32_t crc = esp_rom_crc32_le(0, (const uint8_t*)script.c_str(), script.length());
            SlaveManagerClass::ScriptStart result = SlaveManager.runScript(slaveId, script, brightness, on);
            if (result != SlaveManagerClass::ScriptStart::Ok) {
                switch (result) {
                    case SlaveManagerClass::ScriptStart::UnknownSlave: p.scriptNote = "Der Slave ist gerade nicht erreichbar"; break;
                    case SlaveManagerClass::ScriptStart::TooOld:
                        p.scriptNote = "Der Slave kann noch keine Skripte (Firmware 0.3.000 oder neuer nötig)";
                        break;
                    default: p.scriptNote = "Das Skript ist zu lang für den Slave"; break;
                }
                p.scriptRetryAt = now + (result == SlaveManagerClass::ScriptStart::UnknownSlave ? 2000 : 10000);
                p.scriptCrc = 0;
                return false;
            }
            p.scriptCrc = crc;
            p.scriptStartedAt = now;
            settingsChanged = valuesChanged = true;
        } else {
            SlaveManager.updateScript(slaveId, brightness, on);
        }
        if (settingsChanged || valuesChanged) {
            SlaveManager.setScriptValues(slaveId, settings, values);
            p.scriptSettingsSent = settings;
            p.scriptValuesSent = values;
        }
        unsigned long age = 0;
        p.scriptHaveStatus = SlaveManager.scriptStatus(slaveId, p.scriptStatus, age);
        p.scriptSegment = (int16_t)segment;
        p.scriptOnSlave = true;
        p.scriptSlaveId = slaveId;
        // The Slave reports every 5 s at most; one that has not said anything for a long while is not
        // running the script, whatever it was told.
        bool silent = p.scriptHaveStatus ? age > 15000 : (now - p.scriptStartedAt) > 15000;
        if (silent) {
            p.scriptNote = "Der Slave meldet sich nicht";
            return false;
        }
    } else {
        uint16_t w = 0, h = 0;
        LEDManager.scriptGeometry((uint8_t)segment, w, h);
        Script::Task* task = MasterScripts.ensure((uint8_t)segment, w, h);
        if (!task) {
            p.scriptNote = "Kein Speicher für das Skript";
            return false;
        }
        if (task->crc() == 0 || task->crc() != p.scriptCrc || p.scriptSegment < 0) {
            if ((int32_t)(now - p.scriptRetryAt) < 0) return false;
            String script;
            if (!readScriptFromFile(p.id, script)) {
                p.scriptNote = "Das Skript konnte nicht gelesen werden";
                p.scriptRetryAt = now + 10000;
                return false;
            }
            p.scriptCrc = esp_rom_crc32_le(0, (const uint8_t*)script.c_str(), script.length());
            task->setScript((const uint8_t*)script.c_str(), script.length(), p.scriptCrc);
            settingsChanged = valuesChanged = true;
        }
        if (settingsChanged || valuesChanged) {
            task->setValues(settings, values);
            p.scriptSettingsSent = settings;
            p.scriptValuesSent = values;
        }
        task->setOn(on);
        p.scriptStatus = task->status();
        p.scriptHaveStatus = true;
        p.scriptSegment = (int16_t)segment;
        p.scriptOnSlave = false;
        p.scriptSlaveId = 0;
    }

    // A script that failed is not in charge: the rules take over, and the reason is kept for the
    // interface. It keeps its slot, so that it is not started again every pass; changing the plugin's
    // settings or switching it off and on starts it afresh.
    if (p.scriptHaveStatus && p.scriptStatus.state == Script::Wire::STATE_FAILED) {
        p.scriptNote = "Das Skript ist fehlgeschlagen: " + p.scriptStatus.message;
        return false;
    }
    p.scriptNote = "";
    return true;
}

// ---------------------------------------------------------------------------------------------
// The plugin task: asks the sources

// One task for every source, so they are asked one after the other: each TLS connection needs
// tens of kilobytes of memory, and two at once could starve the web server. Core 1, below the main
// loop's priority; the stack holds the TLS handshake. It is only started once a plugin is switched
// on, so a controller without plugins does not pay its 10 KB of stack. Called with _lock held.
void PluginManagerClass::ensureTask() {
    if (_task) return;
    xTaskCreatePinnedToCore(&PluginManagerClass::taskEntry, "plugins", 10240, this, 1, &_task, 1);
}


void PluginManagerClass::taskEntry(void* self) {
    static_cast<PluginManagerClass*>(self)->taskLoop();
}

// The next plugin whose turn has come. It is held back for a minute while it is being asked, so
// nobody else picks it; fetchOne sets the real time afterwards.
std::shared_ptr<PluginInstance> PluginManagerClass::pickDue() {
    Guard guard(_lock);
    uint32_t now = millis();
    for (auto& p : _plugins) {
        if (!p->valid || !p->enabled) continue;
        if (p->incompatible && !p->force) continue;
        if ((int32_t)(now - p->nextFetchMs) < 0) continue;
        p->nextFetchMs = now + 60000UL;
        return p;
    }
    return nullptr;
}

void PluginManagerClass::taskLoop() {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(250));
        if (WiFi.status() != WL_CONNECTED) continue;
        if (ESP.getMaxAllocHeap() < MIN_FETCH_HEAP) {
            vTaskDelay(pdMS_TO_TICKS(2000));  // let memory recover before another TLS connection
            continue;
        }
        std::shared_ptr<PluginInstance> due = pickDue();
        if (due) fetchOne(due);
    }
}

void PluginManagerClass::fetchOne(const std::shared_ptr<PluginInstance>& p) {
    // The settings can change under us, so work on a copy. The definition never changes: an
    // update installs a new instance instead.
    std::vector<PluginRun::SettingValue> settings;
    {
        Guard guard(_lock);
        settings = p->values;
    }
    auto lookup = [&settings](const String& key) { return PluginRun::settingText(settings, key); };

    PluginHttp::Request request;
    request.url = oneLine(PluginDef::expand(p->def.source.url, lookup));
    request.timeoutMs = p->def.source.timeoutMs;
    request.followRedirects = true;
    for (const PluginDef::Header& h : p->def.source.headers) {
        String value = oneLine(PluginDef::expand(h.value, lookup));
        if (value.length() == 0) continue;  // e.g. an optional API key left empty
        PluginHttp::Header header;
        header.name = h.name;
        header.value = value;
        request.headers.push_back(header);
    }

    PluginHttp::Response response;
    bool ok = PluginHttp::get(request, response);

    std::vector<PluginExpr::Value> results;
    String raw;
    String problem;  // why this attempt failed; "" when it did not
    bool mismatch = false;
    if (!ok) {
        problem = describeError(response.error, response.status);
    } else {
        JsonDocument filter;
        PluginJson::buildFilter(p->def.responsePaths, filter);
        JsonDocument doc;
        DeserializationError parseError = deserializeJson(doc, response.body, DeserializationOption::Filter(filter));
        raw = response.body.substring(0, 1024);  // for the live view in the interface
        response.body = String();  // the answer is no longer needed; give its memory back
        if (parseError) {
            problem = "Die Antwort der Quelle ist kein JSON";
        } else {
            PluginJson::ResponseScope scope(doc);
            size_t known = 0;
            for (const PluginDef::ValueDef& v : p->def.values) {
                PluginExpr::Value value = v.expr.run(scope);
                if (value.known()) known++;
                results.push_back(value);
            }
            mismatch = !p->def.values.empty() && known == 0;
        }
    }

    Guard guard(_lock);
    if (!p->enabled) return;  // switched off while the request ran: the answer is of no use
    uint32_t now = millis();
    uint16_t every = p->def.source.every;

    if (problem.length() > 0) {
        recordFailure(*p, problem);
        p->nextFetchMs = now + (uint32_t)(every > MIN_RETRY_SECONDS ? every : MIN_RETRY_SECONDS) * 1000UL;
        return;
    }
    p->nextFetchMs = now + (uint32_t)every * 1000UL;
    p->results = results;
    p->lastRaw = raw;
    p->haveData = true;
    p->failures = 0;

    if (!mismatch) {
        p->mismatches = 0;
    } else if (++p->mismatches >= MISMATCH_LIMIT) {
        const String why = "Die Antwort enthält nichts von dem, was das Plugin liest - passt die Quelle zum Plugin?";
        if (p->incompatible) {
            // It runs although the firmware said it would not fit; switched off rather than left to
            // show nonsense.
            p->enabled = false;
            saveSettings(*p);
            setState(*p, PluginState::Off, "Abgeschaltet: " + why);
        } else {
            p->failures = FAIL_LIMIT;
            setState(*p, PluginState::NoConnection, why);
        }
        return;
    }
    if (p->state != PluginState::Running) {
        setState(*p, PluginState::Running, "");
        // Said once per plugin start: how close the task came to its stack limit, so a plugin
        // that needs more is noticed in testing and not by a crash in a living room.
        Serial.printf("Plugins: %s answers; plugin task stack headroom %u bytes\n", p->id.c_str(),
                      (unsigned)uxTaskGetStackHighWaterMark(nullptr));
    }
}

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

// The Home Assistant entities besides the lights (see MqttManager.h):
//
//   <base>/seg<N>/ext              speed, intensity, palette, background and the texts of segment N
//   <base>/seg<N>/<what>/set       speed | intensity | palette | bg | w<element id>
//   <base>/sys                     current scene, playlist, sync
//   <base>/scene|playlist|sync/set
//   <base>/diag                    Wi-Fi signal, memory, IP, start time, link failures
//   <base>/slave<id>               whether a Slave is connected, with its firmware
//   <base>/button<n>               events of the physical buttons (not retained)
//   <base>/update, /update/set     firmware version and installing an update
//   <base>/restart/set             restart
//   <base>/statusled, /statusled/set   the onboard status LED as a light
#include "MqttManager.h"
#include "Config.h"
#include "LEDManager.h"
#include "SlaveManager.h"
#include "ButtonManager.h"
#include "StatusLedManager.h"
#include "UpdateManager.h"
#include "WiFiManager.h"
#include <WiFi.h>
#include <time.h>
#include <algorithm>

namespace {
// The names the web interface shows.
const char* const PALETTE_LABELS[PALETTE_COUNT] = {"Solid", "Regenbogen", "Feuer", "Ozean", "Wald"};
// The background effects the web interface offers, calm ones first (see BG_CALM_EFFECTS in app.js).
const uint8_t BACKGROUND_EFFECTS[] = {24, 21, 20, 13, 2, 27, 9, 1, 0,
                                      4, 23, 26, 28, 22, 15, 16, 17, 14, 7, 8, 3, 5, 6, 12, 18, 19, 11};
const char* const NO_BACKGROUND = "Kein Hintergrund";
const char* const BUTTON_EVENT_NAMES[] = {"kurz", "lang", "umgelegt"};

constexpr unsigned long DIAG_INTERVAL_MS = 60000;
// The newest release is looked up a minute after connecting, then twice a day.
constexpr unsigned long LATEST_FIRST_MS = 60000;
constexpr unsigned long LATEST_INTERVAL_MS = 12UL * 3600UL * 1000UL;

struct Fnv {
    uint32_t h = 2166136261u;
    void add(const void* data, size_t len) {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < len; i++) { h ^= p[i]; h *= 16777619u; }
    }
    template <typename T> void add(const T& v) { add(&v, sizeof(v)); }
    void add(const String& s) { add(s.c_str(), s.length() + 1); }
};

// Speed and intensity are 0-255 inside, percent in the web interface - and so in Home Assistant.
uint8_t toPercent(uint8_t v) { return (v * 100 + 127) / 255; }
uint8_t fromPercent(long p) {
    if (p < 0) p = 0;
    if (p > 100) p = 100;
    return (uint8_t)((p * 255 + 50) / 100);
}

// "None" is what Home Assistant's MQTT sensors and selects read as "unknown".
String isoTime(uint32_t t) {
    if (t == 0) return "None";
    time_t tt = t;
    struct tm tm;
    gmtime_r(&tt, &tm);
    char buf[24];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return String(buf);
}

bool clockIsSet() { return time(nullptr) > 1700000000; }

int onOff(String s) {
    s.trim();
    s.toUpperCase();
    if (s == "ON" || s == "TRUE" || s == "1") return 1;
    if (s == "OFF" || s == "FALSE" || s == "0") return 0;
    return -1;
}

String segmentName(const Segment& seg, uint8_t i) {
    return seg.name.length() ? seg.name : String("Segment ") + String(i + 1);
}
}  // namespace

// ---------------------------------------------------------------------------
// Discovery
// ---------------------------------------------------------------------------

String MqttManagerClass::componentTopic(const char* component, const String& suffix) const {
    return String("homeassistant/") + component + "/hyperled_" + _macAddress + "_" + suffix + "/config";
}

void MqttManagerClass::beginEntity(JsonDocument& doc, const String& suffix, const String& name) const {
    doc["name"] = name;
    doc["unique_id"] = "hyperled_" + _macAddress + "_" + suffix;
    doc["~"] = _base;
    doc["availability_topic"] = "~/status";
    addDevice(doc);
}

void MqttManagerClass::announce(const char* component, const String& suffix, JsonDocument& doc,
                                std::vector<String>& seen) {
    String payload;
    serializeJson(doc, payload);
    publish(componentTopic(component, suffix), payload);
    seen.push_back(String(component) + ":" + suffix);
}

void MqttManagerClass::loadAnnouncedExtras() {
    if (_extrasLoaded) return;
    _extrasLoaded = true;
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, true);
    String stored = prefs.getString(PREF_MQTT_EXTRAS, "");
    prefs.end();
    int start = 0;
    while (start < (int)stored.length()) {
        int end = stored.indexOf('|', start);
        if (end < 0) end = stored.length();
        if (end > start) _announcedExtras.push_back(stored.substring(start, end));
        start = end + 1;
    }
}

void MqttManagerClass::refreshPresets() {
    if (PresetManager.revision() == _presetRevision) return;
    _presetRevision = PresetManager.revision();
    PresetManager.listPresets(_presets);
}

// A scene's name as an option of the scene select. Options must be unique, so a name that
// several presets share gets the preset's number added.
String MqttManagerClass::sceneOption(uint8_t presetId) const {
    String name;
    bool shared = false;
    for (const auto& p : _presets) {
        if (p.id == presetId) name = p.name;
    }
    if (name.isEmpty()) return String("Szene ") + String(presetId);
    for (const auto& p : _presets) {
        if (p.id != presetId && p.name == name) shared = true;
    }
    return shared ? name + " (" + String(presetId) + ")" : name;
}

void MqttManagerClass::relevantSlaves(std::vector<uint8_t>& ids) {
    ids.clear();
    uint8_t numSegs = LEDManager.getNumSegments();
    for (uint8_t i = 0; i < numSegs; i++) {
        const Segment* seg = LEDManager.getSegment(i);
        if (seg && seg->isSlave && seg->slaveId != 254) ids.push_back(seg->slaveId);
    }
    for (const auto& kv : _slaveCache) {
        if (kv.first != 254) ids.push_back(kv.first);
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
}

uint32_t MqttManagerClass::extrasSignature() {
    Fnv f;
    uint8_t numSegs = LEDManager.getNumSegments();
    for (uint8_t i = 0; i < numSegs; i++) {
        const Segment* seg = LEDManager.getSegment(i);
        if (!seg) continue;
        for (const auto& tw : seg->textWidgets) {
            f.add(tw.id);
            f.add(tw.type);
        }
        f.add((uint8_t)0xFF);
    }
    f.add(PresetManager.revision());
    f.add(PresetManager.hasPlaylist());
    std::vector<uint8_t> slaves;
    relevantSlaves(slaves);
    for (uint8_t id : slaves) {
        f.add(id);
        auto it = _slaveCache.find(id);
        if (it != _slaveCache.end()) f.add(it->second.name);
    }
    for (uint8_t b = 0; b < 2; b++) {
        f.add(ButtonManager.isEnabled(b));
        f.add(ButtonManager.isSwitch(b));
    }
    return f.h;
}

// Announcing everything at once took about a third of a second on the main loop - the LEDs
// stood still for it on every connect and every change. So it goes out one section per loop
// pass: first each segment's entities, then the controller's own, then the clean-up.
void MqttManagerClass::startExtraDiscovery() {
    loadAnnouncedExtras();
    refreshPresets();
    _extraSeen.clear();
    _extraStage = 1;
}

void MqttManagerClass::publishExtraStage() {
    std::vector<String>& seen = _extraSeen;
    uint8_t numSegs = LEDManager.getNumSegments();
    uint8_t stage = _extraStage++;

    if (stage <= numSegs) {
        uint8_t i = stage - 1;
        const Segment* seg = LEDManager.getSegment(i);
        if (!seg) return;
        SegInfo info = segmentInfo(*seg);
        String s = "seg" + String(i);
        String name = segmentName(*seg, i);
        String extTopic = "~/" + s + "/ext";

        if (info.caps != CAP_ONOFF) {
            const char* labels[2] = {" Geschwindigkeit", " Intensität"};
            const char* keys[2] = {"speed", "intensity"};
            const char* icons[2] = {"mdi:speedometer", "mdi:tune-variant"};
            for (int k = 0; k < 2; k++) {
                JsonDocument d;
                beginEntity(d, s + "_" + keys[k], name + labels[k]);
                d["command_topic"] = "~/" + s + "/" + keys[k] + "/set";
                d["state_topic"] = extTopic;
                d["value_template"] = String("{{ value_json.") + keys[k] + " }}";
                d["min"] = 0;
                d["max"] = 100;
                d["step"] = 1;
                d["unit_of_measurement"] = "%";
                d["mode"] = "slider";
                d["icon"] = icons[k];
                d["entity_category"] = "config";
                announce("number", s + "_" + keys[k], d, seen);
            }

            JsonDocument d;
            beginEntity(d, s + "_palette", name + " Palette");
            d["command_topic"] = "~/" + s + "/palette/set";
            d["state_topic"] = extTopic;
            d["value_template"] = "{{ value_json.palette }}";
            JsonArray options = d["options"].to<JsonArray>();
            for (const char* label : PALETTE_LABELS) options.add(label);
            d["icon"] = "mdi:palette";
            d["entity_category"] = "config";
            announce("select", s + "_palette", d, seen);
        }

        if (info.matrix) {
            JsonDocument d;
            beginEntity(d, s + "_bg", name + " Hintergrund");
            d["command_topic"] = "~/" + s + "/bg/set";
            d["state_topic"] = extTopic;
            d["value_template"] = "{{ value_json.bg }}";
            JsonArray options = d["options"].to<JsonArray>();
            options.add(NO_BACKGROUND);
            for (uint8_t e : BACKGROUND_EFFECTS) options.add(EFFECT_NAMES[e]);
            d["icon"] = "mdi:image-filter-hdr";
            announce("select", s + "_bg", d, seen);
        }

        // One text entity per text and Lauftext element, numbered when a segment has several.
        uint8_t totals[2] = {0, 0};
        for (const auto& tw : seg->textWidgets) {
            if (tw.type == 2) totals[0]++;
            if (tw.type == 6) totals[1]++;
        }
        uint8_t counts[2] = {0, 0};
        for (const auto& tw : seg->textWidgets) {
            if (tw.type != 2 && tw.type != 6) continue;
            uint8_t k = tw.type == 6 ? 1 : 0;
            counts[k]++;
            String label = name + (k ? " Lauftext" : " Text");
            if (totals[k] > 1) label += " " + String(counts[k]);
            String suffix = s + "_w" + String(tw.id);
            JsonDocument d;
            beginEntity(d, suffix, label);
            d["command_topic"] = "~/" + s + "/w" + String(tw.id) + "/set";
            d["state_topic"] = extTopic;
            d["value_template"] = "{{ value_json.w" + String(tw.id) + " }}";
            d["min"] = 0;
            d["max"] = 64;
            d["mode"] = "text";
            d["icon"] = k ? "mdi:message-text-fast" : "mdi:form-textbox";
            announce("text", suffix, d, seen);
        }
        return;
    }

    if (stage == numSegs + 1) {
    if (!_presets.empty()) {
        JsonDocument d;
        beginEntity(d, "scene", "Szene");
        d["command_topic"] = "~/scene/set";
        d["state_topic"] = "~/sys";
        d["value_template"] = "{{ value_json.scene }}";
        JsonArray options = d["options"].to<JsonArray>();
        for (const auto& p : _presets) options.add(sceneOption(p.id));
        d["icon"] = "mdi:palette-swatch-variant";
        announce("select", "scene", d, seen);
    }
    if (PresetManager.hasPlaylist()) {
        JsonDocument d;
        beginEntity(d, "playlist", "Playlist");
        d["command_topic"] = "~/playlist/set";
        d["state_topic"] = "~/sys";
        d["value_template"] = "{{ value_json.playlist }}";
        d["icon"] = "mdi:playlist-play";
        announce("switch", "playlist", d, seen);
    }
    if (numSegs > 1) {
        JsonDocument d;
        beginEntity(d, "sync", "Gleichlauf");
        d["command_topic"] = "~/sync/set";
        d["state_topic"] = "~/sys";
        d["value_template"] = "{{ value_json.sync }}";
        d["icon"] = "mdi:link-variant";
        announce("switch", "sync", d, seen);
    }
    return;
    }

    if (stage == numSegs + 2) {
    // Diagnostics.
    struct SensorDef {
        const char* suffix;
        const char* name;
        const char* key;
        const char* deviceClass;
        const char* unit;
        const char* stateClass;
        const char* icon;
    };
    const SensorDef sensors[] = {
        {"rssi", "WLAN-Signal", "rssi", "signal_strength", "dBm", "measurement", nullptr},
        {"heap", "Freier Speicher", "heap", nullptr, "kB", "measurement", "mdi:memory"},
        {"ip", "IP-Adresse", "ip", nullptr, nullptr, nullptr, "mdi:ip-network"},
        {"boot", "Gestartet", "boot", "timestamp", nullptr, nullptr, nullptr},
        {"linkfails", "WLAN-Ausfälle", "linkfails", nullptr, nullptr, "total_increasing", "mdi:wifi-alert"},
        {"linkfail_at", "Letzter WLAN-Ausfall", "linkfail_at", "timestamp", nullptr, nullptr, "mdi:wifi-alert"},
    };
    for (const SensorDef& def : sensors) {
        JsonDocument d;
        beginEntity(d, def.suffix, def.name);
        d["state_topic"] = "~/diag";
        d["value_template"] = String("{{ value_json.") + def.key + " }}";
        if (def.deviceClass) d["device_class"] = def.deviceClass;
        if (def.unit) d["unit_of_measurement"] = def.unit;
        if (def.stateClass) d["state_class"] = def.stateClass;
        if (def.icon) d["icon"] = def.icon;
        d["entity_category"] = "diagnostic";
        announce("sensor", def.suffix, d, seen);
    }
    return;
    }

    if (stage == numSegs + 3) {
    // Slaves: every Slave a segment uses or that has reported in.
    std::vector<uint8_t> slaves;
    relevantSlaves(slaves);
    for (uint8_t id : slaves) {
        // Its own name once it has reported in; until then the name of a segment on it.
        auto it = _slaveCache.find(id);
        String name = (it != _slaveCache.end() && it->second.name.length() && it->second.name != "Unknown")
                          ? it->second.name
                          : String("Slave ") + String(id);
        if (it == _slaveCache.end()) {
            for (uint8_t i = 0; i < numSegs; i++) {
                const Segment* seg = LEDManager.getSegment(i);
                if (seg && seg->isSlave && seg->slaveId == id && seg->name.length()) {
                    name = seg->name;
                    break;
                }
            }
        }
        String suffix = "slave" + String(id);
        JsonDocument d;
        beginEntity(d, suffix, name + " Verbindung");
        d["state_topic"] = "~/" + suffix;
        d["value_template"] = "{{ value_json.online }}";
        d["json_attributes_topic"] = "~/" + suffix;
        d["json_attributes_template"] =
            "{{ {'Firmware': value_json.version, 'Verbindung': value_json.link, 'ID': value_json.id} | tojson }}";
        d["device_class"] = "connectivity";
        d["entity_category"] = "diagnostic";
        announce("binary_sensor", suffix, d, seen);
    }
    return;
    }

    if (stage == numSegs + 4) {
    for (uint8_t b = 0; b < 2; b++) {
        if (!ButtonManager.isEnabled(b)) continue;
        String suffix = "button" + String(b + 1);
        JsonDocument d;
        beginEntity(d, suffix, "Taster " + String(b + 1));
        d["state_topic"] = "~/" + suffix;
        JsonArray types = d["event_types"].to<JsonArray>();
        if (ButtonManager.isSwitch(b)) {
            types.add(BUTTON_EVENT_NAMES[BUTTON_FLIPPED]);
        } else {
            types.add(BUTTON_EVENT_NAMES[BUTTON_SHORT]);
            types.add(BUTTON_EVENT_NAMES[BUTTON_LONG]);
        }
        d["device_class"] = "button";
        announce("event", suffix, d, seen);
    }

    {
        JsonDocument d;
        beginEntity(d, "firmware", "Firmware");
        d["state_topic"] = "~/update";
        d["command_topic"] = "~/update/set";
        d["payload_install"] = "install";
        d["device_class"] = "firmware";
        d["entity_category"] = "config";
        announce("update", "firmware", d, seen);
    }
    {
        JsonDocument d;
        beginEntity(d, "restart", "Neustart");
        d["command_topic"] = "~/restart/set";
        d["payload_press"] = "PRESS";
        d["device_class"] = "restart";
        d["entity_category"] = "config";
        announce("button", "restart", d, seen);
    }
    {
        JsonDocument d;
        beginEntity(d, "statusled", "Status-LED");
        d["schema"] = "json";
        d["state_topic"] = "~/statusled";
        d["command_topic"] = "~/statusled/set";
        d["supported_color_modes"][0] = "rgb";
        d["brightness"] = true;
        d["icon"] = "mdi:led-on";
        d["entity_category"] = "config";
        announce("light", "statusled", d, seen);
    }
    return;
    }

    // Last stage. Entities that no longer exist - a deleted element, a Slave that is gone, a button switched
    // off - are removed from Home Assistant.
    for (const String& old : _announcedExtras) {
        if (std::find(seen.begin(), seen.end(), old) != seen.end()) continue;
        int colon = old.indexOf(':');
        if (colon > 0) publish(componentTopic(old.substring(0, colon).c_str(), old.substring(colon + 1)), "");
    }
    if (seen != _announcedExtras && _client.connected()) {
        String joined;
        for (const String& e : seen) {
            if (joined.length()) joined += '|';
            joined += e;
        }
        Preferences prefs;
        prefs.begin(PREF_NAMESPACE, false);
        prefs.putString(PREF_MQTT_EXTRAS, joined);
        prefs.end();
        _announcedExtras = seen;
    }

    // Everything announced gets its state again.
    _extSigs.clear();
    _sysSig = _slaveSig = _updateSig = _statusLedSig = 0;
    _lastDiag = 0;
    _extraSeen.clear();
    _extraStage = 0;
}

void MqttManagerClass::removeExtras() {
    loadAnnouncedExtras();
    for (const String& old : _announcedExtras) {
        int colon = old.indexOf(':');
        if (colon > 0) publish(componentTopic(old.substring(0, colon).c_str(), old.substring(colon + 1)), "");
    }
    uint8_t numSegs = max(_announcedSegs, LEDManager.getNumSegments());
    for (uint8_t i = 0; i < numSegs; i++) publish(segTopic(i) + "/ext", "");
    std::vector<uint8_t> slaves;
    relevantSlaves(slaves);
    for (uint8_t id : slaves) publish(_base + "/slave" + String(id), "");
    for (const char* t : {"/sys", "/diag", "/update", "/statusled"}) publish(_base + t, "");
    if (_client.connected()) {
        _announcedExtras.clear();
        Preferences prefs;
        prefs.begin(PREF_NAMESPACE, false);
        prefs.remove(PREF_MQTT_EXTRAS);
        prefs.end();
    }
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

void MqttManagerClass::handleExtraCommand(const String& rest, const uint8_t* payload, unsigned int length) {
    String value;
    value.reserve(length);
    for (unsigned int i = 0; i < length; i++) value += (char)payload[i];

    if (rest == "scene/set") {
        refreshPresets();
        for (const auto& p : _presets) {
            if (sceneOption(p.id) == value) {
                PresetManager.applyPreset(p.id);
                break;
            }
        }
    } else if (rest == "playlist/set") {
        int on = onOff(value);
        if (on >= 0) PresetManager.setPlaylistEnabled(on == 1);
    } else if (rest == "sync/set") {
        int on = onOff(value);
        if (on >= 0) LEDManager.setSync(on == 1);
    } else if (rest == "restart/set") {
        if (value == "PRESS") WiFiManager.requestRestart(500);
    } else if (rest == "update/set") {
        const String& latest = UpdateManager.latestVersion();
        if (value == "install" && latest.length() && !UpdateManager.isUpdating() &&
            UpdateManagerClass::compareVersions(latest, SOFTWARE_VERSION) > 0) {
            UpdateManager.startOnlineUpdate(latest);
            _updateSig = 0;
        }
    } else if (rest == "statusled/set") {
        JsonDocument doc;
        if (deserializeJson(doc, payload, length) != DeserializationError::Ok) return;
        bool on = StatusLedManager.isOn();
        uint32_t color = StatusLedManager.getColor();
        uint8_t bri = StatusLedManager.getBrightness();
        if (!doc["state"].isNull()) {
            int p = onOff(doc["state"].as<String>());
            if (p >= 0) on = p == 1;
        }
        if (!doc["brightness"].isNull()) bri = doc["brightness"].as<uint8_t>();
        if (!doc["color"].isNull()) {
            color = ((uint32_t)doc["color"]["r"].as<uint8_t>() << 16) |
                    ((uint32_t)doc["color"]["g"].as<uint8_t>() << 8) | doc["color"]["b"].as<uint8_t>();
        }
        StatusLedManager.setState(on, color, bri);
    } else if (rest.startsWith("seg")) {
        int slash = rest.indexOf('/');
        if (slash < 4) return;
        String idStr = rest.substring(3, slash);
        for (size_t i = 0; i < idStr.length(); i++) {
            if (!isDigit(idStr[i])) return;
        }
        long segId = idStr.toInt();
        if (segId >= LEDManager.getNumSegments()) return;
        String what = rest.substring(slash + 1);

        if (what == "speed/set") {
            LEDManager.setSpeed(segId, fromPercent(value.toInt()));
        } else if (what == "intensity/set") {
            LEDManager.setIntensity(segId, fromPercent(value.toInt()));
        } else if (what == "palette/set") {
            for (uint8_t p = 0; p < PALETTE_COUNT; p++) {
                if (value == PALETTE_LABELS[p]) LEDManager.setPalette(segId, p);
            }
        } else if (what == "bg/set") {
            uint8_t effect = HYPERBUS_BACKGROUND_NONE;
            bool known = value == NO_BACKGROUND;
            for (uint8_t e : BACKGROUND_EFFECTS) {
                if (value == EFFECT_NAMES[e]) {
                    effect = e;
                    known = true;
                }
            }
            if (known) {
                JsonDocument bg;
                bg["effect"] = effect;
                LEDManager.setPanelBackground(segId, bg.as<JsonObject>());
            }
        } else if (what.startsWith("w") && what.endsWith("/set")) {
            long widgetId = what.substring(1, what.length() - 4).toInt();
            if (widgetId > 0 && widgetId < 256) LEDManager.setTextWidgetText(segId, (uint8_t)widgetId, value);
        }
    }
    _lastStateCheck = 0;  // report the result on the next pass
}

void MqttManagerClass::buttonEvent(uint8_t index, ButtonEvent event) {
    if (!_enabled || index > 1 || _buttonQueueLen >= sizeof(_buttonQueue)) return;
    _buttonQueue[_buttonQueueLen++] = (uint8_t)((index << 4) | event);
}

// ---------------------------------------------------------------------------
// States
// ---------------------------------------------------------------------------

void MqttManagerClass::loopExtras(bool force) {
    unsigned long now = millis();
    publishSegmentExtras(force);
    publishSysState(force);
    publishStatusLed(force);
    if (force) publishSlaves(true);
    if (force || _lastDiag == 0 || now - _lastDiag >= DIAG_INTERVAL_MS) publishDiagnostics();

    bool due = _latestChecked ? now - _lastLatestCheck >= LATEST_INTERVAL_MS
                              : now - _connectedAt >= LATEST_FIRST_MS;
    if (due) {
        UpdateManager.requestLatestCheck();
        _latestChecked = true;
        _lastLatestCheck = now;
    }
    publishUpdateState(force);
    flushButtonEvents();
}

void MqttManagerClass::publishSegmentExtras(bool force) {
    uint8_t numSegs = LEDManager.getNumSegments();
    if (_extSigs.size() != numSegs) {
        _extSigs.assign(numSegs, 0);
        force = true;
    }
    for (uint8_t i = 0; i < numSegs; i++) {
        const Segment* seg = LEDManager.getSegment(i);
        if (!seg) continue;
        SegInfo info = segmentInfo(*seg);
        Fnv f;
        f.add(seg->speed);
        f.add(seg->intensity);
        f.add(seg->palette);
        f.add(seg->background.effect);
        f.add(info.matrix);
        for (const auto& tw : seg->textWidgets) {
            if (tw.type != 2 && tw.type != 6) continue;
            f.add(tw.id);
            f.add(tw.text);
        }
        if (!force && f.h == _extSigs[i]) continue;

        JsonDocument doc;
        doc["speed"] = toPercent(seg->speed);
        doc["intensity"] = toPercent(seg->intensity);
        doc["palette"] = PALETTE_LABELS[seg->palette < PALETTE_COUNT ? seg->palette : 0];
        if (info.matrix) {
            const char* bg = NO_BACKGROUND;
            for (uint8_t e : BACKGROUND_EFFECTS) {
                if (seg->background.active() && seg->background.effect == e) bg = EFFECT_NAMES[e];
            }
            doc["bg"] = bg;
        }
        for (const auto& tw : seg->textWidgets) {
            if (tw.type == 2 || tw.type == 6) doc["w" + String(tw.id)] = tw.text;
        }
        String payload;
        serializeJson(doc, payload);
        if (publish(segTopic(i) + "/ext", payload)) _extSigs[i] = f.h;
    }
}

void MqttManagerClass::publishSysState(bool force) {
    // The scene select shows the scene applied last - until anything about the lights changes.
    Fnv lights;
    for (uint32_t sig : _stateSigs) lights.add(sig);
    if (PresetManager.applyCount() != _seenApplyCount) {
        _seenApplyCount = PresetManager.applyCount();
        _sceneId = PresetManager.lastApplied();
        _sceneSig = lights.h;
    } else if (_sceneId && lights.h != _sceneSig) {
        _sceneId = 0;
    }

    refreshPresets();
    String scene = _sceneId ? sceneOption(_sceneId) : String("None");
    bool playlist = PresetManager.playlistEnabled();
    bool sync = LEDManager.getSync();
    Fnv f;
    f.add(scene);
    f.add(playlist);
    f.add(sync);
    if (!force && f.h == _sysSig) return;

    JsonDocument doc;
    doc["scene"] = scene;
    doc["playlist"] = playlist ? "ON" : "OFF";
    doc["sync"] = sync ? "ON" : "OFF";
    String payload;
    serializeJson(doc, payload);
    if (publish(_base + "/sys", payload)) _sysSig = f.h;
}

void MqttManagerClass::publishDiagnostics() {
    static uint32_t bootTime = 0;
    if (bootTime == 0 && clockIsSet()) bootTime = (uint32_t)(time(nullptr) - millis() / 1000);

    JsonDocument doc;
    doc["rssi"] = WiFi.RSSI();
    doc["heap"] = ESP.getFreeHeap() / 1024;
    doc["ip"] = WiFi.localIP().toString();
    doc["boot"] = isoTime(bootTime);
    doc["linkfails"] = WiFiManager.getLinkFailureCount();
    doc["linkfail_at"] = isoTime(WiFiManager.getLastLinkFailureTime());
    String payload;
    serializeJson(doc, payload);
    if (publish(_base + "/diag", payload)) _lastDiag = millis();
}

void MqttManagerClass::publishSlaves(bool force) {
    std::vector<uint8_t> ids;
    relevantSlaves(ids);
    Fnv f;
    for (uint8_t id : ids) {
        f.add(id);
        f.add(SlaveManager.slaveIsOnline(id));
        auto it = _slaveCache.find(id);
        if (it != _slaveCache.end()) f.add(it->second.version);
    }
    if (!force && f.h == _slaveSig) return;

    bool ok = true;
    for (uint8_t id : ids) {
        auto it = _slaveCache.find(id);
        JsonDocument doc;
        doc["online"] = SlaveManager.slaveIsOnline(id) ? "ON" : "OFF";
        doc["id"] = id;
        doc["version"] = it != _slaveCache.end() ? it->second.version : String("unbekannt");
        doc["link"] = it == _slaveCache.end() ? "unbekannt" : (it->second.wireless ? "Funk" : "Kabel");
        String payload;
        serializeJson(doc, payload);
        ok = publish(_base + "/slave" + String(id), payload) && ok;
    }
    if (ok) _slaveSig = f.h;
}

void MqttManagerClass::publishUpdateState(bool force) {
    const String& latest = UpdateManager.latestVersion();
    bool inProgress = UpdateManager.isUpdating();
    Fnv f;
    f.add(latest);
    f.add(inProgress);
    if (!force && f.h == _updateSig) return;

    JsonDocument doc;
    doc["installed_version"] = SOFTWARE_VERSION;
    if (latest.length()) {
        doc["latest_version"] = latest;
        doc["release_url"] = "https://github.com/KaelanTesseract/HyperLED/releases/tag/" + latest;
    }
    doc["title"] = "HyperLED";
    doc["in_progress"] = inProgress;
    String payload;
    serializeJson(doc, payload);
    if (publish(_base + "/update", payload)) _updateSig = f.h;
}

void MqttManagerClass::publishStatusLed(bool force) {
    bool on = StatusLedManager.isOn();
    uint32_t color = StatusLedManager.getColor();
    uint8_t bri = StatusLedManager.getBrightness();
    Fnv f;
    f.add(on);
    f.add(color);
    f.add(bri);
    if (!force && f.h == _statusLedSig) return;

    JsonDocument doc;
    doc["state"] = on ? "ON" : "OFF";
    doc["brightness"] = bri;
    doc["color_mode"] = "rgb";
    doc["color"]["r"] = (color >> 16) & 0xFF;
    doc["color"]["g"] = (color >> 8) & 0xFF;
    doc["color"]["b"] = color & 0xFF;
    String payload;
    serializeJson(doc, payload);
    if (publish(_base + "/statusled", payload)) _statusLedSig = f.h;
}

void MqttManagerClass::flushButtonEvents() {
    for (uint8_t i = 0; i < _buttonQueueLen; i++) {
        uint8_t index = _buttonQueue[i] >> 4;
        uint8_t event = _buttonQueue[i] & 0x0F;
        if (event > BUTTON_FLIPPED) continue;
        String payload = String("{\"event_type\":\"") + BUTTON_EVENT_NAMES[event] + "\"}";
        // Not retained: an event is a moment, not a state to replay to a restarted Home Assistant.
        publish(_base + "/button" + String(index + 1), payload, false);
    }
    _buttonQueueLen = 0;
}

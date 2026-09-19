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
#include "MqttManager.h"
#include "Config.h"
#include "LEDManager.h"
#include "SlaveManager.h"
#include <WiFi.h>
#include <lwip/sockets.h>

MqttManagerClass MqttManager;

namespace {
// Colour temperature range offered to Home Assistant. The segment's cct runs from 0 (warm) to
// 255 (cold) and maps linearly onto it.
constexpr uint16_t KELVIN_MIN = 2000;
constexpr uint16_t KELVIN_MAX = 6500;

constexpr unsigned long STATE_CHECK_MS = 250;
constexpr unsigned long DISCOVERY_CHECK_MS = 1000;
constexpr uint32_t RECONNECT_MIN_MS = 5000;
constexpr uint32_t RECONNECT_MAX_MS = 60000;

// A base topic set in an earlier version as a placeholder; it was never used, so it means "none".
const char* const LEGACY_DEFAULT_TOPIC = "hyperled/device";

uint16_t cctToKelvin(uint8_t cct) {
    return KELVIN_MIN + (uint32_t)cct * (KELVIN_MAX - KELVIN_MIN) / 255;
}

uint8_t kelvinToCct(uint32_t kelvin) {
    if (kelvin < KELVIN_MIN) kelvin = KELVIN_MIN;
    if (kelvin > KELVIN_MAX) kelvin = KELVIN_MAX;
    return (kelvin - KELVIN_MIN) * 255 / (KELVIN_MAX - KELVIN_MIN);
}

// FNV-1a, to notice when something that has been published has changed.
struct Fnv {
    uint32_t h = 2166136261u;
    void add(const void* data, size_t len) {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < len; i++) { h ^= p[i]; h *= 16777619u; }
    }
    template <typename T> void add(const T& v) { add(&v, sizeof(v)); }
    void add(const String& s) { add(s.c_str(), s.length() + 1); }
};

// Accepts what Home Assistant sends ("ON"/"OFF") and the usual spellings people type by hand.
// Returns -1 for anything else.
int parsePower(String s) {
    s.trim();
    s.toUpperCase();
    if (s == "ON" || s == "TRUE" || s == "1") return 1;
    if (s == "OFF" || s == "FALSE" || s == "0") return 0;
    return -1;
}
}  // namespace

void MqttManagerClass::begin() {
    memset(_slaveTypes, 255, sizeof(_slaveTypes));
    _macColons = WiFi.macAddress();
    _macColons.toLowerCase();
    _macAddress = _macColons;
    _macAddress.replace(":", "");
    loadConfig();

    // Connecting blocks the main loop - the LEDs stand still meanwhile. A broker on the home
    // network answers within milliseconds; one that is switched off should cost 1 s, not 3.
    _wifiClient.setConnectionTimeout(1000);
    _client.setClient(_wifiClient);
    _client.setBufferSize(2048);   // a discovery message with the full effect list is ~1.3 KB
    _client.setSocketTimeout(3);   // waiting for CONNACK must not stall the LEDs for 15 s
    _client.setServer(_server.c_str(), _port);
    _client.setCallback([this](char* topic, uint8_t* payload, unsigned int length) {
        handleMessage(topic, payload, length);
    });
}

void MqttManagerClass::loadConfig() {
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, true);
    _enabled = prefs.getBool(PREF_MQTT_ENABLE, false);
    _server = prefs.getString(PREF_MQTT_SERVER, "");
    _port = prefs.getUShort(PREF_MQTT_PORT, 1883);
    _user = prefs.getString(PREF_MQTT_USER, "");
    _pass = prefs.getString(PREF_MQTT_PASS, "");
    String topic = prefs.getString(PREF_MQTT_TOPIC, "");
    _announcedSegs = prefs.getUChar(PREF_MQTT_ANNOUNCED, 0);
    prefs.end();

    // A base topic has to be a plain topic: wildcards cannot be published to, and a trailing or
    // leading slash would produce empty topic levels. Anything unusable falls back to the default.
    topic.trim();
    while (topic.endsWith("/")) topic.remove(topic.length() - 1);
    while (topic.startsWith("/")) topic.remove(0, 1);
    bool usable = topic.length() > 0 && topic.length() <= 96 && topic != LEGACY_DEFAULT_TOPIC &&
                  topic.indexOf('+') < 0 && topic.indexOf('#') < 0 && !topic.startsWith("$");
    _base = usable ? topic : "hyperled/" + _macAddress;
}

void MqttManagerClass::requestResync() {
    _discoveryDue = true;
    _discoveryDueAt = millis();
}

void MqttManagerClass::loop() {
    if (!_enabled) return;

    if (!_client.connected()) {
        reconnect();
        return;
    }
    _client.loop();
    if (!_client.connected()) return;

    unsigned long now = millis();
    if (now - _lastDiscoveryCheck >= DISCOVERY_CHECK_MS) {
        _lastDiscoveryCheck = now;
        refreshSlaveTypes();
        if (discoverySignature() != _discoverySig) requestResync();
    }
    if (_discoveryDue && (long)(now - _discoveryDueAt) >= 0) {
        _discoveryDue = false;
        publishDiscovery();
        _forceState = true;
    }
    if (_forceState || now - _lastStateCheck >= STATE_CHECK_MS) {
        _lastStateCheck = now;
        publishChangedStates(_forceState);
        _forceState = false;
    }
}

void MqttManagerClass::reconnect() {
    if (_server.isEmpty() || WiFi.status() != WL_CONNECTED) return;

    unsigned long now = millis();
    if (_everAttempted && now - _lastReconnectAttempt < _reconnectDelay) return;
    _everAttempted = true;
    _lastReconnectAttempt = now;

    String clientId = "HyperLED-" + _macAddress;
    String willTopic = _base + "/status";
    const char* user = _user.isEmpty() ? nullptr : _user.c_str();
    const char* pass = _user.isEmpty() ? nullptr : _pass.c_str();

    if (_client.connect(clientId.c_str(), user, pass, willTopic.c_str(), 0, true, "offline")) {
        Serial.printf("[MQTT] Connected to %s:%u as %s, base topic %s\n",
                      _server.c_str(), _port, clientId.c_str(), _base.c_str());
        onConnected();
        // Only a connection that survived setting itself up counts as good; one the broker stops
        // answering straight away should not be retried every 5 s.
        _reconnectDelay = _client.connected() ? RECONNECT_MIN_MS : min(_reconnectDelay * 2, RECONNECT_MAX_MS);
    } else {
        _reconnectDelay = min(_reconnectDelay * 2, RECONNECT_MAX_MS);
        // PubSubClient's state: -4 timeout, -2 connect failed, 4 bad credentials, 5 not authorised.
        Serial.printf("[MQTT] Connecting to %s:%u failed (state %d), next try in %lu s\n",
                      _server.c_str(), _port, _client.state(), (unsigned long)(_reconnectDelay / 1000));
    }
}

void MqttManagerClass::onConnected() {
    publish(_base + "/status", "online");

    _client.subscribe((_base + "/ha/set").c_str());
    _client.subscribe((_base + "/+/ha/set").c_str());
    // Home Assistant announces itself here after a restart; discovery has to be sent again then.
    _client.subscribe("homeassistant/status");

    refreshSlaveTypes();
    publishDiscovery();
    _stateSigs.clear();
    _forceState = true;
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

void MqttManagerClass::handleMessage(char* topicRaw, uint8_t* payload, unsigned int length) {
    String topic(topicRaw);

    if (topic == "homeassistant/status") {
        if (length == 6 && memcmp(payload, "online", 6) == 0) {
            // Spread out, so a house full of devices does not answer Home Assistant all at once.
            _discoveryDue = true;
            _discoveryDueAt = millis() + 500 + random(2000);
        }
        return;
    }

    String prefix = _base + "/";
    if (!topic.startsWith(prefix) || !topic.endsWith("ha/set")) return;
    String rest = topic.substring(prefix.length());

    JsonDocument doc;
    if (deserializeJson(doc, payload, length) != DeserializationError::Ok || !doc.is<JsonObject>()) {
        // A bare "ON" / "OFF" is accepted too, for people testing by hand.
        String plain;
        plain.reserve(length);
        for (unsigned int i = 0; i < length; i++) plain += (char)payload[i];
        int power = parsePower(plain);
        if (power < 0) return;
        doc.clear();
        doc["state"] = power ? "ON" : "OFF";
    }

    if (rest == "ha/set") {
        applyToAll(doc);
        return;
    }
    if (!rest.startsWith("seg")) return;
    String idStr = rest.substring(3, rest.length() - 7);  // between "seg" and "/ha/set"
    if (idStr.isEmpty() || idStr.length() > 3) return;
    for (size_t i = 0; i < idStr.length(); i++) {
        if (!isDigit(idStr[i])) return;
    }
    long segId = idStr.toInt();
    if (segId < LEDManager.getNumSegments()) applyToSegment((uint8_t)segId, doc);
}

void MqttManagerClass::applyToSegment(uint8_t segId, JsonDocument& doc) {
    const Segment* seg = LEDManager.getSegment(segId);
    if (!seg) return;
    SegInfo info = segmentInfo(*seg);

    if (!doc["state"].isNull()) {
        int power = parsePower(doc["state"].as<String>());
        if (power >= 0) LEDManager.setPower(segId, power == 1);
    }
    if (!doc["brightness"].isNull() && info.caps != CAP_ONOFF) {
        LEDManager.setBrightness(segId, doc["brightness"].as<uint8_t>());
    }

    bool hasColor = info.caps == CAP_RGB || info.caps == CAP_RGBW || info.caps == CAP_RGB_CT;
    if (!doc["color"].isNull() && hasColor) {
        JsonObject col = doc["color"];
        uint32_t w = (seg->color >> 24) & 0xFF;
        if (info.caps == CAP_RGBW && !col["w"].isNull()) w = col["w"].as<uint8_t>();
        uint32_t color = (w << 24) | ((uint32_t)col["r"].as<uint8_t>() << 16) |
                         ((uint32_t)col["g"].as<uint8_t>() << 8) | col["b"].as<uint8_t>();
        LEDManager.setColor(segId, color);
        // Picking a colour means showing it: leave the white-only mode.
        if (seg->whiteOnly) LEDManager.setWhiteOnly(segId, false);
        if (seg->effect == 10) LEDManager.setEffect(segId, 0);
    }

    bool hasCt = info.caps == CAP_COLOR_TEMP || info.caps == CAP_RGB_CT;
    if (!doc["color_temp"].isNull() && hasCt) {
        LEDManager.setCct(segId, kelvinToCct(doc["color_temp"].as<uint32_t>()));
        LEDManager.setWhiteOnly(segId, true);
    }

    if (!doc["effect"].isNull()) {
        String name = doc["effect"].as<String>();
        for (uint8_t e = 0; e < EFFECT_COUNT; e++) {
            if (name == EFFECT_NAMES[e] && effectOffered(e, info)) {
                LEDManager.setEffect(segId, e);
                break;
            }
        }
    }
    _lastStateCheck = 0;  // report the result on the next pass instead of within 250 ms
}

// "Alle Segmente" switches and dims every segment. Colour and effect stay per segment - one
// colour for a strip and a panel with a clock on it would rarely be what anyone wants.
void MqttManagerClass::applyToAll(JsonDocument& doc) {
    uint8_t numSegs = LEDManager.getNumSegments();
    if (!doc["state"].isNull()) {
        int power = parsePower(doc["state"].as<String>());
        if (power >= 0) {
            for (uint8_t i = 0; i < numSegs; i++) LEDManager.setPower(i, power == 1);
        }
    }
    if (!doc["brightness"].isNull()) {
        uint8_t bri = doc["brightness"].as<uint8_t>();
        for (uint8_t i = 0; i < numSegs; i++) LEDManager.setBrightness(i, bri);
    }
    _lastStateCheck = 0;
}

// ---------------------------------------------------------------------------
// What each segment can do
// ---------------------------------------------------------------------------

MqttManagerClass::Caps MqttManagerClass::capsForType(uint8_t type) {
    switch (type) {
        case TYPE_ONOFF:
            return CAP_ONOFF;
        case TYPE_ANALOG_1CH:
            return CAP_BRIGHTNESS;
        case TYPE_ANALOG_2CH:
            return CAP_COLOR_TEMP;
        case TYPE_SK6812_RGBW:
        case TYPE_TM1814:
        case TYPE_FW1906:
        case TYPE_UCS8904:
        case TYPE_ANALOG_4CH:
            return CAP_RGBW;
        // Two white channels: colour, or warm-to-cold white in the white-only mode.
        case TYPE_WS2805:
        case TYPE_SM16825:
        case TYPE_ANALOG_5CH:
            return CAP_RGB_CT;
        default:
            return CAP_RGB;
    }
}

void MqttManagerClass::refreshSlaveTypes() {
    bool anySlave = false;
    uint8_t numSegs = LEDManager.getNumSegments();
    for (uint8_t i = 0; i < numSegs && !anySlave; i++) {
        const Segment* seg = LEDManager.getSegment(i);
        anySlave = seg && seg->isSlave;
    }
    if (!anySlave) return;
    for (const DiscoveredSlave& s : SlaveManager.getDiscoveredSlaves()) {
        if (s.ledType != 255) _slaveTypes[s.currentId] = s.ledType;
    }
}

MqttManagerClass::SegInfo MqttManagerClass::segmentInfo(const Segment& seg) const {
    SegInfo info;
    if (seg.isSlave) {
        // A Slave that has not reported its LED type yet is treated as a colour strip.
        uint8_t type = _slaveTypes[seg.slaveId];
        info.caps = type == 255 ? CAP_RGB : capsForType(type);
        info.hub75 = type == TYPE_HUB75;
        info.matrix = info.hub75;
    } else {
        info.caps = capsForType(LEDManager.getType());
        info.hub75 = LEDManager.getType() == TYPE_HUB75;
        info.matrix = info.hub75 || LEDManager.isMatrix();
    }
    return info;
}

// The same choice the web interface offers for that hardware.
bool MqttManagerClass::effectOffered(uint8_t effect, const SegInfo& info) {
    if (effect >= EFFECT_COUNT || info.caps == CAP_ONOFF) return false;
    if (effect == 10) return info.caps == CAP_RGBW;  // on two-white strips that is colour_temp
    if (effect == 25) return info.matrix;
    if (effect >= EFFECT_HUB75_SHOWCASE_START) return info.hub75;
    return true;
}

// ---------------------------------------------------------------------------
// Discovery
// ---------------------------------------------------------------------------

uint32_t MqttManagerClass::discoverySignature() {
    Fnv f;
    uint8_t numSegs = LEDManager.getNumSegments();
    f.add(numSegs);
    for (uint8_t i = 0; i < numSegs; i++) {
        const Segment* seg = LEDManager.getSegment(i);
        if (!seg) continue;
        SegInfo info = segmentInfo(*seg);
        f.add(seg->name);
        f.add((uint8_t)info.caps);
        f.add(info.hub75);
        f.add(info.matrix);
    }
    f.add(WiFi.localIP().toString());  // part of the device's configuration link
    return f.h;
}

String MqttManagerClass::segTopic(uint8_t segId) const {
    return _base + "/seg" + String(segId);
}

String MqttManagerClass::discoveryTopic(const String& objectId) const {
    return "homeassistant/light/" + objectId + "/config";
}

void MqttManagerClass::addDevice(JsonDocument& doc) const {
    JsonObject dev = doc["device"].to<JsonObject>();
    dev["identifiers"][0] = _macAddress;
    dev["connections"][0][0] = "mac";
    dev["connections"][0][1] = _macColons;
    dev["name"] = "HyperLED-" + _macAddress.substring(6);
    dev["manufacturer"] = "HyperLED";
    dev["model"] = "ESP32-S3 LED-Controller";
    dev["sw_version"] = SOFTWARE_VERSION;
    dev["configuration_url"] = "http://" + WiFi.localIP().toString() + "/";

    JsonObject origin = doc["origin"].to<JsonObject>();
    origin["name"] = "HyperLED";
    origin["sw_version"] = SOFTWARE_VERSION;
}

void MqttManagerClass::publishDiscovery() {
    uint8_t numSegs = LEDManager.getNumSegments();
    String groupId = "hyperled_" + _macAddress;

    // With a single segment "Alle Segmente" would just be a second copy of it.
    if (numSegs > 1) {
        JsonDocument doc;
        doc["name"] = "Alle Segmente";
        doc["unique_id"] = groupId;
        doc["~"] = _base;
        doc["schema"] = "json";
        doc["state_topic"] = "~/ha/state";
        doc["command_topic"] = "~/ha/set";
        doc["availability_topic"] = "~/status";
        doc["supported_color_modes"][0] = "brightness";
        addDevice(doc);
        String payload;
        serializeJson(doc, payload);
        publish(discoveryTopic(groupId), payload);
    } else {
        publish(discoveryTopic(groupId), "");
    }

    for (uint8_t i = 0; i < numSegs; i++) {
        const Segment* seg = LEDManager.getSegment(i);
        if (!seg) continue;
        SegInfo info = segmentInfo(*seg);
        String objectId = groupId + "_seg" + String(i);

        JsonDocument doc;
        doc["name"] = seg->name.length() ? seg->name : String("Segment ") + String(i + 1);
        doc["unique_id"] = objectId;
        doc["~"] = _base;
        doc["schema"] = "json";
        doc["state_topic"] = "~/seg" + String(i) + "/ha/state";
        doc["command_topic"] = "~/seg" + String(i) + "/ha/set";
        doc["availability_topic"] = "~/status";

        JsonArray modes = doc["supported_color_modes"].to<JsonArray>();
        switch (info.caps) {
            case CAP_ONOFF:      modes.add("onoff"); break;
            case CAP_BRIGHTNESS: modes.add("brightness"); break;
            case CAP_COLOR_TEMP: modes.add("color_temp"); break;
            case CAP_RGB:        modes.add("rgb"); break;
            case CAP_RGBW:       modes.add("rgbw"); break;
            case CAP_RGB_CT:     modes.add("rgb"); modes.add("color_temp"); break;
        }
        if (info.caps == CAP_RGB || info.caps == CAP_RGBW || info.caps == CAP_RGB_CT) {
            doc["brightness"] = true;
        }
        if (info.caps == CAP_COLOR_TEMP || info.caps == CAP_RGB_CT) {
            doc["color_temp_kelvin"] = true;
            doc["min_kelvin"] = KELVIN_MIN;
            doc["max_kelvin"] = KELVIN_MAX;
        }
        if (info.caps != CAP_ONOFF) {
            doc["effect"] = true;
            JsonArray fx = doc["effect_list"].to<JsonArray>();
            for (uint8_t e = 0; e < EFFECT_COUNT; e++) {
                if (effectOffered(e, info)) fx.add(EFFECT_NAMES[e]);
            }
        }
        addDevice(doc);

        String payload;
        serializeJson(doc, payload);
        publish(discoveryTopic(objectId), payload);
    }

    // Segments deleted since the last announcement, also across restarts.
    if (_announcedSegs > numSegs) clearSegmentDiscovery(numSegs, _announcedSegs);
    if (_announcedSegs != numSegs && _client.connected()) {
        _announcedSegs = numSegs;
        Preferences prefs;
        prefs.begin(PREF_NAMESPACE, false);
        prefs.putUChar(PREF_MQTT_ANNOUNCED, numSegs);
        prefs.end();
    }
    _discoverySig = discoverySignature();
}

void MqttManagerClass::clearSegmentDiscovery(uint8_t from, uint8_t to) {
    for (uint16_t i = from; i < to; i++) {
        publish(discoveryTopic("hyperled_" + _macAddress + "_seg" + String(i)), "");
    }
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

void MqttManagerClass::fillSegmentState(JsonDocument& doc, const Segment& seg, const SegInfo& info) const {
    doc["state"] = seg.isOn ? "ON" : "OFF";
    if (info.caps == CAP_ONOFF) {
        doc["color_mode"] = "onoff";
        return;
    }
    doc["brightness"] = seg.brightness;

    bool whiteMode = seg.whiteOnly || seg.effect == 10;
    switch (info.caps) {
        case CAP_BRIGHTNESS:
            doc["color_mode"] = "brightness";
            break;
        case CAP_COLOR_TEMP:
            doc["color_mode"] = "color_temp";
            doc["color_temp"] = cctToKelvin(seg.cct);
            break;
        case CAP_RGBW:
            doc["color_mode"] = "rgbw";
            doc["color"]["w"] = (seg.color >> 24) & 0xFF;
            break;
        case CAP_RGB_CT:
            doc["color_mode"] = whiteMode ? "color_temp" : "rgb";
            doc["color_temp"] = cctToKelvin(seg.cct);
            break;
        default:
            doc["color_mode"] = "rgb";
            break;
    }
    if (info.caps == CAP_RGB || info.caps == CAP_RGBW || info.caps == CAP_RGB_CT) {
        doc["color"]["r"] = (seg.color >> 16) & 0xFF;
        doc["color"]["g"] = (seg.color >> 8) & 0xFF;
        doc["color"]["b"] = seg.color & 0xFF;
    }
    // An effect Home Assistant does not offer for this light (e.g. "Nur Weiß" on a two-white
    // strip, shown as colour temperature instead) is left out rather than reported unknown.
    if (effectOffered(seg.effect, info)) doc["effect"] = EFFECT_NAMES[seg.effect];
}

void MqttManagerClass::publishChangedStates(bool force) {
    uint8_t numSegs = LEDManager.getNumSegments();
    if (_stateSigs.size() != numSegs) {
        _stateSigs.assign(numSegs, 0);
        force = true;
    }

    // "Alle Segmente" is on when any segment is, at the brightness of the brightest lit one.
    bool anyOn = false;
    uint8_t briLit = 0, briAll = 0;
    for (uint8_t i = 0; i < numSegs; i++) {
        const Segment* seg = LEDManager.getSegment(i);
        if (!seg) continue;
        SegInfo info = segmentInfo(*seg);

        if (seg->isOn) {
            anyOn = true;
            briLit = max(briLit, seg->brightness);
        }
        briAll = max(briAll, seg->brightness);

        Fnv f;
        f.add(seg->isOn);
        f.add(seg->brightness);
        f.add(seg->color);
        f.add(seg->effect);
        f.add(seg->whiteOnly);
        f.add(seg->cct);
        f.add((uint8_t)info.caps);
        f.add(info.hub75);
        f.add(info.matrix);
        if (!force && f.h == _stateSigs[i]) continue;

        JsonDocument doc;
        fillSegmentState(doc, *seg, info);
        String payload;
        serializeJson(doc, payload);
        if (publish(segTopic(i) + "/ha/state", payload)) _stateSigs[i] = f.h;
    }

    if (numSegs > 1) {
        uint8_t groupBri = anyOn ? briLit : briAll;
        Fnv g;
        g.add(anyOn);
        g.add(groupBri);
        if (force || g.h != _groupSig) {
            JsonDocument doc;
            doc["state"] = anyOn ? "ON" : "OFF";
            doc["brightness"] = groupBri;
            doc["color_mode"] = "brightness";
            String payload;
            serializeJson(doc, payload);
            if (publish(_base + "/ha/state", payload)) _groupSig = g.h;
        }
    }
}

// A broker that stops taking data (it hangs, or the network path does) fills the socket's send
// buffer, and every further write then blocks the main loop for up to 10 s in NetworkClient -
// three of those and the loop watchdog restarts the controller. A burst of discovery messages
// fills the buffer for a moment too, until the broker acknowledges them - on a home network
// within milliseconds. So wait briefly for room, and treat a buffer that stays full as a dead
// connection.
bool MqttManagerClass::socketWritable() {
    int fd = _wifiClient.fd();
    if (fd < 0) return false;
    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd, &set);
    struct timeval tv = {0, 500000};
    return select(fd + 1, nullptr, &set, nullptr, &tv) > 0 && FD_ISSET(fd, &set);
}

void MqttManagerClass::dropConnection(const char* reason) {
    Serial.printf("[MQTT] Dropping the connection: %s\n", reason);
    _wifiClient.stop();  // not _client.disconnect(): that would write to the stuck socket too
    _lastReconnectAttempt = millis();
}

bool MqttManagerClass::publish(const String& topic, const String& payload, bool retained) {
    if (!_client.connected()) return false;

    // Fixed header (up to 5 bytes) + topic length + topic + payload must fit the client's buffer.
    size_t needed = 5 + 2 + topic.length() + payload.length();
    if (needed > _client.getBufferSize()) {
        _publishFailures++;
        unsigned long now = millis();
        // Logged at most every 10 s: a flood of serial output can stall a board with native USB.
        if (now - _lastFailureLog >= 10000 || _lastFailureLog == 0) {
            _lastFailureLog = now;
            Serial.printf("[MQTT] %s not sent: %u bytes, buffer %u (%lu such so far)\n", topic.c_str(),
                          (unsigned)needed, (unsigned)_client.getBufferSize(), (unsigned long)_publishFailures);
        }
        return false;
    }

    if (!socketWritable()) {
        dropConnection("broker is not taking data");
        return false;
    }
    if (_client.publish(topic.c_str(), payload.c_str(), retained)) return true;
    // A write that failed halfway leaves the stream unusable.
    if (_client.connected()) dropConnection("publish failed");
    return false;
}

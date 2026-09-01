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
#include "PresetManager.h"
#include <LittleFS.h>
#include "LEDManager.h"

PresetManagerClass PresetManager;

static const char* PRESETS_FILE = "/presets.json";
static const char* PLAYLIST_FILE = "/playlist.json";

void PresetManagerClass::begin() {
    loadPlaylist();
}

void PresetManagerClass::loop() {
    advancePlaylist();
}

bool PresetManagerClass::savePreset(uint8_t id, const String& name) {
    if (id == 0) return false;

    JsonDocument doc;
    if (LittleFS.exists(PRESETS_FILE)) {
        File f = LittleFS.open(PRESETS_FILE, "r");
        if (f) {
            if (deserializeJson(doc, f)) doc.clear();
            f.close();
        }
    }

    JsonObject preset = doc[String(id)].to<JsonObject>();
    preset["name"] = name;
    JsonArray segArr = preset["seg"].to<JsonArray>();
    LEDManager.getSegmentsJson(segArr);

    File out = LittleFS.open(PRESETS_FILE, "w");
    if (!out) return false;
    serializeJson(doc, out);
    out.close();
    return true;
}

bool PresetManagerClass::applyPreset(uint8_t id) {
    if (id == 0 || !LittleFS.exists(PRESETS_FILE)) return false;

    File f = LittleFS.open(PRESETS_FILE, "r");
    if (!f) return false;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;

    JsonVariant preset = doc[String(id)];
    if (preset.isNull() || !preset["seg"].is<JsonArray>()) return false;

    LEDManager.setSegmentsFromJson(preset["seg"].as<JsonArray>());
    LEDManager.recalculateSegments();
    return true;
}

bool PresetManagerClass::deletePreset(uint8_t id) {
    if (id == 0 || !LittleFS.exists(PRESETS_FILE)) return false;

    File f = LittleFS.open(PRESETS_FILE, "r");
    if (!f) return false;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;

    if (doc[String(id)].isNull()) return false;
    doc.remove(String(id));

    File out = LittleFS.open(PRESETS_FILE, "w");
    if (!out) return false;
    serializeJson(doc, out);
    out.close();
    return true;
}

uint8_t PresetManagerClass::nextFreePresetId() {
    uint8_t maxId = 0;
    if (LittleFS.exists(PRESETS_FILE)) {
        File f = LittleFS.open(PRESETS_FILE, "r");
        if (f) {
            JsonDocument doc;
            if (!deserializeJson(doc, f)) {
                for (JsonPair kv : doc.as<JsonObject>()) {
                    uint8_t id = (uint8_t)atoi(kv.key().c_str());
                    if (id > maxId) maxId = id;
                }
            }
            f.close();
        }
    }
    return maxId + 1;
}

void PresetManagerClass::getPlaylistJson(JsonDocument& doc) const {
    doc["enabled"] = _playlistEnabled;
    JsonArray arr = doc["entries"].to<JsonArray>();
    for (const auto& pe : _playlist) {
        JsonObject o = arr.add<JsonObject>();
        o["id"] = pe.presetId;
        o["duration"] = pe.durationSec;
    }
}

bool PresetManagerClass::setPlaylistFromJson(JsonVariant data) {
    _playlist.clear();
    _playlistEnabled = data["enabled"] | false;
    if (data["entries"].is<JsonArray>()) {
        for (JsonObject e : data["entries"].as<JsonArray>()) {
            PlaylistEntry pe;
            pe.presetId = e["id"] | 0;
            pe.durationSec = e["duration"] | 10;
            if (pe.presetId > 0 && pe.durationSec > 0) _playlist.push_back(pe);
        }
    }
    _playlistIndex = -1; // restart from the beginning
    savePlaylist();
    return true;
}

void PresetManagerClass::loadPlaylist() {
    _playlist.clear();
    _playlistEnabled = false;
    if (!LittleFS.exists(PLAYLIST_FILE)) return;

    File f = LittleFS.open(PLAYLIST_FILE, "r");
    if (!f) return;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return;

    _playlistEnabled = doc["enabled"] | false;
    if (doc["entries"].is<JsonArray>()) {
        for (JsonObject e : doc["entries"].as<JsonArray>()) {
            PlaylistEntry pe;
            pe.presetId = e["id"] | 0;
            pe.durationSec = e["duration"] | 10;
            if (pe.presetId > 0) _playlist.push_back(pe);
        }
    }
}

void PresetManagerClass::savePlaylist() {
    JsonDocument doc;
    getPlaylistJson(doc);
    File f = LittleFS.open(PLAYLIST_FILE, "w");
    if (!f) return;
    serializeJson(doc, f);
    f.close();
}

void PresetManagerClass::advancePlaylist() {
    if (!_playlistEnabled || _playlist.empty()) return;

    unsigned long now = millis();

    if (_playlistIndex < 0) {
        _playlistIndex = 0;
        applyPreset(_playlist[_playlistIndex].presetId);
        _playlistLastSwitch = now;
        return;
    }

    unsigned long durationMs = (unsigned long)_playlist[_playlistIndex].durationSec * 1000UL;
    if (now - _playlistLastSwitch >= durationMs) {
        _playlistIndex = (_playlistIndex + 1) % (int8_t)_playlist.size();
        applyPreset(_playlist[_playlistIndex].presetId);
        _playlistLastSwitch = now;
    }
}

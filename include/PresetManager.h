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
#include <vector>
#include <ArduinoJson.h>

class PresetManagerClass {
public:
    void begin();
    void loop();

    // Presets are stored in LittleFS as /presets.json, keyed by numeric id (string keys).
    // Reading/writing goes straight to the filesystem - no full in-RAM cache is kept.
    bool savePreset(uint8_t id, const String& name);
    bool applyPreset(uint8_t id);
    bool deletePreset(uint8_t id);
    uint8_t nextFreePresetId();

    // Playlist: an ordered list of {presetId, durationSeconds}, persisted as /playlist.json.
    // Kept resident in RAM (small) so loop() can advance it without re-reading the file.
    void getPlaylistJson(JsonDocument& doc) const;
    bool setPlaylistFromJson(JsonVariant data);

    // For Home Assistant (see MqttManager).
    struct PresetInfo {
        uint8_t id;
        String name;
    };
    // Every stored preset, ordered by id. Reads the file, so not for every loop pass.
    void listPresets(std::vector<PresetInfo>& out) const;
    // Bumped whenever a preset is saved or deleted, so a cached list knows it is stale.
    uint32_t revision() const { return _revision; }
    // The preset applied last (0 = none yet), and how many presets have been applied - from any
    // source: web interface, playlist, schedule or Home Assistant.
    uint8_t lastApplied() const { return _lastApplied; }
    uint32_t applyCount() const { return _applyCount; }
    bool hasPlaylist() const { return !_playlist.empty(); }
    bool playlistEnabled() const { return _playlistEnabled; }
    // Starts (from the first entry) or stops the playlist, keeping its entries.
    void setPlaylistEnabled(bool enabled);

private:
    struct PlaylistEntry {
        uint8_t presetId;
        uint16_t durationSec;
    };

    void loadPlaylist();
    void savePlaylist();
    void advancePlaylist();

    uint32_t _revision = 0;
    uint8_t _lastApplied = 0;
    uint32_t _applyCount = 0;
    std::vector<PlaylistEntry> _playlist;
    bool _playlistEnabled = false;
    int8_t _playlistIndex = -1;
    unsigned long _playlistLastSwitch = 0;
};

extern PresetManagerClass PresetManager;

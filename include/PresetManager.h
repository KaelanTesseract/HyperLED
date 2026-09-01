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

private:
    struct PlaylistEntry {
        uint8_t presetId;
        uint16_t durationSec;
    };

    void loadPlaylist();
    void savePlaylist();
    void advancePlaylist();

    std::vector<PlaylistEntry> _playlist;
    bool _playlistEnabled = false;
    int8_t _playlistIndex = -1;
    unsigned long _playlistLastSwitch = 0;
};

extern PresetManagerClass PresetManager;

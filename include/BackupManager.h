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

// Backup and restore of everything this controller has been told, as one JSON file that can be
// restored here or on another controller:
//   - every entry of the settings namespace (PREF_NAMESPACE) with its type, whatever keys it
//     holds - Wi-Fi, LED hardware, segments with their elements, MQTT, buttons, weather, ...
//   - the user files on LittleFS: presets, playlist, schedules and the element images.
// The Slaves keep their own configuration and are not part of it.
//
// Both directions do their work in the main loop, never in the web server's task. Reading the
// settings while something writes them can make the NVS iterator skip entries - which is exactly
// how a namespace move once lost half the settings. In the loop nothing else writes meanwhile.
class BackupManagerClass {
public:
    static const int FORMAT = 1;
    static const char* const BACKUP_PATH;
    static const char* const RESTORE_PATH;

    // Asks the loop for a fresh backup at BACKUP_PATH.
    void requestBackup();
    bool backupPending() const { return _backupRequested; }
    bool backupAvailable() const { return _backupOk; }
    const String& backupError() const { return _backupError; }

    // Checks the uploaded file at RESTORE_PATH completely before anything is changed. If it is
    // usable, the loop replaces all settings and files with it and restarts the controller.
    // takeWifi: also take the Wi-Fi credentials from the file; otherwise this controller keeps
    // its own.
    bool prepareRestore(bool takeWifi, String& error);
    bool restorePending() const { return _restorePending; }

    void loop();

private:
    volatile bool _backupRequested = false;
    bool _backupOk = false;
    String _backupError;

    volatile bool _restorePending = false;
    bool _restoreTakeWifi = false;
    unsigned long _restoreAt = 0;

    bool writeBackup(String& error);
    void applyRestore();
};

extern BackupManagerClass BackupManager;

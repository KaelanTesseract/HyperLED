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
#include "ScheduleManager.h"
#include <time.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <Preferences.h>
#include "Config.h"
#include "LEDManager.h"
#include "PresetManager.h"

ScheduleManagerClass ScheduleManager;

static const char* SCHEDULES_FILE = "/schedules.json";
// Central European Time incl. automatic DST (matches the project's primary user base).
static const char* DEFAULT_TZ = "CET-1CEST,M3.5.0,M10.5.0/3";

void ScheduleManagerClass::begin() {
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, true);
    _timezone = prefs.getString("tz", DEFAULT_TZ);
    prefs.end();

    loadSchedules();
}

void ScheduleManagerClass::applyTimezone() {
    configTzTime(_timezone.c_str(), "pool.ntp.org", "time.nist.gov");
}

void ScheduleManagerClass::setTimezone(const String& tz) {
    _timezone = tz;
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, false);
    prefs.putString("tz", _timezone);
    prefs.end();

    if (WiFi.status() == WL_CONNECTED) {
        applyTimezone();
    }
}

bool ScheduleManagerClass::isTimeSynced() const {
    // Before NTP sync, the ESP32's clock sits near the epoch; a plausible
    // "real" timestamp is the standard way to detect a completed sync.
    return time(nullptr) > 8 * 3600 * 2;
}

void ScheduleManagerClass::loop() {
    unsigned long now = millis();
    if (now - _lastCheck < 1000) return;
    _lastCheck = now;
    checkSchedules();
}

void ScheduleManagerClass::checkSchedules() {
    if (_schedules.empty() || !isTimeSynced()) return;

    time_t nowEpoch = time(nullptr);
    struct tm ti;
    localtime_r(&nowEpoch, &ti);

    int curMinuteOfDay = ti.tm_hour * 60 + ti.tm_min;
    if (curMinuteOfDay == _lastCheckedMinuteOfDay) return; // already handled this minute
    _lastCheckedMinuteOfDay = curMinuteOfDay;

    uint8_t dayBit = 1 << ti.tm_wday; // tm_wday: 0 = Sunday .. 6 = Saturday

    for (const auto& sch : _schedules) {
        if (!sch.enabled) continue;
        if (sch.hour != ti.tm_hour || sch.minute != ti.tm_min) continue;
        if (!(sch.daysMask & dayBit)) continue;

        switch (sch.action) {
            case SCHEDULE_ACTION_ON:
                for (uint8_t i = 0; i < LEDManager.getNumSegments(); i++) LEDManager.setPower(i, true);
                break;
            case SCHEDULE_ACTION_OFF:
                for (uint8_t i = 0; i < LEDManager.getNumSegments(); i++) LEDManager.setPower(i, false);
                break;
            case SCHEDULE_ACTION_PRESET:
                PresetManager.applyPreset(sch.presetId);
                break;
        }
    }
}

void ScheduleManagerClass::getSchedulesJson(JsonDocument& doc) const {
    doc["timezone"] = _timezone;
    JsonArray arr = doc["entries"].to<JsonArray>();
    for (const auto& sch : _schedules) {
        JsonObject o = arr.add<JsonObject>();
        o["hour"] = sch.hour;
        o["minute"] = sch.minute;
        o["days"] = sch.daysMask;
        o["action"] = sch.action;
        o["presetId"] = sch.presetId;
        o["enabled"] = sch.enabled;
    }
}

bool ScheduleManagerClass::setSchedulesFromJson(JsonVariant data) {
    _schedules.clear();
    if (data["entries"].is<JsonArray>()) {
        for (JsonObject e : data["entries"].as<JsonArray>()) {
            Schedule sch;
            sch.hour = e["hour"] | 0;
            sch.minute = e["minute"] | 0;
            sch.daysMask = e["days"] | 0x7F;
            sch.action = e["action"] | SCHEDULE_ACTION_ON;
            sch.presetId = e["presetId"] | 0;
            sch.enabled = e["enabled"] | true;
            if (sch.hour < 24 && sch.minute < 60) _schedules.push_back(sch);
        }
    }
    _lastCheckedMinuteOfDay = -1; // force a fresh check so a just-added schedule can fire this minute
    saveSchedules();

    if (!data["timezone"].isNull()) {
        setTimezone(data["timezone"].as<String>());
    }
    return true;
}

void ScheduleManagerClass::loadSchedules() {
    _schedules.clear();
    if (!LittleFS.exists(SCHEDULES_FILE)) return;

    File f = LittleFS.open(SCHEDULES_FILE, "r");
    if (!f) return;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return;

    if (doc["entries"].is<JsonArray>()) {
        for (JsonObject e : doc["entries"].as<JsonArray>()) {
            Schedule sch;
            sch.hour = e["hour"] | 0;
            sch.minute = e["minute"] | 0;
            sch.daysMask = e["days"] | 0x7F;
            sch.action = e["action"] | SCHEDULE_ACTION_ON;
            sch.presetId = e["presetId"] | 0;
            sch.enabled = e["enabled"] | true;
            if (sch.hour < 24 && sch.minute < 60) _schedules.push_back(sch);
        }
    }
}

void ScheduleManagerClass::saveSchedules() {
    JsonDocument doc;
    JsonArray arr = doc["entries"].to<JsonArray>();
    for (const auto& sch : _schedules) {
        JsonObject o = arr.add<JsonObject>();
        o["hour"] = sch.hour;
        o["minute"] = sch.minute;
        o["days"] = sch.daysMask;
        o["action"] = sch.action;
        o["presetId"] = sch.presetId;
        o["enabled"] = sch.enabled;
    }
    File f = LittleFS.open(SCHEDULES_FILE, "w");
    if (!f) return;
    serializeJson(doc, f);
    f.close();
}

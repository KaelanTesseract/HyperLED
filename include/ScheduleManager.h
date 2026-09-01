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

// Schedule actions
#define SCHEDULE_ACTION_ON 0
#define SCHEDULE_ACTION_OFF 1
#define SCHEDULE_ACTION_PRESET 2

class ScheduleManagerClass {
public:
    void begin();
    void loop();

    // Call once WiFi STA is connected (and again whenever the timezone changes)
    // to (re-)synchronize the system clock via NTP for the configured timezone.
    void applyTimezone();
    void setTimezone(const String& tz);
    String getTimezone() const { return _timezone; }
    bool isTimeSynced() const;

    // Schedules are kept resident in RAM (small list) and persisted as /schedules.json.
    void getSchedulesJson(JsonDocument& doc) const;
    bool setSchedulesFromJson(JsonVariant data);

private:
    struct Schedule {
        uint8_t hour;
        uint8_t minute;
        uint8_t daysMask; // bit0 = Sunday .. bit6 = Saturday, matches struct tm's tm_wday
        uint8_t action;   // SCHEDULE_ACTION_*
        uint8_t presetId;
        bool enabled;
    };

    void loadSchedules();
    void saveSchedules();
    void checkSchedules();

    std::vector<Schedule> _schedules;
    String _timezone;
    unsigned long _lastCheck = 0;
    int _lastCheckedMinuteOfDay = -1;
};

extern ScheduleManagerClass ScheduleManager;

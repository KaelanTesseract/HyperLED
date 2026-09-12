/*
 * HyperLED - Open Source LED Controller
 *
 * Copyright (c) 2026 Dennis Guse
 *
 * Licensed under the EUPL, Version 1.2 or - as soon they will be approved by
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
#include "LoopWatch.h"

LoopWatchClass LoopWatch;

// RTC_NOINIT_ATTR: kept in RTC RAM and, unlike RTC_DATA_ATTR, deliberately not cleared on a
// warm boot. That is the whole point - the note has to outlive the reset that follows the hang.
// A power cycle leaves it as noise, which is what the magic value is for.
#define LOOPWATCH_MAGIC 0x48594C57u  // "HYLW"

struct LoopWatchRecord {
    uint32_t magic;
    uint32_t step;        // step index last entered
    uint32_t midStep;     // 1 while inside a step, 0 between passes
    uint32_t uptime;      // seconds at the last update
    uint32_t iterations;  // completed passes since boot
};

static RTC_NOINIT_ATTR LoopWatchRecord g_record;

// Must match the order of the HYPERLED_LOOP_STEP calls in main.cpp.
static const char* const STEP_NAMES[] = {
    "WiFiManager", "WebServerManager", "MqttManager", "ButtonManager", "LEDManager",
    "UpdateManager", "SlaveManager", "PresetManager", "ScheduleManager", "WeatherManager",
    "StatusLedManager"
};
static const uint8_t STEP_COUNT = sizeof(STEP_NAMES) / sizeof(STEP_NAMES[0]);

const char* LoopWatchClass::stepName(uint8_t step) {
    if (step >= STEP_COUNT) return "unknown";
    return STEP_NAMES[step];
}

void LoopWatchClass::begin() {
    if (g_record.magic == LOOPWATCH_MAGIC) {
        _hadPrevious = true;
        _previousStep = (uint8_t)g_record.step;
        _previousMidStep = (g_record.midStep != 0);
        _previousUptime = g_record.uptime;
        _previousIterations = g_record.iterations;

        // Printed once at boot, where it is most likely to be seen, and only when the previous
        // life ended without finishing a pass - a clean restart has nothing to explain.
        if (_previousMidStep) {
            Serial.printf("LoopWatch: previous run stopped inside %s after %lus (%lu passes)\n",
                          stepName(_previousStep),
                          (unsigned long)_previousUptime,
                          (unsigned long)_previousIterations);
        }
    }

    g_record.magic = LOOPWATCH_MAGIC;
    g_record.step = 0;
    g_record.midStep = 0;
    g_record.uptime = 0;
    g_record.iterations = 0;
}

void LoopWatchClass::enter(uint8_t step) {
    g_record.step = step;
    g_record.midStep = 1;
    g_record.uptime = millis() / 1000;
}

void LoopWatchClass::completed() {
    g_record.midStep = 0;
    g_record.iterations++;
    g_record.uptime = millis() / 1000;
}

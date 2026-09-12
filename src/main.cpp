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
#include <Arduino.h>
#include "Config.h"
#include "WiFiManager.h"
#include "AppWebServer.h"
#include "LEDManager.h"
#include "MqttManager.h"
#include "UpdateManager.h"
#include "SlaveManager.h"
#include "ButtonManager.h"
#include "PresetManager.h"
#include "ScheduleManager.h"
#include "WeatherManager.h"
#include "StatusLedManager.h"
#include "LoopWatch.h"
#include "esp_task_wdt.h"

void setup() {
    Serial.begin(115200);
    // Native USB-CDC: without this, a write blocks while no host is draining the port, and on
    // this board that is enough to stop whichever task was printing - the Wi-Fi task included,
    // which takes the network down with it. Dropping characters nobody is reading is the lesser
    // evil by a wide margin.
    Serial.setTxTimeoutMs(0);
    delay(1000);
    Serial.println("HyperLED Starting...");

    LoopWatch.begin();

    LEDManager.begin();
    StatusLedManager.begin();
    ScheduleManager.begin(); // must load the timezone before WiFiManager connects and syncs NTP
    WiFiManager.begin();
    WebServerManager.begin();
    MqttManager.begin();
    UpdateManager.begin();
    SlaveManager.begin();
    ButtonManager.begin();
    PresetManager.begin();
    WeatherManager.begin();

    // Last line of defence. Everything above has been made not to block, but "not supposed to"
    // is not a guarantee, and a loop that stops leaves no trace at all: the board stays
    // associated, answers nothing, prints nothing, and cannot be told apart from a dead one.
    // The watchdog turns that into a reboot with a recorded reason, so it both recovers on its
    // own and says what happened. Thirty seconds is far longer than any legitimate pass.
    esp_task_wdt_config_t wdtConfig = {};
    wdtConfig.timeout_ms = 30000;
    wdtConfig.idle_core_mask = 0;
    wdtConfig.trigger_panic = true;
    if (esp_task_wdt_init(&wdtConfig) == ESP_ERR_INVALID_STATE) {
        esp_task_wdt_reconfigure(&wdtConfig);
    }
    esp_task_wdt_add(nullptr);
}

// Reports any manager that holds up the loop. While one does, the Master sends no pings and a
// Slave declares it lost after five seconds - so a blocking call in here is not a slow frame, it
// is a dropped Slave. Cheap enough to leave in: two millis() reads per manager.
// Records which step is about to run BEFORE running it, then reports it if it took too long.
// The order matters: a step that never returns prints nothing at all, so the only way to learn
// its name is to have written it down in advance - LoopWatch keeps that note somewhere a reset
// does not erase.
#define HYPERLED_LOOP_STEP(idx, call) { \
    LoopWatch.enter(idx); \
    unsigned long _t0 = millis(); \
    call; \
    unsigned long _took = millis() - _t0; \
    if (_took > 500) { \
        Serial.print("LOOP STALL: "); Serial.print(#call); \
        Serial.print(" blocked for "); Serial.print(_took); Serial.println("ms"); \
    } \
}

void loop() {
    HYPERLED_LOOP_STEP(0, WiFiManager.loop());
    HYPERLED_LOOP_STEP(1, WebServerManager.loop());
    HYPERLED_LOOP_STEP(2, MqttManager.loop());
    HYPERLED_LOOP_STEP(3, ButtonManager.loop());
    HYPERLED_LOOP_STEP(4, LEDManager.loop());
    HYPERLED_LOOP_STEP(5, UpdateManager.loop());
    HYPERLED_LOOP_STEP(6, SlaveManager.loop());
    HYPERLED_LOOP_STEP(7, PresetManager.loop());
    HYPERLED_LOOP_STEP(8, ScheduleManager.loop());
    HYPERLED_LOOP_STEP(9, WeatherManager.loop());
    HYPERLED_LOOP_STEP(10, StatusLedManager.loop());

#if DEBUG_SERIAL
    static unsigned long lastMemPrint = 0;
    if (millis() - lastMemPrint > 5000) {
        lastMemPrint = millis();
        Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    }
#endif

    LoopWatch.completed();
    esp_task_wdt_reset();
    delay(1);
}

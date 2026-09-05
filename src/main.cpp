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

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("HyperLED Starting...");

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
}

void loop() {
    WiFiManager.loop();
    WebServerManager.loop();
    MqttManager.loop();
    ButtonManager.loop();
    LEDManager.loop();
    UpdateManager.loop();
    SlaveManager.loop();
    PresetManager.loop();
    ScheduleManager.loop();
    WeatherManager.loop();
    StatusLedManager.loop();

#if DEBUG_SERIAL
    static unsigned long lastMemPrint = 0;
    if (millis() - lastMemPrint > 5000) {
        lastMemPrint = millis();
        Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    }
#endif

    delay(1);
}

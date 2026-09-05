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
#include "Config.h"

// Drives the board's single onboard WS2812, completely independently of the LED strips
// LEDManager renders - it has its own NeoPixelBus instance on its own RMT channel, so it
// keeps working no matter which bus type (or none) the user configured for their strip.
//
// Besides the user-set colour there is a status override: firmware code can call
// showStatus() to signal something (e.g. an error) without destroying the user's setting -
// clearStatus() restores whatever the user had configured.
class StatusLedManagerClass {
public:
    void begin();
    void loop();

    // User-facing settings (persisted).
    bool isOn() const { return _on; }
    uint32_t getColor() const { return _color; }
    uint8_t getBrightness() const { return _brightness; }
    void setState(bool on, uint32_t color, uint8_t brightness);

    // Firmware-driven override, e.g. for error codes. blinkMs == 0 means solid.
    // The user's own setting is left untouched and returns on clearStatus().
    void showStatus(uint32_t color, uint16_t blinkMs = 0);
    void clearStatus();
    bool hasStatus() const { return _statusActive; }

private:
    bool _on = true;
    uint32_t _color = 0x0000FF; // blue by default - visible, but clearly not "all good/off"
    uint8_t _brightness = 40;   // these LEDs are glaringly bright at full power

    bool _statusActive = false;
    uint32_t _statusColor = 0;
    uint16_t _statusBlinkMs = 0;
    unsigned long _lastBlinkToggle = 0;
    bool _blinkPhaseOn = true;

    bool _dirty = true;

    void render();
    void save();
};

extern StatusLedManagerClass StatusLedManager;

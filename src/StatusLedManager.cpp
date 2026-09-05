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
#include "StatusLedManager.h"
#include <NeoPixelBus.h>
#include <Preferences.h>

StatusLedManagerClass StatusLedManager;

// RMT channel 0 - the main strip bus uses Neo800KbpsMethod, which maps to channel 1 on this
// chip, so the two never fight over the same channel.
//
// NeoRgbFeature, not the usual NeoGrbFeature: the onboard LED of this board expects plain
// RGB byte order. With GRB the red and green channels come out swapped (blue stays correct),
// which is exactly how a wrong order shows up here - so don't "fix" this back to GRB.
static NeoPixelBus<NeoRgbFeature, NeoEsp32Rmt0Ws2812xMethod>* _statusStrip = nullptr;

void StatusLedManagerClass::begin() {
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, true);
    _on = prefs.getBool(PREF_STATUSLED_ON, true);
    _color = prefs.getUInt(PREF_STATUSLED_COLOR, 0x0000FF);
    _brightness = prefs.getUChar(PREF_STATUSLED_BRI, 40);
    prefs.end();

    _statusStrip = new NeoPixelBus<NeoRgbFeature, NeoEsp32Rmt0Ws2812xMethod>(1, STATUS_LED_PIN);
    _statusStrip->Begin();
    _dirty = true;
    render();
}

void StatusLedManagerClass::loop() {
    if (_statusActive && _statusBlinkMs > 0) {
        if (millis() - _lastBlinkToggle >= _statusBlinkMs) {
            _lastBlinkToggle = millis();
            _blinkPhaseOn = !_blinkPhaseOn;
            _dirty = true;
        }
    }
    if (_dirty) render();
}

void StatusLedManagerClass::render() {
    if (!_statusStrip) return;
    _dirty = false;

    uint32_t color = 0;
    uint8_t bri = _brightness;

    if (_statusActive) {
        // A status signal must stay visible even if the user turned the LED off or set it
        // very dim, so it ignores those settings and uses a fixed, readable level.
        color = (_statusBlinkMs > 0 && !_blinkPhaseOn) ? 0 : _statusColor;
        bri = 60;
    } else if (_on) {
        color = _color;
    }

    uint8_t r = ((color >> 16) & 0xFF) * bri / 255;
    uint8_t g = ((color >> 8) & 0xFF) * bri / 255;
    uint8_t b = (color & 0xFF) * bri / 255;

    _statusStrip->SetPixelColor(0, RgbColor(r, g, b));
    _statusStrip->Show();
}

void StatusLedManagerClass::setState(bool on, uint32_t color, uint8_t brightness) {
    _on = on;
    _color = color;
    _brightness = brightness;
    _dirty = true;
    save();
}

void StatusLedManagerClass::save() {
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, false);
    prefs.putBool(PREF_STATUSLED_ON, _on);
    prefs.putUInt(PREF_STATUSLED_COLOR, _color);
    prefs.putUChar(PREF_STATUSLED_BRI, _brightness);
    prefs.end();
}

void StatusLedManagerClass::showStatus(uint32_t color, uint16_t blinkMs) {
    _statusActive = true;
    _statusColor = color;
    _statusBlinkMs = blinkMs;
    _blinkPhaseOn = true;
    _lastBlinkToggle = millis();
    _dirty = true;
}

void StatusLedManagerClass::clearStatus() {
    if (!_statusActive) return;
    _statusActive = false;
    _dirty = true;
}

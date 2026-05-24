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
#include "ButtonManager.h"

ButtonManagerClass ButtonManager;

void ButtonManagerClass::begin() {
    Preferences prefs;
    prefs.begin("wled_clone", true);
    
    _btn1Enabled = prefs.getBool("btn1_en", false);
    _btn1Type = prefs.getString("btn1_type", "push");
    
    _btn2Enabled = prefs.getBool("btn2_en", false);
    _btn2Type = prefs.getString("btn2_type", "switch");
    
    prefs.end();
    
    if (_btn1Enabled) pinMode(_pin1, INPUT_PULLUP);
    if (_btn2Enabled) pinMode(_pin2, INPUT_PULLUP);
    
    // Read initial states
    if (_btn1Enabled) _btn1LastState = digitalRead(_pin1);
    if (_btn2Enabled) _btn2LastState = digitalRead(_pin2);
}

void ButtonManagerClass::loop() {
    if (_btn1Enabled) {
        if (_btn1Type == "push") {
            handlePushButton(_pin1, _btn1LastState, _btn1PressTime, _btn1Handled, _btn1DimmingDown, _btn1LastDimTime);
        } else {
            handleSwitchButton(_pin1, _btn1LastState, _btn1PressTime);
        }
    }
    
    if (_btn2Enabled) {
        if (_btn2Type == "push") {
            handlePushButton(_pin2, _btn2LastState, _btn2PressTime, _btn2Handled, _btn2DimmingDown, _btn2LastDimTime);
        } else {
            handleSwitchButton(_pin2, _btn2LastState, _btn2PressTime);
        }
    }
}

void ButtonManagerClass::handlePushButton(int pin, bool& lastState, unsigned long& pressTime, bool& handled, bool& dimmingDown, unsigned long& lastDimTime) {
    bool currentState = digitalRead(pin);
    unsigned long now = millis();
    
    // Button pressed (LOW because INPUT_PULLUP)
    if (currentState == LOW && lastState == HIGH) {
        pressTime = now;
        handled = false;
        delay(20); // Basic debounce
    }
    // Button held down
    else if (currentState == LOW && lastState == LOW) {
        if (!handled && (now - pressTime > 500)) { // Held for 500ms
            if (now - lastDimTime > 30) {
                lastDimTime = now;
                uint8_t currentBri = LEDManager.getBrightness(0);
                if (dimmingDown) {
                    if (currentBri > 5) LEDManager.setBrightness(0, currentBri - 5);
                    else { LEDManager.setBrightness(0, 0); dimmingDown = false; }
                } else {
                    if (currentBri < 250) LEDManager.setBrightness(0, currentBri + 5);
                    else { LEDManager.setBrightness(0, 255); dimmingDown = true; }
                }
            }
        }
    }
    // Button released
    else if (currentState == HIGH && lastState == LOW) {
        if (!handled && (now - pressTime > 20) && (now - pressTime < 500)) { // Short press
            // Toggle Segment 0
            bool isOn = LEDManager.getPower(0);
            LEDManager.setPower(0, !isOn);
        }
        if (now - pressTime >= 500) {
            // End of hold, flip dimming direction for next hold
            dimmingDown = !dimmingDown;
        }
        handled = true;
        delay(20); // Debounce
    }
    
    lastState = currentState;
}

void ButtonManagerClass::handleSwitchButton(int pin, bool& lastState, unsigned long& pressTime) {
    bool currentState = digitalRead(pin);
    unsigned long now = millis();
    
    if (currentState != lastState && (now - pressTime > 50)) { // 50ms debounce
        pressTime = now;
        lastState = currentState;
        
        bool isOn = LEDManager.getPower(0);
        LEDManager.setPower(0, !isOn);
    }
}

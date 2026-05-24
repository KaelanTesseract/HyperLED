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
#ifndef BUTTONMANAGER_H
#define BUTTONMANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "LEDManager.h"

class ButtonManagerClass {
public:
    void begin();
    void loop();

private:
    bool _btn1Enabled = false;
    bool _btn2Enabled = false;
    String _btn1Type = "push";
    String _btn2Type = "push";
    
    int _pin1 = 12;
    int _pin2 = 13;
    
    // State for Button 1
    bool _btn1LastState = HIGH;
    unsigned long _btn1PressTime = 0;
    bool _btn1Handled = false;
    bool _btn1DimmingDown = true;
    unsigned long _btn1LastDimTime = 0;
    
    // State for Button 2
    bool _btn2LastState = HIGH;
    unsigned long _btn2PressTime = 0;
    bool _btn2Handled = false;
    bool _btn2DimmingDown = true;
    unsigned long _btn2LastDimTime = 0;

    void handlePushButton(int pin, bool& lastState, unsigned long& pressTime, bool& handled, bool& dimmingDown, unsigned long& lastDimTime);
    void handleSwitchButton(int pin, bool& lastState, unsigned long& pressTime);
};

extern ButtonManagerClass ButtonManager;

#endif

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
#pragma once

#include <Arduino.h>

// A note the loop leaves about where it is, kept somewhere a reset does not erase.
//
// A stalled manager announces itself, because the timing check prints once the call comes back.
// A manager that never comes back announces nothing: the board stays associated to Wi-Fi,
// answers no requests, prints nothing, and from outside is indistinguishable from one that has
// been unplugged. Worse, on this hardware the obvious way to look - opening the serial port -
// resets the chip through DTR/RTS and destroys the very state being investigated.
//
// So the loop writes down which step it is about to run, and that note lives in RTC memory,
// which survives a watchdog reset and a panic. After the reboot /api/info reports it, and the
// step that was running when everything stopped is named rather than guessed at.
class LoopWatchClass {
public:
    void begin();
    // Called before each manager, with its index in the step table.
    void enter(uint8_t step);
    // Called once a full pass is done, so a completed pass is distinguishable from one that hung.
    void completed();

    // What the previous life was doing when it ended. Valid only when hasPrevious() is true -
    // after a power-on the RTC contents are meaningless and are deliberately not reported.
    bool hasPrevious() const { return _hadPrevious; }
    uint8_t previousStep() const { return _previousStep; }
    const char* previousStepName() const { return stepName(_previousStep); }
    uint32_t previousUptime() const { return _previousUptime; }
    bool previousWasMidStep() const { return _previousMidStep; }
    uint32_t previousIterations() const { return _previousIterations; }

    static const char* stepName(uint8_t step);

private:
    bool _hadPrevious = false;
    uint8_t _previousStep = 0;
    uint32_t _previousUptime = 0;
    bool _previousMidStep = false;
    uint32_t _previousIterations = 0;
};

extern LoopWatchClass LoopWatch;

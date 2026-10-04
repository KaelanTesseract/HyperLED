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
#include <map>
#include "ScriptTask.h"

// The scripts that draw this Master's own segments: one Script::Task per segment, created when the
// first script for the segment arrives and ended when it goes. Main loop only (LEDManager and the
// plugin manager call it from there).
class MasterScriptsClass {
public:
    // The task for this segment on a width x height canvas; created, or replaced when the size
    // differs. nullptr when there is no memory.
    Script::Task* ensure(uint8_t segId, uint16_t width, uint16_t height);
    Script::Task* get(uint8_t segId);
    void stop(uint8_t segId);
    void stopAll();
    size_t count() const { return _tasks.size(); }

private:
    std::map<uint8_t, Script::Task*> _tasks;
};

extern MasterScriptsClass MasterScripts;

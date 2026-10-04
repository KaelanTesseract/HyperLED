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
#include "MasterScripts.h"

MasterScriptsClass MasterScripts;

Script::Task* MasterScriptsClass::ensure(uint8_t segId, uint16_t width, uint16_t height) {
    auto it = _tasks.find(segId);
    if (it != _tasks.end()) {
        Script::Task* task = it->second;
        if (task->running() && task->width() == width && task->height() == height) return task;
        task->stop();
        delete task;
        _tasks.erase(it);
    }
    if (width == 0 || height == 0) return nullptr;
    Script::Limits limits;
    limits.memoryBytes = 49152;  // measured: the Lua heap in PSRAM costs no time
    // The time budget is for one call into the script, in the task of its own: it guards against an
    // endless loop and bounds how late a frame can be, not how long the main loop waits (it never
    // does). A big matrix takes longer to fill: a 64x64 plasma is about 64 ms on the Master.
    limits.budgetUs = (uint32_t)width * height > 1024 ? 120000 : 40000;
    limits.useSpiram = true;
    Script::Task* task = new Script::Task();
    if (!task->start(width, height, limits)) {
        delete task;
        return nullptr;
    }
    _tasks[segId] = task;
    return task;
}

Script::Task* MasterScriptsClass::get(uint8_t segId) {
    auto it = _tasks.find(segId);
    return it == _tasks.end() ? nullptr : it->second;
}

void MasterScriptsClass::stop(uint8_t segId) {
    auto it = _tasks.find(segId);
    if (it == _tasks.end()) return;
    it->second->stop();
    delete it->second;
    _tasks.erase(it);
}

void MasterScriptsClass::stopAll() {
    for (auto& entry : _tasks) {
        entry.second->stop();
        delete entry.second;
    }
    _tasks.clear();
}

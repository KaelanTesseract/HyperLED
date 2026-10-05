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

// The last minute of everything the firmware prints, kept in the controller itself.
//
// The serial port is only useful while a computer is reading it, and the interesting moment - the
// radio stops, the controller restarts - comes without warning. So every line the firmware prints
// is also copied into a small ring in RTC memory, which a software restart, a crash and a watchdog
// reset all leave alone. Only the last minute is kept: entries older than that, or that no longer
// fit, are dropped, so the ring never grows and nothing is ever written to flash while it runs.
//
// After a restart that was not routine - a crash, a watchdog, a restart because the link or the
// radio had died - the minute before it is written once to /lastlog.txt (one file, overwritten
// by the next such restart), where GET /api/lastlog reads it. A routine restart (an update, a
// settings change) leaves that file alone. GET /api/log shows the last minute of the running
// controller.
//
// How the firmware's output gets here: the project's own sources are compiled with this header
// forced in (build_src_flags in platformio.ini), which makes `Serial` mean LogSerial there - a
// Print that copies every write into the ring and then hands it to the real port, unchanged. The
// libraries are not touched.
class LogRingClass {
public:
    static const uint32_t WINDOW_MS = 60000;  // how much history is kept
    static const size_t CAPACITY = 6144;      // bytes of ring, headers included

    // First thing in setup(): rescues what the previous run left in RTC memory and starts a fresh ring.
    void begin();
    // Once the filesystem is mounted: keeps the previous run's log if it ended badly.
    void persistPrevious();

    // From any task. Copies the bytes into the ring; never blocks, never fails.
    void append(const uint8_t* data, size_t length);

    // Call before a restart that is a symptom rather than a routine step (the radio or the link is dead).
    void noteRestart(const char* why);

    String current();      // the last minute of this run, as text
    String lastKept();     // the minute before the last restart that was not routine; empty if none

private:
    bool _ready = false;
    String _keptText;      // the previous run's log until persistPrevious() has stored it
};

extern LogRingClass LogRing;

#ifdef HYPERLED_LOG_RING
// Stands in for `Serial` in the project's sources. Same behaviour, plus the copy into the ring.
class LogSerialClass : public Print {
public:
    void begin(unsigned long baud);
    void setTxTimeoutMs(uint32_t ms);
    void flush() override;
    size_t write(uint8_t c) override;
    size_t write(const uint8_t* buffer, size_t size) override;
    using Print::write;
};

extern LogSerialClass LogSerial;
// In the core `Serial` is itself a macro (HWCDCSerial on this board). Keep it, so that LogRing.cpp
// can put it back and reach the real port.
#pragma push_macro("Serial")
#undef Serial
#define Serial LogSerial
#endif

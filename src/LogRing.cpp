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
#include "LogRing.h"
#include <LittleFS.h>
#include <esp_system.h>

// This file talks to the real port: put the core's `Serial` back (see LogRing.h).
#ifdef HYPERLED_LOG_RING
#pragma pop_macro("Serial")
#endif

LogRingClass LogRing;

namespace {

const uint32_t MAGIC = 0x4C4F4752;  // "LOGR"; change it when the layout changes
const size_t CAPACITY = LogRingClass::CAPACITY;
const size_t HEADER = 6;            // uint32 time in ms, uint16 length
const size_t MAX_CHUNK = 240;
const char* const KEPT_FILE = "/lastlog.txt";

// Lives in RTC memory, which a software restart does not clear. After power-on its content is
// anything, which is why begin() checks it before believing it.
struct Store {
    uint32_t magic;
    uint32_t head;      // where the oldest entry starts
    uint32_t used;      // bytes in use, headers included
    uint32_t flags;     // bit 0: the restart that ends this run was not routine
    uint32_t lastMs;    // time of the newest entry
    char reason[32];    // why, when bit 0 is set
    uint8_t data[CAPACITY];
};
RTC_NOINIT_ATTR Store g_store;
portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;

inline uint8_t at(const Store& s, uint32_t pos) { return s.data[pos % CAPACITY]; }
inline uint32_t readTime(const Store& s, uint32_t pos) {
    return (uint32_t)at(s, pos) | ((uint32_t)at(s, pos + 1) << 8) | ((uint32_t)at(s, pos + 2) << 16) |
           ((uint32_t)at(s, pos + 3) << 24);
}
inline uint16_t readLength(const Store& s, uint32_t pos) {
    return (uint16_t)(at(s, pos + 4) | (at(s, pos + 5) << 8));
}

// Whether the ring is a ring: every entry has a sane length and they add up exactly.
bool consistent(const Store& s) {
    if (s.magic != MAGIC || s.head >= CAPACITY || s.used > CAPACITY) return false;
    uint32_t pos = 0;
    while (pos < s.used) {
        if (s.used - pos < HEADER) return false;
        uint16_t length = readLength(s, s.head + pos);
        if (length == 0 || length > MAX_CHUNK || pos + HEADER + length > s.used) return false;
        pos += HEADER + length;
    }
    return pos == s.used;
}

// The ring as text, oldest first. Each line starts with how long before `refMs` it was printed.
String render(const Store& s, uint32_t refMs) {
    String out;
    out.reserve(s.used + 256);
    bool lineStart = true;
    uint32_t pos = 0;
    while (pos < s.used) {
        uint32_t base = s.head + pos;
        uint32_t time = readTime(s, base);
        uint16_t length = readLength(s, base);
        uint32_t ago = refMs - time;
        if (ago > LogRingClass::WINDOW_MS) {  // trimming only happens when something is printed
            pos += HEADER + length;
            continue;
        }
        char prefix[20];
        snprintf(prefix, sizeof(prefix), "[-%lu.%lus] ", (unsigned long)(ago / 1000), (unsigned long)((ago % 1000) / 100));
        for (uint16_t i = 0; i < length; i++) {
            char c = (char)at(s, base + HEADER + i);
            if (c == '\r') continue;
            if (lineStart && c != '\n') out += prefix;
            out += c;
            lineStart = (c == '\n');
        }
        pos += HEADER + length;
    }
    return out;
}

const char* resetName(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON: return "power-on";
        case ESP_RST_SW: return "software restart";
        case ESP_RST_PANIC: return "crash (panic)";
        case ESP_RST_INT_WDT: return "interrupt watchdog";
        case ESP_RST_TASK_WDT: return "task watchdog";
        case ESP_RST_WDT: return "watchdog";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_DEEPSLEEP: return "deep sleep";
        default: return "other";
    }
}

bool badReset(esp_reset_reason_t reason) {
    return reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT ||
           reason == ESP_RST_WDT || reason == ESP_RST_BROWNOUT;
}

// Drops the oldest entry. Under the lock.
void dropOldest() {
    uint16_t length = readLength(g_store, g_store.head);
    uint32_t size = HEADER + length;
    g_store.head = (g_store.head + size) % CAPACITY;
    g_store.used -= size;
}

void appendOne(const uint8_t* data, size_t length, uint32_t now) {
    portENTER_CRITICAL(&g_mux);
    while (g_store.used > 0 &&
           (now - readTime(g_store, g_store.head) > LogRingClass::WINDOW_MS ||
            g_store.used + HEADER + length > CAPACITY)) {
        dropOldest();
    }
    uint32_t pos = (g_store.head + g_store.used) % CAPACITY;
    g_store.data[pos % CAPACITY] = (uint8_t)(now & 0xFF);
    g_store.data[(pos + 1) % CAPACITY] = (uint8_t)((now >> 8) & 0xFF);
    g_store.data[(pos + 2) % CAPACITY] = (uint8_t)((now >> 16) & 0xFF);
    g_store.data[(pos + 3) % CAPACITY] = (uint8_t)((now >> 24) & 0xFF);
    g_store.data[(pos + 4) % CAPACITY] = (uint8_t)(length & 0xFF);
    g_store.data[(pos + 5) % CAPACITY] = (uint8_t)((length >> 8) & 0xFF);
    for (size_t i = 0; i < length; i++) g_store.data[(pos + HEADER + i) % CAPACITY] = data[i];
    g_store.lastMs = now;
    g_store.used += HEADER + length;  // last, so a reset in the middle leaves the ring consistent
    portEXIT_CRITICAL(&g_mux);
}

}  // namespace

void LogRingClass::begin() {
    esp_reset_reason_t reason = esp_reset_reason();
    bool usable = reason != ESP_RST_POWERON && consistent(g_store);
    if (usable && g_store.used > 0) {
        bool kept = (g_store.flags & 1) != 0 || badReset(reason);
        if (kept) {
            char why[48];
            if (g_store.flags & 1) snprintf(why, sizeof(why), "%s", g_store.reason);
            else snprintf(why, sizeof(why), "%s", resetName(reason));
            String text;
            text.reserve(g_store.used + 200);
            text += "HyperLED: the last minute before a restart that was not routine\n";
            text += "Reason: ";
            text += why;
            text += "\nUptime of that run: ";
            text += String((unsigned long)(g_store.lastMs / 1000));
            text += " s (the times below are seconds before its last line)\n---\n";
            text += render(g_store, g_store.lastMs);
            _keptText = text;
        }
    }
    memset(&g_store, 0, sizeof(g_store));
    g_store.magic = MAGIC;
    _ready = true;
}

void LogRingClass::persistPrevious() {
    if (_keptText.length() == 0) return;
    if (!LittleFS.begin()) return;  // no-op if already mounted
    File f = LittleFS.open(KEPT_FILE, "w");
    if (!f) return;
    size_t written = f.write((const uint8_t*)_keptText.c_str(), _keptText.length());
    f.close();
    if (written == _keptText.length()) {
        Serial.printf("LogRing: the minute before the last restart was kept in %s (%u bytes)\n", KEPT_FILE,
                      (unsigned)written);
        _keptText = String();  // it is in the file now; give the memory back
    }
}

void LogRingClass::append(const uint8_t* data, size_t length) {
    if (!_ready || data == nullptr || length == 0) return;
    uint32_t now = millis();
    while (length > 0) {
        size_t n = length > MAX_CHUNK ? MAX_CHUNK : length;
        appendOne(data, n, now);
        data += n;
        length -= n;
    }
}

void LogRingClass::noteRestart(const char* why) {
    if (!_ready) return;
    char line[96];
    int n = snprintf(line, sizeof(line), "LogRing: restarting because of %s\n", why);
    if (n > 0) append((const uint8_t*)line, (size_t)(n < (int)sizeof(line) ? n : sizeof(line) - 1));
    portENTER_CRITICAL(&g_mux);
    g_store.flags |= 1;
    strlcpy(g_store.reason, why, sizeof(g_store.reason));
    portEXIT_CRITICAL(&g_mux);
}

String LogRingClass::current() {
    // Copy under the lock, render outside it: the copy is a few microseconds, the rendering is not.
    Store* copy = (Store*)malloc(sizeof(Store));
    if (!copy) return String("not enough memory\n");
    portENTER_CRITICAL(&g_mux);
    memcpy(copy, &g_store, sizeof(Store));
    portEXIT_CRITICAL(&g_mux);
    uint32_t now = millis();
    String out = "HyperLED: the last minute of this run (uptime ";
    out += String((unsigned long)(now / 1000));
    out += " s; the times are seconds ago)\n---\n";
    out += render(*copy, now);
    free(copy);
    return out;
}

String LogRingClass::lastKept() {
    if (_keptText.length() > 0) return _keptText;
    File f = LittleFS.open(KEPT_FILE, "r");
    if (!f) return String();
    String text = f.readString();
    f.close();
    return text;
}

#ifdef HYPERLED_LOG_RING
LogSerialClass LogSerial;

void LogSerialClass::begin(unsigned long baud) { Serial.begin(baud); }
void LogSerialClass::setTxTimeoutMs(uint32_t ms) { Serial.setTxTimeoutMs(ms); }
void LogSerialClass::flush() { Serial.flush(); }

size_t LogSerialClass::write(uint8_t c) {
    LogRing.append(&c, 1);
    return Serial.write(c);
}

size_t LogSerialClass::write(const uint8_t* buffer, size_t size) {
    LogRing.append(buffer, size);
    return Serial.write(buffer, size);
}
#endif

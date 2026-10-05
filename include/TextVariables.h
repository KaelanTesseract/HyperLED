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
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <functional>
#include <map>

// Text that contains {name.part} stands for a value that someone else provides: "Outside
// {weather.temp} degrees". This is the small, general building block behind that. Whoever has values
// puts them in TextVariables under a key "name.part"; whoever draws text asks it to fill them in.
// Neither side knows the other - the plugin manager is one provider today, anything else could be
// one tomorrow.
namespace TextTemplate {

static const size_t MAX_RESULT = 64;       // bytes of a text with placeholders filled in
static const char* const UNKNOWN = "--";   // what a placeholder shows while its value is not known

// text[open] is '{': is this a placeholder {name.part}? Then `key` is "name.part" and `end` is the index
// behind the '}'. name: a-z 0-9 _ - (1 to 32); part: a-z 0-9 _ (1 to 24). Anything else in braces is
// not a placeholder and stays as it is.
bool parseKey(const String& text, size_t open, String& key, size_t& end);

// Fills in every placeholder. lookup(key, value) says whether the key is known and gives its value.
// The filled-in text is not searched again, so a value can never bring a placeholder of its own. When
// something was filled in, the result is cut to MAX_RESULT bytes; a text without a placeholder comes
// back untouched.
String expand(const String& text, const std::function<bool(const String&, String&)>& lookup);

}  // namespace TextTemplate

class TextVariablesClass {
public:
    // Creates the lock. Call once, before anything else.
    void begin();

    // Replaces all variables at once. True, and the generation goes up, only if something changed.
    // There is one provider today (the plugin manager, which publishes everything every pass); a
    // second one would need to keep its own keys apart from the first one's.
    bool set(std::map<String, String>&& all);

    // The text with its placeholders filled in from the variables (unknown ones show "--").
    String expand(const String& text) const;

    // Goes up whenever the variables change; a drawn text can be kept as long as it stays the same.
    uint32_t generation() const { return _generation; }

private:
    mutable SemaphoreHandle_t _lock = nullptr;
    std::map<String, String> _vars;
    volatile uint32_t _generation = 1;
};

extern TextVariablesClass TextVariables;

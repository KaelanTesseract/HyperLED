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
#include "TextVariables.h"

TextVariablesClass TextVariables;

namespace {

bool nameChar(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'; }
bool partChar(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; }

}  // namespace

bool TextTemplate::parseKey(const String& text, size_t open, String& key, size_t& end) {
    size_t i = open + 1;
    size_t n = text.length();
    size_t nameStart = i;
    while (i < n && nameChar(text[i])) i++;
    size_t nameLength = i - nameStart;
    if (nameLength < 1 || nameLength > 32 || i >= n || text[i] != '.') return false;
    size_t partStart = ++i;
    while (i < n && partChar(text[i])) i++;
    size_t partLength = i - partStart;
    if (partLength < 1 || partLength > 24 || i >= n || text[i] != '}') return false;
    key = text.substring(nameStart, i);
    end = i + 1;
    return true;
}

String TextTemplate::expand(const String& text, const std::function<bool(const String&, String&)>& lookup) {
    if (text.indexOf('{') < 0) return text;
    String out;
    out.reserve(text.length() + 16);
    bool filled = false;
    for (size_t i = 0; i < text.length();) {
        String key;
        size_t end = 0;
        if (text[i] == '{' && parseKey(text, i, key, end)) {
            String value;
            if (lookup(key, value)) out += value;
            else out += UNKNOWN;
            filled = true;
            i = end;
        } else {
            out += text[i++];
        }
        if (filled && out.length() > MAX_RESULT) break;  // no point in going on
    }
    if (filled && out.length() > MAX_RESULT) out.remove(MAX_RESULT);
    return out;
}

void TextVariablesClass::begin() {
    if (_lock == nullptr) _lock = xSemaphoreCreateMutex();
}

bool TextVariablesClass::set(std::map<String, String>&& all) {
    if (_lock == nullptr) return false;
    xSemaphoreTake(_lock, portMAX_DELAY);
    bool changed = !(_vars == all);
    if (changed) {
        _vars.swap(all);
        _generation = _generation + 1;
    }
    xSemaphoreGive(_lock);
    return changed;
}

String TextVariablesClass::expand(const String& text) const {
    if (text.indexOf('{') < 0 || _lock == nullptr) return text;
    xSemaphoreTake(_lock, portMAX_DELAY);
    String out = TextTemplate::expand(text, [this](const String& key, String& value) {
        auto it = _vars.find(key);
        if (it == _vars.end()) return false;
        value = it->second;
        return true;
    });
    xSemaphoreGive(_lock);
    return out;
}

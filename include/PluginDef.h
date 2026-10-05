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
#include <ArduinoJson.h>
#include <functional>
#include <vector>
#include "PluginExpr.h"

// A plugin file, read and checked. The checks are strict on purpose: whoever writes a plugin gets
// told at once - in German, with the place - what is wrong, instead of a plugin that half works.
namespace PluginDef {

constexpr size_t MAX_FILE = 16384;
constexpr size_t MAX_SCRIPT = 8192;
constexpr size_t MAX_SETTINGS = 24;
constexpr size_t MAX_VALUES = 16;
constexpr size_t MAX_RULES = 12;
constexpr uint16_t MIN_EVERY_SECONDS = 2;

// A text a person reads, in the languages of the web interface. A language the plugin does not give
// falls back to German, and German to whatever there is.
struct Text {
    String de, en, ru;
    String get(const String& lang) const {
        const String* pick = lang == "en" ? &en : (lang == "ru" ? &ru : &de);
        if (pick->length() > 0) return *pick;
        if (de.length() > 0) return de;
        if (en.length() > 0) return en;
        return ru;
    }
    bool empty() const { return de.length() == 0 && en.length() == 0 && ru.length() == 0; }
};

constexpr size_t MAX_LABEL = 60;
constexpr size_t MAX_HINT = 200;

struct Setting {
    String key;
    String type;  // text, password, number, switch, list, color, effect, segment
    String defaultValue;
    bool optional = false;
    bool hasRange = false;
    double minValue = 0;
    double maxValue = 0;
    std::vector<String> options;  // for type list: the values
    Text label;                   // what the person sees next to the field
    Text hint;                    // a line of help under it
    std::vector<Text> optionLabels;  // for type list, one per option (the value itself when the plugin gives none)
};

struct ValueDef {
    String name;
    PluginExpr::Program expr;  // reads paths into the response
};

// What a rule does to its segment. Brightness is deliberately not here: the user's sliders act
// literally.
struct Show {
    int16_t effect = -1;                // index into EFFECT_NAMES, -1 = leave as it is
    String effectKey;                   // key of a setting of type effect, instead of a fixed effect
    String color;                       // "#rrggbb" or "{setting}", empty = leave
    PluginExpr::Program speed;          // percent 0-100, empty = leave
    PluginExpr::Program intensity;      // percent 0-100, empty = leave; "value" is an alias
    int8_t power = -1;                  // -1 leave, 0 off, 1 on
    bool any() const {
        return effect >= 0 || effectKey.length() > 0 || color.length() > 0 || !speed.empty() || !intensity.empty() || power >= 0;
    }
};

struct Rule {
    PluginExpr::Program when;  // empty for on_error
    String whenText;           // the condition as written (at most 120 characters), for the interface
    Show show;
};

struct Header {
    String name;
    String value;  // may hold {setting} placeholders
};

struct Source {
    String url;  // may hold {setting} placeholders
    uint16_t every = 5;
    uint16_t timeoutMs = 5000;
    std::vector<Header> headers;
};

struct Definition {
    String id, name, version, author, license, description;
    uint16_t api = 1;       // plugin interface level this plugin needs
    String minFirmware;     // "" = no minimum
    uint8_t scriptLevel = 0;  // script support this plugin needs; 0 = it has no script
    bool hasScript = false;   // the file holds a script (only its presence is kept in memory)
    std::vector<Setting> settings;
    Source source;
    std::vector<ValueDef> values;
    std::vector<Rule> rules;
    bool hasOnError = false;
    Show onError;
    String segmentKey;                  // key of the one setting of type segment, "" if none
    bool wantsPower = false;            // some rule switches on or off
    std::vector<String> responsePaths;  // every path the value expressions read
};

// Reads and checks a plugin file. On failure `error` says what is wrong, `out` is empty.
bool parse(const String& json, Definition& out, String& error);

// Fills in the {name} placeholders of a template; `lookup` supplies the values.
String expand(const String& tmpl, const std::function<String(const String&)>& lookup);

const Setting* findSetting(const Definition& def, const String& key);

// An effect by its name (as in EFFECT_NAMES): its index, or -1. And whether plugins may not use
// it: "Nur Weiß", "Bild" and "Uhr / Text" need what only the user's own segment holds.
int effectIndex(const String& name);
bool effectBlocked(int index);

}  // namespace PluginDef

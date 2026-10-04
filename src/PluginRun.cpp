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
#include "PluginRun.h"

#include <math.h>

namespace PluginRun {

String settingText(const std::vector<SettingValue>& values, const String& key) {
    for (const SettingValue& v : values) {
        if (v.key == key) return v.value;
    }
    return String();
}

PluginExpr::Value settingValue(const PluginDef::Setting& setting, const String& text) {
    if (setting.type == "number") {
        if (text.length() == 0) return PluginExpr::Value();
        return PluginExpr::Value::number(text.toDouble());
    }
    if (setting.type == "switch") return PluginExpr::Value::boolean(text == "true");
    return PluginExpr::Value::string(text);
}

PluginExpr::Value InstanceScope::get(const String& name) const {
    for (size_t i = 0; i < _def.values.size() && i < _results.size(); i++) {
        if (_def.values[i].name == name) return _results[i];
    }
    const PluginDef::Setting* setting = PluginDef::findSetting(_def, name);
    if (setting) return settingValue(*setting, settingText(_settings, name));
    return PluginExpr::Value();
}

int chooseIndex(const PluginDef::Definition& def, const PluginExpr::Scope& scope, bool failing) {
    if (failing) return def.hasOnError ? RULE_ON_ERROR : RULE_NONE;
    for (size_t i = 0; i < def.rules.size(); i++) {
        if (def.rules[i].when.run(scope).truthy()) return (int)i;
    }
    return RULE_NONE;
}

const PluginDef::Show* choose(const PluginDef::Definition& def, const PluginExpr::Scope& scope, bool failing) {
    int index = chooseIndex(def, scope, failing);
    if (index == RULE_ON_ERROR) return &def.onError;
    if (index >= 0) return &def.rules[index].show;
    return nullptr;
}

int32_t parseColor(const String& text) {
    if (text.length() != 7 || text[0] != '#') return -1;
    for (size_t i = 1; i < 7; i++) {
        if (!isxdigit((unsigned char)text[i])) return -1;
    }
    return (int32_t)(strtol(text.c_str() + 1, nullptr, 16) & 0xFFFFFF);
}

std::vector<Script::Item> scriptSettings(const PluginDef::Definition& def, const std::vector<SettingValue>& settings) {
    std::vector<Script::Item> out;
    for (const PluginDef::Setting& s : def.settings) {
        if (s.type == "password") continue;
        String text = settingText(settings, s.key);
        if (s.type == "switch") {
            out.push_back(Script::Item::flag(s.key, text == "true"));
            continue;
        }
        if (text.length() == 0) continue;
        if (s.type == "number" || s.type == "segment") {
            out.push_back(Script::Item::num(s.key, text.toDouble()));
        } else if (s.type == "color") {
            int32_t rgb = parseColor(text);
            if (rgb >= 0) out.push_back(Script::Item::num(s.key, (double)rgb));
        } else {
            out.push_back(Script::Item::txt(s.key, text));
        }
    }
    return out;
}

std::vector<Script::Item> scriptValues(const PluginDef::Definition& def, const std::vector<PluginExpr::Value>& results) {
    std::vector<Script::Item> out;
    for (size_t i = 0; i < def.values.size() && i < results.size(); i++) {
        const PluginExpr::Value& v = results[i];
        if (!v.known()) continue;
        if (v.type == PluginExpr::Type::Number) out.push_back(Script::Item::num(def.values[i].name, v.num));
        else if (v.type == PluginExpr::Type::Bool) out.push_back(Script::Item::flag(def.values[i].name, v.num != 0));
        else out.push_back(Script::Item::txt(def.values[i].name, v.text));
    }
    return out;
}

bool sameItems(const std::vector<Script::Item>& a, const std::vector<Script::Item>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) {
        if (a[i].name != b[i].name || a[i].kind != b[i].kind || a[i].number != b[i].number || a[i].text != b[i].text) return false;
    }
    return true;
}

namespace {

// A percentage expression as 0-255, or -1 when there is none or its value is unknown or text.
int16_t percentTo255(const PluginExpr::Program& program, const PluginExpr::Scope& scope) {
    if (program.empty()) return -1;
    PluginExpr::Value v = program.run(scope);
    if (!v.known() || v.type == PluginExpr::Type::Text) return -1;
    double percent = v.num;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    return (int16_t)lround(percent * 255.0 / 100.0);
}

}  // namespace

SegmentOverlay toOverlay(const PluginDef::Show& show, const std::vector<SettingValue>& settings,
                         const PluginExpr::Scope& scope, bool allowPower) {
    SegmentOverlay overlay;
    if (show.effect >= 0) {
        overlay.effect = show.effect;
    } else if (show.effectKey.length() > 0) {
        int idx = PluginDef::effectIndex(settingText(settings, show.effectKey));
        if (idx >= 0 && !PluginDef::effectBlocked(idx)) overlay.effect = (int16_t)idx;
    }
    if (show.color.length() > 0) {
        String color = show.color;
        if (color.startsWith("{")) color = settingText(settings, color.substring(1, color.length() - 1));
        overlay.color = parseColor(color);
    }
    overlay.speed = percentTo255(show.speed, scope);
    overlay.intensity = percentTo255(show.intensity, scope);
    if (allowPower && show.power >= 0) overlay.power = show.power;
    return overlay;
}

bool overlayEmpty(const SegmentOverlay& o) {
    return o.effect < 0 && o.color < 0 && o.speed < 0 && o.intensity < 0 && o.power < 0 && !o.script;
}

bool sameOverlay(const SegmentOverlay& a, const SegmentOverlay& b) {
    return a.effect == b.effect && a.color == b.color && a.speed == b.speed &&
           a.intensity == b.intensity && a.power == b.power && a.script == b.script;
}

}  // namespace PluginRun

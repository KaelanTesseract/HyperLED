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
#include <vector>
#include "LEDManager.h"  // SegmentOverlay
#include "PluginDef.h"
#include "PluginExpr.h"
#include "ScriptHost.h"

// What a plugin shows: which of its rules applies to what the source answered, and what that does
// to the segment. No network and no storage in here, so all of it can be checked on its own.
namespace PluginRun {

// One setting as the user has it, kept as text whatever its type.
struct SettingValue {
    String key;
    String value;
};

// The text of a setting; empty when there is no such setting.
String settingText(const std::vector<SettingValue>& values, const String& key);

// A setting as an expression value: a number setting is a number, a switch a boolean, anything
// else text. An empty number setting is Unknown.
PluginExpr::Value settingValue(const PluginDef::Setting& setting, const String& text);

// Where a plugin's rules get their names from: first the values read from the source (`results`,
// in the order of def.values), then the settings.
class InstanceScope : public PluginExpr::Scope {
public:
    InstanceScope(const PluginDef::Definition& def, const std::vector<PluginExpr::Value>& results,
                  const std::vector<SettingValue>& settings)
        : _def(def), _results(results), _settings(settings) {}
    PluginExpr::Value get(const String& name) const override;

private:
    const PluginDef::Definition& _def;
    const std::vector<PluginExpr::Value>& _results;
    const std::vector<SettingValue>& _settings;
};

// What the plugin shows: the first rule whose condition holds - or, when the source is failing,
// on_error instead of the rules. nullptr when nothing applies; the segment then stays the user's.
const PluginDef::Show* choose(const PluginDef::Definition& def, const PluginExpr::Scope& scope, bool failing);
// The same decision as its place: the number of the rule, RULE_ON_ERROR for on_error, RULE_NONE for nothing.
constexpr int RULE_NONE = -1;
constexpr int RULE_ON_ERROR = -2;
int chooseIndex(const PluginDef::Definition& def, const PluginExpr::Scope& scope, bool failing);

// The overlay a show stands for. Percentages (0-100) become 0-255; a value that is unknown leaves
// its field alone. Power is only taken over when the user allowed it.
SegmentOverlay toOverlay(const PluginDef::Show& show, const std::vector<SettingValue>& settings,
                         const PluginExpr::Scope& scope, bool allowPower);

// Whether the overlay changes nothing (the `active` flag is not looked at; a script is a change).
bool overlayEmpty(const SegmentOverlay& overlay);
bool sameOverlay(const SegmentOverlay& a, const SegmentOverlay& b);

// "#rrggbb" as 0xRRGGBB; -1 when it is not a colour.
int32_t parseColor(const String& text);

// What a plugin's script sees. Settings: numbers and segments become numbers, switches booleans,
// colours numbers (0xRRGGBB), text, lists and effects text; an empty setting or a colour that is not
// one is left out, and passwords are never passed on - the script may run on a Slave, and everything
// it is given travels over the air.
std::vector<Script::Item> scriptSettings(const PluginDef::Definition& def, const std::vector<SettingValue>& settings);
// The values read from the source, in the order of def.values; unknown ones are left out.
std::vector<Script::Item> scriptValues(const PluginDef::Definition& def, const std::vector<PluginExpr::Value>& results);
// Whether two lists hold the same names, kinds and values (to hand a script new data only when it changed).
bool sameItems(const std::vector<Script::Item>& a, const std::vector<Script::Item>& b);

}  // namespace PluginRun

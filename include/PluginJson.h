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
#include <vector>
#include "PluginExpr.h"

// Reading what a source answered. A path is "a.b.c", with [n] for a list entry: "items[1].name".
namespace PluginJson {

// Builds the ArduinoJson filter that keeps only what the paths need. Everything below a path that
// uses [n] is kept whole - the filter cannot pick single list entries.
void buildFilter(const std::vector<String>& paths, JsonDocument& filter);

// A scope over a parsed response: a name is a path into it. Anything that is not there - or is a
// list or an object - is Unknown.
class ResponseScope : public PluginExpr::Scope {
public:
    explicit ResponseScope(const JsonDocument& doc) : _doc(doc) {}
    PluginExpr::Value get(const String& name) const override;

private:
    const JsonDocument& _doc;
};

}  // namespace PluginJson

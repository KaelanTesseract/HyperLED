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
#include "PluginJson.h"

namespace PluginJson {

void buildFilter(const std::vector<String>& paths, JsonDocument& filter) {
    filter.to<JsonObject>();
    for (const String& path : paths) {
        JsonObject node = filter.as<JsonObject>();
        size_t i = 0;
        while (i < path.length()) {
            size_t start = i;
            while (i < path.length() && path[i] != '.' && path[i] != '[') i++;
            String key = path.substring(start, i);
            if (key.length() == 0) break;
            bool last = i >= path.length();
            bool indexNext = !last && path[i] == '[';
            if (last || indexNext) {
                // The end of the path, or a list entry follows: keep everything below this key.
                node[key] = true;
                break;
            }
            i++;  // skip the '.'
            JsonVariant child = node[key];
            if (child.is<bool>() && child.as<bool>()) break;  // already keeping all of it
            node = child.is<JsonObject>() ? child.as<JsonObject>() : node[key].to<JsonObject>();
        }
    }
}

PluginExpr::Value ResponseScope::get(const String& name) const {
    JsonVariantConst cur = _doc.as<JsonVariantConst>();
    size_t i = 0;
    while (i < name.length() && !cur.isNull()) {
        if (name[i] == '.') {
            i++;
            continue;
        }
        if (name[i] == '[') {
            int close = name.indexOf(']', (unsigned int)i);
            if (close < 0) return PluginExpr::Value();
            long index = name.substring(i + 1, (unsigned int)close).toInt();
            cur = cur[(size_t)index];
            i = (size_t)close + 1;
            continue;
        }
        size_t start = i;
        while (i < name.length() && name[i] != '.' && name[i] != '[') i++;
        String key = name.substring(start, i);
        cur = cur[key.c_str()];
    }
    if (cur.isNull()) return PluginExpr::Value();
    if (cur.is<bool>()) return PluginExpr::Value::boolean(cur.as<bool>());
    if (cur.is<double>()) return PluginExpr::Value::number(cur.as<double>());
    if (cur.is<const char*>()) return PluginExpr::Value::string(String(cur.as<const char*>()));
    return PluginExpr::Value();  // a list or an object
}

}  // namespace PluginJson

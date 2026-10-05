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
#include "PluginDef.h"

#include "LEDManager.h"  // EFFECT_NAMES, EFFECT_COUNT

namespace PluginDef {

namespace {

bool nameChar(char c, bool allowDash) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || (allowDash && c == '-');
}

bool validName(const String& s, size_t maxLen, bool allowDash) {
    if (s.length() == 0 || s.length() > maxLen) return false;
    for (size_t i = 0; i < s.length(); i++) {
        if (!nameChar(s[i], allowDash)) return false;
    }
    return true;
}

const char* const SETTING_TYPES[] = {"text", "password", "number", "switch", "list", "color", "effect", "segment"};

bool knownType(const String& t) {
    for (const char* k : SETTING_TYPES) {
        if (t == k) return true;
    }
    return false;
}

// Words an expression reserves; a value cannot be named like one.
const char* const RESERVED[] = {"true", "false", "wahr", "falsch", "and", "or", "not", "und", "oder",
                                "nicht", "round", "min", "max", "contains"};

bool reserved(const String& s) {
    for (const char* r : RESERVED) {
        if (s == r) return true;
    }
    return false;
}

// Every key of the object must be one of `allowed`.
bool onlyKeys(JsonObjectConst obj, const char* const* allowed, size_t n, const String& where, String& error) {
    for (JsonPairConst kv : obj) {
        bool ok = false;
        for (size_t i = 0; i < n; i++) {
            if (strcmp(kv.key().c_str(), allowed[i]) == 0) {
                ok = true;
                break;
            }
        }
        if (!ok) {
            error = String("Unbekanntes Feld '") + kv.key().c_str() + "' in " + where;
            return false;
        }
    }
    return true;
}

bool readText(JsonObjectConst obj, const char* key, bool required, size_t maxLen, String& out,
              const String& where, String& error) {
    JsonVariantConst v = obj[key];
    if (v.isNull()) {
        if (!required) return true;
        error = String("Das Feld '") + key + "' fehlt (" + where + ")";
        return false;
    }
    if (!v.is<const char*>()) {
        error = String("Das Feld '") + key + "' muss Text sein (" + where + ")";
        return false;
    }
    out = v.as<const char*>();
    if (required && out.length() == 0) {
        error = String("Das Feld '") + key + "' darf nicht leer sein (" + where + ")";
        return false;
    }
    if (out.length() > maxLen) {
        error = String("Das Feld '") + key + "' ist länger als " + String((unsigned)maxLen) + " Bytes (ä, ö, ü und kyrillische Buchstaben zählen 2) (" + where + ")";
        return false;
    }
    return true;
}

// A text for a person: plain text, or an object with one text per language (de, en, ru; other
// languages are accepted and ignored, so a plugin can already carry them).
bool readLocalText(JsonVariantConst v, size_t maxLen, Text& out, const String& where, const char* field, String& error) {
    if (v.is<const char*>()) {
        out.de = v.as<const char*>();
        if (out.de.length() > maxLen) {
            error = where + ": '" + field + "' ist länger als " + String((unsigned)maxLen) + " Bytes (ä, ö, ü und kyrillische Buchstaben zählen 2)";
            return false;
        }
        return true;
    }
    if (!v.is<JsonObjectConst>()) {
        error = where + ": '" + field + "' muss Text sein oder {\"de\": …, \"en\": …}";
        return false;
    }
    for (JsonPairConst kv : v.as<JsonObjectConst>()) {
        const char* lang = kv.key().c_str();
        if (strlen(lang) != 2 || !islower((unsigned char)lang[0]) || !islower((unsigned char)lang[1])) {
            error = where + ": In '" + field + "' ist '" + lang + "' keine Sprache (zwei Kleinbuchstaben, zum Beispiel de, en)";
            return false;
        }
        if (!kv.value().is<const char*>()) {
            error = where + ": '" + field + "' (" + lang + ") muss Text sein";
            return false;
        }
        String text = kv.value().as<const char*>();
        if (text.length() > maxLen) {
            error = where + ": '" + field + "' (" + lang + ") ist länger als " + String((unsigned)maxLen) + " Bytes (ä, ö, ü und kyrillische Buchstaben zählen 2)";
            return false;
        }
        if (strcmp(lang, "de") == 0) out.de = text;
        else if (strcmp(lang, "en") == 0) out.en = text;
        else if (strcmp(lang, "ru") == 0) out.ru = text;
    }
    return true;
}

// {name} placeholders of a template; each must be a valid name.
bool placeholderKeys(const String& tmpl, std::vector<String>& keys, String& error) {
    size_t i = 0;
    while (i < tmpl.length()) {
        if (tmpl[i] != '{') {
            i++;
            continue;
        }
        int close = tmpl.indexOf('}', (unsigned int)i + 1);
        if (close < 0) {
            error = "Eine Klammer '{' wird nicht geschlossen";
            return false;
        }
        String key = tmpl.substring(i + 1, (unsigned int)close);
        if (!validName(key, 24, false)) {
            error = "Ungültiger Platzhalter '{" + key + "}'";
            return false;
        }
        keys.push_back(key);
        i = (size_t)close + 1;
    }
    return true;
}

bool settingKeyKnown(const Definition& def, const String& key) {
    return findSetting(def, key) != nullptr;
}

// Every {placeholder} of a template must name a setting.
bool checkPlaceholders(const Definition& def, const String& tmpl, const String& where, String& error) {
    std::vector<String> keys;
    String perr;
    if (!placeholderKeys(tmpl, keys, perr)) {
        error = where + ": " + perr;
        return false;
    }
    for (const String& k : keys) {
        if (!settingKeyKnown(def, k)) {
            error = where + ": '{" + k + "}' ist keine Einstellung";
            return false;
        }
    }
    return true;
}

bool compileExpr(JsonVariantConst v, PluginExpr::Program& prog, const String& where, String& error) {
    String src;
    if (v.is<const char*>()) {
        src = v.as<const char*>();
    } else if (v.is<double>()) {
        src = String(v.as<double>(), 3);  // a plain number is an expression too
    } else {
        error = where + ": Zahl oder Ausdruck erwartet";
        return false;
    }
    String err;
    if (!prog.compile(src, err)) {
        error = where + ": " + err;
        return false;
    }
    return true;
}

// The names an expression reads must be values or settings.
bool namesAllowed(const PluginExpr::Program& p, const std::vector<String>& allowed, const String& where,
                  String& error) {
    for (const String& n : p.names()) {
        bool ok = false;
        for (const String& a : allowed) {
            if (a == n) {
                ok = true;
                break;
            }
        }
        if (!ok) {
            error = where + ": '" + n + "' ist weder ein Wert noch eine Einstellung";
            return false;
        }
    }
    return true;
}

bool validColor(const String& s) {
    if (s.length() != 7 || s[0] != '#') return false;
    for (size_t i = 1; i < 7; i++) {
        if (!isxdigit((unsigned char)s[i])) return false;
    }
    return true;
}

bool parseShow(JsonObjectConst obj, const Definition& def, const std::vector<String>& allowed, Show& out,
               const String& where, String& error) {
    static const char* const KEYS[] = {"effect", "color", "speed", "intensity", "value", "power"};
    if (!onlyKeys(obj, KEYS, 6, where, error)) return false;

    if (!obj["effect"].isNull()) {
        if (!obj["effect"].is<const char*>()) {
            error = where + ": 'effect' muss Text sein";
            return false;
        }
        String name = obj["effect"].as<const char*>();
        if (name.startsWith("{") && name.endsWith("}")) {
            // The user picks the effect in the plugin's settings.
            String key = name.substring(1, name.length() - 1);
            const Setting* s = findSetting(def, key);
            if (!s || s->type != "effect") {
                error = where + ": '" + name + "' ist keine Effekt-Einstellung";
                return false;
            }
            out.effectKey = key;
        } else {
        int idx = effectIndex(name);
        if (idx < 0) {
            error = where + ": Unbekannter Effekt '" + name + "'";
            return false;
        }
        // These need what only the user's own segment holds: white mode, an image, the elements.
        if (effectBlocked(idx)) {
            error = where + ": Der Effekt '" + name + "' ist für Plugins gesperrt";
            return false;
        }
        out.effect = (int16_t)idx;
        }
    }

    if (!obj["color"].isNull()) {
        if (!obj["color"].is<const char*>()) {
            error = where + ": 'color' muss Text sein";
            return false;
        }
        String color = obj["color"].as<const char*>();
        if (color.startsWith("{") && color.endsWith("}")) {
            String key = color.substring(1, color.length() - 1);
            const Setting* s = findSetting(def, key);
            if (!s || s->type != "color") {
                error = where + ": '" + color + "' ist keine Farb-Einstellung";
                return false;
            }
        } else if (!validColor(color)) {
            error = where + ": 'color' muss #rrggbb oder {einstellung} sein";
            return false;
        }
        out.color = color;
    }

    if (!obj["speed"].isNull()) {
        if (!compileExpr(obj["speed"], out.speed, where + " speed", error)) return false;
        if (!namesAllowed(out.speed, allowed, where + " speed", error)) return false;
    }
    if (!obj["intensity"].isNull() && !obj["value"].isNull()) {
        error = where + ": 'value' und 'intensity' gleichzeitig - 'value' ist nur ein anderer Name";
        return false;
    }
    JsonVariantConst level = !obj["value"].isNull() ? obj["value"] : obj["intensity"];
    if (!level.isNull()) {
        if (!compileExpr(level, out.intensity, where + " value", error)) return false;
        if (!namesAllowed(out.intensity, allowed, where + " value", error)) return false;
    }

    if (!obj["power"].isNull()) {
        String p = obj["power"].is<const char*>() ? obj["power"].as<const char*>() : "";
        if (p == "on") out.power = 1;
        else if (p == "off") out.power = 0;
        else {
            error = where + ": 'power' muss \"on\" oder \"off\" sein";
            return false;
        }
    }
    return true;
}

bool parseSettings(JsonArrayConst arr, Definition& def, String& error) {
    if (arr.size() > MAX_SETTINGS) {
        error = "Mehr als 16 Einstellungen";
        return false;
    }
    static const char* const KEYS[] = {"key", "type", "label", "default", "optional", "min", "max", "options", "hint"};
    for (JsonVariantConst item : arr) {
        if (!item.is<JsonObjectConst>()) {
            error = "Jede Einstellung muss ein Objekt sein";
            return false;
        }
        JsonObjectConst s = item.as<JsonObjectConst>();
        Setting set;
        String where = "Einstellung";
        if (!onlyKeys(s, KEYS, 9, where, error)) return false;
        if (!readText(s, "key", true, 24, set.key, where, error)) return false;
        where = "Einstellung '" + set.key + "'";
        if (!validName(set.key, 24, false)) {
            error = where + ": 'key' darf nur a-z, 0-9 und _ enthalten";
            return false;
        }
        if (findSetting(def, set.key)) {
            error = where + ": Der Schlüssel '" + set.key + "' kommt doppelt vor";
            return false;
        }
        if (!readText(s, "type", true, 12, set.type, where, error)) return false;
        if (!knownType(set.type)) {
            error = where + ": Unbekannter Typ '" + set.type + "'";
            return false;
        }
        JsonVariantConst label = s["label"];
        if (label.isNull() || !(label.is<const char*>() || label.is<JsonObjectConst>())) {
            error = where + ": 'label' fehlt (Text oder {\"de\": …, \"en\": …})";
            return false;
        }
        if (!readLocalText(label, MAX_LABEL, set.label, where, "label", error)) return false;
        if (!s["hint"].isNull() && !readLocalText(s["hint"], MAX_HINT, set.hint, where, "hint", error)) return false;
        JsonVariantConst d = s["default"];
        if (d.is<const char*>()) set.defaultValue = d.as<const char*>();
        else if (d.is<bool>()) set.defaultValue = d.as<bool>() ? "true" : "false";
        else if (d.is<double>()) set.defaultValue = PluginExpr::Value::number(d.as<double>()).toText();
        set.optional = s["optional"] | false;
        if (!s["min"].isNull() || !s["max"].isNull()) {
            set.hasRange = true;
            set.minValue = s["min"] | 0.0;
            set.maxValue = s["max"] | 0.0;
        }
        if (set.type == "list") {
            JsonArrayConst opts = s["options"];
            if (opts.isNull() || opts.size() == 0 || opts.size() > 20) {
                error = where + ": 'options' braucht 1 bis 20 Einträge";
                return false;
            }
            size_t texts = 0, objects = 0;
            for (JsonVariantConst o : opts) {
                if (o.is<const char*>()) texts++;
                else if (o.is<JsonObjectConst>()) objects++;
                else {
                    error = where + ": 'options' darf nur Text oder {\"value\": …, \"label\": …} enthalten";
                    return false;
                }
            }
            if (texts > 0 && objects > 0) {
                error = where + ": 'options': entweder alle Einträge Text oder alle Objekte";
                return false;
            }
            for (JsonVariantConst o : opts) {
                Text label;
                String value;
                if (o.is<const char*>()) {
                    value = o.as<const char*>();
                    label.de = value;
                } else {
                    JsonObjectConst obj = o.as<JsonObjectConst>();
                    if (!obj["value"].is<const char*>() || String(obj["value"].as<const char*>()).length() == 0) {
                        error = where + ": Jede Option braucht ein 'value' (Text)";
                        return false;
                    }
                    value = obj["value"].as<const char*>();
                    if (obj["label"].isNull()) label.de = value;
                    else if (!readLocalText(obj["label"], MAX_LABEL, label, where + ", Option '" + value + "'", "label", error)) return false;
                }
                if (value.length() > 60) {
                    error = where + ": Eine Option ist länger als 60 Bytes (ä, ö, ü und kyrillische Buchstaben zählen 2)";
                    return false;
                }
                set.options.push_back(value);
                set.optionLabels.push_back(label);
            }
        }
        if (set.type == "segment") {
            if (def.segmentKey.length()) {
                error = where + ": Es darf nur eine Einstellung vom Typ Segment geben";
                return false;
            }
            def.segmentKey = set.key;
        }
        def.settings.push_back(set);
    }
    return true;
}

bool parseInto(const String& json, Definition& out, String& error) {
    if (json.length() > MAX_FILE) {
        error = "Die Datei ist größer als 16 KB";
        return false;
    }
    JsonDocument doc;
    DeserializationError jerr = deserializeJson(doc, json);
    if (jerr) {
        error = String("Kein gültiges JSON: ") + jerr.c_str();
        return false;
    }
    if (!doc.is<JsonObject>()) {
        error = "Die oberste Ebene muss ein Objekt sein";
        return false;
    }
    JsonObjectConst root = doc.as<JsonObjectConst>();
    static const char* const ROOT[] = {"format", "id", "name", "version", "author", "license",
                                       "description", "needs", "settings", "source", "values", "rules", "on_error",
                                       "script"};
    if (!onlyKeys(root, ROOT, 14, "der Datei", error)) return false;

    if ((root["format"] | 0) != 1) {
        error = "Das Dateiformat ('format') ist unbekannt: erwartet 1";
        return false;
    }
    if (!readText(root, "id", true, 32, out.id, "der Datei", error)) return false;
    if (!validName(out.id, 32, true)) {
        error = "Die 'id' darf nur a-z, 0-9, - und _ enthalten (höchstens 32 Zeichen)";
        return false;
    }
    if (!readText(root, "name", true, 40, out.name, "der Datei", error)) return false;
    if (!readText(root, "version", true, 16, out.version, "der Datei", error)) return false;
    if (!readText(root, "author", false, 60, out.author, "der Datei", error)) return false;
    if (!readText(root, "license", true, 40, out.license, "der Datei", error)) return false;
    if (!readText(root, "description", false, 200, out.description, "der Datei", error)) return false;

    if (!root["needs"].isNull()) {
        if (!root["needs"].is<JsonObjectConst>()) {
            error = "'needs' muss ein Objekt sein";
            return false;
        }
        JsonObjectConst needs = root["needs"];
        static const char* const NEEDS[] = {"api", "firmware", "script"};
        if (!onlyKeys(needs, NEEDS, 3, "needs", error)) return false;
        int api = needs["api"] | 1;
        if (api < 1 || api > 1000) {
            error = "'needs.api' muss eine ganze Zahl ab 1 sein";
            return false;
        }
        out.api = (uint16_t)api;
        if (!readText(needs, "firmware", false, 16, out.minFirmware, "needs", error)) return false;
        int script = needs["script"] | 0;
        if (script < 0 || script > 100) {
            error = "'needs.script' muss eine ganze Zahl von 0 bis 100 sein";
            return false;
        }
        out.scriptLevel = (uint8_t)script;
    }

    if (!root["settings"].isNull()) {
        if (!root["settings"].is<JsonArrayConst>()) {
            error = "'settings' muss eine Liste sein";
            return false;
        }
        if (!parseSettings(root["settings"], out, error)) return false;
    }

    // source
    if (!root["source"].is<JsonObjectConst>()) {
        error = "Das Feld 'source' fehlt oder ist kein Objekt";
        return false;
    }
    JsonObjectConst src = root["source"];
    static const char* const SRC[] = {"url", "every", "timeout", "header"};
    if (!onlyKeys(src, SRC, 4, "source", error)) return false;
    if (!readText(src, "url", true, 300, out.source.url, "source", error)) return false;
    if (!out.source.url.startsWith("http://") && !out.source.url.startsWith("https://")) {
        error = "source: 'url' muss mit http:// oder https:// beginnen";
        return false;
    }
    if (!checkPlaceholders(out, out.source.url, "source url", error)) return false;
    int every = src["every"] | 5;
    if (every < (int)MIN_EVERY_SECONDS || every > 3600) {
        error = "source: 'every' muss mindestens 2 und höchstens 3600 Sekunden sein";
        return false;
    }
    out.source.every = (uint16_t)every;
    int timeout = src["timeout"] | 5;
    if (timeout < 1 || timeout > 10) {
        error = "source: 'timeout' muss zwischen 1 und 10 Sekunden liegen";
        return false;
    }
    out.source.timeoutMs = (uint16_t)(timeout * 1000);
    if (!src["header"].isNull()) {
        if (!src["header"].is<JsonObjectConst>() || src["header"].as<JsonObjectConst>().size() > 4) {
            error = "source: 'header' muss ein Objekt mit höchstens 4 Einträgen sein";
            return false;
        }
        for (JsonPairConst kv : src["header"].as<JsonObjectConst>()) {
            if (!kv.value().is<const char*>()) {
                error = "source: Die Werte in 'header' müssen Text sein";
                return false;
            }
            Header h;
            h.name = kv.key().c_str();
            h.value = kv.value().as<const char*>();
            if (!checkPlaceholders(out, h.value, "source header '" + h.name + "'", error)) return false;
            out.source.headers.push_back(h);
        }
    }

    // values: expressions over the response
    if (!root["values"].isNull()) {
        if (!root["values"].is<JsonObjectConst>() || root["values"].as<JsonObjectConst>().size() > MAX_VALUES) {
            error = "'values' muss ein Objekt mit höchstens 16 Einträgen sein";
            return false;
        }
        for (JsonPairConst kv : root["values"].as<JsonObjectConst>()) {
            ValueDef v;
            v.name = kv.key().c_str();
            String where = "Wert '" + v.name + "'";
            if (!validName(v.name, 24, false) || reserved(v.name)) {
                error = where + ": Der Name darf nur a-z, 0-9 und _ enthalten und kein Schlüsselwort sein";
                return false;
            }
            if (settingKeyKnown(out, v.name)) {
                error = where + ": Der Name '" + v.name + "' ist schon eine Einstellung";
                return false;
            }
            if (!compileExpr(kv.value(), v.expr, where, error)) return false;
            for (const String& path : v.expr.names()) {
                bool known = false;
                for (const String& p : out.responsePaths) {
                    if (p == path) known = true;
                }
                if (!known) out.responsePaths.push_back(path);
            }
            out.values.push_back(v);
        }
    }

    // What rules and on_error may read: the values and the settings.
    std::vector<String> allowed;
    for (const ValueDef& v : out.values) allowed.push_back(v.name);
    for (const Setting& s : out.settings) allowed.push_back(s.key);

    bool anyShow = false;
    if (!root["rules"].isNull()) {
        if (!root["rules"].is<JsonArrayConst>() || root["rules"].as<JsonArrayConst>().size() > MAX_RULES) {
            error = "'rules' muss eine Liste mit höchstens 12 Regeln sein";
            return false;
        }
        int n = 0;
        for (JsonVariantConst item : root["rules"].as<JsonArrayConst>()) {
            n++;
            String where = "Regel " + String(n);
            if (!item.is<JsonObjectConst>()) {
                error = where + ": muss ein Objekt sein";
                return false;
            }
            JsonObjectConst r = item.as<JsonObjectConst>();
            static const char* const RULE[] = {"when", "show"};
            if (!onlyKeys(r, RULE, 2, where, error)) return false;
            Rule rule;
            if (!compileExpr(r["when"], rule.when, where + " when", error)) return false;
            if (r["when"].is<const char*>()) rule.whenText = String(r["when"].as<const char*>()).substring(0, 120);
            if (!namesAllowed(rule.when, allowed, where + " when", error)) return false;
            if (!r["show"].is<JsonObjectConst>()) {
                error = where + ": 'show' fehlt";
                return false;
            }
            if (!parseShow(r["show"], out, allowed, rule.show, where + " show", error)) return false;
            if (!rule.show.any()) {
                error = where + ": 'show' bewirkt nichts";
                return false;
            }
            anyShow = true;
            if (rule.show.power >= 0) out.wantsPower = true;
            out.rules.push_back(rule);
        }
    }
    if (!root["on_error"].isNull()) {
        if (!root["on_error"].is<JsonObjectConst>()) {
            error = "'on_error' muss ein Objekt sein";
            return false;
        }
        JsonObjectConst oe = root["on_error"];
        static const char* const OE[] = {"show"};
        if (!onlyKeys(oe, OE, 1, "on_error", error)) return false;
        if (!oe["show"].is<JsonObjectConst>()) {
            error = "on_error: 'show' fehlt";
            return false;
        }
        if (!parseShow(oe["show"], out, allowed, out.onError, "on_error show", error)) return false;
        if (!out.onError.any()) {
            error = "on_error: 'show' bewirkt nichts";
            return false;
        }
        out.hasOnError = true;
        anyShow = true;
        if (out.onError.power >= 0) out.wantsPower = true;
    }

    // A script: kept in the file, not read into memory here. This firmware cannot run scripts (see
    // PLUGIN_SCRIPT_LEVEL), so a plugin that has one is installed and then switched off with a clear
    // reason; after a firmware update that can run it, it works without being installed again.
    if (!root["script"].isNull()) {
        if (!root["script"].is<const char*>()) {
            error = "Das Feld 'script' muss Text sein";
            return false;
        }
        size_t length = strlen(root["script"].as<const char*>());
        if (length == 0 || length > MAX_SCRIPT) {
            error = "Das Skript ist leer oder länger als 8 KB";
            return false;
        }
        if (out.scriptLevel < 1) {
            error = "Das Plugin hat ein Skript, nennt aber keine Skript-Stufe (needs.script)";
            return false;
        }
        out.hasScript = true;
    } else if (out.scriptLevel >= 1) {
        error = "'needs.script' ist gesetzt, aber das Plugin hat kein 'script'";
        return false;
    }

    if ((anyShow || out.hasScript) && out.segmentKey.isEmpty()) {
        error = "Das Plugin steuert ein Segment, hat aber keine Einstellung vom Typ Segment";
        return false;
    }
    return true;
}

}  // namespace

bool parse(const String& json, Definition& out, String& error) {
    out = Definition();
    error = "";
    bool ok = parseInto(json, out, error);
    if (!ok) out = Definition();
    return ok;
}

String expand(const String& tmpl, const std::function<String(const String&)>& lookup) {
    String out;
    size_t i = 0;
    while (i < tmpl.length()) {
        if (tmpl[i] == '{') {
            int close = tmpl.indexOf('}', (unsigned int)i + 1);
            if (close > 0) {
                out += lookup(tmpl.substring(i + 1, (unsigned int)close));
                i = (size_t)close + 1;
                continue;
            }
        }
        out += tmpl[i++];
    }
    return out;
}

const Setting* findSetting(const Definition& def, const String& key) {
    for (const Setting& s : def.settings) {
        if (s.key == key) return &s;
    }
    return nullptr;
}

int effectIndex(const String& name) {
    for (uint8_t e = 0; e < EFFECT_COUNT; e++) {
        if (name.equalsIgnoreCase(String(EFFECT_NAMES[e]))) return e;
    }
    return -1;
}

bool effectBlocked(int index) {
    return index == 10 || index == 25 || index == 29;
}

}  // namespace PluginDef

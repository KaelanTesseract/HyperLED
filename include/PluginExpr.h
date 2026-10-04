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

// The small expression language of plugin rules and values.
//
// Numbers, text and true/false; == != < > <= >=; and/or/not (also und/oder/nicht); + - * /;
// parentheses; round(), min(), max() and contains(). No loops, no assignments, no definitions:
// an expression is compiled once into a short list of steps and then runs through it exactly
// once, so it always ends - whatever a plugin author writes.
//
// Names are looked up in a Scope. A name may be a path ("result.status.state", "items[0].name");
// what the path means is up to the scope. A value that is missing is Unknown; comparing an
// Unknown value is false (also with !=), and computing with it gives Unknown again.
namespace PluginExpr {

enum class Type : uint8_t { Unknown, Number, Text, Bool };

struct Value {
    Type type = Type::Unknown;
    double num = 0;  // Number; Bool as 0 or 1
    String text;     // Text

    static Value number(double n) { Value v; v.type = Type::Number; v.num = n; return v; }
    static Value boolean(bool b) { Value v; v.type = Type::Bool; v.num = b ? 1 : 0; return v; }
    static Value string(const String& s) { Value v; v.type = Type::Text; v.text = s; return v; }

    bool known() const { return type != Type::Unknown; }
    // Whether the value counts as "yes": a non-zero number, true, a non-empty text.
    bool truthy() const;
    // The value as text: whole numbers without decimals, other numbers with two.
    String toText() const;
};

// Where the names of an expression get their values from.
class Scope {
public:
    virtual ~Scope() {}
    // The value of a name; an Unknown value when there is none.
    virtual Value get(const String& name) const = 0;
};

class Program {
public:
    static const size_t MAX_SOURCE = 200;

    // Compiles the source. On failure the program stays empty and `error` says what is wrong
    // and where, in German, for the person who wrote the plugin.
    bool compile(const String& source, String& error);

    // Runs the program. Never fails: whatever cannot be computed is an Unknown value.
    Value run(const Scope& scope) const;

    bool empty() const { return _ops.empty(); }
    const String& source() const { return _source; }
    // Every name the expression reads, without duplicates.
    const std::vector<String>& names() const { return _names; }

private:
    friend struct Parser;
    struct Op {
        uint8_t code;
        uint16_t arg;
    };
    std::vector<Op> _ops;
    std::vector<double> _nums;
    std::vector<String> _strs;
    std::vector<String> _names;
    String _source;
};

}  // namespace PluginExpr

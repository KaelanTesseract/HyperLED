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
#include "PluginExpr.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace PluginExpr {

namespace {

enum OpCode : uint8_t {
    OP_NUM, OP_STR, OP_TRUE, OP_FALSE, OP_LOAD,
    OP_NEG, OP_NOT,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV,
    OP_EQ, OP_NE, OP_LT, OP_GT, OP_LE, OP_GE,
    OP_AND, OP_OR,
    OP_ROUND, OP_MIN, OP_MAX, OP_CONTAINS
};

constexpr size_t MAX_OPS = 64;
constexpr size_t MAX_STACK = 16;

bool isIdentStart(char c) { return isalpha((unsigned char)c) || c == '_'; }
bool isIdentChar(char c) { return isalnum((unsigned char)c) || c == '_'; }

struct Function {
    const char* name;
    uint8_t arity;
    OpCode code;
};
const Function FUNCTIONS[] = {
    {"round", 1, OP_ROUND},
    {"min", 2, OP_MIN},
    {"max", 2, OP_MAX},
    {"contains", 2, OP_CONTAINS},
};

}  // namespace

bool Value::truthy() const {
    switch (type) {
        case Type::Number:
        case Type::Bool:
            return num != 0;
        case Type::Text:
            return text.length() > 0;
        default:
            return false;
    }
}

String Value::toText() const {
    switch (type) {
        case Type::Number:
            if (num == floor(num) && fabs(num) < 1e9) return String((long)num);
            return String(num, 2);
        case Type::Bool:
            return num != 0 ? "true" : "false";
        case Type::Text:
            return text;
        default:
            return "";
    }
}

// Turns the source text into steps, in one pass, by recursive descent. From the loosest to the
// tightest binding: or, and, not, == !=, < > <= >=, + -, * /, unary minus, primary.
struct Parser {
    const String& src;
    Program& prog;
    size_t pos = 0;
    int depth = 0;  // how many values the finished steps would leave on the stack
    String error;

    Parser(const String& s, Program& p) : src(s), prog(p) {}

    bool fail(const String& what) {
        if (error.isEmpty()) error = what + " (Stelle " + String((unsigned)pos + 1) + ")";
        return false;
    }
    char peek() const { return pos < src.length() ? src[pos] : '\0'; }
    void skipSpace() {
        while (pos < src.length() && isspace((unsigned char)src[pos])) pos++;
    }

    // Adds a step and keeps track of how deep the stack gets. The depth is known at compile
    // time, so running never has to check for an overflow.
    bool emit(OpCode code, uint16_t arg, int stackChange) {
        if (prog._ops.size() >= MAX_OPS) return fail("Ausdruck ist zu lang");
        depth += stackChange;
        if (depth > (int)MAX_STACK) return fail("Ausdruck ist zu tief verschachtelt");
        prog._ops.push_back({(uint8_t)code, arg});
        return true;
    }

    // Consumes the symbol if it is next. Longer symbols must be tried before their prefixes.
    bool accept(const char* sym) {
        skipSpace();
        size_t n = strlen(sym);
        if (strncmp(src.c_str() + pos, sym, n) != 0) return false;
        pos += n;
        return true;
    }
    // A word counts only as a whole word: "android" is not "and" followed by "roid".
    bool acceptWord(const char* word) {
        skipSpace();
        size_t n = strlen(word);
        if (strncmp(src.c_str() + pos, word, n) != 0) return false;
        if (pos + n < src.length() && isIdentChar(src[pos + n])) return false;
        pos += n;
        return true;
    }

    bool parseOr() {
        if (!parseAnd()) return false;
        while (acceptWord("or") || acceptWord("oder")) {
            if (!parseAnd() || !emit(OP_OR, 0, -1)) return false;
        }
        return true;
    }
    bool parseAnd() {
        if (!parseNot()) return false;
        while (acceptWord("and") || acceptWord("und")) {
            if (!parseNot() || !emit(OP_AND, 0, -1)) return false;
        }
        return true;
    }
    bool parseNot() {
        if (acceptWord("not") || acceptWord("nicht")) return parseNot() && emit(OP_NOT, 0, 0);
        return parseEq();
    }
    bool parseEq() {
        if (!parseCmp()) return false;
        for (;;) {
            OpCode code;
            if (accept("==")) code = OP_EQ;
            else if (accept("!=")) code = OP_NE;
            else return true;
            if (!parseCmp() || !emit(code, 0, -1)) return false;
        }
    }
    bool parseCmp() {
        if (!parseAdd()) return false;
        for (;;) {
            OpCode code;
            if (accept("<=")) code = OP_LE;
            else if (accept(">=")) code = OP_GE;
            else if (accept("<")) code = OP_LT;
            else if (accept(">")) code = OP_GT;
            else return true;
            if (!parseAdd() || !emit(code, 0, -1)) return false;
        }
    }
    bool parseAdd() {
        if (!parseMul()) return false;
        for (;;) {
            OpCode code;
            if (accept("+")) code = OP_ADD;
            else if (accept("-")) code = OP_SUB;
            else return true;
            if (!parseMul() || !emit(code, 0, -1)) return false;
        }
    }
    bool parseMul() {
        if (!parseUnary()) return false;
        for (;;) {
            OpCode code;
            if (accept("*")) code = OP_MUL;
            else if (accept("/")) code = OP_DIV;
            else return true;
            if (!parseUnary() || !emit(code, 0, -1)) return false;
        }
    }
    bool parseUnary() {
        if (accept("-")) return parseUnary() && emit(OP_NEG, 0, 0);
        return parsePrimary();
    }

    bool parsePrimary() {
        skipSpace();
        char c = peek();
        if (c == '\0') return fail("Der Ausdruck endet zu früh");
        if (c == '(') {
            pos++;
            if (!parseOr()) return false;
            if (!accept(")")) return fail("')' fehlt");
            return true;
        }
        bool digit = isdigit((unsigned char)c) != 0;
        bool dotDigit = c == '.' && pos + 1 < src.length() && isdigit((unsigned char)src[pos + 1]);
        if (digit || dotDigit) {
            char* end = nullptr;
            const char* start = src.c_str() + pos;
            double v = strtod(start, &end);
            if (end == start) return fail("Zahl nicht lesbar");
            pos += (size_t)(end - start);
            prog._nums.push_back(v);
            return emit(OP_NUM, (uint16_t)(prog._nums.size() - 1), +1);
        }
        if (c == '\'' || c == '"') {
            int close = src.indexOf(c, (unsigned int)pos + 1);
            if (close < 0) return fail("Text nicht beendet");
            prog._strs.push_back(src.substring(pos + 1, (unsigned int)close));
            pos = (size_t)close + 1;
            return emit(OP_STR, (uint16_t)(prog._strs.size() - 1), +1);
        }
        if (isIdentStart(c)) return parseName();
        return fail(String("Unerwartetes Zeichen '") + c + "'");
    }

    // A word, a function call or a path into a response.
    bool parseName() {
        size_t start = pos;
        while (pos < src.length() && isIdentChar(src[pos])) pos++;
        String word = src.substring(start, pos);

        if (word == "true" || word == "wahr") return emit(OP_TRUE, 0, +1);
        if (word == "false" || word == "falsch") return emit(OP_FALSE, 0, +1);
        if (word == "and" || word == "or" || word == "not" || word == "und" || word == "oder" ||
            word == "nicht") {
            return fail("'" + word + "' steht an der falschen Stelle");
        }

        // A plain name followed by '(' is a function call.
        size_t afterWord = pos;
        skipSpace();
        if (peek() == '(') {
            for (const Function& f : FUNCTIONS) {
                if (word != f.name) continue;
                pos++;  // '('
                uint8_t args = 0;
                skipSpace();
                if (peek() != ')') {
                    do {
                        if (!parseOr()) return false;
                        args++;
                    } while (accept(","));
                }
                if (!accept(")")) return fail("')' fehlt");
                if (args != f.arity) {
                    return fail(String("'") + f.name + "' braucht " + String(f.arity) + " Werte");
                }
                return emit(f.code, 0, 1 - (int)f.arity);
            }
            return fail("Unbekannte Funktion '" + word + "'");
        }
        pos = afterWord;

        // Otherwise a path: name, then any number of .name or [number].
        for (;;) {
            if (peek() == '.' && pos + 1 < src.length() && isIdentChar(src[pos + 1])) {
                pos++;
                while (pos < src.length() && isIdentChar(src[pos])) pos++;
            } else if (peek() == '[') {
                size_t p = pos + 1;
                while (p < src.length() && isdigit((unsigned char)src[p])) p++;
                if (p == pos + 1 || p >= src.length() || src[p] != ']') return fail("Ungültiger Index");
                pos = p + 1;
            } else {
                break;
            }
        }
        String name = src.substring(start, pos);
        size_t index = prog._names.size();
        for (size_t i = 0; i < prog._names.size(); i++) {
            if (prog._names[i] == name) {
                index = i;
                break;
            }
        }
        if (index == prog._names.size()) prog._names.push_back(name);
        return emit(OP_LOAD, (uint16_t)index, +1);
    }
};

bool Program::compile(const String& source, String& error) {
    _ops.clear();
    _nums.clear();
    _strs.clear();
    _names.clear();
    _source = "";
    if (source.length() == 0) {
        error = "Der Ausdruck ist leer";
        return false;
    }
    if (source.length() > MAX_SOURCE) {
        error = "Der Ausdruck ist länger als 200 Bytes";
        return false;
    }
    Parser p(source, *this);
    bool ok = p.parseOr();
    if (ok) {
        p.skipSpace();
        if (p.pos < source.length()) ok = p.fail(String("Unerwartetes Zeichen '") + source[p.pos] + "'");
    }
    if (!ok) {
        error = p.error;
        _ops.clear();
        _nums.clear();
        _strs.clear();
        _names.clear();
        return false;
    }
    _source = source;
    return true;
}

namespace {

bool numeric(const Value& v) { return v.type == Type::Number || v.type == Type::Bool; }

// == and !=. An unknown value makes both false: "not known" is no answer to "is it x?".
Value equality(const Value& a, const Value& b, bool wantEqual) {
    if (!a.known() || !b.known()) return Value::boolean(false);
    bool equal;
    if (numeric(a) && numeric(b)) equal = fabs(a.num - b.num) < 1e-9;
    else if (a.type == Type::Text && b.type == Type::Text) equal = a.text == b.text;
    else equal = false;
    return Value::boolean(wantEqual ? equal : !equal);
}

Value ordering(const Value& a, const Value& b, OpCode code) {
    int cmp;
    if (numeric(a) && numeric(b)) cmp = a.num < b.num - 1e-9 ? -1 : (a.num > b.num + 1e-9 ? 1 : 0);
    else if (a.type == Type::Text && b.type == Type::Text) cmp = a.text.compareTo(b.text);
    else return Value::boolean(false);
    switch (code) {
        case OP_LT: return Value::boolean(cmp < 0);
        case OP_GT: return Value::boolean(cmp > 0);
        case OP_LE: return Value::boolean(cmp <= 0);
        default:    return Value::boolean(cmp >= 0);
    }
}

Value plus(const Value& a, const Value& b) {
    if (numeric(a) && numeric(b)) return Value::number(a.num + b.num);
    // Text and anything else: joined as text.
    if (a.known() && b.known()) return Value::string(a.toText() + b.toText());
    return Value();
}

}  // namespace

Value Program::run(const Scope& scope) const {
    Value st[MAX_STACK];
    size_t sp = 0;
    auto push = [&](const Value& v) {
        if (sp < MAX_STACK) st[sp++] = v;
    };

    for (const Op& op : _ops) {
        switch (op.code) {
            case OP_NUM:   push(Value::number(_nums[op.arg])); break;
            case OP_STR:   push(Value::string(_strs[op.arg])); break;
            case OP_TRUE:  push(Value::boolean(true)); break;
            case OP_FALSE: push(Value::boolean(false)); break;
            case OP_LOAD:  push(scope.get(_names[op.arg])); break;

            case OP_NEG:
                if (sp < 1) return Value();
                st[sp - 1] = numeric(st[sp - 1]) ? Value::number(-st[sp - 1].num) : Value();
                break;
            case OP_NOT:
                if (sp < 1) return Value();
                st[sp - 1] = Value::boolean(!st[sp - 1].truthy());
                break;

            case OP_ROUND: {
                if (sp < 1) return Value();
                const Value& a = st[sp - 1];
                if (!numeric(a)) {
                    st[sp - 1] = Value();
                } else {
                    double r = a.num < 0 ? -floor(-a.num + 0.5) : floor(a.num + 0.5);
                    st[sp - 1] = Value::number(r);
                }
                break;
            }

            default: {  // every other step takes two values
                if (sp < 2) return Value();
                Value b = st[--sp];
                Value a = st[--sp];
                Value r;
                switch (op.code) {
                    case OP_ADD: r = plus(a, b); break;
                    case OP_SUB: if (numeric(a) && numeric(b)) r = Value::number(a.num - b.num); break;
                    case OP_MUL: if (numeric(a) && numeric(b)) r = Value::number(a.num * b.num); break;
                    case OP_DIV:
                        if (numeric(a) && numeric(b) && b.num != 0) r = Value::number(a.num / b.num);
                        break;
                    case OP_EQ: r = equality(a, b, true); break;
                    case OP_NE: r = equality(a, b, false); break;
                    case OP_LT:
                    case OP_GT:
                    case OP_LE:
                    case OP_GE: r = ordering(a, b, (OpCode)op.code); break;
                    case OP_AND: r = Value::boolean(a.truthy() && b.truthy()); break;
                    case OP_OR:  r = Value::boolean(a.truthy() || b.truthy()); break;
                    case OP_MIN: if (numeric(a) && numeric(b)) r = Value::number(a.num < b.num ? a.num : b.num); break;
                    case OP_MAX: if (numeric(a) && numeric(b)) r = Value::number(a.num > b.num ? a.num : b.num); break;
                    case OP_CONTAINS:
                        r = Value::boolean(a.type == Type::Text && b.type == Type::Text &&
                                           a.text.indexOf(b.text) >= 0);
                        break;
                    default: break;
                }
                push(r);
                break;
            }
        }
    }
    return sp == 1 ? st[0] : Value();
}

}  // namespace PluginExpr

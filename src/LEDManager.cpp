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
#include "LEDManager.h"
#include "SlaveManager.h"

const char* const EFFECT_NAMES[EFFECT_COUNT] = {
    "Solid", "Breathe", "Rainbow", "Chase", "Fire", "Color Wipe", "Scanner", "Twinkle", "Meteor", "Matrix Rain"
};

LEDManagerClass LEDManager;

void LEDManagerClass::begin() {
    loadSettings();
    applySettings();
}

void LEDManagerClass::loadSettings() {
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, true);
    
    _pins[0] = prefs.getUChar(PREF_LED_PIN_0, DEFAULT_LED_PIN);
    _pins[1] = prefs.getUChar(PREF_LED_PIN_1, 255);
    _pins[2] = prefs.getUChar(PREF_LED_PIN_2, 255);
    _pins[3] = prefs.getUChar(PREF_LED_PIN_3, 255);
    _pins[4] = prefs.getUChar(PREF_LED_PIN_4, 255);
    _count = prefs.getUShort(PREF_LED_COUNT, DEFAULT_LED_COUNT);
    _type = prefs.getUChar(PREF_LED_TYPE, TYPE_WS2812_RGB);

    _ablEnabled = prefs.getBool(PREF_ABL_ENABLE, true);
    _ablMaxmA = prefs.getUShort(PREF_ABL_MA, 850);
    
    _isMatrix = prefs.getBool("isMatrix", false);
    _matrixWidth = prefs.getUShort("matW", 16);
    _matrixHeight = prefs.getUShort("matH", 16);
    _matrixLayout = prefs.getUChar("matL", 0);
    
    String segJson = prefs.getString("segments", "");
    prefs.end();

    _segments.clear();
    if (!segJson.isEmpty()) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, segJson);
        if (!err && doc.is<JsonArray>()) {
            JsonArray arr = doc.as<JsonArray>();
            for (JsonObject s : arr) {
                Segment seg;
                const char* nameStr = s["name"];
                if (nameStr) seg.name = String(nameStr);
                else seg.name = "Segment " + String(_segments.size());
                seg.start = s["start"] | 0;
                seg.stop = s["stop"] | _count;
                seg.isOn = s["on"] | true;
                seg.brightness = s["bri"] | 255;
                seg.effect = s["effect"] | 0;
                seg.speed = s["speed"] | 128;
                seg.color = s["color"] | 0xFF0000;
                seg.effectStep = 0;
                seg.lastUpdate = 0;
                seg.effectStep = 0;
                seg.lastUpdate = 0;
                seg.isSlave = s["isSlave"] | false;
                seg.slaveId = s["slaveId"] | 0;
                
                if (!seg.isSlave) {
                    if (seg.start < _count && seg.stop <= _count && seg.start < seg.stop) {
                        _segments.push_back(seg);
                    }
                } else {
                    if (seg.start < seg.stop) {
                        _segments.push_back(seg);
                    }
                }
            }
        }
    }
    
    bool hasMaster = false;
    for (const auto& seg : _segments) {
        if (!seg.isSlave) {
            hasMaster = true;
            break;
        }
    }
    
    if (!hasMaster && _count > 0) {
        Segment seg = {"Master", 0, _count, true, 255, 0, 128, 0xFF0000, 0, 0};
        _segments.insert(_segments.begin(), seg);
    }
}

void LEDManagerClass::saveSettings() {
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, false);
    
    prefs.putUChar(PREF_LED_PIN_0, _pins[0]);
    prefs.putUChar(PREF_LED_PIN_1, _pins[1]);
    prefs.putUChar(PREF_LED_PIN_2, _pins[2]);
    prefs.putUChar(PREF_LED_PIN_3, _pins[3]);
    prefs.putUChar(PREF_LED_PIN_4, _pins[4]);
    prefs.putUShort(PREF_LED_COUNT, _count);
    prefs.putUChar(PREF_LED_TYPE, _type);
    
    prefs.putBool(PREF_ABL_ENABLE, _ablEnabled);
    prefs.putUShort(PREF_ABL_MA, _ablMaxmA);
    
    prefs.putBool("isMatrix", _isMatrix);
    prefs.putUShort("matW", _matrixWidth);
    prefs.putUShort("matH", _matrixHeight);
    prefs.putUChar("matL", _matrixLayout);
    
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const auto& seg : _segments) {
        JsonObject s = arr.add<JsonObject>();
        s["name"] = seg.name;
        s["start"] = seg.start;
        s["stop"] = seg.stop;
        s["on"] = seg.isOn;
        s["bri"] = seg.brightness;
        s["effect"] = seg.effect;
        s["speed"] = seg.speed;
        s["color"] = seg.color;
        if (seg.isSlave) {
            s["isSlave"] = true;
            s["slaveId"] = seg.slaveId;
        }
    }
    String segJson;
    serializeJson(doc, segJson);
    prefs.putString("segments", segJson);
    
    prefs.end();
}

void LEDManagerClass::applySettings() {
    if (_bus) {
        delete _bus;
        _bus = nullptr;
    }

    if (_count == 0 || _pins[0] == 255) return;

    // Constrain LOCAL segments to count. Leave SLAVE segments alone.
    for (auto it = _segments.begin(); it != _segments.end(); ) {
        if (!it->isSlave) {
            if (it->start >= _count) {
                it = _segments.erase(it);
                continue;
            } else {
                if (it->stop > _count) it->stop = _count;
            }
        }
        ++it;
    }
    if (_segments.empty()) {
        Segment seg = {"Segment 0", 0, _count, true, 255, 0, 128, 0xFF0000, 0, 0};
        _segments.push_back(seg);
    }

    uint16_t totalCount = _count;
    for (const auto& seg : _segments) {
        if (seg.stop > totalCount) {
            totalCount = seg.stop;
        }
    }

    IBus* localBus = nullptr;

    switch (_type) {
        case TYPE_WS2812_RGB:
            localBus = new BusDigitalRgb<NeoGrbFeature, Neo800KbpsMethod>(_count, _pins[0]); break;
        case TYPE_SK6812_RGBW:
            localBus = new BusDigitalRgbw<NeoGrbwFeature, Neo800KbpsMethod>(_count, _pins[0]); break;
        case TYPE_TM1814:
            localBus = new BusDigitalRgbw<NeoWrgbTm1814Feature, Neo800KbpsMethod>(_count, _pins[0]); break;
        case TYPE_400KHZ:
            localBus = new BusDigitalRgb<NeoGrbFeature, Neo400KbpsMethod>(_count, _pins[0]); break;
        case TYPE_APA102:
            localBus = new BusDigitalSpiRgb<DotStarBgrFeature, DotStarSpiMethod>(_count, _pins[1], _pins[0]); break;
        case TYPE_LPD8806:
            localBus = new BusDigitalSpiRgb<Lpd8806GrbFeature, Lpd8806SpiMethod>(_count, _pins[1], _pins[0]); break;
        case TYPE_TM1829:
        case TYPE_UCS8903:
        case TYPE_APA106:
        case TYPE_TM1914:
        case TYPE_WS2811_W:
        case TYPE_WS281X_WWA:
            localBus = new BusDigitalRgb<NeoGrbFeature, Neo800KbpsMethod>(_count, _pins[0]); break;
        case TYPE_FW1906:
        case TYPE_UCS8904:
        case TYPE_WS2805:
        case TYPE_SM16825:
            localBus = new BusDigitalRgbw<NeoGrbwFeature, Neo800KbpsMethod>(_count, _pins[0]); break;
        case TYPE_WS2801:
        case TYPE_LPD6803:
        case TYPE_PP9813:
            localBus = new BusDigitalSpiRgb<DotStarBgrFeature, DotStarSpiMethod>(_count, _pins[1], _pins[0]); break;
        case TYPE_ONOFF:
            localBus = new BusOnOff(_count, _pins[0]); break;
        case TYPE_ANALOG_1CH:
            localBus = new BusPwm(_count, 1, _pins[0]); break;
        case TYPE_ANALOG_2CH:
            localBus = new BusPwm(_count, 2, _pins[0], _pins[1]); break;
        case TYPE_ANALOG_3CH:
            localBus = new BusPwm(_count, 3, _pins[0], _pins[1], _pins[2]); break;
        case TYPE_ANALOG_4CH:
            localBus = new BusPwm(_count, 4, _pins[0], _pins[1], _pins[2], _pins[3]); break;
        case TYPE_ANALOG_5CH:
            localBus = new BusPwm(_count, 5, _pins[0], _pins[1], _pins[2], _pins[3], _pins[4]); break;
        default:
            localBus = new BusDigitalRgb<NeoGrbFeature, Neo800KbpsMethod>(_count, _pins[0]); break;
    }

    if (totalCount > _count) {
        _bus = new BusVirtual(localBus, _count, totalCount);
    } else {
        _bus = localBus;
    }

    if (_bus) {
        _bus->Begin();
    }
}

void LEDManagerClass::setConfig(uint8_t p0, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4, uint16_t count, uint8_t type, bool ablEnabled, uint16_t ablMaxmA) {
    _pins[0] = p0;
    _pins[1] = p1;
    _pins[2] = p2;
    _pins[3] = p3;
    _pins[4] = p4;
    _count = count;
    _type = type;
    _ablEnabled = ablEnabled;
    _ablMaxmA = ablMaxmA;
    saveSettings();
    applySettings();
}

void LEDManagerClass::setMatrixConfig(bool isMatrix, uint16_t width, uint16_t height, uint8_t layout) {
    _isMatrix = isMatrix;
    _matrixWidth = width;
    _matrixHeight = height;
    _matrixLayout = layout;
    saveSettings();
}

uint16_t LEDManagerClass::getPixelXY(uint16_t x, uint16_t y) const {
    if (!_isMatrix) return y * _matrixWidth + x; // Fallback linear
    
    // Bounds check
    if (x >= _matrixWidth || y >= _matrixHeight) return 65535;
    
    uint16_t pos = 0;
    if (_matrixLayout == 0) {
        // Serpentine Top-Left (Zick-Zack)
        if (y % 2 == 0) {
            // Even row goes right
            pos = (y * _matrixWidth) + x;
        } else {
            // Odd row goes left
            pos = (y * _matrixWidth) + (_matrixWidth - 1 - x);
        }
    } else {
        // Linear Zeilenweise Top-Left
        pos = (y * _matrixWidth) + x;
    }
    
    if (pos >= _count) return 65535;
    return pos;
}

void LEDManagerClass::setPixelColorXY(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
    if (!_bus) return;
    uint16_t idx = getPixelXY(x, y);
    if (idx != 65535) {
        _bus->SetPixelColor(idx, r, g, b, w);
    }
}

uint8_t LEDManagerClass::getPin(uint8_t index) const {
    if (index < 5) return _pins[index];
    return 255;
}

bool LEDManagerClass::hasWhiteChannel() const {
    switch (_type) {
        case TYPE_SK6812_RGBW:
        case TYPE_TM1814:
        case TYPE_FW1906:
        case TYPE_UCS8904:
        case TYPE_WS2805:
        case TYPE_SM16825:
        case TYPE_ANALOG_4CH:
        case TYPE_ANALOG_5CH:
            return true;
        default:
            return false;
    }
}

void LEDManagerClass::setPower(uint8_t segId, bool on) {
    if (segId < _segments.size()) {
        _segments[segId].isOn = on;
        triggerSave();
    }
}

void LEDManagerClass::setBrightness(uint8_t segId, uint8_t bri) {
    if (segId < _segments.size()) {
        _segments[segId].brightness = bri;
        triggerSave();
    }
}

void LEDManagerClass::setEffect(uint8_t segId, uint8_t effectId) {
    if (segId < _segments.size()) {
        _segments[segId].effect = effectId;
        _segments[segId].effectStep = 0;
        triggerSave();
    }
}

void LEDManagerClass::setSpeed(uint8_t segId, uint8_t speed) {
    if (segId < _segments.size()) {
        _segments[segId].speed = speed;
        triggerSave();
    }
}

void LEDManagerClass::setColor(uint8_t segId, uint32_t color) {
    if (segId < _segments.size()) {
        _segments[segId].color = color;
        triggerSave();
    }
}

bool LEDManagerClass::getPower(uint8_t segId) const {
    if (segId < _segments.size()) return _segments[segId].isOn;
    return false;
}

uint8_t LEDManagerClass::getBrightness(uint8_t segId) const {
    if (segId < _segments.size()) return _segments[segId].brightness;
    return 0;
}

uint8_t LEDManagerClass::getEffect(uint8_t segId) const {
    if (segId < _segments.size()) return _segments[segId].effect;
    return 0;
}

uint8_t LEDManagerClass::getSpeed(uint8_t segId) const {
    if (segId < _segments.size()) return _segments[segId].speed;
    return 128;
}

uint32_t LEDManagerClass::getColor(uint8_t segId) const {
    if (segId < _segments.size()) return _segments[segId].color;
    return 0;
}

void LEDManagerClass::setSegmentsFromJson(JsonArray segmentsArray) {
    _segments.clear();
    for (JsonObject s : segmentsArray) {
        Segment seg;
        const char* nameStr = s["name"];
        if (nameStr) seg.name = String(nameStr);
        else seg.name = "Segment " + String(_segments.size());
        seg.start = s["start"] | 0;
        seg.stop = s["stop"] | _count;
        seg.isOn = s["on"] | true;
        seg.brightness = s["bri"] | 255;
        seg.effect = s["effect"] | 0;
        seg.speed = s["speed"] | 128;
        seg.color = s["color"] | 0xFF0000;
        seg.effectStep = 0;
        seg.lastUpdate = 0;
        seg.isSlave = s["isSlave"] | false;
        seg.slaveId = s["slaveId"] | 0;
        
        if (!seg.isSlave) {
            if (seg.start >= _count && _count > 0) seg.start = _count - 1;
            if (seg.stop > _count) seg.stop = _count;
        }
        
        if (seg.start < seg.stop) {
            _segments.push_back(seg);
        }
    }
    bool hasMaster = false;
    for (const auto& seg : _segments) {
        if (!seg.isSlave) {
            hasMaster = true;
            break;
        }
    }
    
    if (!hasMaster && _count > 0) {
        Segment seg = {"Master", 0, _count, true, 255, 0, 128, 0xFF0000, 0, 0};
        _segments.insert(_segments.begin(), seg);
    }
    
    std::sort(_segments.begin(), _segments.end(), [](const Segment& a, const Segment& b) {
        return a.start < b.start;
    });
    
    saveSettings();
}

void LEDManagerClass::getSegmentsJson(JsonArray array) const {
    for (const auto& seg : _segments) {
        JsonObject s = array.add<JsonObject>();
        s["name"] = seg.name;
        s["start"] = seg.start;
        s["stop"] = seg.stop;
        s["on"] = seg.isOn;
        s["bri"] = seg.brightness;
        s["effect"] = seg.effect;
        s["speed"] = seg.speed;
        s["color"] = seg.color;
        if (seg.isSlave) {
            s["isSlave"] = true;
            s["slaveId"] = seg.slaveId;
        }
    }
}

uint8_t LEDManagerClass::getNumSegments() const {
    return _segments.size();
}

const Segment* LEDManagerClass::getSegment(uint8_t segId) const {
    if (segId < _segments.size()) return &_segments[segId];
    return nullptr;
}

void LEDManagerClass::addSlaveSegment(uint8_t slaveId, uint16_t count, const String& name) {
    bool found = false;
    for (auto& seg : _segments) {
        if (seg.isSlave && seg.slaveId == slaveId) {
            seg.name = name;
            seg.stop = seg.start + count;
            found = true;
            break;
        }
    }
    
    if (!found) {
        uint16_t startPos = _count;
        if (!_segments.empty()) {
            if (_segments.back().stop > startPos) {
                startPos = _segments.back().stop;
            }
        }
        
        Segment seg;
        seg.name = name;
        seg.start = startPos;
        seg.stop = startPos + count;
        seg.isOn = true;
        seg.brightness = 255;
        seg.effect = 0;
        seg.speed = 128;
        seg.color = 0xFF0000;
        seg.effectStep = 0;
        seg.lastUpdate = 0;
        seg.isSlave = true;
        seg.slaveId = slaveId;
        
        _segments.push_back(seg);
    }
    
    recalculateSegments();
}

void LEDManagerClass::recalculateSegments() {
    uint16_t currentPos = _count;
    for (auto& seg : _segments) {
        if (seg.isSlave) {
            uint16_t len = seg.stop - seg.start;
            seg.start = currentPos;
            seg.stop = currentPos + len;
            currentPos = seg.stop;
        }
    }
    
    applySettings();
    triggerSave();
}



// --- Effects Engine ---

void LEDManagerClass::loop() {
    if (_bus == nullptr) return;

    unsigned long now = millis();
    if (_savePending && now - _saveTimer > 2000) {
        saveSettings();
        _savePending = false;
    }

    bool shouldShow = false;
    uint8_t ablCap = getGlobalAblCap();

    if (_syncActive && !_segments.empty()) {
        Segment unifiedSeg = _segments[0];
        unifiedSeg.start = 0;
        
        uint16_t tCount = _count;
        for (const auto& s : _segments) {
            if (s.stop > tCount) tCount = s.stop;
        }
        unifiedSeg.stop = tCount;
        
        unsigned int delayMs = 500 - (unifiedSeg.speed * 490 / 255);
        if (unifiedSeg.effect == 0) delayMs = 100;
        
        if (now - _segments[0].lastUpdate > delayMs) {
            _segments[0].lastUpdate = now;
            shouldShow = true;

            if (!unifiedSeg.isOn) {
                for (uint16_t i = unifiedSeg.start; i < unifiedSeg.stop; i++) {
                    _bus->SetPixelColor(i, 0, 0, 0, 0);
                }
            } else {
                switch (unifiedSeg.effect) {
                    case 0: effectSolid(unifiedSeg, ablCap); break;
                    case 1: effectBreathe(unifiedSeg, ablCap); break;
                    case 2: effectRainbow(unifiedSeg, ablCap); break;
                    case 3: effectChase(unifiedSeg, ablCap); break;
                    case 4: effectFire(unifiedSeg, ablCap); break;
                    case 5: effectColorWipe(unifiedSeg, ablCap); break;
                    case 6: effectScanner(unifiedSeg, ablCap); break;
                    case 7: effectTwinkle(unifiedSeg, ablCap); break;
                    case 8: effectMeteor(unifiedSeg, ablCap); break;
                    case 9: effectMatrixRain(unifiedSeg, ablCap); break;
                    default: effectSolid(unifiedSeg, ablCap); break;
                }
                // Write back effectStep so animations continue
                _segments[0].effectStep = unifiedSeg.effectStep;
            }
        }
    } else {
        for (auto& seg : _segments) {
            unsigned int delayMs = 500 - (seg.speed * 490 / 255);
            if (seg.effect == 0) delayMs = 100;

            if (now - seg.lastUpdate > delayMs) {
                seg.lastUpdate = now;
                shouldShow = true;

                if (!seg.isOn) {
                    for (uint16_t i = seg.start; i < seg.stop; i++) {
                        _bus->SetPixelColor(i, 0, 0, 0, 0);
                    }
                } else {
                    switch (seg.effect) {
                        case 0: effectSolid(seg, ablCap); break;
                        case 1: effectBreathe(seg, ablCap); break;
                        case 2: effectRainbow(seg, ablCap); break;
                        case 3: effectChase(seg, ablCap); break;
                        case 4: effectFire(seg, ablCap); break;
                        case 5: effectColorWipe(seg, ablCap); break;
                        case 6: effectScanner(seg, ablCap); break;
                        case 7: effectTwinkle(seg, ablCap); break;
                        case 8: effectMeteor(seg, ablCap); break;
                        case 9: effectMatrixRain(seg, ablCap); break;
                        default: effectSolid(seg, ablCap); break;
                    }
                }
            }
        }
    }

    if (shouldShow) {
        _bus->Show();
        
        // Push slave pixels by hardware block
        const uint8_t* buffer = _bus->getBuffer();
        if (buffer) {
            uint16_t totalCount = _count;
            for (const auto& seg : _segments) {
                if (seg.stop > totalCount) totalCount = seg.stop;
            }
            
            for (const auto& seg : _segments) {
                if (seg.isSlave && seg.slaveId != 254 && seg.stop > seg.start) {
                    uint16_t ledsToSend = seg.stop - seg.start;
                    if (totalCount >= seg.stop) {
                        SlaveManager.sendLEDData(seg.slaveId, &buffer[seg.start * 4], ledsToSend * 4);
                    }
                }
            }
        }
    }
}

void LEDManagerClass::effectSolid(Segment& seg, uint8_t ablCap) {
    uint8_t r = (seg.color >> 16) & 0xFF;
    uint8_t g = (seg.color >> 8) & 0xFF;
    uint8_t b = seg.color & 0xFF;
    
    uint16_t currentBri = (seg.brightness * ablCap) / 255;
    r = (r * currentBri) / 255;
    g = (g * currentBri) / 255;
    b = (b * currentBri) / 255;

    for (uint16_t i = seg.start; i < seg.stop; i++) {
        _bus->SetPixelColor(i, r, g, b, 0);
    }
}

void LEDManagerClass::effectBreathe(Segment& seg, uint8_t ablCap) {
    float breath = (exp(sin(millis() / 2000.0 * PI)) - 0.36787944) * 108.0;
    uint16_t currentBri = (seg.brightness * ablCap) / 255;
    
    uint8_t r = (((seg.color >> 16) & 0xFF) * breath * currentBri) / 65025;
    uint8_t g = (((seg.color >> 8) & 0xFF) * breath * currentBri) / 65025;
    uint8_t b = ((seg.color & 0xFF) * breath * currentBri) / 65025;

    for (uint16_t i = seg.start; i < seg.stop; i++) {
        _bus->SetPixelColor(i, r, g, b, 0);
    }
}

void LEDManagerClass::effectRainbow(Segment& seg, uint8_t ablCap) {
    uint16_t currentBri = (seg.brightness * ablCap) / 255;
    uint16_t count = seg.stop - seg.start;
    if (count == 0) return;
    
    for (uint16_t i = seg.start; i < seg.stop; i++) {
        uint32_t c = Wheel((((i - seg.start) * 256 / count) + seg.effectStep) & 255);
        uint8_t r = (((c >> 16) & 0xFF) * currentBri) / 255;
        uint8_t g = (((c >> 8) & 0xFF) * currentBri) / 255;
        uint8_t b = ((c & 0xFF) * currentBri) / 255;
        _bus->SetPixelColor(i, r, g, b, 0);
    }
    seg.effectStep += 5;
}

void LEDManagerClass::effectChase(Segment& seg, uint8_t ablCap) {
    uint16_t currentBri = (seg.brightness * ablCap) / 255;
    uint8_t baseR = (((seg.color >> 16) & 0xFF) * currentBri) / 255;
    uint8_t baseG = (((seg.color >> 8) & 0xFF) * currentBri) / 255;
    uint8_t baseB = ((seg.color & 0xFF) * currentBri) / 255;

    for (uint16_t i = seg.start; i < seg.stop; i++) {
        if (((i - seg.start) + seg.effectStep) % 3 == 0) {
            _bus->SetPixelColor(i, baseR, baseG, baseB, 0);
        } else {
            _bus->SetPixelColor(i, 0, 0, 0, 0);
        }
    }
    seg.effectStep++;
}

uint32_t LEDManagerClass::Wheel(byte WheelPos) {
    WheelPos = 255 - WheelPos;
    if (WheelPos < 85) {
        return (((uint32_t)(255 - WheelPos * 3) << 16) | ((uint32_t)(0) << 8) | (WheelPos * 3));
    }
    if (WheelPos < 170) {
        WheelPos -= 85;
        return (((uint32_t)(0) << 16) | ((uint32_t)(WheelPos * 3) << 8) | (255 - WheelPos * 3));
    }
    WheelPos -= 170;
    return (((uint32_t)(WheelPos * 3) << 16) | ((uint32_t)(255 - WheelPos * 3) << 8) | (0));
}

uint8_t LEDManagerClass::getGlobalAblCap() {
    if (!_ablEnabled) return 255;
    
    uint32_t base_mA = 100; // Base ESP current
    if (_ablMaxmA <= base_mA) return 0;
    
    uint32_t total_led_mA_full = 0;
    
    for (const auto& seg : _segments) {
        if (!seg.isOn || seg.brightness == 0) continue;
        uint16_t count = seg.stop - seg.start;
        if (count == 0) continue;
        uint32_t seg_mA = 0;
        
        if (seg.effect == 0) {
            uint32_t r = (seg.color >> 16) & 0xFF;
            uint32_t g = (seg.color >> 8) & 0xFF;
            uint32_t b = seg.color & 0xFF;
            seg_mA = ((uint32_t)count * (r + g + b) * 50) / 765;
        } else if (seg.effect == 2 || seg.effect == 3) {
            seg_mA = ((uint32_t)count * 50) / 2;
        } else {
            seg_mA = (uint32_t)count * 50;
        }
        
        // Scale by segment brightness relative to 255
        total_led_mA_full += (seg_mA * seg.brightness) / 255;
    }
    
    uint32_t current_mA_full = base_mA + total_led_mA_full;
    uint8_t abl_cap = 255;
    
    if (current_mA_full > _ablMaxmA) {
        uint32_t allowed_led_mA = _ablMaxmA - base_mA;
        if (total_led_mA_full == 0) return 255;
        abl_cap = (255 * allowed_led_mA) / total_led_mA_full;
    }
    
    return abl_cap;
}

// --- New Advanced Effects ---

void LEDManagerClass::effectFire(Segment& seg, uint8_t ablCap) {
    uint16_t currentBri = (seg.brightness * ablCap) / 255;
    uint16_t count = seg.stop - seg.start;
    if (count == 0) return;
    
    // Simple fire approximation using effectStep as seed
    for (uint16_t i = seg.start; i < seg.stop; i++) {
        uint8_t heat = random(0, 255);
        uint8_t r = 255;
        uint8_t g = heat > 128 ? heat : 0;
        uint8_t b = heat > 200 ? heat / 2 : 0;
        
        r = (r * currentBri * heat) / 65025;
        g = (g * currentBri * heat) / 65025;
        b = (b * currentBri * heat) / 65025;
        
        _bus->SetPixelColor(i, r, g, b, 0);
    }
}

void LEDManagerClass::effectColorWipe(Segment& seg, uint8_t ablCap) {
    uint16_t currentBri = (seg.brightness * ablCap) / 255;
    uint16_t count = seg.stop - seg.start;
    if (count == 0) return;
    
    uint8_t r = (((seg.color >> 16) & 0xFF) * currentBri) / 255;
    uint8_t g = (((seg.color >> 8) & 0xFF) * currentBri) / 255;
    uint8_t b = ((seg.color & 0xFF) * currentBri) / 255;
    
    uint16_t pos = seg.effectStep % (count * 2);
    
    for (uint16_t i = seg.start; i < seg.stop; i++) {
        uint16_t relPos = i - seg.start;
        if (pos < count) {
            // Wiping on
            if (relPos <= pos) _bus->SetPixelColor(i, r, g, b, 0);
            else _bus->SetPixelColor(i, 0, 0, 0, 0);
        } else {
            // Wiping off
            if (relPos <= (pos - count)) _bus->SetPixelColor(i, 0, 0, 0, 0);
            else _bus->SetPixelColor(i, r, g, b, 0);
        }
    }
    seg.effectStep++;
}

void LEDManagerClass::effectScanner(Segment& seg, uint8_t ablCap) {
    uint16_t currentBri = (seg.brightness * ablCap) / 255;
    uint16_t count = seg.stop - seg.start;
    if (count == 0) return;
    
    uint8_t r = (((seg.color >> 16) & 0xFF) * currentBri) / 255;
    uint8_t g = (((seg.color >> 8) & 0xFF) * currentBri) / 255;
    uint8_t b = ((seg.color & 0xFF) * currentBri) / 255;
    
    // Clear all
    for (uint16_t i = seg.start; i < seg.stop; i++) {
        _bus->SetPixelColor(i, 0, 0, 0, 0);
    }
    
    uint16_t pos = seg.effectStep % (count * 2 - 2);
    if (pos >= count) pos = (count * 2 - 2) - pos;
    
    _bus->SetPixelColor(seg.start + pos, r, g, b, 0);
    seg.effectStep++;
}

void LEDManagerClass::effectTwinkle(Segment& seg, uint8_t ablCap) {
    uint16_t currentBri = (seg.brightness * ablCap) / 255;
    uint16_t count = seg.stop - seg.start;
    if (count == 0) return;
    
    uint8_t r = (((seg.color >> 16) & 0xFF) * currentBri) / 255;
    uint8_t g = (((seg.color >> 8) & 0xFF) * currentBri) / 255;
    uint8_t b = ((seg.color & 0xFF) * currentBri) / 255;
    
    for (uint16_t i = seg.start; i < seg.stop; i++) {
        if (random(0, 100) < 5) { // 5% chance to twinkle
            _bus->SetPixelColor(i, r, g, b, 0);
        } else {
            _bus->SetPixelColor(i, 0, 0, 0, 0);
        }
    }
}

void LEDManagerClass::effectMeteor(Segment& seg, uint8_t ablCap) {
    uint16_t currentBri = (seg.brightness * ablCap) / 255;
    uint16_t count = seg.stop - seg.start;
    if (count == 0) return;
    
    uint8_t r = (((seg.color >> 16) & 0xFF) * currentBri) / 255;
    uint8_t g = (((seg.color >> 8) & 0xFF) * currentBri) / 255;
    uint8_t b = ((seg.color & 0xFF) * currentBri) / 255;
    
    // Clear
    for (uint16_t i = seg.start; i < seg.stop; i++) {
        _bus->SetPixelColor(i, 0, 0, 0, 0);
    }
    
    uint16_t pos = seg.effectStep % count;
    for (uint16_t i = 0; i < 5; i++) {
        if (pos >= i) {
            uint16_t pixel = seg.start + pos - i;
            if (pixel < seg.stop) {
                uint8_t fadeR = r / (i + 1);
                uint8_t fadeG = g / (i + 1);
                uint8_t fadeB = b / (i + 1);
                _bus->SetPixelColor(pixel, fadeR, fadeG, fadeB, 0);
            }
        }
    }
    seg.effectStep++;
}

void LEDManagerClass::effectMatrixRain(Segment& seg, uint8_t ablCap) {
    // 2D Matrix Effect!
    uint16_t currentBri = (seg.brightness * ablCap) / 255;
    if (!_isMatrix) {
        effectChase(seg, ablCap); // Fallback if not configured as matrix
        return;
    }
    
    // Clear Matrix softly
    for (uint16_t i = seg.start; i < seg.stop; i++) {
        _bus->SetPixelColor(i, 0, 0, 0, 0);
    }
    
    // Draw droplets
    uint16_t numDrops = _matrixWidth / 2;
    for (uint16_t d = 0; d < numDrops; d++) {
        uint16_t x = random(0, _matrixWidth);
        uint16_t y = (seg.effectStep + random(0, _matrixHeight)) % _matrixHeight;
        
        uint8_t r = (((seg.color >> 16) & 0xFF) * currentBri) / 255;
        uint8_t g = (((seg.color >> 8) & 0xFF) * currentBri) / 255;
        uint8_t b = ((seg.color & 0xFF) * currentBri) / 255;
        
        setPixelColorXY(x, y, r, g, b, 0);
    }
    seg.effectStep++;
}

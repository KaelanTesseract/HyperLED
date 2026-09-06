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
#include "EffectEngine.h"
#include "SlaveManager.h"
#include "Font5x7.h"
#include "Font3x5.h"
#include "WeatherIcons.h"
#include "WeatherManager.h"
#include <LittleFS.h>
#include <math.h>

// Image widgets (see TextWidget) persist their pixel data as a small file
// under /img rather than embedding it in the segments JSON stored in NVS -
// NVS is only 20KB total and shared with every other setting, while LittleFS
// has over 1MB free. Keyed by the widget's stable id, not its segment/index,
// so the file stays correctly associated even if the widget list is reordered.
static String widgetImagePath(uint8_t widgetId) {
    return "/img/w" + String(widgetId) + ".rgb";
}

static void loadWidgetImageFromFs(TextWidget& tw) {
    tw.imgData.clear();
    if (tw.imgW == 0 || tw.imgH == 0) return;
    if (!LittleFS.begin()) return; // no-op if already mounted
    String path = widgetImagePath(tw.id);
    if (!LittleFS.exists(path)) return;
    File f = LittleFS.open(path, "r");
    if (!f) return;
    size_t expected = (size_t)tw.imgW * tw.imgH * 3;
    tw.imgData.resize(expected);
    size_t got = f.read(tw.imgData.data(), expected);
    f.close();
    if (got != expected) tw.imgData.clear();
}

static void saveWidgetImageToFs(const TextWidget& tw) {
    if (!LittleFS.begin()) return;
    if (!LittleFS.exists("/img")) LittleFS.mkdir("/img");
    File f = LittleFS.open(widgetImagePath(tw.id), "w");
    if (!f) return;
    f.write(tw.imgData.data(), tw.imgData.size());
    f.close();
}

const char* const EFFECT_NAMES[EFFECT_COUNT] = {
    "Solid", "Breathe", "Rainbow", "Chase", "Fire", "Color Wipe", "Scanner", "Twinkle", "Meteor", "Matrix Rain", "Nur Weiß",
    "Strobe", "Bounce", "Palette Rainbow",
    "Sinelon", "Confetti", "Juggle", "BPM", "Theater Chase Rainbow", "Running Lights", "Color Waves",
    "Plasma", "Ripple", "Fire 2D", "Pacifica",
    "Bild",
    "Fireworks", "Starfield", "Bouncing Balls",
    "Uhr / Text"
};

const char* const PALETTE_NAMES[PALETTE_COUNT] = {
    "Solid", "Rainbow", "Fire", "Ocean", "Forest"
};

// Color stops (RGB) for each palette, evenly spaced across a 0-255 position.
// Index 0 ("Solid") is never looked up here - callers use seg.color directly.
struct PaletteDef {
    uint8_t stopCount;
    uint32_t stops[4];
};
static const PaletteDef PALETTE_STOPS[PALETTE_COUNT] = {
    {0, {}},                                                            // Solid (unused)
    {4, {0xFF0000, 0xFFFF00, 0x00FF00, 0x0000FF}},                      // Rainbow
    {3, {0x000000, 0xFF4500, 0xFFFF00}},                                // Fire
    {3, {0x000033, 0x0077BE, 0x00FFFF}},                                // Ocean
    {3, {0x013220, 0x228B22, 0x7CFC00}},                                // Forest
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
    _ledsPerIC = prefs.getUChar("ledsPerIC", 1);
    if(_ledsPerIC == 0) _ledsPerIC = 1;
    _numIcs = _count / _ledsPerIC;
    if (_numIcs == 0) _numIcs = 1;

    _ablEnabled = prefs.getBool(PREF_ABL_ENABLE, true);
    _ablMaxmA = prefs.getUShort(PREF_ABL_MA, 850);
    
    _isMatrix = prefs.getBool("isMatrix", false);
    _matrixWidth = prefs.getUShort("matW", 16);
    _matrixHeight = prefs.getUShort("matH", 16);
    _matrixLayout = prefs.getUChar("matL", 0);
    _hub75ShiftDriver = prefs.getUChar("h75sd", 0);
    
    String segJson = prefs.getString("segments", "");
    String canvasJson = prefs.getString("canvas", "");
    prefs.end();

    _canvasPanels.clear();
    if (!canvasJson.isEmpty()) {
        JsonDocument canvasDoc;
        DeserializationError canvasErr = deserializeJson(canvasDoc, canvasJson);
        if (!canvasErr && canvasDoc.is<JsonArray>()) {
            for (JsonObject p : canvasDoc.as<JsonArray>()) {
                CanvasPanel panel;
                panel.slaveId = p["slaveId"] | 0;
                panel.width = p["width"] | 16;
                panel.height = p["height"] | 16;
                panel.layout = p["layout"] | 0;
                panel.offsetX = p["offsetX"] | 0;
                panel.offsetY = p["offsetY"] | 0;
                if (panel.slaveId != 0 && panel.width > 0 && panel.height > 0) {
                    _canvasPanels.push_back(panel);
                }
            }
        }
    }

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
                uint16_t reqStart = s["start"] | 0;
          uint16_t reqStop = s["stop"] | (_numIcs * _ledsPerIC);
          bool isSlave = s["isSlave"] | false;
          if (!isSlave) {
              seg.start = reqStart / _ledsPerIC;
              seg.stop = reqStop / _ledsPerIC;
          } else {
              seg.start = reqStart;
              seg.stop = reqStop;
          }
                seg.isOn = s["on"] | true;
                seg.whiteOnly = s["whiteOnly"] | false;
                seg.cct = s["cct"] | 128;
                seg.palette = s["palette"] | 0;
                seg.intensity = s["intensity"] | 128;
                seg.color2 = s["color2"] | 0x0000FF;
                seg.color2Enabled = s["color2Enabled"] | false;
                seg.brightness = s["bri"] | 255;
                seg.effect = s["effect"] | 0;
                seg.speed = s["speed"] | 128;
                seg.color = s["color"] | 0xFF0000;
                seg.textWidgets.clear();
                if (s["widgets"].is<JsonArray>()) {
                    for (JsonObject w : s["widgets"].as<JsonArray>()) {
                        if (seg.textWidgets.size() >= TEXT_WIDGET_MAX) break;
                        TextWidget tw;
                        tw.id = w["id"] | 0;
                        tw.type = w["type"] | 0;
                        tw.x = w["x"] | 0;
                        tw.y = w["y"] | 0;
                        tw.color = w["color"] | 0xFFFFFF;
                        const char* txt = w["text"]; if (txt) tw.text = String(txt);
                        tw.imgW = w["w"] | 0;
                        tw.imgH = w["h"] | 0;
                        tw.scale = w["scale"] | 1;
                        tw.format = w["format"] | 0;
                        tw.font = w["font"] | 0;
                        seg.textWidgets.push_back(tw);
                    }
                }
                seg.effectStep = 0;
                seg.lastUpdate = 0;
                seg.effectStep = 0;
                seg.lastUpdate = 0;
                seg.isSlave = s["isSlave"] | false;
                seg.slaveId = s["slaveId"] | 0;
        seg.sharesPower = s["sharesPower"] | false;
                
                if (!seg.isSlave) {
                    if (seg.start < _numIcs && seg.stop <= _numIcs && seg.start < seg.stop) {
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
    
    if (!hasMaster && _numIcs > 0) {
        Segment seg = {"Master", 0, _numIcs, true, 255, 0, 128, 0xFF0000, 0, 0};
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
    prefs.putUChar("ledsPerIC", _ledsPerIC);
    
    prefs.putBool(PREF_ABL_ENABLE, _ablEnabled);
    prefs.putUShort(PREF_ABL_MA, _ablMaxmA);
    
    prefs.putBool("isMatrix", _isMatrix);
    prefs.putUShort("matW", _matrixWidth);
    prefs.putUShort("matH", _matrixHeight);
    prefs.putUChar("matL", _matrixLayout);
    prefs.putUChar("h75sd", _hub75ShiftDriver);
    
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const auto& seg : _segments) {
        JsonObject s = arr.add<JsonObject>();
        s["name"] = seg.name;
        if (!seg.isSlave) {
              s["start"] = seg.start * _ledsPerIC;
              s["stop"] = seg.stop * _ledsPerIC;
          } else {
              s["start"] = seg.start;
              s["stop"] = seg.stop;
          }
        s["on"] = seg.isOn;
        s["whiteOnly"] = seg.whiteOnly;
        s["cct"] = seg.cct;
        s["palette"] = seg.palette;
        s["intensity"] = seg.intensity;
        s["color2"] = seg.color2;
        s["color2Enabled"] = seg.color2Enabled;
        s["bri"] = seg.brightness;
        s["effect"] = seg.effect;
        s["speed"] = seg.speed;
        s["color"] = seg.color;
        {
            JsonArray widgetsArr = s["widgets"].to<JsonArray>();
            for (const auto& tw : seg.textWidgets) {
                JsonObject w = widgetsArr.add<JsonObject>();
                w["id"] = tw.id;
                w["type"] = tw.type;
                w["x"] = tw.x;
                w["y"] = tw.y;
                w["color"] = tw.color;
                w["text"] = tw.text;
                w["w"] = tw.imgW;
                w["h"] = tw.imgH;
                w["scale"] = tw.scale;
                w["format"] = tw.format;
                w["font"] = tw.font;
            }
        }
        if (seg.isSlave) {
            s["isSlave"] = true;
            s["slaveId"] = seg.slaveId;
            s["sharesPower"] = seg.sharesPower;
        }
    }
    String segJson;
    serializeJson(doc, segJson);
    prefs.putString("segments", segJson);

    JsonDocument canvasDoc;
    JsonArray canvasArr = canvasDoc.to<JsonArray>();
    getCanvasPanelsJson(canvasArr);
    String canvasJson;
    serializeJson(canvasDoc, canvasJson);
    prefs.putString("canvas", canvasJson);

    prefs.end();
}

void LEDManagerClass::applySettings() {
    if (_bus) {
        delete _bus;
        _bus = nullptr;
    }

    // HUB75 has no per-pin dropdown (its 14 GPIOs are fixed, see Config.h) and is
    // inherently a 2D panel, so its own count/matrix dimensions are derived here
    // rather than taken from the normal pin/count/matrix UI fields.
    if (_type == TYPE_HUB75) {
        _isMatrix = true;
        _ledsPerIC = 1;
        _count = (uint16_t)_matrixWidth * _matrixHeight;
        _numIcs = _count;
    }

    if (_count == 0 || (_type != TYPE_HUB75 && _pins[0] == 255)) return;

    // Constrain LOCAL segments to count. Leave SLAVE segments alone.
    for (auto it = _segments.begin(); it != _segments.end(); ) {
        if (!it->isSlave) {
            if (it->start >= _numIcs) {
                it = _segments.erase(it);
                continue;
            } else {
                if (it->stop > _numIcs) it->stop = _numIcs;
            }
        }
        ++it;
    }
    if (_segments.empty()) {
        Segment seg = {"Segment 0", 0, _numIcs, true, 255, 0, 128, 0xFF0000, 0, 0};
        _segments.push_back(seg);
    }

    uint16_t totalCount = _numIcs;
    for (const auto& seg : _segments) {
        if (seg.stop > totalCount) {
            totalCount = seg.stop;
        }
    }

    IBus* localBus = nullptr;

    switch (_type) {
        case TYPE_WS2812_RGB:
            localBus = new BusDigitalRgb<NeoGrbFeature, Neo800KbpsMethod>(_count, _pins[0], _ledsPerIC); break;
        case TYPE_SK6812_RGBW:
            localBus = new BusDigitalRgbw<NeoGrbwFeature, Neo800KbpsMethod>(_count, _pins[0], _ledsPerIC); break;
        case TYPE_TM1814:
            localBus = new BusDigitalRgbw<NeoWrgbTm1814Feature, Neo800KbpsMethod>(_count, _pins[0]); break;
        case TYPE_400KHZ:
            localBus = new BusDigitalRgb<NeoGrbFeature, Neo400KbpsMethod>(_count, _pins[0]); break;
        case TYPE_APA102:
            localBus = new BusDigitalSpiRgb<DotStarBgrFeature, DotStarSpiMethod>(_count, _pins[1], _pins[0]); break;
        case TYPE_LPD8806:
            localBus = new BusDigitalSpiRgb<Lpd8806GrbFeature, Lpd8806SpiMethod>(_count, _pins[1], _pins[0]); break;
        case TYPE_TM1914:
            // TM1914 requires a chip-specific mode-select settings header before the pixel
            // data (handled by NeoGrbTm1914Feature) - a plain NeoGrbFeature frame omits it.
            localBus = new BusDigitalRgb<NeoGrbTm1914Feature, Neo800KbpsMethod>(_count, _pins[0], _ledsPerIC); break;
        case TYPE_TM1829:
        case TYPE_UCS8903:
        case TYPE_APA106:
        case TYPE_WS2811_W:
        case TYPE_WS281X_WWA:
            localBus = new BusDigitalRgb<NeoGrbFeature, Neo800KbpsMethod>(_count, _pins[0], _ledsPerIC); break;
        case TYPE_FW1906:
        case TYPE_UCS8904:
            localBus = new BusDigitalRgbw<NeoGrbwFeature, Neo800KbpsMethod>(_count, _pins[0], _ledsPerIC); break;
        case TYPE_WS2805:
        case TYPE_SM16825:
            localBus = new BusDigitalRgbww<NeoGrbwcFeature, Neo800KbpsMethod>(_count, _pins[0], _ledsPerIC); break;
        case TYPE_WS2801:
            // WS2801 is plain RGB-over-SPI with no start/end frame - NOT DotStar-protocol-compatible.
            localBus = new BusDigitalSpiRgb<NeoRgbFeature, Ws2801SpiMethod>(_count, _pins[1], _pins[0]); break;
        case TYPE_LPD6803:
            // LPD6803 uses 16-bit 5-5-5 words with a 1-start-bit marker per pixel - NOT DotStar-protocol-compatible.
            localBus = new BusDigitalSpiRgb<Lpd6803RgbFeature, Lpd6803SpiMethod>(_count, _pins[1], _pins[0]); break;
        case TYPE_PP9813:
            // P9813 has its own checksum-byte framing per pixel - NOT DotStar-protocol-compatible.
            localBus = new BusDigitalSpiRgb<P9813BgrFeature, P9813SpiMethod>(_count, _pins[1], _pins[0]); break;
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
        case TYPE_HUB75:
            localBus = new BusHub75(_matrixWidth, _matrixHeight, static_cast<Hub75ShiftDriver>(_hub75ShiftDriver)); break;
        default:
            localBus = new BusDigitalRgb<NeoGrbFeature, Neo800KbpsMethod>(_count, _pins[0], _ledsPerIC); break;
    }

    if (totalCount > _numIcs) {
        _bus = new BusVirtual(localBus, _numIcs, totalCount);
    } else {
        _bus = localBus;
    }

    if (_bus) {
        _bus->Begin();
    }
}

void LEDManagerClass::setConfig(uint8_t p0, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4, uint16_t count, uint8_t type, bool ablEnabled, uint16_t ablMaxmA, uint8_t ledsPerIC) {
    _pins[0] = p0;
    _pins[1] = p1;
    _pins[2] = p2;
    _pins[3] = p3;
    _pins[4] = p4;
    _count = count;
    if (ledsPerIC == 0) ledsPerIC = 1;
    _ledsPerIC = ledsPerIC;
    _numIcs = _count / _ledsPerIC;
    if (_numIcs == 0) _numIcs = 1;
    
    if (_segments.size() == 1 && !_segments[0].isSlave && _segments[0].start == 0) {
        _segments[0].stop = _numIcs;
    }
    
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

// Shared serpentine/row-major XY-to-local-index math, used both by getPixelXY
// (Master's own matrix) and getCanvasPixelXY (any panel in the virtual canvas).
static uint16_t mapXYToLocalIndex(uint16_t x, uint16_t y, uint16_t width, uint8_t layout) {
    if (layout == 0) {
        if (y % 2 == 0) return (y * width) + x;
        return (y * width) + (width - 1 - x);
    }
    return (y * width) + x;
}

uint16_t LEDManagerClass::getCanvasWidth() const {
    uint16_t w = _isMatrix ? _matrixWidth : 0;
    for (const auto& panel : _canvasPanels) {
        uint16_t edge = panel.offsetX + panel.width;
        if (edge > w) w = edge;
    }
    return w;
}

uint16_t LEDManagerClass::getCanvasHeight() const {
    uint16_t h = _isMatrix ? _matrixHeight : 0;
    for (const auto& panel : _canvasPanels) {
        uint16_t edge = panel.offsetY + panel.height;
        if (edge > h) h = edge;
    }
    return h;
}

uint16_t LEDManagerClass::getCanvasPixelXY(uint16_t cx, uint16_t cy) const {
    // The Master's own matrix is always implicit panel 0, anchored at (0,0).
    if (_isMatrix && cx < _matrixWidth && cy < _matrixHeight) {
        uint16_t pos = mapXYToLocalIndex(cx, cy, _matrixWidth, _matrixLayout);
        return pos < _count ? pos : 65535;
    }

    for (const auto& panel : _canvasPanels) {
        if (cx < panel.offsetX || cy < panel.offsetY) continue;
        uint16_t lx = cx - panel.offsetX;
        uint16_t ly = cy - panel.offsetY;
        if (lx >= panel.width || ly >= panel.height) continue;

        for (const auto& seg : _segments) {
            if (seg.isSlave && seg.slaveId == panel.slaveId) {
                uint16_t localIdx = mapXYToLocalIndex(lx, ly, panel.width, panel.layout);
                if (localIdx >= (uint16_t)(seg.stop - seg.start)) return 65535;
                return seg.start + localIdx;
            }
        }
        return 65535; // panel configured but its Slave isn't currently registered
    }
    return 65535;
}

void LEDManagerClass::setCanvasPixelColor(uint16_t cx, uint16_t cy, uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
    if (!_bus) return;
    uint16_t idx = getCanvasPixelXY(cx, cy);
    if (idx != 65535) {
        _bus->SetPixelColor(idx, r, g, b, w);
    }
    updateMatrixPreview(cx, cy, r, g, b);
}

void LEDManagerClass::updateMatrixPreview(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {
    if (!_isMatrix || x >= _matrixWidth || y >= _matrixHeight) return;
    size_t total = (size_t)_matrixWidth * _matrixHeight * 3;
    if (_matrixPreviewBuf.size() != total) _matrixPreviewBuf.assign(total, 0);
    size_t off = ((size_t)y * _matrixWidth + x) * 3;
    _matrixPreviewBuf[off] = r;
    _matrixPreviewBuf[off + 1] = g;
    _matrixPreviewBuf[off + 2] = b;
}

void LEDManagerClass::getMatrixPreviewJson(JsonArray array) const {
    if (!_isMatrix) return;
    size_t total = (size_t)_matrixWidth * _matrixHeight;
    bool haveData = _matrixPreviewBuf.size() == total * 3;
    for (size_t i = 0; i < total; i++) {
        uint32_t rgb = 0;
        if (haveData) {
            rgb = ((uint32_t)_matrixPreviewBuf[i * 3] << 16) |
                  ((uint32_t)_matrixPreviewBuf[i * 3 + 1] << 8) |
                  _matrixPreviewBuf[i * 3 + 2];
        }
        array.add(rgb);
    }
}

void LEDManagerClass::setCanvasPanels(JsonArray panels) {
    _canvasPanels.clear();
    for (JsonObject p : panels) {
        CanvasPanel panel;
        panel.slaveId = p["slaveId"] | 0;
        panel.width = p["width"] | 16;
        panel.height = p["height"] | 16;
        panel.layout = p["layout"] | 0;
        panel.offsetX = p["offsetX"] | 0;
        panel.offsetY = p["offsetY"] | 0;
        if (panel.slaveId != 0 && panel.width > 0 && panel.height > 0) {
            _canvasPanels.push_back(panel);
        }
    }
    saveSettings();
}

void LEDManagerClass::getCanvasPanelsJson(JsonArray array) const {
    for (const auto& panel : _canvasPanels) {
        JsonObject p = array.add<JsonObject>();
        p["slaveId"] = panel.slaveId;
        p["width"] = panel.width;
        p["height"] = panel.height;
        p["layout"] = panel.layout;
        p["offsetX"] = panel.offsetX;
        p["offsetY"] = panel.offsetY;
    }
}

void LEDManagerClass::setSegmentPixelColor(Segment& seg, uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
    if (!_bus) return;
    if (seg.whiteOnly || seg.effect == 10) {
        uint8_t bri = r > g ? (r > b ? r : b) : (g > b ? g : b);
        if (w > bri) bri = w;
        uint8_t w1 = (bri * (255 - seg.cct)) / 255;
        uint8_t w2 = (bri * seg.cct) / 255;
        _bus->SetPixelColor(index, 0, 0, 0, w1, w2);
    } else {
        _bus->SetPixelColor(index, r, g, b, w, 0);
    }
}

void LEDManagerClass::setPixelColorXY(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
    if (!_bus) return;
    uint16_t idx = getPixelXY(x, y);
    if (idx != 65535) {
        _bus->SetPixelColor(idx, r, g, b, w);
    }
    updateMatrixPreview(x, y, r, g, b);
}

void LEDManagerClass::showNow() {
    if (_bus) _bus->Show();
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
        uint16_t reqStart = s["start"] | 0;
          uint16_t reqStop = s["stop"] | (_numIcs * _ledsPerIC);
          bool isSlave = s["isSlave"] | false;
          if (!isSlave) {
              seg.start = reqStart / _ledsPerIC;
              seg.stop = reqStop / _ledsPerIC;
          } else {
              seg.start = reqStart;
              seg.stop = reqStop;
          }
        seg.isOn = s["on"] | true;
                seg.whiteOnly = s["whiteOnly"] | false;
                seg.cct = s["cct"] | 128;
                seg.palette = s["palette"] | 0;
        seg.intensity = s["intensity"] | 128;
        seg.color2 = s["color2"] | 0x0000FF;
                seg.color2Enabled = s["color2Enabled"] | false;
        seg.brightness = s["bri"] | 255;
        seg.effect = s["effect"] | 0;
        seg.speed = s["speed"] | 128;
        seg.color = s["color"] | 0xFF0000;
        seg.textWidgets.clear();
        if (s["widgets"].is<JsonArray>()) {
            for (JsonObject w : s["widgets"].as<JsonArray>()) {
                if (seg.textWidgets.size() >= TEXT_WIDGET_MAX) break;
                TextWidget tw;
                tw.id = w["id"] | 0;
                tw.type = w["type"] | 0;
                tw.x = w["x"] | 0;
                tw.y = w["y"] | 0;
                tw.color = w["color"] | 0xFFFFFF;
                const char* txt = w["text"]; if (txt) tw.text = String(txt);
                tw.imgW = w["w"] | 0;
                tw.imgH = w["h"] | 0;
                tw.scale = w["scale"] | 1;
                tw.format = w["format"] | 0;
                tw.font = w["font"] | 0;
                seg.textWidgets.push_back(tw);
            }
        }
        seg.effectStep = 0;
        seg.lastUpdate = 0;
        seg.isSlave = s["isSlave"] | false;
        seg.slaveId = s["slaveId"] | 0;
        seg.sharesPower = s["sharesPower"] | false;

        if (!seg.isSlave) {
            if (seg.start >= _numIcs && _numIcs > 0) seg.start = _numIcs - 1;
              if (seg.stop > _numIcs) seg.stop = _numIcs;
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
    
    if (!hasMaster && _numIcs > 0) {
        Segment seg = {"Master", 0, _numIcs, true, 255, 0, 128, 0xFF0000, 0, 0};
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
        if (!seg.isSlave) {
              s["start"] = seg.start * _ledsPerIC;
              s["stop"] = seg.stop * _ledsPerIC;
          } else {
              s["start"] = seg.start;
              s["stop"] = seg.stop;
          }
        s["on"] = seg.isOn;
        s["whiteOnly"] = seg.whiteOnly;
        s["cct"] = seg.cct;
        s["palette"] = seg.palette;
        s["intensity"] = seg.intensity;
        s["color2"] = seg.color2;
        s["color2Enabled"] = seg.color2Enabled;
        s["bri"] = seg.brightness;
        s["effect"] = seg.effect;
        s["speed"] = seg.speed;
        s["color"] = seg.color;
        {
            JsonArray widgetsArr = s["widgets"].to<JsonArray>();
            for (const auto& tw : seg.textWidgets) {
                JsonObject w = widgetsArr.add<JsonObject>();
                w["id"] = tw.id;
                w["type"] = tw.type;
                w["x"] = tw.x;
                w["y"] = tw.y;
                w["color"] = tw.color;
                w["text"] = tw.text;
                w["w"] = tw.imgW;
                w["h"] = tw.imgH;
                w["scale"] = tw.scale;
                w["format"] = tw.format;
                w["font"] = tw.font;
            }
        }
        if (seg.isSlave) {
            s["isSlave"] = true;
            s["slaveId"] = seg.slaveId;
            s["sharesPower"] = seg.sharesPower;
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
        uint16_t startPos = _numIcs;
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
        seg.whiteOnly = false;
        seg.cct = 128;
        seg.brightness = 255;
        seg.effect = 0;
        seg.speed = 128;
        seg.color = 0xFF0000;
        seg.effectStep = 0;
        seg.lastUpdate = 0;
        seg.isSlave = true;
        seg.slaveId = slaveId;
        seg.sharesPower = false;
        
        _segments.push_back(seg);
    }
    
    recalculateSegments();
}

void LEDManagerClass::recalculateSegments() {
    uint16_t currentPos = _numIcs;
    for (auto& seg : _segments) {
        if (seg.isSlave) {
            uint16_t len = seg.stop - seg.start;
            seg.start = currentPos;
            seg.stop = currentPos + len;
            currentPos = seg.stop;
        }
    }
    
    applySettings();
    saveSettings();
}



// --- Effects Engine ---

// Routes EffectEngine output into this Master's own bus. Effects live in the shared
// EffectEngine (include/EffectEngine.h) so the Master and a locally-rendering Slave run exactly
// the same code and cannot drift apart; this adapter is the only Master-specific part.
class MasterEffectSink : public IEffectSink {
public:
    MasterEffectSink(LEDManagerClass* mgr, IBus* bus, Segment& seg, bool isMatrix,
                     uint16_t canvasW, uint16_t canvasH)
        : _mgr(mgr), _bus(bus), _seg(seg), _isMatrix(isMatrix), _cw(canvasW), _ch(canvasH) {}

    void setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w, uint8_t w2) override {
        // Writes straight to the bus rather than through setSegmentPixelColor(), because the
        // engine has already applied the white-only/CCT conversion itself.
        if (_bus) _bus->SetPixelColor(_seg.start + index, r, g, b, w, w2);
    }
    uint16_t pixelCount() const override { return _seg.stop - _seg.start; }
    // 2D effects run across the whole virtual canvas (this Master's matrix plus any Slave
    // panels), which is what makes a wave pattern flow across panel boundaries.
    uint16_t matrixWidth() const override { return _isMatrix ? _cw : 0; }
    uint16_t matrixHeight() const override { return _isMatrix ? _ch : 0; }
    void setPixelXY(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) override {
        _mgr->setCanvasPixelColor(x, y, r, g, b, 0);
    }

private:
    LEDManagerClass* _mgr;
    IBus* _bus;
    Segment& _seg;
    bool _isMatrix;
    uint16_t _cw, _ch;
};

void LEDManagerClass::renderWithEngine(Segment& seg, uint8_t ablCap, uint8_t effectOverride) {
    // Deliberately a reference to the segment's own state, not a local: the engine keeps its
    // per-pixel buffers in here, and rebuilding them every frame would reset every effect that
    // depends on what the previous frame drew.
    EffectState& st = seg.renderState;
    st.effect = (effectOverride == 255) ? seg.effect : effectOverride;
    // The engine scales colours by brightness alone, so the ABL cap is folded in here - the same
    // (brightness * ablCap) / 255 the effects used to compute for themselves.
    st.brightness = (uint8_t)(((uint16_t)seg.brightness * ablCap) / 255);
    st.speed = seg.speed;
    st.intensity = seg.intensity;
    st.palette = seg.palette;
    st.isOn = true; // the caller already handled the off case
    st.color = getEffectiveColor(seg);
    st.color2 = seg.color2;
    st.color2Enabled = seg.color2Enabled;
    st.whiteOnly = seg.whiteOnly;
    st.cct = seg.cct;
    st.effectStep = seg.effectStep;

    MasterEffectSink sink(this, _bus, seg, _isMatrix, getCanvasWidth(), getCanvasHeight());
    EffectEngine::draw(st, sink);
    seg.effectStep = st.effectStep; // the engine owns the animation state while it draws
}

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
                    setSegmentPixelColor(unifiedSeg, i, 0, 0, 0, 0);
                }
            } else {
                if (EffectEngine::canRender(unifiedSeg.effect)) {
                    // Shared implementation - identical to what a Slave renders locally.
                    renderWithEngine(unifiedSeg, ablCap);
                } else switch (unifiedSeg.effect) {
                    case 25: effectImage(unifiedSeg, ablCap); break;
                    case 29: effectText(unifiedSeg, ablCap); break;
                    default: renderWithEngine(unifiedSeg, ablCap); break;
                }
                // Write back per-frame animation state so it persists across ticks
                // instead of being reset every frame (unifiedSeg is a throwaway copy).
                _segments[0].effectStep = unifiedSeg.effectStep;
                _segments[0].renderState = std::move(unifiedSeg.renderState);
                // Carries back any image widget pixel data effectText lazily loaded
                // from LittleFS this tick, so it isn't re-read from disk every frame.
                _segments[0].textWidgets = std::move(unifiedSeg.textWidgets);
            }
        }
    } else {
        for (auto& seg : _segments) {
            // Skip segments a Slave draws for itself. Computing those pixels here would be pure
            // waste - nothing reads them, since only the effect parameters get sent - and for a
            // panel-sized segment it is a lot of waste: a 64x64 panel is 4096 pixels per frame.
            // Leaving that work undone is the point of local rendering; it keeps the Master free
            // for the web interface and for managing the other Slaves.
            if (seg.isSlave && seg.slaveId != 254 &&
                EffectEngine::canRender(seg.effect) &&
                SlaveManager.slaveRendersLocally(seg.slaveId)) {
                continue;
            }

            unsigned int delayMs = 500 - (seg.speed * 490 / 255);
            if (seg.effect == 0) delayMs = 100;

            if (now - seg.lastUpdate > delayMs) {
                seg.lastUpdate = now;
                shouldShow = true;

                if (!seg.isOn) {
                    for (uint16_t i = seg.start; i < seg.stop; i++) {
                        setSegmentPixelColor(seg, i, 0, 0, 0, 0);
                    }
                } else {
                    if (EffectEngine::canRender(seg.effect)) {
                        // Shared implementation - identical to what a Slave renders locally.
                        renderWithEngine(seg, ablCap);
                    } else switch (seg.effect) {
                        case 25: effectImage(seg, ablCap); break;
                        case 29: effectText(seg, ablCap); break;
                        default: renderWithEngine(seg, ablCap); break;
                    }
                }
            }
        }
    }

    if (shouldShow) {
        // Send slave data over UART before driving the local RMT strip, so the
        // interrupt-heavy UART transfer doesn't interleave with the timing-sensitive
        // RMT WS2812 signal generation triggered by Show(). The buffer is already
        // fully populated by the effect functions above, so this ordering is safe.
        const uint8_t* buffer = _bus->getBuffer();
        if (buffer) {
            uint16_t totalCount = _numIcs;
            for (const auto& seg : _segments) {
                if (seg.stop > totalCount) totalCount = seg.stop;
            }

            for (const auto& seg : _segments) {
                if (seg.isSlave && seg.slaveId != 254 && seg.stop > seg.start) {
                    // Hand the effect to the Slave when it can draw it itself. A panel of any real
                    // size cannot be fed frame by frame - 64x64 alone is 4096 pixels - so the
                    // parameters go over instead and the Slave renders. Effects the Slave has no
                    // way to produce (image data, clock and weather) still stream as pixels.
                    // With sync on, every segment shows one continuous effect spanning the whole
                    // chain, so the Slave gets the leading segment's parameters plus the window it
                    // occupies - it renders the full-length effect and displays only its slice.
                    // Without sync each segment is independent and the Slave renders its own.
                    const Segment& src = _syncActive ? _segments[0] : seg;
                    uint16_t winOffset = _syncActive ? seg.start : 0;
                    uint16_t winTotal = _syncActive ? totalCount : 0;

                    if (EffectEngine::canRender(src.effect) &&
                        SlaveManager.slaveRendersLocally(seg.slaveId)) {
                        SlaveManager.sendSegmentConfig(seg.slaveId, src.effect, src.brightness,
                                                       src.speed, src.intensity, src.palette,
                                                       src.isOn, getEffectiveColor(src), src.color2,
                                                       src.color2Enabled, src.whiteOnly, src.cct,
                                                       src.effectStep, winOffset, winTotal);
                        continue;
                    }
                    uint16_t ledsToSend = seg.stop - seg.start;
                    if (totalCount >= seg.stop) {
                        SlaveManager.sendLEDData(seg.slaveId, &buffer[seg.start * 5], ledsToSend * 5);
                    }
                }
            }
        }

        _bus->Show();
    }
}

uint8_t LEDManagerClass::getGlobalAblCap() {
    // HUB75 panels draw from their own dedicated external PSU (sized by the user
    // for the panel's real max draw), not through the ESP32 - the per-LED current
    // model below doesn't apply, so ABL is a no-op for this LED type.
    if (_type == TYPE_HUB75) return 255;
    if (!_ablEnabled) return 255;
    
    uint32_t base_mA = 100; // Base ESP current
    
    // Add 100mA for every Slave that shares power
    for (const auto& seg : _segments) {
        if (seg.isSlave && seg.sharesPower) {
            base_mA += 100;
        }
    }
    
    if (_ablMaxmA <= base_mA) return 0;
    
    uint32_t total_led_mA_full = 0;
    
    for (const auto& seg : _segments) {
        if (!seg.isOn || seg.brightness == 0) continue;
        
        // Skip slaves that don't share power
        if (seg.isSlave && !seg.sharesPower) continue;
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
        
        // Deliberately NOT scaled by seg.brightness: the effects apply this cap
        // multiplicatively as (seg.brightness * ablCap) / 255. Folding brightness in here
        // too would cancel it out entirely - the cap would then hold the output at a fixed
        // level and the brightness slider would do nothing until it drops below the point
        // where ABL stops limiting at all. Estimating at full brightness keeps the cap a
        // true ceiling: at 100% the output lands exactly on the current budget, and every
        // step below scales down linearly from there (and thus stays safely under it).
        total_led_mA_full += seg_mA;
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

// tm_wday-indexed (0 = Sunday), used by the "Wochentag" date format below.
static const char* const WEEKDAY_ABBR[7] = {"SO", "MO", "DI", "MI", "DO", "FR", "SA"};

void LEDManagerClass::effectImage(Segment& seg, uint8_t ablCap) {
    // Intentionally does nothing: pixels for this segment are written directly by
    // /api/matrix (via setPixelColorXY), and this effect just holds them in place
    // instead of animating over them.
    (void)seg;
    (void)ablCap;
}

void LEDManagerClass::effectText(Segment& seg, uint8_t ablCap) {
    // Renders every widget in seg.textWidgets (clock/date/text/image) at its
    // own freely-positioned (x,y) using the built-in 5x7 bitmap font (see
    // Font5x7.h). Makes no sense on a plain 1D strip, so it no-ops there (like
    // the other HUB75 showcase effects).
    if (!_isMatrix) return;
    uint16_t cw = getCanvasWidth();
    uint16_t ch = getCanvasHeight();
    if (cw == 0 || ch == 0) return;

    for (uint16_t y = 0; y < ch; y++) {
        for (uint16_t x = 0; x < cw; x++) {
            setCanvasPixelColor(x, y, 0, 0, 0, 0);
        }
    }

    if (seg.textWidgets.empty()) return;

    uint16_t currentBri = (seg.brightness * ablCap) / 255;
    time_t nowEpoch = time(nullptr);
    struct tm timeinfo;
    localtime_r(&nowEpoch, &timeinfo);
    char buf[16];

    for (auto& tw : seg.textWidgets) {
        uint8_t scale = tw.scale;
        if (scale < 1) scale = 1;
        if (scale > TEXT_WIDGET_SCALE_MAX) scale = TEXT_WIDGET_SCALE_MAX;

        if (tw.type == 5) {
            // Wetter - icon (WeatherIcons.h) + temperature text (Font5x7/Font3x5,
            // like the other text widgets). tw.format: 0 = icon+temp, 1 = icon
            // only, 2 = temp only. Data comes from WeatherManager's background
            // polling (see WeatherManager.cpp) - never fetched from here.
            uint8_t r = ((tw.color >> 16) & 0xFF) * currentBri / 255;
            uint8_t g = ((tw.color >> 8) & 0xFF) * currentBri / 255;
            uint8_t b = (tw.color & 0xFF) * currentBri / 255;

            int16_t curX = tw.x;
            if (tw.format != 2) {
                uint8_t icon = WeatherManager.getWeatherIcon();
                for (uint8_t col = 0; col < WEATHER_ICON_WIDTH; col++) {
                    for (uint8_t row = 0; row < WEATHER_ICON_HEIGHT; row++) {
                        if (!weather_icon_pixel(icon, col, row)) continue;
                        for (uint8_t sy = 0; sy < scale; sy++) {
                            int16_t py = tw.y + (int16_t)row * scale + sy;
                            if (py < 0 || py >= (int16_t)ch) continue;
                            for (uint8_t sx = 0; sx < scale; sx++) {
                                int16_t px = curX + (int16_t)col * scale + sx;
                                if (px < 0 || px >= (int16_t)cw) continue;
                                setCanvasPixelColor(px, py, r, g, b, 0);
                            }
                        }
                    }
                }
                curX += (int16_t)((WEATHER_ICON_WIDTH + 1) * scale);
            }
            if (tw.format != 1) {
                char wbuf[8];
                if (WeatherManager.hasData()) {
                    snprintf(wbuf, sizeof(wbuf), "%d`C", (int)lroundf(WeatherManager.getTemperature()));
                } else {
                    snprintf(wbuf, sizeof(wbuf), "--`C");
                }
                bool mini = (tw.font == 1);
                uint8_t glyphW = mini ? FONT3X5_GLYPH_WIDTH : FONT5X7_GLYPH_WIDTH;
                uint8_t glyphH = mini ? FONT3X5_GLYPH_HEIGHT : FONT5X7_GLYPH_HEIGHT;
                bool (*glyphPixel)(char, uint8_t, uint8_t) = mini ? font3x5_pixel : font5x7_pixel;
                uint8_t charAdvance = (glyphW + 1) * scale;
                for (size_t i = 0; wbuf[i] != '\0'; i++) {
                    int16_t charX = curX + (int16_t)(i * charAdvance);
                    if (charX + glyphW * scale < 0 || charX >= (int16_t)cw) continue;
                    for (uint8_t col = 0; col < glyphW; col++) {
                        for (uint8_t row = 0; row < glyphH; row++) {
                            if (!glyphPixel(wbuf[i], col, row)) continue;
                            for (uint8_t sy = 0; sy < scale; sy++) {
                                int16_t py = tw.y + (int16_t)row * scale + sy;
                                if (py < 0 || py >= (int16_t)ch) continue;
                                for (uint8_t sx = 0; sx < scale; sx++) {
                                    int16_t px = charX + (int16_t)col * scale + sx;
                                    if (px < 0 || px >= (int16_t)cw) continue;
                                    setCanvasPixelColor(px, py, r, g, b, 0);
                                }
                            }
                        }
                    }
                }
            }
            continue;
        }

        if (tw.type == 4) {
            // Analoguhr - drawn with trig instead of a font; tw.imgW doubles as the
            // face diameter (no separate size field needed) and tw.format selects
            // the design: 0=Klassisch (Kreis+Ziffernpunkte), 1=Minimal (nur Zeiger),
            // 2=Ziffernpunkte+Sekundenzeiger, 3=Kreuz (nur 12/3/6/9-Punkte+Kreis).
            uint16_t diameter = tw.imgW > 0 ? tw.imgW : 16;
            if (diameter < 8) diameter = 8;
            float radius = diameter / 2.0f;
            float ccx = tw.x + radius;
            float ccy = tw.y + radius;

            uint8_t r = ((tw.color >> 16) & 0xFF) * currentBri / 255;
            uint8_t g = ((tw.color >> 8) & 0xFF) * currentBri / 255;
            uint8_t b = (tw.color & 0xFF) * currentBri / 255;

            auto plot = [&](float px, float py) {
                int16_t ix = (int16_t)lroundf(px);
                int16_t iy = (int16_t)lroundf(py);
                if (ix < 0 || ix >= (int16_t)cw || iy < 0 || iy >= (int16_t)ch) return;
                setCanvasPixelColor(ix, iy, r, g, b, 0);
            };
            auto plotHand = [&](float angleDeg, float len) {
                float rad = angleDeg * (float)PI / 180.0f;
                float dx = sinf(rad), dy = -cosf(rad);
                int steps = (int)len + 1;
                for (int s = 0; s <= steps; s++) {
                    float t = (float)s * len / steps;
                    plot(ccx + dx * t, ccy + dy * t);
                }
            };

            if (tw.format == 0 || tw.format == 3) { // face outline
                for (float a = 0; a < 360.0f; a += 2.0f) {
                    float rad = a * (float)PI / 180.0f;
                    plot(ccx + radius * sinf(rad), ccy - radius * cosf(rad));
                }
            }
            // Tick marks are drawn as short inward-pointing spokes (not single points
            // sitting on the outline) so they're actually visible as distinct marks
            // instead of blending into the face outline.
            if (tw.format == 0) { // 12 short ticks
                for (int i = 0; i < 12; i++) {
                    float rad = i * 30.0f * (float)PI / 180.0f;
                    float sn = sinf(rad), cs = cosf(rad);
                    for (float rr = radius * 0.75f; rr <= radius; rr += 1.0f) {
                        plot(ccx + rr * sn, ccy - rr * cs);
                    }
                }
            } else if (tw.format == 2) { // 12 dots (no outline for this design, so plain dots stand out)
                for (int i = 0; i < 12; i++) {
                    float rad = i * 30.0f * (float)PI / 180.0f;
                    plot(ccx + radius * sinf(rad), ccy - radius * cosf(rad));
                }
            } else if (tw.format == 3) { // 4 long ticks (12/3/6/9) - clearly longer than "Klassisch"'s
                for (int i = 0; i < 4; i++) {
                    float rad = i * 90.0f * (float)PI / 180.0f;
                    float sn = sinf(rad), cs = cosf(rad);
                    for (float rr = radius * 0.55f; rr <= radius; rr += 1.0f) {
                        plot(ccx + rr * sn, ccy - rr * cs);
                    }
                }
            }

            plotHand(((timeinfo.tm_hour % 12) + timeinfo.tm_min / 60.0f) * 30.0f, radius * 0.5f);
            plotHand(timeinfo.tm_min * 6.0f, radius * 0.85f);
            if (tw.format == 2) {
                plotHand(timeinfo.tm_sec * 6.0f, radius * 0.9f);
            }
            continue;
        }

        if (tw.type == 3) {
            // Image widget - lazily loaded from its LittleFS file on first use
            // (can't be loaded any earlier than this: LEDManager::begin() runs
            // before WebServerManager mounts LittleFS at startup).
            if (tw.imgData.empty() && tw.imgW > 0 && tw.imgH > 0) {
                loadWidgetImageFromFs(tw);
            }
            if (tw.imgData.size() != (size_t)tw.imgW * tw.imgH * 3) continue;
            for (uint16_t iy = 0; iy < tw.imgH; iy++) {
                for (uint16_t ix = 0; ix < tw.imgW; ix++) {
                    size_t off = ((size_t)iy * tw.imgW + ix) * 3;
                    uint8_t r = (uint16_t)tw.imgData[off] * currentBri / 255;
                    uint8_t g = (uint16_t)tw.imgData[off + 1] * currentBri / 255;
                    uint8_t b = (uint16_t)tw.imgData[off + 2] * currentBri / 255;
                    if (!r && !g && !b) continue;
                    for (uint8_t sy = 0; sy < scale; sy++) {
                        int16_t py = tw.y + (int16_t)iy * scale + sy;
                        if (py < 0 || py >= (int16_t)ch) continue;
                        for (uint8_t sx = 0; sx < scale; sx++) {
                            int16_t px = tw.x + (int16_t)ix * scale + sx;
                            if (px < 0 || px >= (int16_t)cw) continue;
                            setCanvasPixelColor(px, py, r, g, b, 0);
                        }
                    }
                }
            }
            continue;
        }

        String text;
        if (tw.type == 0) { // Uhrzeit
            switch (tw.format) {
                case 1: // HH:MM:SS
                    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                    break;
                case 2: { // 12h AM/PM
                    int h12 = timeinfo.tm_hour % 12;
                    if (h12 == 0) h12 = 12;
                    snprintf(buf, sizeof(buf), "%02d:%02d%s", h12, timeinfo.tm_min, timeinfo.tm_hour < 12 ? "AM" : "PM");
                    break;
                }
                default: // HH:MM
                    snprintf(buf, sizeof(buf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
                    break;
            }
            text = buf;
        } else if (tw.type == 1) { // Datum
            switch (tw.format) {
                case 1: // DD.MM.YYYY
                    snprintf(buf, sizeof(buf), "%02d.%02d.%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
                    break;
                case 2: // DD.MM.YY
                    snprintf(buf, sizeof(buf), "%02d.%02d.%02d", timeinfo.tm_mday, timeinfo.tm_mon + 1, (timeinfo.tm_year + 1900) % 100);
                    break;
                case 3: // YYYY-MM-DD (ISO)
                    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
                    break;
                case 4: // MM/DD/YYYY (US)
                    snprintf(buf, sizeof(buf), "%02d/%02d/%04d", timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_year + 1900);
                    break;
                case 5: // Wochentag DD.MM.
                    snprintf(buf, sizeof(buf), "%s %02d.%02d.", WEEKDAY_ABBR[timeinfo.tm_wday], timeinfo.tm_mday, timeinfo.tm_mon + 1);
                    break;
                default: // DD.MM.
                    snprintf(buf, sizeof(buf), "%02d.%02d.", timeinfo.tm_mday, timeinfo.tm_mon + 1);
                    break;
            }
            text = buf;
        } else { // Eigener Text
            text = tw.text;
        }
        if (text.length() == 0) continue;

        uint8_t r = ((tw.color >> 16) & 0xFF) * currentBri / 255;
        uint8_t g = ((tw.color >> 8) & 0xFF) * currentBri / 255;
        uint8_t b = (tw.color & 0xFF) * currentBri / 255;

        bool mini = (tw.font == 1);
        uint8_t glyphW = mini ? FONT3X5_GLYPH_WIDTH : FONT5X7_GLYPH_WIDTH;
        uint8_t glyphH = mini ? FONT3X5_GLYPH_HEIGHT : FONT5X7_GLYPH_HEIGHT;
        bool (*glyphPixel)(char, uint8_t, uint8_t) = mini ? font3x5_pixel : font5x7_pixel;

        const uint8_t charAdvance = (glyphW + 1) * scale; // 1px gap between glyphs, scaled
        for (size_t i = 0; i < text.length(); i++) {
            int16_t charX = tw.x + i * charAdvance;
            if (charX + glyphW * scale < 0 || charX >= (int16_t)cw) continue;
            for (uint8_t col = 0; col < glyphW; col++) {
                for (uint8_t row = 0; row < glyphH; row++) {
                    if (!glyphPixel(text[i], col, row)) continue;
                    for (uint8_t sy = 0; sy < scale; sy++) {
                        int16_t py = tw.y + (int16_t)row * scale + sy;
                        if (py < 0 || py >= (int16_t)ch) continue;
                        for (uint8_t sx = 0; sx < scale; sx++) {
                            int16_t px = charX + (int16_t)col * scale + sx;
                            if (px < 0 || px >= (int16_t)cw) continue;
                            setCanvasPixelColor(px, py, r, g, b, 0);
                        }
                    }
                }
            }
        }
    }
}

void LEDManagerClass::setWhiteOnly(uint8_t segId, bool whiteOnly) {
    if (segId < _segments.size()) {
        _segments[segId].whiteOnly = whiteOnly;
        triggerSave();
    }
}

void LEDManagerClass::setCct(uint8_t segId, uint8_t cct) {
    if (segId < _segments.size()) {
        _segments[segId].cct = cct;
        triggerSave();
    }
}

void LEDManagerClass::setPalette(uint8_t segId, uint8_t palette) {
    if (segId < _segments.size() && palette < PALETTE_COUNT) {
        _segments[segId].palette = palette;
        triggerSave();
    }
}

uint8_t LEDManagerClass::getPalette(uint8_t segId) const {
    if (segId < _segments.size()) return _segments[segId].palette;
    return 0;
}

void LEDManagerClass::setIntensity(uint8_t segId, uint8_t intensity) {
    if (segId < _segments.size()) {
        _segments[segId].intensity = intensity;
        triggerSave();
    }
}

uint8_t LEDManagerClass::getIntensity(uint8_t segId) const {
    if (segId < _segments.size()) return _segments[segId].intensity;
    return 128;
}

void LEDManagerClass::setColor2(uint8_t segId, uint32_t color2) {
    if (segId < _segments.size()) {
        _segments[segId].color2 = color2;
        triggerSave();
    }
}

uint32_t LEDManagerClass::getColor2(uint8_t segId) const {
    if (segId < _segments.size()) return _segments[segId].color2;
    return 0x0000FF;
}

void LEDManagerClass::setColor2Enabled(uint8_t segId, bool enabled) {
    if (segId < _segments.size()) {
        _segments[segId].color2Enabled = enabled;
        triggerSave();
    }
}

bool LEDManagerClass::getColor2Enabled(uint8_t segId) const {
    if (segId < _segments.size()) return _segments[segId].color2Enabled;
    return false;
}

uint8_t LEDManagerClass::getNextWidgetId() const {
    uint8_t maxId = 0;
    for (const auto& seg : _segments) {
        for (const auto& tw : seg.textWidgets) {
            if (tw.id > maxId) maxId = tw.id;
        }
    }
    return maxId + 1;
}

void LEDManagerClass::setTextWidgets(uint8_t segId, JsonArray widgets) {
    if (segId >= _segments.size()) return;
    uint8_t nextId = getNextWidgetId();

    std::vector<TextWidget> newWidgets;
    for (JsonObject w : widgets) {
        if (newWidgets.size() >= TEXT_WIDGET_MAX) break;
        TextWidget tw;
        tw.id = w["id"] | 0;
        if (tw.id == 0) tw.id = nextId++;
        tw.type = w["type"] | 0;
        if (tw.type > 5) tw.type = 0;
        tw.x = w["x"] | 0;
        tw.y = w["y"] | 0;
        tw.color = w["color"] | 0xFFFFFF;
        const char* txt = w["text"];
        if (txt) tw.text = String(txt);
        if (tw.text.length() > 64) tw.text = tw.text.substring(0, 64);
        uint8_t imgW = w["w"] | 0;
        uint8_t imgH = w["h"] | 0;
        tw.imgW = imgW > TEXT_WIDGET_IMG_MAX ? TEXT_WIDGET_IMG_MAX : imgW;
        tw.imgH = imgH > TEXT_WIDGET_IMG_MAX ? TEXT_WIDGET_IMG_MAX : imgH;
        uint8_t scale = w["scale"] | 1;
        if (scale < 1) scale = 1;
        if (scale > TEXT_WIDGET_SCALE_MAX) scale = TEXT_WIDGET_SCALE_MAX;
        tw.scale = scale;
        tw.format = w["format"] | 0;
        tw.font = w["font"] | 0;
        if (tw.type == 3 && tw.imgW > 0 && tw.imgH > 0) {
            loadWidgetImageFromFs(tw); // re-associate with its existing image, if any
        }
        newWidgets.push_back(std::move(tw));
    }
    _segments[segId].textWidgets = std::move(newWidgets);
    // Auto-activate "Uhr / Text" so whatever was just configured shows up in the
    // live preview immediately, without a separate trip to the effects grid -
    // mirrors how /api/matrix already auto-switches to "Bild" for the same reason.
    if (!_segments[segId].textWidgets.empty() && _segments[segId].effect != 29) {
        setEffect(segId, 29);
    }
    triggerSave();
}

void LEDManagerClass::getTextWidgetsJson(uint8_t segId, JsonArray array) const {
    if (segId >= _segments.size()) return;
    for (const auto& tw : _segments[segId].textWidgets) {
        JsonObject w = array.add<JsonObject>();
        w["id"] = tw.id;
        w["type"] = tw.type;
        w["x"] = tw.x;
        w["y"] = tw.y;
        w["color"] = tw.color;
        w["text"] = tw.text;
        w["w"] = tw.imgW;
        w["h"] = tw.imgH;
        w["scale"] = tw.scale;
        w["format"] = tw.format;
        w["font"] = tw.font;
    }
}

void LEDManagerClass::setTextWidgetImage(uint8_t segId, uint8_t widgetId, uint8_t w, uint8_t h, const std::vector<uint8_t>& rgbData) {
    if (segId >= _segments.size()) return;
    if (w > TEXT_WIDGET_IMG_MAX) w = TEXT_WIDGET_IMG_MAX;
    if (h > TEXT_WIDGET_IMG_MAX) h = TEXT_WIDGET_IMG_MAX;
    if (rgbData.size() != (size_t)w * h * 3) return;
    for (auto& tw : _segments[segId].textWidgets) {
        if (tw.id == widgetId) {
            tw.type = 3;
            tw.imgW = w;
            tw.imgH = h;
            tw.imgData = rgbData;
            saveWidgetImageToFs(tw);
            if (_segments[segId].effect != 29) setEffect(segId, 29);
            triggerSave();
            return;
        }
    }
}

uint32_t LEDManagerClass::getPaletteColor(uint8_t paletteId, uint8_t pos) const {
    if (paletteId == 0 || paletteId >= PALETTE_COUNT) return 0;

    const PaletteDef& pal = PALETTE_STOPS[paletteId];
    uint8_t stops = pal.stopCount;
    if (stops == 0) return 0;
    if (stops == 1) return pal.stops[0];

    uint16_t segWidth = 256 / (stops - 1);
    uint8_t segIdx = pos / segWidth;
    if (segIdx >= stops - 1) segIdx = stops - 2;
    uint8_t localPos = pos - segIdx * segWidth;
    uint8_t blend = (uint16_t)localPos * 255 / segWidth;

    uint32_t c1 = pal.stops[segIdx];
    uint32_t c2 = pal.stops[segIdx + 1];
    uint8_t r = (((c1 >> 16) & 0xFF) * (255 - blend) + ((c2 >> 16) & 0xFF) * blend) / 255;
    uint8_t g = (((c1 >> 8) & 0xFF) * (255 - blend) + ((c2 >> 8) & 0xFF) * blend) / 255;
    uint8_t b = ((c1 & 0xFF) * (255 - blend) + (c2 & 0xFF) * blend) / 255;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

uint32_t LEDManagerClass::getEffectiveColor(const Segment& seg) const {
    if (seg.palette == 0) return seg.color;
    uint8_t pos = (seg.effectStep / 2) & 0xFF; // slow cycle through the palette over time
    uint32_t rgb = getPaletteColor(seg.palette, pos);
    return (seg.color & 0xFF000000) | rgb; // keep the segment's own white channel
}

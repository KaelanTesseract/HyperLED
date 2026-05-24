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
#include "BusWrapper.h"
#include <Preferences.h>
#include "Config.h"
#include <ArduinoJson.h>

#define EFFECT_COUNT 10
extern const char* const EFFECT_NAMES[EFFECT_COUNT];

struct Segment {
    String name;
    uint16_t start;
    uint16_t stop;
    bool isOn;
    uint8_t brightness;
    uint8_t effect;
    uint8_t speed;
    uint32_t color;
    uint16_t effectStep;
    unsigned long lastUpdate;
    
    // Slave support
    bool isSlave = false;
    uint8_t slaveId = 0;
};

class LEDManagerClass {
public:
    void begin();
    void loop();

    // Configuration
    void setConfig(uint8_t p0, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4, uint16_t count, uint8_t type, bool ablEnabled, uint16_t ablMaxmA);
    void setMatrixConfig(bool isMatrix, uint16_t width, uint16_t height, uint8_t layout);
    uint8_t getPin(uint8_t index) const;
    uint16_t getCount() const { return _count; }
    uint8_t getType() const { return _type; }
    bool hasWhiteChannel() const;

    // Segment State Control
    void setPower(uint8_t segId, bool on);
    void setBrightness(uint8_t segId, uint8_t bri);
    void setEffect(uint8_t segId, uint8_t effectId);
    void setSpeed(uint8_t segId, uint8_t speed);
    void setColor(uint8_t segId, uint32_t color);
    void setSync(bool sync) { _syncActive = sync; }
    bool getSync() const { return _syncActive; }
    void triggerSave() {
        _savePending = true;
        _saveTimer = millis();
    }

    bool getPower(uint8_t segId) const;
    uint8_t getBrightness(uint8_t segId) const;
    uint8_t getEffect(uint8_t segId) const;
    uint8_t getSpeed(uint8_t segId) const;
    uint32_t getColor(uint8_t segId) const;
    
    // Segment Management
    void setSegmentsFromJson(JsonArray segmentsArray);
    void getSegmentsJson(JsonArray array) const;
    uint8_t getNumSegments() const;
    const Segment* getSegment(uint8_t segId) const;
    void addSlaveSegment(uint8_t slaveId, uint16_t count, const String& name);
    void recalculateSegments();
    
    // Matrix 2D Mapping
    uint16_t getPixelXY(uint16_t x, uint16_t y) const;
    void setPixelColorXY(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0);
    bool isMatrix() const { return _isMatrix; }
    uint16_t getMatrixWidth() const { return _matrixWidth; }
    uint16_t getMatrixHeight() const { return _matrixHeight; }
    uint8_t getMatrixLayout() const { return _matrixLayout; }

    bool getAblEnabled() const { return _ablEnabled; }
    uint16_t getAblMaxmA() const { return _ablMaxmA; }

private:
    IBus* _bus = nullptr;

    // Config
    uint8_t _pins[5];
    uint16_t _count;
    uint8_t _type;
    bool _ablEnabled;
    uint16_t _ablMaxmA;

    // Segments
    std::vector<Segment> _segments;
    
    // Matrix configuration
    bool _isMatrix = false;
    uint16_t _matrixWidth = 16;
    uint16_t _matrixHeight = 16;
    uint8_t _matrixLayout = 0; // 0 = Zick-Zack/Serpentine, 1 = Zeilenweise

    bool _syncActive = false;
    bool _savePending = false;
    unsigned long _saveTimer = 0;

    void loadSettings();
    void saveSettings();
    void applySettings();

    // Effects
    void effectSolid(Segment& seg, uint8_t ablCap);
    void effectBreathe(Segment& seg, uint8_t ablCap);
    void effectRainbow(Segment& seg, uint8_t ablCap);
    void effectChase(Segment& seg, uint8_t ablCap);
    void effectFire(Segment& seg, uint8_t ablCap);
    void effectColorWipe(Segment& seg, uint8_t ablCap);
    void effectScanner(Segment& seg, uint8_t ablCap);
    void effectTwinkle(Segment& seg, uint8_t ablCap);
    void effectMeteor(Segment& seg, uint8_t ablCap);
    void effectMatrixRain(Segment& seg, uint8_t ablCap);
    
    uint8_t getGlobalAblCap();
    uint32_t Wheel(byte WheelPos);
};

extern LEDManagerClass LEDManager;

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

#define EFFECT_COUNT 30
// Effects at/after this index are HUB75 showcase effects: full-canvas 2D effects
// designed to look striking on a big, sharp, full-color scan-matrix panel. The
// WebUI only shows them as selectable once the LED type is set to HUB75.
#define EFFECT_HUB75_SHOWCASE_START 26
extern const char* const EFFECT_NAMES[EFFECT_COUNT];

// "Uhr / Text" effect (see below): a segment can carry up to this many
// independently positioned clock/date/text/image elements, and an image
// element can be at most this many pixels square (kept small since each one
// is persisted as its own tiny file in LittleFS).
#define TEXT_WIDGET_MAX 12
// Large enough to cover a full-canvas "background" image widget (the Pixel Art
// Converter/Editor's merged-in output) on a big HUB75 panel, while keeping
// per-image LittleFS files and the /api/text_widget_image JSON payload bounded.
#define TEXT_WIDGET_IMG_MAX 64
// Per-widget size multiplier (1 = native size): each font/image pixel is drawn
// as an NxN block. Capped to keep a single widget's per-tick render cost bounded.
#define TEXT_WIDGET_SCALE_MAX 8

// Palette 0 is always "Solid" (uses the segment's own `color`, no gradient).
#define PALETTE_COUNT 5
extern const char* const PALETTE_NAMES[PALETTE_COUNT];

// A physical Slave panel placed within the shared virtual canvas (see "Multi-Panel
// Canvas" below). The Master's own matrix (isMatrix/matrixWidth/matrixHeight/
// matrixLayout) is always implicitly panel 0, anchored at canvas origin (0,0);
// this struct only describes the *additional* Slave panels.
struct CanvasPanel {
    uint8_t slaveId;
    uint16_t width;
    uint16_t height;
    uint8_t layout; // 0 = serpentine, 1 = row-major, same meaning as _matrixLayout
    // Position within the canvas, relative to the Master panel's top-left corner
    // at (0,0). Panels can only extend right/down from the Master (no negative
    // coordinates) - keeps the canvas coordinate space simple (plain uint16_t,
    // same convention as the rest of the 2D pixel API).
    uint16_t offsetX;
    uint16_t offsetY;
};

// One independently positioned element drawn by the "Uhr / Text" effect (see
// LEDManagerClass::effectText). `id` is a stable, globally unique identifier
// assigned once at creation (see getNextWidgetId()) - unlike a Segment's own
// index, it never shifts when the widget list is reordered/edited, so it can
// safely be used as the on-disk filename for an image widget's pixel data
// (see widgetImagePath() in LEDManager.cpp).
struct TextWidget {
    uint8_t id = 0;
    uint8_t type = 0; // 0 = clock (HH:MM), 1 = date (DD.MM.), 2 = custom text, 3 = image
    int16_t x = 0;
    int16_t y = 0;
    uint32_t color = 0xFFFFFF; // used by clock/date/text (not image)
    String text = "";          // used by type == 2
    uint8_t imgW = 0;          // used by type == 3
    uint8_t imgH = 0;
    // Size multiplier (1-TEXT_WIDGET_SCALE_MAX): each font/image pixel is drawn
    // as an NxN block, so a widget can be made bigger without more source detail.
    uint8_t scale = 1;
    // Display format for type == 0 (clock) or type == 1 (date); see effectText
    // in LEDManager.cpp for the exact layouts. Ignored by other widget types.
    uint8_t format = 0;
    // Font for type == 0/1/2 (clock/date/text): 0 = normal 5x7 (Font5x7.h),
    // 1 = compact 3x5 "mini" (Font3x5.h). Ignored by image widgets.
    uint8_t font = 0;
    // Pixel data for an image widget (RGB triplets, imgW*imgH*3 bytes) - kept
    // in RAM only; not part of the JSON round-trip that persists to NVS. Lazily
    // loaded from its LittleFS file on first use (see effectText).
    std::vector<uint8_t> imgData;
};

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
    bool sharesPower = false;
    bool whiteOnly = false;
    uint8_t cct = 128;
    uint8_t palette = 0;

    // Generic second effect parameter (0-255), interpreted per-effect: sparkle
    // density, trail length, cooling rate, etc. Mirrors WLED's "intensity" slider.
    uint8_t intensity = 128;
    // Secondary color, used by effects that blend/alternate between two colors
    // (e.g. Chase, Color Wipe) instead of color-to-black. Off by default so those
    // effects keep their original color-to-black look until explicitly enabled.
    uint32_t color2 = 0x0000FF;
    bool color2Enabled = false;

    // "Uhr / Text" effect config: an ordered list of independently positioned
    // clock/date/text/image elements. Only meaningful when effect == the text
    // effect id, but kept even when another effect is active (so switching
    // back to "Uhr / Text" restores the previous layout).
    std::vector<TextWidget> textWidgets;

    // Per-pixel animation state for effects that need memory across frames
    // (lazily sized to the segment's pixel count on first use by that effect).
    std::vector<uint8_t> twinkleState;
    std::vector<uint8_t> fireHeat; // reused by both 1D Fire (count-sized) and Fire 2D (width*height-sized)
    std::vector<uint8_t> sinelonState;
    std::vector<uint8_t> confettiState;
    std::vector<uint8_t> confettiHue;
    std::vector<uint8_t> juggleState; // 3 bytes (r,g,b) per pixel
    std::vector<uint8_t> rippleState; // 3 bytes (x, y, radius) per ripple
    std::vector<uint8_t> fireworksState; // 6 bytes (x, y, peakY, hue, timer, state) per rocket
    std::vector<uint8_t> starfieldState; // 3 bytes (dx, dy, radius) per star
    std::vector<uint8_t> ballsState;     // 5 bytes (x, y, vx, vy, hue) per ball
};

class LEDManagerClass {
public:
    void begin();
    void loop();

    // Configuration
    void setConfig(uint8_t p0, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4, uint16_t count, uint8_t type, bool ablEnabled, uint16_t ablMaxmA, uint8_t ledsPerIC);
    void setMatrixConfig(bool isMatrix, uint16_t width, uint16_t height, uint8_t layout);
    uint8_t getPin(uint8_t index) const;
    uint16_t getCount() const { return _count; }
    uint8_t getType() const { return _type; }
    uint8_t getLedsPerIC() const { return _ledsPerIC; }
    void setLedsPerIC(uint8_t ledsPerIC) { _ledsPerIC = ledsPerIC; }
    bool hasWhiteChannel() const;

    // HUB75 shift-driver chip variant (0=Generic, 1=FM6126A, 2=ICN2038S, 3=FM6124,
    // 4=MBI5124, 5=DP3246 - same order as the Hub75ShiftDriver enum). Most panels
    // work with Generic; pick another if colors look wrong/ghosted on first boot.
    void setHub75ShiftDriver(uint8_t driver) { _hub75ShiftDriver = driver; saveSettings(); applySettings(); }
    uint8_t getHub75ShiftDriver() const { return _hub75ShiftDriver; }

    // Segment State Control
    void setPower(uint8_t segId, bool on);
    void setBrightness(uint8_t segId, uint8_t bri);
    void setEffect(uint8_t segId, uint8_t effectId);
    void setSpeed(uint8_t segId, uint8_t speed);
    void setColor(uint8_t segId, uint32_t color);
    void setWhiteOnly(uint8_t segId, bool whiteOnly);
    void setCct(uint8_t segId, uint8_t cct);
    void setPalette(uint8_t segId, uint8_t palette);
    uint8_t getPalette(uint8_t segId) const;
    void setIntensity(uint8_t segId, uint8_t intensity);
    uint8_t getIntensity(uint8_t segId) const;
    void setColor2(uint8_t segId, uint32_t color2);
    uint32_t getColor2(uint8_t segId) const;
    void setColor2Enabled(uint8_t segId, bool enabled);
    bool getColor2Enabled(uint8_t segId) const;
    // "Uhr / Text" widget list (see TextWidget above). setTextWidgets replaces
    // the whole list for a segment (widgets keep their `id` across edits, e.g.
    // dragging a position, so their image files - if any - stay associated).
    // A widget with id == 0 in the incoming JSON is treated as new and gets
    // the next free id assigned.
    void setTextWidgets(uint8_t segId, JsonArray widgets);
    void getTextWidgetsJson(uint8_t segId, JsonArray array) const;
    void setTextWidgetImage(uint8_t segId, uint8_t widgetId, uint8_t w, uint8_t h, const std::vector<uint8_t>& rgbData);
    uint8_t getNextWidgetId() const;
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
    void setSegmentPixelColor(Segment& seg, uint16_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0);
    bool isMatrix() const { return _isMatrix; }
    uint16_t getMatrixWidth() const { return _matrixWidth; }
    uint16_t getMatrixHeight() const { return _matrixHeight; }
    uint8_t getMatrixLayout() const { return _matrixLayout; }

    // Multi-Panel Canvas: lets 2D effects (Plasma, Fire 2D, Ripple, Pacifica, Matrix
    // Rain) flow seamlessly across the Master's own matrix AND any number of Slave
    // panels arranged in a shared coordinate space, instead of being confined to a
    // single device's grid like WLED's per-device 2D effects are. With no extra
    // panels configured this collapses to exactly the old Master-only matrix.
    void setCanvasPanels(JsonArray panels);
    void getCanvasPanelsJson(JsonArray array) const;
    // Draws one frame of a segment through the shared EffectEngine. effectOverride renders a
    // different effect than the segment's own, which is how 2D effects reach their 1D fallback
    // when no matrix is configured.
    void renderWithEngine(Segment& seg, uint8_t ablCap, uint8_t effectOverride = 255);

    uint16_t getCanvasWidth() const;
    uint16_t getCanvasHeight() const;
    void setCanvasPixelColor(uint16_t cx, uint16_t cy, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0);

    // Live preview: a shadow copy of the Master's own matrix pixels (updated by
    // every setPixelColorXY/setCanvasPixelColor call, whichever effect drives
    // them), so the WebUI can poll and render what's currently on the panel
    // without needing pixel readback support from the underlying LED bus driver.
    void getMatrixPreviewJson(JsonArray array) const;

    // Pushes whatever is currently in the bus buffer to the physical LEDs right away,
    // bypassing the normal effect-tick timer. Used by /api/matrix so a streamed
    // still image is visible immediately instead of waiting for the next effect tick.
    void showNow();

    bool getAblEnabled() const { return _ablEnabled; }
    uint16_t getAblMaxmA() const { return _ablMaxmA; }

private:
    IBus* _bus = nullptr;

    // Config
    uint8_t _pins[5];
    uint16_t _count;
    uint16_t _numIcs;
    uint8_t _type;
    uint8_t _ledsPerIC;
    bool _ablEnabled;
    uint16_t _ablMaxmA;
    uint8_t _hub75ShiftDriver = 0; // 0 = Hub75ShiftDriver::GENERIC

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

    // Live preview shadow buffer (RGB triplets, Master's own matrix only - see
    // getMatrixPreviewJson above). Lazily (re)sized on write.
    std::vector<uint8_t> _matrixPreviewBuf;
    void updateMatrixPreview(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b);

    // Additional Slave panels making up the virtual canvas (Master's own matrix is
    // always implicit panel 0 and is not stored in this list).
    std::vector<CanvasPanel> _canvasPanels;
    // Maps a canvas coordinate to an absolute index in the shared LED buffer (the
    // same index space used by Segment::start/stop for local + Slave pixels).
    // Returns 65535 if no panel covers that point.
    uint16_t getCanvasPixelXY(uint16_t cx, uint16_t cy) const;

    void loadSettings();
    void saveSettings();
    void applySettings();

    // Effects
    // "Bild" - deliberately does nothing, so a still image streamed in via
    // /api/matrix stays on screen instead of being overwritten by the next tick.
    void effectImage(Segment& seg, uint8_t ablCap);
    // "Uhr / Text" - renders a clock, date, or custom text message using the
    // built-in 5x7 bitmap font (see Font5x7.h); scrolls if it doesn't fit.
    void effectText(Segment& seg, uint8_t ablCap);

    // HUB75 showcase effects (see EFFECT_HUB75_SHOWCASE_START above).

    uint8_t getGlobalAblCap();

    // Palettes: seg.palette == 0 means "Solid" (use seg.color as-is). Any other
    // palette id returns a color interpolated from that palette's stops, sampled
    // at a position derived from the segment's animation step (so palette-driven
    // effects slowly cycle through the palette over time).
    uint32_t getPaletteColor(uint8_t paletteId, uint8_t pos) const;
    uint32_t getEffectiveColor(const Segment& seg) const;
};

extern LEDManagerClass LEDManager;

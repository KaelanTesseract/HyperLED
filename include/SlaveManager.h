/*
 * HyperLED - Open Source LED Controller
 * 
 * Copyright (c) 2026 Dennis Guse
 * 
 * Licensed under the EUPL, Version 1.2 or � as soon they will be approved by 
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
#ifndef SLAVEMANAGER_H
#define SLAVEMANAGER_H

#include <Arduino.h>
#include <vector>
#include <map>
#include "HyperBus.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "EspNowBus.h"

struct DiscoveredSlave {
    uint8_t currentId; // Usually 254 if unconfigured
    uint16_t ledCount;
    String name;
    String version;
    unsigned long lastSeen;
    bool isWireless;
    // Set from the reported firmware version - see slaveRendersLocally().
    bool rendersLocally = false;
    // What the Slave reports about its own output. Only a Slave knows this - the Master stores
    // nothing about it - so without it the UI cannot show an existing configuration and would
    // overwrite it with its defaults on the next save. 255 means the Slave did not report.
    uint8_t ledType = 255;
    uint16_t matrixWidth = 0;
    uint16_t matrixHeight = 0;
    uint8_t hub75ShiftDriver = 0;
};

class SlaveManagerClass {
public:
    void begin();
    void loop();
    
    std::vector<DiscoveredSlave> getDiscoveredSlaves();
    // matrixWidth/matrixHeight/hub75ShiftDriver only matter when type == TYPE_HUB75
    // (see HUB75_PIN_* in Config.h) - harmless to send for any other type.
    void configureSlave(uint8_t currentId, uint8_t newId, uint8_t pin, uint8_t pin2, uint16_t count, uint8_t type, const String& name, uint16_t matrixWidth = 16, uint16_t matrixHeight = 16, uint8_t hub75ShiftDriver = 0);
    // Onboard status LED of a Slave. Only sets what "everything is fine" looks like - the
    // Slave still signals problems (unconfigured, lost connection) on its own, even when
    // the LED is switched off here.
    void setSlaveStatusLed(uint8_t slaveId, bool on, uint32_t color, uint8_t brightness);
    void triggerSlaveUpdate(uint8_t slaveId, const String& ssid, const String& pass, const String& url);
    void sendLEDData(uint8_t slaveId, const uint8_t* rgbData, uint16_t length);

    // Hands effect parameters to a Slave that renders locally, instead of streaming pixels.
    // Only actually transmits when something changed or the periodic refresh is due, so this is
    // safe to call every frame.
    // windowTotal > 0 puts the Slave into sync mode: it renders the effect across windowTotal
    // pixels and shows only the slice at windowOffset, so one effect runs unbroken across the
    // Master and every Slave instead of restarting in each segment.
    void sendSegmentConfig(uint8_t slaveId, uint8_t effect, uint8_t brightness, uint8_t speed,
                           uint8_t intensity, uint8_t palette, bool isOn, uint32_t color,
                           uint32_t color2, bool color2Enabled, bool whiteOnly, uint8_t cct,
                           uint16_t effectStep, uint16_t windowOffset = 0, uint16_t windowTotal = 0);
    // Whether this Slave's firmware can render effects on its own. Older Slaves keep receiving
    // streamed pixel data, so a mixed set of firmware versions stays functional.
    bool slaveRendersLocally(uint8_t slaveId) const;
    // Pixel size of the HUB75 panel this Slave reported, if it is one. Lets a 2D effect draw into
    // the Slave's own panel instead of the shared canvas.
    bool getSlavePanelSize(uint8_t slaveId, uint16_t& w, uint16_t& h) const;
    // Counters for /api/espnow_status: how many LED packets actually went out, and how many
    // whole frames they amounted to. The ratio shows whether the delta path is doing its job -
    // a full 64x64 frame is 87 packets, a clock tick should be a handful.
    uint32_t getLedPacketsSent() const { return _ledPacketsSent; }
    uint32_t getLedFramesSent() const { return _ledFramesSent; }

    // For the ESP-NOW diagnostics on /api/espnow_status.
    EspNowBusClass* getEspBus() { return _espBus; }

    // True while a configuration sent to this Slave has not been confirmed yet. Lets the UI
    // show that a change is still in flight rather than presenting it as already applied.
    bool isConfigPending(uint8_t slaveId) const;

private:
    // A configuration that has been sent but not yet acknowledged. CMD_SET_CONFIG goes out as a
    // single ESP-NOW broadcast, which has no link-layer acknowledgement or retry - one lost frame
    // used to leave the Master believing a Slave was reconfigured while the Slave never heard it,
    // and the two then disagreed about the Slave's ID until someone noticed. So the packet is
    // repeated until the Slave's own PONG reports the new ID and pixel count back.
    struct PendingConfig {
        uint8_t addressedId;   // id the packet is sent to (the Slave's current id)
        uint8_t expectedId;    // id the Slave should report once it has applied the config
        uint16_t expectedCount;
        String name;
        std::vector<uint8_t> payload;
        uint8_t attempts;
        unsigned long lastSent;
    };
    static const uint8_t CONFIG_MAX_ATTEMPTS = 12;   // ~5s at the retry interval below
    static const unsigned long CONFIG_RETRY_MS = 400;
    std::vector<PendingConfig> _pendingConfigs;
    void retryPendingConfigs();
    void confirmPendingConfig(uint8_t senderId, uint16_t ledCount);

    HyperBusClass* _uartBus;
    EspNowBusClass* _espBus;
    std::vector<DiscoveredSlave> _discoveredSlaves;

    // One lock for everything this class owns.
    //
    // The web server runs on its own task and reaches in here directly - it copies the slave
    // list for /api/slaves, and saving a Slave's settings calls configureSlave() straight from
    // the request handler - while the main loop is adding, removing and sending to those same
    // Slaves. A vector that grows or shrinks under another task's iterator hands out pointers
    // into freed memory, and the resulting panic carries no hint of where it came from. The lock
    // is recursive because these entry points legitimately call one another.
    SemaphoreHandle_t _lock = nullptr;
    // Held only for bookkeeping, never across anything slow: the longest thing inside it is a
    // single ESP-NOW chunk.
    struct Guard {
        SemaphoreHandle_t h;
        explicit Guard(SemaphoreHandle_t s) : h(s) { if (h) xSemaphoreTakeRecursive(h, portMAX_DELAY); }
        ~Guard() { if (h) xSemaphoreGiveRecursive(h); }
    };
    unsigned long _lastPingTime = 0;
    unsigned long _pauseLedsUntil = 0;

    // Frame rate cap per Slave. The effect engine renders far faster than a Slave link can carry:
    // measured at roughly 70 frames/s, which is over 200 ESP-NOW packets/s for a single 100-pixel
    // Slave. That saturated the radio badly enough that PINGs stopped getting through - the Slave
    // declared the Master lost and rescanned - while the Master's own web interface stopped
    // responding. 30 frames/s is past the point of being visible on LEDs, and the UART link cannot
    // carry more than that anyway at 115200 baud.
    // This now caps how often a frame is examined, not how much goes on the air - the pump below
    // decides that, and it does so by time rather than by frame. 30 frames/s is past the point of
    // being visible on LEDs, and the UART link cannot carry more than that anyway at 115200 baud,
    // so looking more often than this only burns cycles on a comparison that will find nothing.
    static const unsigned long MIN_LED_FRAME_INTERVAL_MS = 33;
    std::map<uint8_t, unsigned long> _lastLedSend;

    // Pixel data for a Slave is queued here and sent a little at a time from loop(), never in
    // one burst. Sending it in one go is what cost the Slaves their connection: a 64x64 panel is
    // 20KB, which splits into 86 packets, and each one takes about 20ms to get onto the air. The
    // whole frame therefore held LEDManager.loop() for 1.8 seconds - measured - and for that time
    // the Master sent no pings, answered no HTTP and did nothing else at all. The Slaves, which
    // expect a ping several times a second, concluded the Master was gone.
    //
    // Chunks are 47 pixels: 235 bytes plus the two offset bytes stays inside the 240-byte limit.
    static const uint16_t DELTA_PIXELS_PER_CHUNK = 47;
    static const uint16_t LED_CHUNK_BYTES = DELTA_PIXELS_PER_CHUNK * HYPERBUS_LED_BYTES_PER_PIXEL;

    // How long a single pump may spend sending. One chunk can overrun this - a send that blocks
    // cannot be cut short - so the real guarantee is one chunk per pump at worst, which is two
    // orders of magnitude below the stall it replaces.
    static const unsigned long LED_TX_BUDGET_US = 4000;

    // A Slave can miss a packet, and nothing in the protocol tells us so. The cure is to resend
    // what it already has from time to time - but resending the whole frame every few seconds is
    // how a still picture kept the link saturated forever. Only a slice is refreshed per
    // interval, so the panel is covered completely every LED_REPAIR_SLICES intervals at a small
    // fraction of the airtime.
    static const unsigned long LED_REPAIR_INTERVAL_MS = 2000;
    static const uint8_t LED_REPAIR_SLICES = 8;

    struct LedTx {
        std::vector<uint8_t> data;      // what the Slave should be showing
        std::vector<uint8_t> dirty;     // one bit per chunk: not known to be on the Slave yet
        uint16_t chunks = 0;
        uint16_t cursor = 0;            // next chunk to look at, so no region starves
        uint8_t repairSlice = 0;
        unsigned long lastRepair = 0;
    };
    std::map<uint8_t, LedTx> _ledTx;

    uint32_t _ledPacketsSent = 0;
    uint32_t _ledFramesSent = 0;

    // Queues a frame: works out which chunks differ from what the Slave has and marks them.
    void queueFrame(uint8_t slaveId, const uint8_t* rgbData, uint16_t length);
    // Sends marked chunks until the budget runs out. Called every loop().
    void pumpLedTx();

    // Last effect parameters sent to each Slave, so unchanged ones are not resent every frame.
    // The refresh interval exists so a Slave that rebooted picks its effect back up on its own
    // rather than sitting dark until the user touches a control.
    struct SentSegment {
        uint8_t payload[HYPERBUS_SEGMENT_PAYLOAD_LEN];
        unsigned long lastSent;
        bool valid = false;
    };
    static const unsigned long SEGMENT_REFRESH_MS = 2000;
    std::map<uint8_t, SentSegment> _sentSegments;
    
    void handlePacket(const HyperBusPacket& packet);
    static void staticHandlePacket(const HyperBusPacket& packet);
};

extern SlaveManagerClass SlaveManager;

#endif

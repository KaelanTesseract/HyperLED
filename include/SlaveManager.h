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
    unsigned long _lastPingTime = 0;
    unsigned long _pauseLedsUntil = 0;

    // Frame rate cap per Slave. The effect engine renders far faster than a Slave link can carry:
    // measured at roughly 70 frames/s, which is over 200 ESP-NOW packets/s for a single 100-pixel
    // Slave. That saturated the radio badly enough that PINGs stopped getting through - the Slave
    // declared the Master lost and rescanned - while the Master's own web interface stopped
    // responding. 30 frames/s is past the point of being visible on LEDs, and the UART link cannot
    // carry more than that anyway at 115200 baud.
    static const unsigned long MIN_LED_FRAME_INTERVAL_MS = 33;
    // Large streamed segments need a slower rate still. A 64x64 panel is 20KB per frame, which is
    // 86 ESP-NOW packets - at 30 frames/s that is thousands of packets per second and the link
    // collapses. The interval therefore scales with the payload, aiming at roughly 90 packets/s,
    // which measurement showed the radio carries comfortably. This is what makes the clock and
    // text effects usable on a panel: they cannot be rendered on the Slave (they need the time,
    // weather and uploaded images the Master holds) so they must stream, and a clock does not
    // care about frame rate.
    static const unsigned long TARGET_PACKETS_PER_SEC = 90;
    static const uint16_t BYTES_PER_PACKET = 240;
    static unsigned long frameIntervalFor(uint16_t length) {
        unsigned long packets = (length + BYTES_PER_PACKET - 1) / BYTES_PER_PACKET;
        if (packets == 0) packets = 1;
        unsigned long interval = (packets * 1000UL) / TARGET_PACKETS_PER_SEC;
        return interval > MIN_LED_FRAME_INTERVAL_MS ? interval : MIN_LED_FRAME_INTERVAL_MS;
    }
    std::map<uint8_t, unsigned long> _lastLedSend;

    // Fingerprint of the last frame sent to each Slave. A still picture - an uploaded image, a
    // clock between ticks - was otherwise retransmitted in full forever: 86 packets per second
    // for a 64x64 panel, every one of them carrying what the Slave already had. That congestion
    // is what made an uploaded image take so long to settle, because the packets it cost could
    // only be made good by the next full frame, which arrived into the same congestion.
    // An unchanged frame is now repeated only occasionally, which both frees the link and
    // repairs anything lost earlier.
    struct SentFrame {
        uint32_t hash = 0;
        unsigned long lastSent = 0;
        bool valid = false;
    };
    static const unsigned long LED_REFRESH_MS = 5000;
    std::map<uint8_t, SentFrame> _lastLedFrame;
    static uint32_t frameHash(const uint8_t* data, uint16_t length);

    // The last frame actually sent to each Slave, kept so only the parts that changed have to go
    // out again. A clock on a 64x64 panel alters a few dozen pixels out of 4096, yet the whole
    // 20KB frame was retransmitted for it - 86 packets where four would do. Sending only the
    // changed pieces is what keeps a still image stable: the link stays free, so the packets
    // carrying it stop being lost in the first place.
    // Chunks are 47 pixels: 235 bytes plus the two offset bytes stays inside the 240-byte limit.
    static const uint16_t DELTA_PIXELS_PER_CHUNK = 47;
    std::map<uint8_t, std::vector<uint8_t>> _lastFrameData;
    uint32_t _ledPacketsSent = 0;
    uint32_t _ledFramesSent = 0;
    void sendChangedChunks(uint8_t slaveId, const uint8_t* rgbData, uint16_t length,
                           std::vector<uint8_t>& prev);

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

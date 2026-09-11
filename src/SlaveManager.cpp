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
#include "SlaveManager.h"
#include "Config.h"
#include <ArduinoJson.h>
#include <algorithm>

SlaveManagerClass SlaveManager;

void SlaveManagerClass::staticHandlePacket(const HyperBusPacket& packet) {
    SlaveManager.handlePacket(packet);
}

void SlaveManagerClass::begin() {
    _uartBus = new HyperBusClass(Serial1);
    _uartBus->begin(115200, HYPERBUS_UART_RX, HYPERBUS_UART_TX);
    _uartBus->setCallback(staticHandlePacket);
    
    _espBus = new EspNowBusClass();
    // Master doesn't hop, it stays on the channel the router assigned it
    _espBus->begin(WIFI_STA, false);
    _espBus->setCallback(staticHandlePacket);
}

void SlaveManagerClass::loop() {
    _uartBus->loop();
    _espBus->loop();
    
    unsigned long now = millis();
    
    static unsigned long lastValidUartPkt = millis();
    bool hasUartSlave = false;
    for (const auto& s : _discoveredSlaves) {
        if (!s.isWireless) {
            hasUartSlave = true;
            lastValidUartPkt = millis();
            break;
        }
    }
    
    if (hasUartSlave && millis() - lastValidUartPkt > 5000) {
        Serial.println("Master UART Hardware Lockup suspected! Restarting peripheral...");
        _uartBus->begin(115200, HYPERBUS_UART_RX, HYPERBUS_UART_TX);
        pinMode(HYPERBUS_UART_RX, INPUT_PULLUP);
        lastValidUartPkt = millis();
    }
    
    retryPendingConfigs();

    // PING frequently so slaves scanning channels can find us quickly
    if (now - _lastPingTime > 250) {
        _lastPingTime = now;
        _uartBus->sendPacket(HYPERBUS_BROADCAST_ID, HYPERBUS_MASTER_ID, CMD_PING, nullptr, 0);

        // The wireless PING carries the channel we are actually on. A scanning Slave cannot
        // derive it from reception alone: 2.4GHz channels overlap, so our PING is still picked
        // up while the Slave listens one or two channels off. It then locks onto that wrong
        // channel and its PONG - sent from there - never reaches us, so it receives everything
        // but stays invisible. Telling it the number outright removes the guesswork.
        uint8_t pingPayload[1] = {0};
        wifi_second_chan_t second;
        uint8_t primary = 0;
        if (esp_wifi_get_channel(&primary, &second) == ESP_OK) {
            pingPayload[0] = primary;
        }

        // Broadcast PING for discovery
        _espBus->sendPacket(HYPERBUS_BROADCAST_ID, HYPERBUS_MASTER_ID, CMD_PING, pingPayload, 1);

        // Unicast PING for keep-alive (since Broadcasts drop in Power Save mode)
        for (const auto& s : _discoveredSlaves) {
            if (s.isWireless) {
                _espBus->sendPacket(s.currentId, HYPERBUS_MASTER_ID, CMD_PING, pingPayload, 1);
            }
        }

        // Cleanup old slaves every few seconds
        static unsigned long lastCleanup = 0;
        if (now - lastCleanup > 15000) {
            lastCleanup = now;
            for (auto it = _discoveredSlaves.begin(); it != _discoveredSlaves.end(); ) {
                if (now - it->lastSeen > 15000) {
                    it = _discoveredSlaves.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
}

static bool versionRendersLocally(const String& version);
static bool versionReportsConfig(const String& version);

// A corrupted/garbled PONG (e.g. from a protocol-version mismatch or a noisy wire) can contain raw
// control bytes. ArduinoJson does not escape those, which produces invalid JSON on /api/slaves and
// breaks the whole Slaves page in the browser - so replace anything non-printable before it's stored.
static void sanitizeForJson(String& s) {
    for (size_t i = 0; i < s.length(); i++) {
        uint8_t c = (uint8_t)s[i];
        if (c < 0x20 || c == 0x7F) {
            s.setCharAt(i, '?');
        }
    }
}

void SlaveManagerClass::handlePacket(const HyperBusPacket& packet) {
    if (packet.command == CMD_PONG) {
        // PONG Payload: [LED Count L] [LED Count H] [Version Length] [Version String...] [Name...]
        if (packet.length >= 3) {
            uint16_t count = packet.payload[0] | (packet.payload[1] << 8);
            uint8_t verLen = packet.payload[2];
            
            String sVersion = "Unknown";
            if (verLen > 0 && packet.length >= 3 + verLen) {
                char verBuf[16] = {0};
                memcpy(verBuf, &packet.payload[3], min((int)verLen, 15));
                sVersion = String(verBuf);
                sanitizeForJson(sVersion);
            }

            // Slaves from 0.2.1 on report their own output configuration before the name.
            // Older ones go straight to the name, so the version decides how to read the rest.
            bool hasConfig = versionReportsConfig(sVersion);
            uint16_t nameAt = 3 + verLen;
            uint8_t sType = 255;
            uint16_t sMatW = 0, sMatH = 0;
            uint8_t sShift = 0;
            if (hasConfig && packet.length >= nameAt + 6) {
                sType  = packet.payload[nameAt];
                sMatW  = packet.payload[nameAt + 1] | (packet.payload[nameAt + 2] << 8);
                sMatH  = packet.payload[nameAt + 3] | (packet.payload[nameAt + 4] << 8);
                sShift = packet.payload[nameAt + 5];
                nameAt += 6;
            }

            String sName = "Unknown";
            if (packet.length > nameAt) {
                char nameBuf[64] = {0};
                int nameLen = min((int)(packet.length - nameAt), 63);
                memcpy(nameBuf, &packet.payload[nameAt], nameLen);
                sName = String(nameBuf);
                sanitizeForJson(sName);
            }
            
            confirmPendingConfig(packet.senderId, count);

            bool found = false;
            for (auto& s : _discoveredSlaves) {
                if (s.currentId == packet.senderId) {
                    s.ledCount = count;
                    s.name = sName;
                    s.version = sVersion;
                    s.rendersLocally = versionRendersLocally(sVersion);
                    if (sType != 255) {
                        s.ledType = sType;
                        s.matrixWidth = sMatW;
                        s.matrixHeight = sMatH;
                        s.hub75ShiftDriver = sShift;
                    }
                    s.lastSeen = millis();
                    s.isWireless = packet.isWireless;
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                DiscoveredSlave ds;
                ds.currentId = packet.senderId;
                ds.ledCount = count;
                ds.name = sName;
                ds.version = sVersion;
                ds.rendersLocally = versionRendersLocally(sVersion);
                ds.ledType = sType;
                ds.matrixWidth = sMatW;
                ds.matrixHeight = sMatH;
                ds.hub75ShiftDriver = sShift;
                ds.lastSeen = millis();
                ds.isWireless = packet.isWireless;
                _discoveredSlaves.push_back(ds);
            }
        }
    }
}

std::vector<DiscoveredSlave> SlaveManagerClass::getDiscoveredSlaves() {
    std::vector<DiscoveredSlave> sorted = _discoveredSlaves;
    std::sort(sorted.begin(), sorted.end(), [](const DiscoveredSlave& a, const DiscoveredSlave& b) {
        return a.currentId < b.currentId;
    });
    return sorted;
}

// Helper to determine the correct bus for a slave ID
BusInterface* getBusForSlave(uint8_t slaveId, std::vector<DiscoveredSlave>& discovered, BusInterface* uartBus, BusInterface* espBus) {
    if (slaveId == HYPERBUS_BROADCAST_ID) return nullptr; // Special case
    
    for (const auto& s : discovered) {
        if (s.currentId == slaveId) {
            return s.isWireless ? espBus : uartBus;
        }
    }
    // Default to UART if unknown
    return uartBus;
}

void SlaveManagerClass::configureSlave(uint8_t currentId, uint8_t newId, uint8_t pin, uint8_t pin2, uint16_t count, uint8_t type, const String& name, uint16_t matrixWidth, uint16_t matrixHeight, uint8_t hub75ShiftDriver) {
    uint16_t len = 11 + name.length();
    uint8_t* payload = (uint8_t*)malloc(len);
    payload[0] = newId;
    payload[1] = pin;
    payload[2] = pin2;
    payload[3] = type;
    payload[4] = count & 0xFF;
    payload[5] = (count >> 8) & 0xFF;
    payload[6] = matrixWidth & 0xFF;
    payload[7] = (matrixWidth >> 8) & 0xFF;
    payload[8] = matrixHeight & 0xFF;
    payload[9] = (matrixHeight >> 8) & 0xFF;
    payload[10] = hub75ShiftDriver;
    memcpy(&payload[11], name.c_str(), name.length());

    BusInterface* targetBus = getBusForSlave(currentId, _discoveredSlaves, _uartBus, _espBus);
    targetBus->sendPacket(currentId, HYPERBUS_MASTER_ID, CMD_SET_CONFIG, payload, len);

    // Remember it until the Slave confirms. The discovery list is deliberately NOT updated to the
    // new ID here: doing that on send alone is what made a lost packet invisible, since the Master
    // then looked as if the change had taken effect. confirmPendingConfig() updates it once the
    // Slave reports the new ID and pixel count in its PONG.
    PendingConfig pending;
    pending.addressedId = currentId;
    pending.expectedId = newId;
    pending.expectedCount = count;
    pending.name = name;
    pending.payload.assign(payload, payload + len);
    pending.attempts = 1;
    pending.lastSent = millis();

    // Replace any earlier unconfirmed config for the same Slave - only the newest one matters.
    for (auto it = _pendingConfigs.begin(); it != _pendingConfigs.end(); ) {
        if (it->addressedId == currentId || it->expectedId == newId) it = _pendingConfigs.erase(it);
        else ++it;
    }
    _pendingConfigs.push_back(pending);

    free(payload);
}

bool SlaveManagerClass::isConfigPending(uint8_t slaveId) const {
    for (const auto& p : _pendingConfigs) {
        if (p.addressedId == slaveId || p.expectedId == slaveId) return true;
    }
    return false;
}

void SlaveManagerClass::retryPendingConfigs() {
    unsigned long now = millis();
    for (auto it = _pendingConfigs.begin(); it != _pendingConfigs.end(); ) {
        if (now - it->lastSent < CONFIG_RETRY_MS) { ++it; continue; }

        if (it->attempts >= CONFIG_MAX_ATTEMPTS) {
            Serial.printf("SlaveManager: slave %u never confirmed its new configuration after %u attempts - giving up\n",
                          it->addressedId, it->attempts);
            it = _pendingConfigs.erase(it);
            continue;
        }

        BusInterface* targetBus = getBusForSlave(it->addressedId, _discoveredSlaves, _uartBus, _espBus);
        targetBus->sendPacket(it->addressedId, HYPERBUS_MASTER_ID, CMD_SET_CONFIG,
                              it->payload.data(), (uint16_t)it->payload.size());
        it->attempts++;
        it->lastSent = now;
        ++it;
    }
}

void SlaveManagerClass::confirmPendingConfig(uint8_t senderId, uint16_t ledCount) {
    for (auto it = _pendingConfigs.begin(); it != _pendingConfigs.end(); ++it) {
        // The pixel count is checked as well as the ID: a Slave keeping its ID across a
        // reconfiguration would otherwise confirm the change with a PONG it had already sent
        // before receiving it.
        if (senderId != it->expectedId || ledCount != it->expectedCount) continue;

        for (auto& s : _discoveredSlaves) {
            if (s.currentId == it->addressedId) {
                s.currentId = it->expectedId;
                s.name = it->name;
                break;
            }
        }
        Serial.printf("SlaveManager: slave %u confirmed its configuration after %u attempt(s)\n",
                      senderId, it->attempts);
        _pendingConfigs.erase(it);
        return;
    }
}

void SlaveManagerClass::setSlaveStatusLed(uint8_t slaveId, bool on, uint32_t color, uint8_t brightness) {
    uint8_t payload[5];
    payload[0] = on ? 1 : 0;
    payload[1] = (color >> 16) & 0xFF;
    payload[2] = (color >> 8) & 0xFF;
    payload[3] = color & 0xFF;
    payload[4] = brightness;

    if (slaveId == HYPERBUS_BROADCAST_ID) {
        _uartBus->sendPacket(HYPERBUS_BROADCAST_ID, HYPERBUS_MASTER_ID, CMD_SET_STATUS_LED, payload, 5);
        // Broadcasts are dropped in Power Save mode, so wireless Slaves get a unicast each.
        for (const auto& s : _discoveredSlaves) {
            if (s.isWireless) {
                _espBus->sendPacket(s.currentId, HYPERBUS_MASTER_ID, CMD_SET_STATUS_LED, payload, 5);
            }
        }
        return;
    }

    BusInterface* targetBus = getBusForSlave(slaveId, _discoveredSlaves, _uartBus, _espBus);
    targetBus->sendPacket(slaveId, HYPERBUS_MASTER_ID, CMD_SET_STATUS_LED, payload, 5);
}

void SlaveManagerClass::triggerSlaveUpdate(uint8_t slaveId, const String& ssid, const String& pass, const String& url) {
    JsonDocument doc;
    doc["ssid"] = ssid;
    doc["pass"] = pass;
    doc["url"] = url;
    String json;
    serializeJson(doc, json);
    
    _pauseLedsUntil = millis() + 45000; // Pause LED transmission for 45 seconds to allow Wi-Fi OTA to complete
    
    if (slaveId == HYPERBUS_BROADCAST_ID) {
        // Send via UART Broadcast (reliable over wire, but we send 3 times just in case of CRC errors on long cables)
        for (int i = 0; i < 3; i++) {
            _uartBus->sendPacket(HYPERBUS_BROADCAST_ID, HYPERBUS_MASTER_ID, CMD_TRIGGER_UPDATE, (const uint8_t*)json.c_str(), json.length());
            delay(50);
        }
        
        // Send via ESP-NOW Unicast to EACH wireless slave (Broadcasts are dropped in Power Save mode)
        for (const auto& s : _discoveredSlaves) {
            if (s.isWireless) {
                _espBus->sendPacket(s.currentId, HYPERBUS_MASTER_ID, CMD_TRIGGER_UPDATE, (const uint8_t*)json.c_str(), json.length());
            }
        }
    } else {
        BusInterface* targetBus = getBusForSlave(slaveId, _discoveredSlaves, _uartBus, _espBus);
        targetBus->sendPacket(slaveId, HYPERBUS_MASTER_ID, CMD_TRIGGER_UPDATE, (const uint8_t*)json.c_str(), json.length());
    }
}

// Local rendering arrived in Slave firmware 0.2.0. Anything older only understands streamed
// pixels, and must keep getting them - a mixed set of firmware versions has to stay functional.
static bool versionRendersLocally(const String& version) {
    int firstDot = version.indexOf('.');
    if (firstDot < 0) return false;
    int secondDot = version.indexOf('.', firstDot + 1);
    long major = version.substring(0, firstDot).toInt();
    long minor = (secondDot > firstDot) ? version.substring(firstDot + 1, secondDot).toInt()
                                        : version.substring(firstDot + 1).toInt();
    return (major > 0) || (major == 0 && minor >= 2);
}

// Reporting the output configuration in the PONG arrived in Slave firmware 0.2.1.
static bool versionReportsConfig(const String& version) {
    int firstDot = version.indexOf('.');
    if (firstDot < 0) return false;
    int secondDot = version.indexOf('.', firstDot + 1);
    if (secondDot < 0) return false;
    long major = version.substring(0, firstDot).toInt();
    long minor = version.substring(firstDot + 1, secondDot).toInt();
    long patch = version.substring(secondDot + 1).toInt();
    if (major > 0) return true;
    if (minor > 2) return true;
    return (minor == 2 && patch >= 1);
}

bool SlaveManagerClass::slaveRendersLocally(uint8_t slaveId) const {
    for (const auto& s : _discoveredSlaves) {
        if (s.currentId == slaveId) return s.rendersLocally;
    }
    return false;
}

bool SlaveManagerClass::getSlavePanelSize(uint8_t slaveId, uint16_t& w, uint16_t& h) const {
    for (const auto& s : _discoveredSlaves) {
        if (s.currentId != slaveId) continue;
        if (s.ledType != TYPE_HUB75) return false;
        w = s.matrixWidth;
        h = s.matrixHeight;
        return w > 0 && h > 0;
    }
    return false;
}

void SlaveManagerClass::sendSegmentConfig(uint8_t slaveId, uint8_t effect, uint8_t brightness,
                                          uint8_t speed, uint8_t intensity, uint8_t palette,
                                          bool isOn, uint32_t color, uint32_t color2,
                                          bool color2Enabled, bool whiteOnly, uint8_t cct,
                                          uint16_t effectStep, uint16_t windowOffset,
                                          uint16_t windowTotal) {
    if (millis() < _pauseLedsUntil) return;

    uint8_t payload[HYPERBUS_SEGMENT_PAYLOAD_LEN] = {0};
    payload[0] = effect;
    payload[1] = brightness;
    payload[2] = speed;
    payload[3] = intensity;
    payload[4] = palette;
    payload[5] = (isOn ? 0x01 : 0) | (color2Enabled ? 0x02 : 0) | (whiteOnly ? 0x04 : 0);
    payload[6] = (color >> 16) & 0xFF;
    payload[7] = (color >> 8) & 0xFF;
    payload[8] = color & 0xFF;
    payload[9] = (color2 >> 16) & 0xFF;
    payload[10] = (color2 >> 8) & 0xFF;
    payload[11] = color2 & 0xFF;
    payload[12] = cct;
    payload[13] = effectStep & 0xFF;
    payload[14] = (effectStep >> 8) & 0xFF;
    payload[15] = windowOffset & 0xFF;
    payload[16] = (windowOffset >> 8) & 0xFF;
    payload[17] = windowTotal & 0xFF;
    payload[18] = (windowTotal >> 8) & 0xFF;

    SentSegment& sent = _sentSegments[slaveId];
    unsigned long now = millis();
    // The step counter is deliberately excluded from the comparison: it changes every frame, so
    // including it would defeat the whole point and resend the parameters continuously.
    bool changed = !sent.valid || memcmp(sent.payload, payload, 13) != 0
                   || memcmp(&sent.payload[15], &payload[15], 4) != 0;
    // In sync mode the step counter has to travel every frame, otherwise the Master and the Slave
    // advance the same effect on their own clocks and the pattern tears at the segment boundary -
    // exactly what sync is meant to prevent. It is 19 bytes, so the traffic is negligible.
    if (windowTotal == 0 && !changed && now - sent.lastSent < SEGMENT_REFRESH_MS) return;

    memcpy(sent.payload, payload, HYPERBUS_SEGMENT_PAYLOAD_LEN);
    sent.lastSent = now;
    sent.valid = true;

    BusInterface* targetBus = getBusForSlave(slaveId, _discoveredSlaves, _uartBus, _espBus);
    targetBus->sendPacket(slaveId, HYPERBUS_MASTER_ID, CMD_SET_SEGMENT, payload,
                          HYPERBUS_SEGMENT_PAYLOAD_LEN);
}

void SlaveManagerClass::sendLEDData(uint8_t slaveId, const uint8_t* rgbData, uint16_t length) {
    if (millis() < _pauseLedsUntil) return;

    // Drop frames the link cannot carry (see MIN_LED_FRAME_INTERVAL_MS). Skipping a frame is
    // invisible; flooding the transport is not, because it starves the discovery traffic that
    // shares it.
    unsigned long now = millis();
    auto lastSent = _lastLedSend.find(slaveId);
    if (lastSent != _lastLedSend.end() && now - lastSent->second < frameIntervalFor(length)) return;
    _lastLedSend[slaveId] = now;

    if (slaveId == HYPERBUS_BROADCAST_ID) {
        _uartBus->sendPacket(HYPERBUS_BROADCAST_ID, HYPERBUS_MASTER_ID, CMD_SET_LEDS, rgbData, length);
        _espBus->sendPacket(HYPERBUS_BROADCAST_ID, HYPERBUS_MASTER_ID, CMD_SET_LEDS, rgbData, length);
    } else {
        BusInterface* targetBus = getBusForSlave(slaveId, _discoveredSlaves, _uartBus, _espBus);
        targetBus->sendPacket(slaveId, HYPERBUS_MASTER_ID, CMD_SET_LEDS, rgbData, length);
    }
}











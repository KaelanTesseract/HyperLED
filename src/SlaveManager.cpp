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
    
    // PING frequently so slaves scanning channels can find us quickly
    if (now - _lastPingTime > 250) {
        _lastPingTime = now;
        _uartBus->sendPacket(HYPERBUS_BROADCAST_ID, HYPERBUS_MASTER_ID, CMD_PING, nullptr, 0);

        // Broadcast PING for discovery
        _espBus->sendPacket(HYPERBUS_BROADCAST_ID, HYPERBUS_MASTER_ID, CMD_PING, nullptr, 0);

        // Unicast PING for keep-alive (since Broadcasts drop in Power Save mode)
        for (const auto& s : _discoveredSlaves) {
            if (s.isWireless) {
                _espBus->sendPacket(s.currentId, HYPERBUS_MASTER_ID, CMD_PING, nullptr, 0);
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

            String sName = "Unknown";
            if (packet.length > 3 + verLen) {
                char nameBuf[64] = {0};
                int nameLen = min((int)(packet.length - 3 - verLen), 63);
                memcpy(nameBuf, &packet.payload[3 + verLen], nameLen);
                sName = String(nameBuf);
                sanitizeForJson(sName);
            }
            
            bool found = false;
            for (auto& s : _discoveredSlaves) {
                if (s.currentId == packet.senderId) {
                    s.ledCount = count;
                    s.name = sName;
                    s.version = sVersion;
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
    free(payload);
    
    // Update ID in the discovery list instead of erasing, so we remember if it's wireless!
    for (auto& s : _discoveredSlaves) {
        if (s.currentId == currentId) {
            s.currentId = newId;
            s.name = name;
            break;
        }
    }
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

void SlaveManagerClass::sendLEDData(uint8_t slaveId, const uint8_t* rgbData, uint16_t length) {
    if (millis() < _pauseLedsUntil) return;
    
    if (slaveId == HYPERBUS_BROADCAST_ID) {
        _uartBus->sendPacket(HYPERBUS_BROADCAST_ID, HYPERBUS_MASTER_ID, CMD_SET_LEDS, rgbData, length);
        _espBus->sendPacket(HYPERBUS_BROADCAST_ID, HYPERBUS_MASTER_ID, CMD_SET_LEDS, rgbData, length);
    } else {
        BusInterface* targetBus = getBusForSlave(slaveId, _discoveredSlaves, _uartBus, _espBus);
        targetBus->sendPacket(slaveId, HYPERBUS_MASTER_ID, CMD_SET_LEDS, rgbData, length);
    }
}











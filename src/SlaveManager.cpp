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
#include "LEDManager.h"
#include "WeatherManager.h"
#include "WidgetRender.h"
#include <sys/time.h>
#include <ArduinoJson.h>
#include <algorithm>

SlaveManagerClass SlaveManager;

void SlaveManagerClass::staticHandlePacket(const HyperBusPacket& packet) {
    SlaveManager.handlePacket(packet);
}

void SlaveManagerClass::begin() {
    if (_lock == nullptr) _lock = xSemaphoreCreateRecursiveMutex();
    _uartBus = new HyperBusClass(Serial1);
    _uartBus->begin(115200, HYPERBUS_UART_RX, HYPERBUS_UART_TX);
    _uartBus->setCallback(staticHandlePacket);
    
    _espBus = new EspNowBusClass();
    // Master doesn't hop, it stays on the channel the router assigned it
    _espBus->begin(WIFI_STA, false);
    _espBus->setCallback(staticHandlePacket);
}

void SlaveManagerClass::loop() {
    Guard guard(_lock);
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

    // Hand over a little of the queued pixel data. Doing it here rather than where the frame is
    // produced is the whole point: the loop keeps turning between chunks, so pings still go out
    // and the web interface still answers while a panel is being filled.
    pumpLedTx();
    pumpImageTransfers();

    if (anySlaveRendersAllWidgets()) {
        if (_clockBroadcastDue || now - _lastClockBroadcast >= CLOCK_BROADCAST_MS) {
            _clockBroadcastDue = false;
            broadcastClock();
        }
        broadcastWeather(false);
    }

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
static bool versionRendersWidgets(const String& version);
static bool versionRendersAllWidgets(const String& version);

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
    Guard guard(_lock);
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
                    s.rendersWidgets = versionRendersWidgets(sVersion);
                    s.rendersAllWidgets = versionRendersAllWidgets(sVersion);
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
            
            SlaveCapability& caps = _slaveCaps[packet.senderId];
            caps.rendersLocally = versionRendersLocally(sVersion);
            caps.rendersWidgets = versionRendersWidgets(sVersion);
            bool drawsAll = versionRendersAllWidgets(sVersion);
            // A Slave that draws the clock should not wait up to ten seconds to learn the time.
            if (drawsAll && (!found || !caps.rendersAllWidgets)) _clockBroadcastDue = true;
            caps.rendersAllWidgets = drawsAll;
            caps.isWireless = packet.isWireless;

            if (!found) {
                DiscoveredSlave ds;
                ds.currentId = packet.senderId;
                ds.ledCount = count;
                ds.name = sName;
                ds.version = sVersion;
                ds.rendersLocally = versionRendersLocally(sVersion);
                ds.rendersWidgets = versionRendersWidgets(sVersion);
                ds.rendersAllWidgets = drawsAll;
                ds.ledType = sType;
                ds.matrixWidth = sMatW;
                ds.matrixHeight = sMatH;
                ds.hub75ShiftDriver = sShift;
                ds.lastSeen = millis();
                ds.isWireless = packet.isWireless;
                _discoveredSlaves.push_back(ds);
            }
        }
    } else if (packet.command == CMD_REQUEST_WIDGET_IMAGE) {
        handleImageRequest(packet.senderId, packet.payload, packet.length);
    }
}

std::vector<DiscoveredSlave> SlaveManagerClass::getDiscoveredSlaves() {
    Guard guard(_lock);
    std::vector<DiscoveredSlave> sorted = _discoveredSlaves;
    std::sort(sorted.begin(), sorted.end(), [](const DiscoveredSlave& a, const DiscoveredSlave& b) {
        return a.currentId < b.currentId;
    });
    return sorted;
}

// Which bus to talk to a Slave on: the transport it last answered on, remembered even after it
// dropped out of the discovery list. A Slave nobody has ever heard from gets nullptr unless the
// caller allows the UART fallback, which only the small control packets do - see _slaveCaps.
BusInterface* SlaveManagerClass::busFor(uint8_t slaveId, bool fallbackToUart) {
    if (slaveId == HYPERBUS_BROADCAST_ID) return nullptr; // Special case

    for (const auto& s : _discoveredSlaves) {
        if (s.currentId == slaveId) {
            return s.isWireless ? (BusInterface*)_espBus : (BusInterface*)_uartBus;
        }
    }
    auto known = _slaveCaps.find(slaveId);
    if (known != _slaveCaps.end()) {
        return known->second.isWireless ? (BusInterface*)_espBus : (BusInterface*)_uartBus;
    }
    return fallbackToUart ? (BusInterface*)_uartBus : nullptr;
}

void SlaveManagerClass::configureSlave(uint8_t currentId, uint8_t newId, uint8_t pin, uint8_t pin2, uint16_t count, uint8_t type, const String& name, uint16_t matrixWidth, uint16_t matrixHeight, uint8_t hub75ShiftDriver) {
    Guard guard(_lock);
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

    BusInterface* targetBus = busFor(currentId);
    if (targetBus) targetBus->sendPacket(currentId, HYPERBUS_MASTER_ID, CMD_SET_CONFIG, payload, len);

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
    Guard guard(_lock);
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

        BusInterface* targetBus = busFor(it->addressedId);
        if (targetBus) {
            targetBus->sendPacket(it->addressedId, HYPERBUS_MASTER_ID, CMD_SET_CONFIG,
                                  it->payload.data(), (uint16_t)it->payload.size());
        }
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
    Guard guard(_lock);
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

    BusInterface* targetBus = busFor(slaveId);
    if (targetBus) targetBus->sendPacket(slaveId, HYPERBUS_MASTER_ID, CMD_SET_STATUS_LED, payload, 5);
}

void SlaveManagerClass::triggerSlaveUpdate(uint8_t slaveId, const String& ssid, const String& pass, const String& url) {
    Guard guard(_lock);
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
        BusInterface* targetBus = busFor(slaveId);
        if (targetBus) targetBus->sendPacket(slaveId, HYPERBUS_MASTER_ID, CMD_TRIGGER_UPDATE, (const uint8_t*)json.c_str(), json.length());
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

// CMD_SET_WIDGETS (local "Uhr / Text" rendering for the custom-text and Lauftext widget types)
// arrived in Slave firmware 0.2.2. An older Slave silently ignores that unknown command - it has
// no fallback of its own - so the Master must know not to rely on it and keep streaming pixels
// for that segment instead, exactly as it already does for a Slave too old for CMD_SET_SEGMENT.
static bool versionRendersWidgets(const String& version) {
    int firstDot = version.indexOf('.');
    if (firstDot < 0) return false;
    int secondDot = version.indexOf('.', firstDot + 1);
    if (secondDot < 0) return false;
    long major = version.substring(0, firstDot).toInt();
    long minor = version.substring(firstDot + 1, secondDot).toInt();
    long patch = version.substring(secondDot + 1).toInt();
    if (major > 0) return true;
    if (minor > 2) return true;
    return (minor == 2 && patch >= 2);
}

static bool versionRendersAllWidgets(const String& version) {
    int firstDot = version.indexOf('.');
    if (firstDot < 0) return false;
    int secondDot = version.indexOf('.', firstDot + 1);
    if (secondDot < 0) return false;
    long major = version.substring(0, firstDot).toInt();
    long minor = version.substring(firstDot + 1, secondDot).toInt();
    long patch = version.substring(secondDot + 1).toInt();
    if (major > 0) return true;
    if (minor > 2) return true;
    return (minor == 2 && patch >= 4);
}

bool SlaveManagerClass::slaveRendersAllWidgets(uint8_t slaveId) const {
    Guard guard(_lock);
    auto known = _slaveCaps.find(slaveId);
    return known != _slaveCaps.end() && known->second.rendersAllWidgets;
}

bool SlaveManagerClass::anySlaveRendersAllWidgets() const {
    Guard guard(_lock);
    for (const auto& s : _discoveredSlaves) {
        if (s.rendersAllWidgets) return true;
    }
    return false;
}

void SlaveManagerClass::broadcastClock() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    time_t now = tv.tv_sec;
    struct tm local;
    localtime_r(&now, &local);
    uint32_t epoch = WidgetRender::localEpoch(local);
    uint16_t ms = (uint16_t)(tv.tv_usec / 1000);

    uint8_t payload[HYPERBUS_TIME_PAYLOAD_LEN];
    // Anything before 2020 means NTP has not answered yet. The time goes out regardless - the
    // Master draws its own clock unsynced too, and the two should show the same.
    payload[0] = (now > 1577836800) ? HYPERBUS_TIME_FLAG_SYNCED : 0;
    payload[1] = (uint8_t)(epoch & 0xFF);
    payload[2] = (uint8_t)((epoch >> 8) & 0xFF);
    payload[3] = (uint8_t)((epoch >> 16) & 0xFF);
    payload[4] = (uint8_t)((epoch >> 24) & 0xFF);
    payload[5] = (uint8_t)(ms & 0xFF);
    payload[6] = (uint8_t)((ms >> 8) & 0xFF);

    Guard guard(_lock);
    _lastClockBroadcast = millis();
    _espBus->sendPacket(HYPERBUS_BROADCAST_ID, HYPERBUS_MASTER_ID, CMD_SET_TIME, payload, sizeof(payload));
    _uartBus->sendPacket(HYPERBUS_BROADCAST_ID, HYPERBUS_MASTER_ID, CMD_SET_TIME, payload, sizeof(payload));
}

void SlaveManagerClass::broadcastWeather(bool force) {
    uint8_t payload[HYPERBUS_WEATHER_PAYLOAD_LEN];
    bool valid = WeatherManager.hasData();
    int16_t temp = valid ? (int16_t)lroundf(WeatherManager.getTemperature()) : 0;
    payload[0] = valid ? HYPERBUS_WEATHER_FLAG_VALID : 0;
    payload[1] = (uint8_t)(temp & 0xFF);
    payload[2] = (uint8_t)((temp >> 8) & 0xFF);
    payload[3] = WeatherManager.getWeatherIcon();

    unsigned long now = millis();
    bool changed = memcmp(payload, _sentWeather, sizeof(payload)) != 0;
    if (!force && !changed && now - _lastWeatherBroadcast < WEATHER_REFRESH_MS) return;

    Guard guard(_lock);
    memcpy(_sentWeather, payload, sizeof(payload));
    _lastWeatherBroadcast = now;
    _espBus->sendPacket(HYPERBUS_BROADCAST_ID, HYPERBUS_MASTER_ID, CMD_SET_WEATHER, payload, sizeof(payload));
    _uartBus->sendPacket(HYPERBUS_BROADCAST_ID, HYPERBUS_MASTER_ID, CMD_SET_WEATHER, payload, sizeof(payload));
}

void SlaveManagerClass::handleImageRequest(uint8_t slaveId, const uint8_t* payload, uint16_t length) {
    if (!payload || length < HYPERBUS_IMAGE_REQUEST_LEN) return;
    uint8_t widgetId = payload[0];
    uint32_t crc = (uint32_t)payload[1] | ((uint32_t)payload[2] << 8) |
                   ((uint32_t)payload[3] << 16) | ((uint32_t)payload[4] << 24);

    // Already on its way: the Slave asks again while it waits, and restarting the transfer each
    // time would mean it never finishes.
    for (const auto& t : _imageTransfers) {
        if (t.slaveId == slaveId && t.widgetId == widgetId && t.crc == crc) return;
    }

    ImageTransfer t;
    t.slaveId = slaveId;
    t.widgetId = widgetId;
    t.crc = crc;
    t.offset = 0;
    // Only for the pixels the Slave was told about; an image changed since then reaches it through
    // the next widget config, and the Slave asks again with the new checksum.
    if (!LEDManager.copyWidgetImage(widgetId, crc, t.data, t.width, t.height)) return;

    // A superseded transfer for the same element is pointless now.
    for (auto it = _imageTransfers.begin(); it != _imageTransfers.end(); ) {
        if (it->slaveId == slaveId && it->widgetId == widgetId) it = _imageTransfers.erase(it);
        else ++it;
    }
    Serial.printf("Slave %u asked for image %u (%ux%u), sending %u bytes\n", (unsigned)slaveId,
                  (unsigned)widgetId, (unsigned)t.width, (unsigned)t.height, (unsigned)t.data.size());
    _imageTransfers.push_back(std::move(t));
}

void SlaveManagerClass::pumpImageTransfers() {
    Guard guard(_lock);
    if (_imageTransfers.empty()) return;
    unsigned long now = millis();
    if (now - _lastImageChunk < IMAGE_CHUNK_INTERVAL_MS) return;
    _lastImageChunk = now;

    ImageTransfer& t = _imageTransfers.front();
    BusInterface* bus = slaveIsOnline(t.slaveId) ? busFor(t.slaveId, false) : nullptr;
    if (!bus || t.offset >= t.data.size()) {
        _imageTransfers.erase(_imageTransfers.begin());
        return;
    }

    uint16_t total = (uint16_t)t.data.size();
    uint16_t piece = total - t.offset;
    if (piece > HYPERBUS_IMAGE_CHUNK_DATA) piece = HYPERBUS_IMAGE_CHUNK_DATA;

    uint8_t packet[HYPERBUS_IMAGE_CHUNK_HEADER + HYPERBUS_IMAGE_CHUNK_DATA];
    packet[0] = t.widgetId;
    packet[1] = t.width;
    packet[2] = t.height;
    packet[3] = (uint8_t)(t.crc & 0xFF);
    packet[4] = (uint8_t)((t.crc >> 8) & 0xFF);
    packet[5] = (uint8_t)((t.crc >> 16) & 0xFF);
    packet[6] = (uint8_t)((t.crc >> 24) & 0xFF);
    packet[7] = (uint8_t)(t.offset & 0xFF);
    packet[8] = (uint8_t)((t.offset >> 8) & 0xFF);
    packet[9] = (uint8_t)(total & 0xFF);
    packet[10] = (uint8_t)((total >> 8) & 0xFF);
    memcpy(&packet[HYPERBUS_IMAGE_CHUNK_HEADER], &t.data[t.offset], piece);
    bus->sendPacket(t.slaveId, HYPERBUS_MASTER_ID, CMD_WIDGET_IMAGE, packet,
                    (uint16_t)(HYPERBUS_IMAGE_CHUNK_HEADER + piece));
    t.offset += piece;
    if (t.offset >= total) _imageTransfers.erase(_imageTransfers.begin());
}

bool SlaveManagerClass::slaveRendersLocally(uint8_t slaveId) const {
    Guard guard(_lock);
    auto known = _slaveCaps.find(slaveId);
    return known != _slaveCaps.end() && known->second.rendersLocally;
}

bool SlaveManagerClass::slaveRendersWidgets(uint8_t slaveId) const {
    Guard guard(_lock);
    auto known = _slaveCaps.find(slaveId);
    return known != _slaveCaps.end() && known->second.rendersWidgets;
}

bool SlaveManagerClass::slaveIsOnline(uint8_t slaveId) const {
    Guard guard(_lock);
    for (const auto& s : _discoveredSlaves) {
        if (s.currentId == slaveId) return true;
    }
    return false;
}

bool SlaveManagerClass::getSlavePanelSize(uint8_t slaveId, uint16_t& w, uint16_t& h) const {
    Guard guard(_lock);
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
    Guard guard(_lock);
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

    BusInterface* targetBus = busFor(slaveId);
    if (targetBus) {
        targetBus->sendPacket(slaveId, HYPERBUS_MASTER_ID, CMD_SET_SEGMENT, payload,
                              HYPERBUS_SEGMENT_PAYLOAD_LEN);
    }
}

void SlaveManagerClass::sendWidgetConfig(uint8_t slaveId, const uint8_t* payload, uint16_t length) {
    Guard guard(_lock);
    if (millis() < _pauseLedsUntil) return;

    SentWidgets& sent = _sentWidgets[slaveId];
    unsigned long now = millis();
    bool changed = !sent.valid || sent.payload.size() != length
                   || memcmp(sent.payload.data(), payload, length) != 0;
    if (!changed && now - sent.lastSent < WIDGETS_REFRESH_MS) return;

    sent.payload.assign(payload, payload + length);
    sent.lastSent = now;
    sent.valid = true;

    BusInterface* targetBus = busFor(slaveId);
    if (targetBus) targetBus->sendPacket(slaveId, HYPERBUS_MASTER_ID, CMD_SET_WIDGETS, payload, length);
}

void SlaveManagerClass::invalidateLedFrame(uint8_t slaveId) {
    Guard guard(_lock);
    _ledTx.erase(slaveId);
    _lastLedSend.erase(slaveId);
}

void SlaveManagerClass::sendLEDData(uint8_t slaveId, const uint8_t* rgbData, uint16_t length) {
    Guard guard(_lock);
    if (millis() < _pauseLedsUntil) return;

    // A broadcast has no per-Slave state to diff against, and anything that fits a single packet
    // is not worth the machinery. Both are cheap enough to send on the spot.
    if (slaveId == HYPERBUS_BROADCAST_ID) {
        _uartBus->sendPacket(HYPERBUS_BROADCAST_ID, HYPERBUS_MASTER_ID, CMD_SET_LEDS, rgbData, length);
        _espBus->sendPacket(HYPERBUS_BROADCAST_ID, HYPERBUS_MASTER_ID, CMD_SET_LEDS, rgbData, length);
        return;
    }
    // A Slave that has not reported in shows nothing, so streaming to it is pure harm: the frame
    // would go out on a guessed transport (UART at 115200 baud - about 1.8s of blocking writes for
    // a 64x64 panel) and starve the pings that would bring it back. Drop what is queued instead;
    // the first frame after it returns is a full one, because the queue was thrown away.
    if (!slaveIsOnline(slaveId)) {
        _ledTx.erase(slaveId);
        _lastLedSend.erase(slaveId);
        return;
    }

    if (length <= 240) {
        BusInterface* targetBus = busFor(slaveId, false);
        if (targetBus) {
            targetBus->sendPacket(slaveId, HYPERBUS_MASTER_ID, CMD_SET_LEDS, rgbData, length);
            _ledPacketsSent++;
            _ledFramesSent++;
        }
        return;
    }

    // Comparing a 20KB frame is cheap, but not free, and the effect engine produces frames far
    // faster than anything can show them.
    unsigned long now = millis();
    auto seen = _lastLedSend.find(slaveId);
    if (seen != _lastLedSend.end() && now - seen->second < MIN_LED_FRAME_INTERVAL_MS) return;
    _lastLedSend[slaveId] = now;

    queueFrame(slaveId, rgbData, length);
}

void SlaveManagerClass::queueFrame(uint8_t slaveId, const uint8_t* rgbData, uint16_t length) {
    LedTx& tx = _ledTx[slaveId];
    unsigned long now = millis();

    if (tx.data.size() != length) {
        tx.data.assign(rgbData, rgbData + length);
        tx.chunks = (length + LED_CHUNK_BYTES - 1) / LED_CHUNK_BYTES;
        tx.dirty.assign((tx.chunks + 7) / 8, 0xFF); // nothing is known to be over there yet
        tx.cursor = 0;
        tx.repairSlice = 0;
        tx.lastRepair = now;
        _ledFramesSent++;
        return;
    }

    // Mark only what actually moved. A clock on a 64x64 panel changes a few dozen pixels out of
    // 4096, so this is almost always two or three chunks.
    bool any = false;
    for (uint16_t c = 0; c < tx.chunks; c++) {
        uint32_t off = (uint32_t)c * LED_CHUNK_BYTES;
        uint16_t size = (uint16_t)((length - off) < LED_CHUNK_BYTES ? (length - off) : LED_CHUNK_BYTES);
        if (memcmp(&tx.data[off], &rgbData[off], size) == 0) continue;
        memcpy(&tx.data[off], &rgbData[off], size);
        tx.dirty[c >> 3] |= (uint8_t)(1 << (c & 7));
        any = true;
    }
    if (any) _ledFramesSent++;

    // Rolling repair: re-offer one slice of the panel per interval, so a chunk lost to
    // interference is restored within a few seconds without ever resending the whole frame.
    if (now - tx.lastRepair >= LED_REPAIR_INTERVAL_MS) {
        tx.lastRepair = now;
        uint16_t from = (uint32_t)tx.chunks * tx.repairSlice / LED_REPAIR_SLICES;
        uint16_t to = (uint32_t)tx.chunks * (tx.repairSlice + 1) / LED_REPAIR_SLICES;
        for (uint16_t c = from; c < to; c++) tx.dirty[c >> 3] |= (uint8_t)(1 << (c & 7));
        tx.repairSlice = (uint8_t)((tx.repairSlice + 1) % LED_REPAIR_SLICES);
    }
}

void SlaveManagerClass::pumpLedTx() {
    if (_ledTx.empty()) return;
    if (millis() < _pauseLedsUntil) return;

    unsigned long t0 = micros();
    uint8_t payload[2 + LED_CHUNK_BYTES];

    for (auto& entry : _ledTx) {
        LedTx& tx = entry.second;
        if (tx.chunks == 0) continue;

        // Went away while its frame was still draining - stop pumping at it (see sendLEDData).
        if (!slaveIsOnline(entry.first)) {
            tx.chunks = 0;
            tx.data.clear();
            tx.dirty.clear();
            continue;
        }

        BusInterface* targetBus = busFor(entry.first, false);
        if (!targetBus) continue;
        uint16_t examined = 0;
        while (examined < tx.chunks) {
            // Leaving the loop on the budget rather than on a chunk count keeps the cost the same
            // whether the link is quick or congested.
            if (micros() - t0 >= LED_TX_BUDGET_US) return;

            uint16_t c = tx.cursor;
            tx.cursor = (uint16_t)((tx.cursor + 1) % tx.chunks);
            examined++;

            uint8_t mask = (uint8_t)(1 << (c & 7));
            if (!(tx.dirty[c >> 3] & mask)) continue;
            tx.dirty[c >> 3] &= (uint8_t)~mask;

            uint32_t off = (uint32_t)c * LED_CHUNK_BYTES;
            uint32_t remaining = tx.data.size() - off;
            uint16_t size = (uint16_t)(remaining < LED_CHUNK_BYTES ? remaining : LED_CHUNK_BYTES);

            uint16_t pixelOffset = (uint16_t)(off / HYPERBUS_LED_BYTES_PER_PIXEL);
            payload[0] = (uint8_t)(pixelOffset & 0xFF);
            payload[1] = (uint8_t)((pixelOffset >> 8) & 0xFF);
            memcpy(&payload[2], &tx.data[off], size);
            targetBus->sendPacket(entry.first, HYPERBUS_MASTER_ID, CMD_SET_LEDS_CHUNK,
                                  payload, size + 2);
            _ledPacketsSent++;
        }
    }
}











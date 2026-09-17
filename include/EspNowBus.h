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
#ifndef ESPNOWBUS_H
#define ESPNOWBUS_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "HyperBus.h"
#include <map>
#include <array> // For BusInterface and HyperBusPacket

class EspNowBusClass : public BusInterface {
public:
    EspNowBusClass();
    
    // mode: WIFI_STA, WIFI_AP, or WIFI_AP_STA
    // autoHop: if true, cycles channels if no PING received (for Slaves)
    void begin(wifi_mode_t mode, bool autoHop = false);
    
    virtual void loop() override;
    virtual bool sendPacket(uint8_t targetId, uint8_t senderId, uint8_t command, const uint8_t* payload, uint16_t length) override;
    virtual void setCallback(PacketReceivedCallback cb) override { _callback = cb; }

    static EspNowBusClass* _instance;
    static void onDataRecv(const esp_now_recv_info_t * esp_now_info, const uint8_t *incomingData, int len);

    // Receive diagnostics, surfaced via /api/espnow_status. Discovery problems are otherwise
    // invisible from the Master side: a Slave that is heard but rejected looks exactly like a
    // Slave that is not transmitting at all.
    uint32_t getPacketsReceived() const { return _packetsReceived; }
    uint8_t getLastSenderId() const { return _lastSenderId; }
    uint8_t getLastCommand() const { return _lastCommand; }
    uint32_t getPongsReceived() const { return _pongsReceived; }
    uint32_t getDroppedIncomplete() const { return _droppedIncomplete; }
    uint32_t getDroppedNoCallback() const { return _droppedNoCallback; }
    uint32_t getDroppedForeign() const { return _droppedForeign; }
    // Packets esp_now_send() refused, almost always because its queue was full. A burst of
    // chunks fills that queue far faster than the radio drains it, and every refusal is a block
    // of pixels that never reaches the panel.
    uint32_t getSendErrors() const { return _sendErrors; }

    // Packets the receive callback had to throw away because the queue was full. The loop was
    // not draining it fast enough - worth knowing, because it looks exactly like radio trouble.
    uint32_t getDroppedQueueFull() const { return _droppedQueueFull; }
    // Packets actually handed to the protocol. Received-but-not-delivered is the gap that
    // matters: the Master was counting thousands of PONGs while listing no Slaves at all, and
    // without this number the loss could only be guessed at.
    uint32_t getDelivered() const { return _delivered; }

    // Why sends fail, not only how often. esp_now_send() returns a reason (queue full, wrong
    // interface, internal error ...) and that reason is what tells a busy radio from a dead one.
    int32_t getLastSendError() const { return _lastSendError; }
    // The reason that opened the current run of failures - the first one is the telling one.
    int32_t getFailRunFirstError() const { return _failRunFirstError; }
    // Consecutive refused sends, and for how long: one accepted send resets both.
    uint32_t getSendFailStreak() const { return _sendFailStreak; }
    unsigned long getSendFailingForMs() const {
        return _sendFailStreak == 0 ? 0 : millis() - _sendFailSince;
    }
    // Time since anything at all was received.
    unsigned long getLastRxAgoMs() const { return millis() - _lastRxAt; }

private:
    void noteSendResult(esp_err_t result);
    int32_t _lastSendError = 0;
    int32_t _failRunFirstError = 0;
    uint32_t _sendFailStreak = 0;
    unsigned long _sendFailSince = 0;
    // Written by the receive callback on the Wi-Fi task; a single 32-bit write, read as a whole.
    volatile unsigned long _lastRxAt = 0;

    // Received packets are parked here and handled from loop(), never in the callback.
    //
    // esp_now_register_recv_cb() delivers on the Wi-Fi task, and the callback used to run the
    // whole protocol from there: SlaveManager::handlePacket() appends to the discovered-slave
    // list while the main loop walks and erases that same list, and the web server task copies
    // it to answer /api/slaves. A vector that reallocates under another task's iterator reads
    // freed memory, which is a panic - sporadic, unattributable, and exactly the kind that shows
    // up as "it ran fine for a while and then the Slaves were gone". Draining on one task makes
    // the whole question go away.
    struct RxPacket {
        uint8_t mac[6];
        uint8_t targetId;
        uint8_t senderId;
        uint8_t command;
        uint8_t length;
        uint8_t payload[240];
    };
    static const uint8_t RX_QUEUE_LEN = 16;
    QueueHandle_t _rxQueue = nullptr;
    uint32_t _droppedQueueFull = 0;
    uint32_t _delivered = 0;
    void registerPeer(const uint8_t* mac, uint8_t senderId);

    PacketReceivedCallback _callback = nullptr;
    bool _autoHop = false;
    unsigned long _lastPingReceived = 0;
    uint8_t _currentChannel = 1;
    bool _locked = false;
    uint32_t _packetsReceived = 0;
    uint8_t _lastSenderId = 0;
    uint8_t _lastCommand = 0;
    uint32_t _pongsReceived = 0;
    uint32_t _droppedIncomplete = 0;
    uint32_t _droppedNoCallback = 0;
    uint32_t _droppedForeign = 0;
    uint32_t _sendErrors = 0;
    
    uint8_t _broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    std::map<uint8_t, std::array<uint8_t, 6>> _peerMacs;

    static const uint16_t TX_BUFFER_SIZE = 250; // Covers header(6) + max chunk(240) with headroom, ESP-NOW payload cap is 250
    uint8_t _txBuffer[TX_BUFFER_SIZE];
};

#endif



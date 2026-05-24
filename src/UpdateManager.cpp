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
#include "UpdateManager.h"

UpdateManagerClass UpdateManager;

void UpdateManagerClass::begin() {
    _progress = 0;
    _status = "idle";
}

void UpdateManagerClass::loop() {
    if (_updatePending) {
        _updatePending = false;
        performUpdate();
    }
}

void UpdateManagerClass::startOnlineUpdate(String version) {
    if (_updatePending || _status == "updating") return;
    _targetVersion = version;
    _updatePending = true;
    _progress = 0;
    _status = "starting";
}

uint8_t UpdateManagerClass::getProgress() {
    return _progress;
}

String UpdateManagerClass::getStatus() {
    return _status;
}

void UpdateManagerClass::performUpdate() {
    _status = "updating FS";
    _progress = 5;
    
    String baseUrl = "https://github.com/KaelanTesseract/HyperLED/releases/download/" + _targetVersion + "/";
    
    if (!downloadAndFlash(baseUrl + "littlefs.bin", U_SPIFFS, 5, 45)) {
        _status = "error_fs";
        Serial.println("LittleFS update failed or file not found.");
        // We could abort, but maybe it's just a firmware update without a new filesystem?
        // Let's abort to be safe, since they usually go together.
        return;
    }
    
    _status = "updating FW";
    if (!downloadAndFlash(baseUrl + "firmware.bin", U_FLASH, 45, 95)) {
        _status = "error_fw";
        Serial.println("Firmware update failed.");
        return;
    }
    
    _status = "success";
    _progress = 100;
    delay(1000);
    ESP.restart();
}

bool UpdateManagerClass::downloadAndFlash(String url, int command, int startProgress, int endProgress) {
    bool success = false;
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(10000);
    
    if (http.begin(client, url)) {
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            int totalLength = http.getSize();
            if (totalLength > 0) {
                int updateSize = (command == U_SPIFFS) ? UPDATE_SIZE_UNKNOWN : totalLength;
                if (Update.begin(updateSize, command)) {
                    _progress = startProgress;
                    
                    WiFiClient* stream = http.getStreamPtr();
                    uint8_t buff[1024];
                    int written = 0;
                    
                    while (http.connected() && (written < totalLength)) {
                        size_t size = stream->available();
                        if (size) {
                            int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
                            Update.write(buff, c);
                            written += c;
                            _progress = startProgress + (written * (endProgress - startProgress) / totalLength);
                        }
                        delay(1);
                    }
                    
                    if (written == totalLength && Update.end()) {
                        if (Update.isFinished()) {
                            success = true;
                        } else {
                            Serial.println("Update not finished");
                        }
                    } else {
                        Serial.println("Written only : " + String(written) + "/" + String(totalLength));
                        Update.abort();
                    }
                } else {
                    Serial.println("Not enough space to begin OTA");
                }
            } else {
                Serial.println("File size is 0");
            }
        } else {
            Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
    } else {
        Serial.println("Unable to connect");
    }
    return success;
}

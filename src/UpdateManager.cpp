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
#include "UpdateManager.h"
#include "BackupManager.h"
#include "esp_task_wdt.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

UpdateManagerClass UpdateManager;

void UpdateManagerClass::begin() {
    _progress = 0;
    _status = "idle";
}

int UpdateManagerClass::compareVersions(const String& a, const String& b) {
    int ia = 0, ib = 0;
    while (ia < (int)a.length() || ib < (int)b.length()) {
        long na = 0, nb = 0;
        while (ia < (int)a.length() && a[ia] != '.') na = na * 10 + (isDigit(a[ia]) ? a[ia] - '0' : 0), ia++;
        while (ib < (int)b.length() && b[ib] != '.') nb = nb * 10 + (isDigit(b[ib]) ? b[ib] - '0' : 0), ib++;
        if (na != nb) return na > nb ? 1 : -1;
        ia++;
        ib++;
    }
    return 0;
}

void UpdateManagerClass::requestLatestCheck() {
    if (_latestRunning || WiFi.status() != WL_CONNECTED) return;
    if (_lastCheckAt && millis() - _lastCheckAt < 60000) return;
    _lastCheckAt = millis();
    _latestRunning = true;
    if (xTaskCreatePinnedToCore(latestTask, "latestCheck", 10240, this, 1, nullptr, 1) != pdPASS) {
        _latestRunning = false;
    }
}

void UpdateManagerClass::latestTask(void* arg) {
    UpdateManagerClass* self = static_cast<UpdateManagerClass*>(arg);
    bool master = fetchLatestTag("HyperLED", self->_latestBuf, sizeof(self->_latestBuf));
    bool slave = fetchLatestTag("HyperLED-Slave", self->_latestSlaveBuf, sizeof(self->_latestSlaveBuf));
    if (master || slave) self->_latestFresh = true;
    if (!master) self->_latestFailed = true;
    self->_latestRunning = false;
    vTaskDelete(nullptr);
}

bool UpdateManagerClass::fetchLatestTag(const char* repo, char* out, size_t outLen) {
    bool ok = false;
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(10000);
    String url = String("https://api.github.com/repos/KaelanTesseract/") + repo + "/releases/latest";
    if (http.begin(client, url)) {
        http.setUserAgent("HyperLED");  // GitHub refuses API requests without one
        if (http.GET() == HTTP_CODE_OK) {
            // Only the tag: the release also carries its notes and asset list.
            JsonDocument filter;
            filter["tag_name"] = true;
            // getString() rather than the raw stream: GitHub may answer chunked, which only the
            // former decodes. The whole answer is a few KB.
            String body = http.getString();
            JsonDocument doc;
            if (!deserializeJson(doc, body, DeserializationOption::Filter(filter))) {
                String tag = doc["tag_name"] | "";
                if (tag.startsWith("v")) tag.remove(0, 1);
                // Digits and dots only: the version becomes part of a download URL.
                bool clean = tag.length() > 0 && tag.length() < outLen;
                for (size_t i = 0; clean && i < tag.length(); i++) clean = isDigit(tag[i]) || tag[i] == '.';
                if (clean) {
                    strncpy(out, tag.c_str(), outLen - 1);
                    out[outLen - 1] = 0;
                    ok = true;
                }
            }
        }
        http.end();
    }
    return ok;
}

void UpdateManagerClass::loop() {
    if (_latestFresh) {
        _latestFresh = false;
        if (_latestBuf[0]) _latest = String(_latestBuf);
        if (_latestSlaveBuf[0]) _latestSlave = String(_latestSlaveBuf);
        _lastCheckOkAt = millis();
        _checkFailed = false;
    }
    if (_latestFailed) {
        _latestFailed = false;
        _checkFailed = true;
    }
    // A minute after start, then twice a day; after a failed check (no internet, GitHub busy)
    // again in half an hour.
    if (!_latestRunning && !isUpdating() && WiFi.status() == WL_CONNECTED && millis() > 60000) {
        unsigned long wait = _lastCheckAt == 0 ? 0 : _checkFailed ? 30UL * 60 * 1000 : 12UL * 3600 * 1000;
        if (_lastCheckAt == 0 || millis() - _lastCheckAt >= wait) requestLatestCheck();
    }
    if (_updatePending) {
        _updatePending = false;
        performUpdate();
    }
}

void UpdateManagerClass::startOnlineUpdate(String version) {
    if (_updatePending || _status == "updating") return;
    // The version becomes part of the download URL: digits and dots only.
    if (version.isEmpty() || version.length() > 16) return;
    for (size_t i = 0; i < version.length(); i++) {
        if (!isDigit(version[i]) && version[i] != '.') return;
    }
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

    // Nothing is overwritten unless every user file could be read first.
    if (!keepUserFiles()) {
        _status = "error_keep";
        freeKeptFiles();
        return;
    }

    // Unmounted while the partition is rewritten, so nothing reads or writes a half-written file
    // system, and mounted fresh afterwards to see the new one.
    LittleFS.end();
    bool fsOk = downloadAndFlash(baseUrl + "littlefs.bin", U_SPIFFS, 5, 45);
    LittleFS.begin();
    if (!fsOk) {
        _status = "error_fs";
        Serial.println("LittleFS update failed or file not found.");
        // A download that failed before writing leaves the old file system in place; one that
        // failed halfway does not, so the files go back either way.
        restoreUserFiles();
        freeKeptFiles();
        return;
    }
    restoreUserFiles();
    freeKeptFiles();

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
                        // Downloading a firmware image legitimately keeps this task busy for
                        // minutes, far past the loop watchdog's patience. Without this the update
                        // would be cut short by a reboot every time.
                        esp_task_wdt_reset();
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

bool UpdateManagerClass::keepUserFiles() {
    freeKeptFiles();
    std::vector<String> paths;
    BackupManagerClass::listUserFiles(paths);
    for (const String& path : paths) {
        File f = LittleFS.open(path, "r");
        if (!f) {
            Serial.printf("Update: cannot open %s - not updating\n", path.c_str());
            return false;
        }
        KeptFile kept;
        kept.path = path;
        kept.len = f.size();
        if (kept.len > 0) {
            // PSRAM: the TLS download that follows needs the internal RAM.
            kept.data = (uint8_t*)heap_caps_malloc(kept.len, MALLOC_CAP_SPIRAM);
            if (!kept.data) kept.data = (uint8_t*)malloc(kept.len);
            size_t got = kept.data ? f.read(kept.data, kept.len) : 0;
            if (got != kept.len) {
                f.close();
                free(kept.data);
                Serial.printf("Update: cannot read %s - not updating\n", path.c_str());
                return false;
            }
        }
        f.close();
        _kept.push_back(kept);
    }
    Serial.printf("Update: keeping %u user files across the file system update\n", (unsigned)_kept.size());
    return true;
}

void UpdateManagerClass::restoreUserFiles() {
    unsigned restored = 0;
    for (const KeptFile& kept : _kept) {
        int slash = kept.path.lastIndexOf('/');
        if (slash > 0) {
            String dir = kept.path.substring(0, slash);
            if (!LittleFS.exists(dir)) LittleFS.mkdir(dir);
        }
        File f = LittleFS.open(kept.path, "w");
        bool opened = (bool)f;
        size_t written = (opened && kept.len) ? f.write(kept.data, kept.len) : 0;
        if (opened) f.close();
        if (opened && written == kept.len) restored++;
        else Serial.printf("Update: could not write %s back\n", kept.path.c_str());
    }
    Serial.printf("Update: %u of %u user files written back\n", restored, (unsigned)_kept.size());
}

void UpdateManagerClass::freeKeptFiles() {
    for (KeptFile& kept : _kept) free(kept.data);
    _kept.clear();
}

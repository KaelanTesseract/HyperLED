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
#include "BackupManager.h"
#include "Config.h"
#include "ScheduleManager.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <nvs.h>
#include <mbedtls/base64.h>
#include <time.h>
#include <vector>

BackupManagerClass BackupManager;

const char* const BackupManagerClass::BACKUP_PATH = "/backup.json";
const char* const BackupManagerClass::RESTORE_PATH = "/restore.json";

// The user files a backup carries. Everything else on LittleFS is the web interface, which comes
// with the firmware and must never be replaced from a backup file.
static const char* const USER_FILES[] = {"/presets.json", "/playlist.json", "/schedules.json"};
static const char* const IMAGE_DIR = "/img";

// "/img/w<id>.rgb" - the element images (see widgetImagePath in LEDManager.cpp).
static bool isImagePath(const String& path) {
    if (!path.startsWith("/img/w") || !path.endsWith(".rgb")) return false;
    String id = path.substring(6, path.length() - 4);
    if (id.length() == 0 || id.length() > 3) return false;
    for (size_t i = 0; i < id.length(); i++) {
        if (!isDigit(id[i])) return false;
    }
    return true;
}

static bool isUserFile(const String& path) {
    for (const char* f : USER_FILES) {
        if (path == f) return true;
    }
    return isImagePath(path);
}

static String toBase64(const uint8_t* data, size_t len) {
    size_t outLen = 0;
    mbedtls_base64_encode(nullptr, 0, &outLen, data, len);
    std::vector<unsigned char> out(outLen + 1);
    if (mbedtls_base64_encode(out.data(), out.size(), &outLen, data, len) != 0) return String();
    out[outLen] = 0;
    return String((const char*)out.data());
}

static bool fromBase64(const char* text, std::vector<uint8_t>& out) {
    size_t inLen = strlen(text);
    size_t outLen = 0;
    mbedtls_base64_decode(nullptr, 0, &outLen, (const unsigned char*)text, inLen);
    out.resize(outLen);
    if (outLen == 0) return inLen == 0;
    return mbedtls_base64_decode(out.data(), out.size(), &outLen, (const unsigned char*)text, inLen) == 0 &&
           (out.resize(outLen), true);
}

// Opens the uploaded file past a UTF-8 byte order mark, which editors on Windows like to add when
// someone opens a backup and saves it again.
static File openRestoreFile() {
    File f = LittleFS.open(BackupManagerClass::RESTORE_PATH, "r");
    if (f && f.size() >= 3 && f.peek() == 0xEF) {
        uint8_t bom[3];
        f.read(bom, 3);
        if (!(bom[1] == 0xBB && bom[2] == 0xBF)) f.seek(0);
    }
    return f;
}

static bool readFile(const char* path, std::vector<uint8_t>& out) {
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    out.resize(f.size());
    size_t got = out.empty() ? 0 : f.read(out.data(), out.size());
    f.close();
    return got == out.size();
}

static const char* nvsTypeName(nvs_type_t type) {
    switch (type) {
        case NVS_TYPE_U8:   return "u8";
        case NVS_TYPE_I8:   return "i8";
        case NVS_TYPE_U16:  return "u16";
        case NVS_TYPE_I16:  return "i16";
        case NVS_TYPE_U32:  return "u32";
        case NVS_TYPE_I32:  return "i32";
        case NVS_TYPE_U64:  return "u64";
        case NVS_TYPE_I64:  return "i64";
        case NVS_TYPE_STR:  return "str";
        case NVS_TYPE_BLOB: return "blob";
        default:            return nullptr;
    }
}

// Reads one settings entry into the JSON object {t, v}. 64-bit values travel as text, because a
// JSON number is only exact up to 2^53.
static bool readSetting(nvs_handle_t h, const nvs_entry_info_t& e, JsonObject o) {
    const char* name = nvsTypeName(e.type);
    if (!name) return false;
    o["t"] = name;
    switch (e.type) {
        case NVS_TYPE_U8:  { uint8_t v;  if (nvs_get_u8(h, e.key, &v) != ESP_OK) return false;  o["v"] = v; return true; }
        case NVS_TYPE_I8:  { int8_t v;   if (nvs_get_i8(h, e.key, &v) != ESP_OK) return false;  o["v"] = v; return true; }
        case NVS_TYPE_U16: { uint16_t v; if (nvs_get_u16(h, e.key, &v) != ESP_OK) return false; o["v"] = v; return true; }
        case NVS_TYPE_I16: { int16_t v;  if (nvs_get_i16(h, e.key, &v) != ESP_OK) return false; o["v"] = v; return true; }
        case NVS_TYPE_U32: { uint32_t v; if (nvs_get_u32(h, e.key, &v) != ESP_OK) return false; o["v"] = v; return true; }
        case NVS_TYPE_I32: { int32_t v;  if (nvs_get_i32(h, e.key, &v) != ESP_OK) return false; o["v"] = v; return true; }
        case NVS_TYPE_U64: { uint64_t v; if (nvs_get_u64(h, e.key, &v) != ESP_OK) return false; o["v"] = String(v); return true; }
        case NVS_TYPE_I64: { int64_t v;  if (nvs_get_i64(h, e.key, &v) != ESP_OK) return false; o["v"] = String(v); return true; }
        case NVS_TYPE_STR: {
            size_t len = 0;
            if (nvs_get_str(h, e.key, nullptr, &len) != ESP_OK) return false;
            std::vector<char> buf(len);
            if (nvs_get_str(h, e.key, buf.data(), &len) != ESP_OK) return false;
            o["v"] = buf.data();
            return true;
        }
        case NVS_TYPE_BLOB: {
            size_t len = 0;
            if (nvs_get_blob(h, e.key, nullptr, &len) != ESP_OK) return false;
            std::vector<uint8_t> buf(len);
            if (len > 0 && nvs_get_blob(h, e.key, buf.data(), &len) != ESP_OK) return false;
            o["v"] = toBase64(buf.data(), len);
            return true;
        }
        default:
            return false;
    }
}

// Checks a {t, v} entry without writing it (h == 0) or writes it.
static bool writeSetting(nvs_handle_t h, const char* key, JsonObjectConst o, bool write) {
    String t = o["t"] | "";
    JsonVariantConst v = o["v"];
    if (strlen(key) == 0 || strlen(key) > NVS_KEY_NAME_MAX_SIZE - 1 || v.isNull()) return false;
    if (t == "u8")  { if (!v.is<uint8_t>())  return false; return !write || nvs_set_u8(h, key, v.as<uint8_t>()) == ESP_OK; }
    if (t == "i8")  { if (!v.is<int8_t>())   return false; return !write || nvs_set_i8(h, key, v.as<int8_t>()) == ESP_OK; }
    if (t == "u16") { if (!v.is<uint16_t>()) return false; return !write || nvs_set_u16(h, key, v.as<uint16_t>()) == ESP_OK; }
    if (t == "i16") { if (!v.is<int16_t>())  return false; return !write || nvs_set_i16(h, key, v.as<int16_t>()) == ESP_OK; }
    if (t == "u32") { if (!v.is<uint32_t>()) return false; return !write || nvs_set_u32(h, key, v.as<uint32_t>()) == ESP_OK; }
    if (t == "i32") { if (!v.is<int32_t>())  return false; return !write || nvs_set_i32(h, key, v.as<int32_t>()) == ESP_OK; }
    if (t == "u64") {
        if (!v.is<const char*>()) return false;
        return !write || nvs_set_u64(h, key, strtoull(v.as<const char*>(), nullptr, 10)) == ESP_OK;
    }
    if (t == "i64") {
        if (!v.is<const char*>()) return false;
        return !write || nvs_set_i64(h, key, strtoll(v.as<const char*>(), nullptr, 10)) == ESP_OK;
    }
    if (t == "str") {
        if (!v.is<const char*>()) return false;
        return !write || nvs_set_str(h, key, v.as<const char*>()) == ESP_OK;
    }
    if (t == "blob") {
        if (!v.is<const char*>()) return false;
        std::vector<uint8_t> data;
        if (!fromBase64(v.as<const char*>(), data)) return false;
        return !write || nvs_set_blob(h, key, data.data(), data.size()) == ESP_OK;
    }
    return false;
}

static String deviceName() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char name[32];
    snprintf(name, sizeof(name), "HyperLED-%02x%02x%02x", mac[3], mac[4], mac[5]);
    return String(name);
}

void BackupManagerClass::requestBackup() {
    _backupOk = false;
    _backupError = "";
    _backupRequested = true;
}

bool BackupManagerClass::writeBackup(String& error) {
    JsonDocument doc;
    doc["hyperled_backup"] = FORMAT;
    doc["firmware"] = SOFTWARE_VERSION;
    doc["device"] = deviceName();
    if (ScheduleManager.isTimeSynced()) {
        time_t now = time(nullptr);
        struct tm ti;
        localtime_r(&now, &ti);
        char buf[24];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &ti);
        doc["created"] = buf;
    }

    // Every settings entry, whatever its key - so a backup stays complete when a later version
    // stores something new.
    JsonObject settings = doc["settings"].to<JsonObject>();
    nvs_handle_t h;
    if (nvs_open(PREF_NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
        nvs_iterator_t it = nullptr;
        esp_err_t err = nvs_entry_find(NVS_DEFAULT_PART_NAME, PREF_NAMESPACE, NVS_TYPE_ANY, &it);
        while (err == ESP_OK && it != nullptr) {
            nvs_entry_info_t info;
            nvs_entry_info(it, &info);
            if (!readSetting(h, info, settings[info.key].to<JsonObject>())) {
                settings.remove(info.key);
                Serial.printf("Backup: could not read setting \"%s\"\n", info.key);
            }
            err = nvs_entry_next(&it);
        }
        nvs_release_iterator(it);
        nvs_close(h);
    }

    // The user files, as base64 so images and text travel the same way.
    JsonObject files = doc["files"].to<JsonObject>();
    std::vector<uint8_t> data;
    for (const char* path : USER_FILES) {
        if (LittleFS.exists(path) && readFile(path, data)) files[path] = toBase64(data.data(), data.size());
    }
    File dir = LittleFS.open(IMAGE_DIR);
    if (dir && dir.isDirectory()) {
        for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
            String path = String(IMAGE_DIR) + "/" + f.name();
            f.close();
            if (isImagePath(path) && readFile(path.c_str(), data)) files[path] = toBase64(data.data(), data.size());
        }
    }

    File out = LittleFS.open(BACKUP_PATH, "w");
    if (!out) {
        error = "cannot write the backup file";
        return false;
    }
    size_t written = serializeJson(doc, out);
    out.close();
    if (written == 0 || doc.overflowed()) {
        error = "not enough memory for the backup";
        LittleFS.remove(BACKUP_PATH);
        return false;
    }
    Serial.printf("Backup: %u settings and %u files, %u bytes\n", (unsigned)settings.size(),
                  (unsigned)files.size(), (unsigned)written);
    return true;
}

bool BackupManagerClass::prepareRestore(bool takeWifi, String& error) {
    if (_restorePending) {
        error = "a restore is already under way";
        return false;
    }
    File f = openRestoreFile();
    if (!f) {
        error = "no file received";
        return false;
    }
    JsonDocument doc;
    DeserializationError jsonErr = deserializeJson(doc, f);
    f.close();
    if (jsonErr) {
        error = String("not a valid backup file (") + jsonErr.c_str() + ")";
        return false;
    }
    int format = doc["hyperled_backup"] | 0;
    if (format < 1) {
        error = "not a HyperLED backup";
        return false;
    }
    if (format > FORMAT) {
        error = "backup is from a newer firmware - update this controller first";
        return false;
    }
    JsonObjectConst settings = doc["settings"];
    if (settings.isNull() || settings.size() == 0) {
        error = "backup contains no settings";
        return false;
    }
    for (JsonPairConst kv : settings) {
        if (!writeSetting(0, kv.key().c_str(), kv.value().as<JsonObjectConst>(), false)) {
            error = String("setting \"") + kv.key().c_str() + "\" is damaged";
            return false;
        }
    }
    JsonObjectConst files = doc["files"];
    for (JsonPairConst kv : files) {
        std::vector<uint8_t> data;
        if (!isUserFile(String(kv.key().c_str())) || !kv.value().is<const char*>() ||
            !fromBase64(kv.value().as<const char*>(), data)) {
            error = String("file \"") + kv.key().c_str() + "\" is not allowed or damaged";
            return false;
        }
    }

    _restoreTakeWifi = takeWifi;
    _restoreAt = millis() + 800; // lets the answer reach the browser first
    _restorePending = true;
    Serial.printf("Backup: restore of %u settings and %u files accepted (from %s, firmware %s)\n",
                  (unsigned)settings.size(), (unsigned)files.size(),
                  (const char*)(doc["device"] | "?"), (const char*)(doc["firmware"] | "?"));
    return true;
}

void BackupManagerClass::applyRestore() {
    File f = openRestoreFile();
    JsonDocument doc;
    DeserializationError jsonErr = f ? deserializeJson(doc, f) : DeserializationError::InvalidInput;
    if (f) f.close();
    if (jsonErr) {
        Serial.println("Backup: restore file vanished, nothing changed");
        _restorePending = false;
        return;
    }

    nvs_handle_t h;
    if (nvs_open(PREF_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) {
        Serial.println("Backup: cannot open the settings, nothing changed");
        _restorePending = false;
        return;
    }
    // This controller's own Wi-Fi, unless the backup's is wanted.
    String keepSsid, keepPass;
    if (!_restoreTakeWifi) {
        size_t len = 0;
        if (nvs_get_str(h, PREF_WIFI_SSID, nullptr, &len) == ESP_OK) {
            std::vector<char> buf(len);
            if (nvs_get_str(h, PREF_WIFI_SSID, buf.data(), &len) == ESP_OK) keepSsid = buf.data();
        }
        len = 0;
        if (nvs_get_str(h, PREF_WIFI_PASS, nullptr, &len) == ESP_OK) {
            std::vector<char> buf(len);
            if (nvs_get_str(h, PREF_WIFI_PASS, buf.data(), &len) == ESP_OK) keepPass = buf.data();
        }
    }

    nvs_erase_all(h);
    uint32_t written = 0, failed = 0;
    for (JsonPairConst kv : doc["settings"].as<JsonObjectConst>()) {
        const char* key = kv.key().c_str();
        if (!_restoreTakeWifi && (strcmp(key, PREF_WIFI_SSID) == 0 || strcmp(key, PREF_WIFI_PASS) == 0)) continue;
        if (writeSetting(h, key, kv.value().as<JsonObjectConst>(), true)) written++;
        else failed++;
    }
    if (!_restoreTakeWifi && keepSsid.length() > 0) {
        nvs_set_str(h, PREF_WIFI_SSID, keepSsid.c_str());
        nvs_set_str(h, PREF_WIFI_PASS, keepPass.c_str());
    }
    nvs_commit(h);
    nvs_close(h);

    // The user files: the old ones go, the backup's come in.
    for (const char* path : USER_FILES) LittleFS.remove(path);
    File dir = LittleFS.open(IMAGE_DIR);
    if (dir && dir.isDirectory()) {
        std::vector<String> old;
        for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
            old.push_back(String(IMAGE_DIR) + "/" + e.name());
            e.close();
        }
        dir.close();
        for (const String& path : old) {
            if (isImagePath(path)) LittleFS.remove(path);
        }
    }
    uint32_t filesWritten = 0;
    for (JsonPairConst kv : doc["files"].as<JsonObjectConst>()) {
        String path = kv.key().c_str();
        std::vector<uint8_t> data;
        if (!isUserFile(path) || !kv.value().is<const char*>() ||
            !fromBase64(kv.value().as<const char*>(), data)) continue;
        if (isImagePath(path) && !LittleFS.exists(IMAGE_DIR)) LittleFS.mkdir(IMAGE_DIR);
        File out = LittleFS.open(path, "w");
        if (!out) continue;
        if (out.write(data.data(), data.size()) == data.size()) filesWritten++;
        out.close();
    }
    LittleFS.remove(RESTORE_PATH);

    Serial.printf("Backup: restored %lu settings (%lu failed) and %lu files%s, restarting\n",
                  (unsigned long)written, (unsigned long)failed, (unsigned long)filesWritten,
                  _restoreTakeWifi ? " including Wi-Fi" : "");
    Serial.flush();
    delay(200);
    ESP.restart();
}

void BackupManagerClass::loop() {
    if (_backupRequested) {
        String error;
        _backupOk = writeBackup(error);
        _backupError = error;
        _backupRequested = false;
        if (!_backupOk) Serial.printf("Backup: failed - %s\n", error.c_str());
    }
    // Applied here and followed straight by the restart: no other part of the loop runs in
    // between, so nothing still held in memory can be written back over the restored settings.
    if (_restorePending && (long)(millis() - _restoreAt) >= 0) {
        applyRestore();
    }
}

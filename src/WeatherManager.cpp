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
#include "WeatherManager.h"
#include "Config.h"
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

WeatherManagerClass WeatherManager;

// Weather doesn't change fast - poll every 15 minutes normally, but retry
// sooner if the last attempt failed (e.g. WiFi hiccup, API briefly down).
#define WEATHER_FETCH_INTERVAL_MS (15UL * 60UL * 1000UL)
#define WEATHER_RETRY_INTERVAL_MS (60UL * 1000UL)

static String urlEncode(const String& s) {
    String out;
    out.reserve(s.length() * 3);
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += c;
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
            out += buf;
        }
    }
    return out;
}

// Maps Open-Meteo's WMO weather codes to our 5 icon categories.
// https://open-meteo.com/en/docs (WMO Weather interpretation codes)
uint8_t WeatherManagerClass::weatherCodeToIcon(int code) {
    if (code == 0 || code == 1) return WEATHER_ICON_SUN;
    if (code >= 95) return WEATHER_ICON_THUNDER;
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) return WEATHER_ICON_SNOW;
    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return WEATHER_ICON_RAIN;
    return WEATHER_ICON_CLOUD; // 2,3 (partly cloudy/overcast), 45/48 (fog), unknown
}

void WeatherManagerClass::begin() {
    loadLocation();
    if (_hasLocation) startFetch();
}

void WeatherManagerClass::loop() {
    if (!_hasLocation) return;
    if (WiFi.status() != WL_CONNECTED) return;
    if (_fetchRunning) return;
    unsigned long interval = _hasData ? WEATHER_FETCH_INTERVAL_MS : WEATHER_RETRY_INTERVAL_MS;
    if (millis() - _lastAttempt < interval) return;
    // Stamped here as well as in fetchWeather(): if the task cannot be created we must not spin
    // on retrying it every single loop.
    _lastAttempt = millis();
    startFetch();
}

void WeatherManagerClass::startFetch() {
    if (_fetchRunning) return;
    _fetchRunning = true;
    // 12KB of stack: the HTTPS request brings mbedTLS with it, which a default-sized task
    // cannot hold. Lowest priority - nothing here is urgent, and it must never crowd out the
    // loop it was moved off in the first place.
    if (xTaskCreate(fetchTaskEntry, "weather", 12288, this, 1, nullptr) != pdPASS) {
        Serial.println("WeatherManager: could not start fetch task");
        _fetchRunning = false;
    }
}

void WeatherManagerClass::fetchTaskEntry(void* arg) {
    WeatherManagerClass* self = static_cast<WeatherManagerClass*>(arg);
    self->fetchWeather();
    self->_fetchRunning = false;
    vTaskDelete(nullptr);
}

void WeatherManagerClass::loadLocation() {
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, true);
    _city = prefs.getString("wxCity", "");
    _lat = prefs.getFloat("wxLat", 0);
    _lon = prefs.getFloat("wxLon", 0);
    prefs.end();
    _hasLocation = _city.length() > 0;
}

void WeatherManagerClass::saveLocation() {
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, false);
    prefs.putString("wxCity", _city);
    prefs.putFloat("wxLat", _lat);
    prefs.putFloat("wxLon", _lon);
    prefs.end();
}

bool WeatherManagerClass::setLocation(const String& city) {
    if (city.length() == 0 || WiFi.status() != WL_CONNECTED) return false;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(10000);

    String url = "https://geocoding-api.open-meteo.com/v1/search?count=1&name=" + urlEncode(city);
    if (!http.begin(client, url)) {
        Serial.println("WeatherManager: geocoding http.begin() failed");
        return false;
    }
    int httpCode = http.GET();
    Serial.printf("WeatherManager: geocoding GET -> %d\n", httpCode);
    bool resolved = false;
    if (httpCode == HTTP_CODE_OK) {
        String body = http.getString(); // avoids chunked-stream parsing quirks with deserializeJson(Stream&)
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (err) {
            Serial.printf("WeatherManager: geocoding JSON parse error: %s\n", err.c_str());
        } else {
            JsonArray results = doc["results"].as<JsonArray>();
            if (!results.isNull() && results.size() > 0) {
                JsonObject first = results[0];
                _lat = first["latitude"] | 0.0f;
                _lon = first["longitude"] | 0.0f;
                const char* resolvedName = first["name"] | city.c_str();
                _city = String(resolvedName);
                resolved = true;
                Serial.printf("WeatherManager: resolved '%s' -> lat=%.4f lon=%.4f\n", _city.c_str(), _lat, _lon);
            } else {
                Serial.println("WeatherManager: geocoding returned no results");
            }
        }
    } else {
        Serial.printf("WeatherManager: geocoding HTTP error, code=%d\n", httpCode);
    }
    http.end();

    if (!resolved) return false;

    _hasLocation = true;
    saveLocation();
    fetchWeather();
    return true;
}

bool WeatherManagerClass::fetchWeather() {
    _lastAttempt = millis();
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(10000);

    char url[160];
    snprintf(url, sizeof(url), "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,weather_code", _lat, _lon);
    if (!http.begin(client, url)) {
        Serial.println("WeatherManager: http.begin() failed");
        return false;
    }
    int httpCode = http.GET();
    Serial.printf("WeatherManager: forecast GET -> %d\n", httpCode);
    bool success = false;
    if (httpCode == HTTP_CODE_OK) {
        String body = http.getString(); // avoids chunked-stream parsing quirks with deserializeJson(Stream&)
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (err) {
            Serial.printf("WeatherManager: JSON parse error: %s\n", err.c_str());
        } else {
            JsonObject current = doc["current"];
            if (!current.isNull() && !current["temperature_2m"].isNull()) {
                _temperature = current["temperature_2m"].as<float>();
                _icon = weatherCodeToIcon(current["weather_code"] | 0);
                _hasData = true;
                success = true;
            } else {
                Serial.println("WeatherManager: response missing current.temperature_2m");
            }
        }
    } else {
        Serial.printf("WeatherManager: forecast HTTP error, code=%d\n", httpCode);
    }
    http.end();
    return success;
}

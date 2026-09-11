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
#pragma once

#include <Arduino.h>

// Weather icon categories - matches the bitmap set in WeatherIcons.h and the
// numbers used by the "Wetter" widget (see LEDManagerClass::effectText).
#define WEATHER_ICON_SUN 0
#define WEATHER_ICON_CLOUD 1
#define WEATHER_ICON_RAIN 2
#define WEATHER_ICON_SNOW 3
#define WEATHER_ICON_THUNDER 4

// Fetches current temperature + weather condition for a user-set location from
// Open-Meteo (free, no API key needed). The city name is resolved to
// coordinates once (via Open-Meteo's geocoding API) when the location is set;
// after that only the lightweight forecast endpoint is polled periodically.
class WeatherManagerClass {
public:
    void begin();
    void loop();

    // Resolves a city name to coordinates and immediately fetches current
    // weather. Returns false (leaving any previous location untouched) if the
    // city can't be resolved or there's no WiFi connection yet.
    bool setLocation(const String& city);
    String getCity() const { return _city; }
    float getLatitude() const { return _lat; }
    float getLongitude() const { return _lon; }
    bool hasLocation() const { return _hasLocation; }

    // Current conditions from the last successful fetch. Returns 0 / WEATHER_ICON_CLOUD
    // (a neutral fallback) until the first successful fetch - check hasData() first.
    float getTemperature() const { return _temperature; }
    uint8_t getWeatherIcon() const { return _icon; }
    bool hasData() const { return _hasData; }

private:
    void loadLocation();
    void saveLocation();
    bool fetchWeather();
    // The fetch runs in its own task, never from loop(). It talks HTTPS, so a single call can
    // occupy several seconds - TLS handshake plus a 10s timeout - and during that the Master
    // sends no pings at all. A Slave gives up after five seconds of silence, so every refresh
    // interval knocked the Slaves off the Master until they found it again.
    void startFetch();
    static void fetchTaskEntry(void* arg);
    volatile bool _fetchRunning = false;
    static uint8_t weatherCodeToIcon(int code);

    String _city;
    float _lat = 0;
    float _lon = 0;
    bool _hasLocation = false;

    float _temperature = 0;
    uint8_t _icon = WEATHER_ICON_CLOUD;
    bool _hasData = false;
    unsigned long _lastAttempt = 0;
};

extern WeatherManagerClass WeatherManager;

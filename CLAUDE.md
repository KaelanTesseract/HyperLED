# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

HyperLED is an ESP32-S3 LED controller firmware (PlatformIO/Arduino, C++) with a glassmorphism WebUI, an Android companion app, and a wired Master/Slave sync protocol called **HyperBus**. This is not a git repository (no `.git` present) — do not assume git-based workflows are available. Both Master and Slave boards are the Waveshare ESP32-S3-Zero (ESP32-S3FH4R2, embedded 4MB Quad flash + 2MB Octal PSRAM); the project previously targeted the ESP32-C6 and was fully migrated off it, so no C6 support remains.

There are three independently buildable pieces in this one folder:

- **Root (`/`)** — the **Master** firmware. PlatformIO project, env `esp32-s3` (default) and `esp32dev`.
- **`HyperLED_Slave/`** — a *separate* PlatformIO project for **Slave** boards. Has its own `platformio.ini`, `include/`, `src/`. Only builds for `esp32-s3`.
- **`HyperLED_Android/`** — a Gradle/Kotlin Android app that is essentially a WebView wrapper with mDNS auto-discovery for the controller's web UI.

`archive/backup_v0_1_100/` is a frozen snapshot of an older version of the root firmware (pre-Slave-Manager/ButtonManager). Do not edit it as if it were live code; only reference it if asked to diff against an old release.

The many `archive/update_*.py` / `archive/fix_*.py` / `archive/replacer.js` / `archive/replacer.ps1` scripts are one-off, already-applied migration scripts used during development to patch `app.js`, `index.html`, `LEDManager.cpp`, etc. They are not part of the build and are not meant to be re-run — treat them as historical, not tooling. They (and the old backup snapshot) were moved out of the project root into `archive/` to keep the root readable.

## Build / flash / monitor commands

All firmware work uses **PlatformIO** (`pio`), run from the project directory (root for Master, `HyperLED_Slave/` for Slave).

```bash
pio run                      # build (default env: esp32-s3)
pio run -e esp32-s3          # build a specific env
pio run -t upload            # build + flash firmware
pio run -t uploadfs          # build + flash the LittleFS filesystem image (root project only — flashes /data)
pio device monitor           # serial monitor (115200 baud)
pio run -t clean             # clean build artifacts
```

There is no unit test suite in this repo (the PlatformIO `test/` folders are stock placeholders). Verification is manual: flash and observe serial output / WebUI behavior.

The root project's `/data` directory (`data/index.html`, `data/app.js`, `data/style.css`, `data/i18n.js`, `data/iro.min.js`) is the LittleFS web UI filesystem image — it must be uploaded separately via `pio run -t uploadfs` after changes; it is not compiled into the firmware binary.

### Android app

```bash
cd HyperLED_Android
./gradlew assembleDebug       # build debug APK
./gradlew installDebug        # build + install on connected device
```

## Firmware architecture (root project)

`src/main.cpp` wires together a fixed set of singleton "manager" classes, each declared as `extern <Name>Class <Name>;` in its header and instantiated once in its `.cpp`. All managers expose `begin()`/`loop()`, called in this order from `setup()`/`loop()`:

```
LEDManager → WiFiManager → WebServerManager → MqttManager → UpdateManager → SlaveManager → ButtonManager
```

- **`LEDManager`** (`LEDManager.h/.cpp`) — owns the `IBus*` strip abstraction (see `BusWrapper.h`), the list of `Segment`s (each segment has its own effect/color/brightness/on-state and can be a local range or a Slave-mapped segment), the effect engine (`effectSolid`, `effectRainbow`, `effectFire`, etc.), 2D matrix pixel mapping, and ABL (Auto Brightness Limiting / current limiting). Segments persist via `Preferences` (NVS).
- **`BusWrapper.h`** — `IBus` interface plus concrete strip drivers (`BusDigitalRgb`, `BusDigitalRgbw`, `BusDigitalRgbww`, `BusDigitalSpiRgb` for NeoPixelBus-backed digital/SPI strips, `BusOnOff` for relays, `BusPwm` for 1–5 channel analog/CCT LEDs, `BusVirtual` for buffering Master/Slave data). LED type IDs are WLED-compatible constants defined in `Config.h` (`TYPE_WS2812_RGB`, `TYPE_SK6812_RGBW`, etc.).
- **`WiFiManager`** — STA connect with fallback to a captive-portal AP (`DEFAULT_AP_SSID`), async network scanning, credential persistence.
- **`AppWebServer`** (class `WebServerManagerClass`) — ESPAsyncWebServer routes: captive portal, OTA upload endpoint, and a **WLED-compatible JSON API** (`/json/state`, etc. — see `docs/en/08_API_Referenz.md`) plus `/api/*` system endpoints (scan, factory reset). Serves the LittleFS `data/` files as the WebUI.
- **`MqttManager`** — optional MQTT integration with Home Assistant auto-discovery (`publishHomeAssistantDiscovery`).
- **`UpdateManager`** — dual-partition OTA (firmware + filesystem) from a remote `version.json` (`UPDATE_JSON_URL` in `Config.h`); partition layout is `partitions.csv` (two 1.44 MB OTA app slots + a 1 MB LittleFS `spiffs` region).
- **`SlaveManager`** — Master-side counterpart of HyperBus: discovers slaves (ping/pong), assigns slave IDs/config, and streams per-frame LED data to them over UART and/or ESP-NOW.
- **`ButtonManager`** — physical push-button / toggle-switch input, pins configurable via WebUI.

### HyperBus (Master/Slave sync protocol)

Defined in `include/HyperBus.h` (framing: `0xAA` start byte, header, payload, CRC16) with two transport implementations sharing the same `BusInterface`:
- `HyperBusClass` — wired UART transport (`src/HyperBus.cpp`), used between Master and Slave over `Serial1`/`Serial0` (default pins: Master TX GPIO 17 → Slave RX GPIO 16).
- `EspNowBusClass` (`include/EspNowBus.h`) — wireless ESP-NOW transport, used as an auto-sensing fallback when no UART link is detected.

Commands (`HyperBusCommand` enum): `CMD_PING`/`CMD_PONG` (discovery), `CMD_SET_CONFIG` (assign slave ID/pins/LED count/name), `CMD_SET_LEDS`/`CMD_SET_LEDS_CHUNK` (per-frame RGBW pixel data), `CMD_TRIGGER_UPDATE` (remote OTA trigger with SSID/URL payload).

`HyperLED_Slave/src/main.cpp` is a standalone firmware for slave boards: it auto-senses UART vs. ESP-NOW transport (locking in after a timeout, persisted via `Preferences`, with hardware-lockup recovery and auto-revert logic), forwards unrecognized/broadcast packets downstream (daisy-chaining slaves via `busUp`/`busDown`), and performs WiFi-on-demand OTA updates (disabling UART and freeing the LED buffer first to survive the SSL handshake on constrained RAM).

When changing the wire protocol, both `include/HyperBus.h` (root) and `HyperLED_Slave/include/HyperBus.h` must be kept in sync — they are separate copies, not a shared header.

## Frontend (`data/`)

Plain JS/HTML/CSS, no build step or bundler — files are uploaded to the device as-is via `pio run -t uploadfs`. `app.js` talks to the firmware's JSON API; `i18n.js` holds UI translations; `iro.min.js` is the vendored color-picker library.

## Documentation

`docs/en/` and `docs/de/` (English/German) are the user-facing wiki, linked from `README.md`. Notably `docs/en/07_Master_Slave_Architektur.md` (HyperBus wiring/usage) and `docs/en/08_API_Referenz.md` (HTTP API) — keep these in sync with actual behavior when changing networking code or the API.

## Licensing

Code is EUPL-1.2 licensed (see `LICENSE`); source files carry an EUPL header comment block — preserve it when creating new source files that mirror existing ones.

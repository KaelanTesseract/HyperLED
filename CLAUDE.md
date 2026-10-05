# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

HyperLED is an ESP32-S3 LED controller firmware (PlatformIO/Arduino, C++) with a glassmorphism WebUI, an Android companion app, and a wired Master/Slave sync protocol called **HyperBus**. This is a git repository for the Master firmware; `HyperLED_Slave/` is a separate repository with its own history (ignored here, never commit one repository's files into the other), and `docs/superpowers/` holds internal design notes that stay out of the repository. Commit and push only when asked. Both Master and Slave boards are the Waveshare ESP32-S3-Zero (ESP32-S3FH4R2, embedded 4MB Quad flash + 2MB Octal PSRAM); the project previously targeted the ESP32-C6 and was fully migrated off it, so no C6 support remains.

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

The root project's `/data` directory (`data/index.html`, `data/app.js`, `data/style.css`, `data/i18n.js`, `data/iro.min.js`) is the LittleFS web UI filesystem image — it must be uploaded separately via `pio run -t uploadfs` after changes; it is not compiled into the firmware binary. `pack_data.py` (a PlatformIO pre-script) builds that image from a freshly gzipped copy of `data/` in `.pio/data_packed/` - only the `.gz` files go onto the controller, so no manual `gzip` step is needed and `data/*.gz` is not used.

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
- **`BusWrapper.h`** — `IBus` interface plus concrete strip drivers (`BusDigitalRgb`, `BusDigitalRgbw`, `BusDigitalRgbww`, `BusDigitalSpiRgb` for NeoPixelBus-backed digital/SPI strips, `BusOnOff` for relays, `BusPwm` for 1–5 channel analog/CCT LEDs, `BusVirtual` for buffering Master/Slave data). LED type IDs are constants defined in `Config.h` (`TYPE_WS2812_RGB`, `TYPE_SK6812_RGBW`, etc.).
- **`WiFiManager`** — STA connect with fallback to a captive-portal AP (`DEFAULT_AP_SSID`), async network scanning, credential persistence.
- **`AppWebServer`** (class `WebServerManagerClass`) — ESPAsyncWebServer routes (a handler for `/api/plugins` also catches every address below it - register the plugin routes with a longer address first, and `/api/plugins` itself last; the JSON handlers do not tell methods apart, so a GET that shares an address with a JSON POST needs an address of its own, like `/api/plugins/fetch_result`): captive portal, OTA upload endpoint, and the `/api/*` endpoints the web interface uses (state, segments, presets, system - see `docs/en/08_API_Referenz.md`). Home Assistant talks to HyperLED over MQTT. Serves the LittleFS `data/` files as the WebUI.
- **`MqttManager`** — optional MQTT integration with Home Assistant auto-discovery (`publishHomeAssistantDiscovery`).
- **`UpdateManager`** — dual-partition OTA (firmware + filesystem) from a remote `version.json` (`UPDATE_JSON_URL` in `Config.h`); partition layout is `partitions.csv` (two 1.625 MB OTA app slots + a 704 KB LittleFS `spiffs` region; a controller flashed with the older table, 1.44 MB slots, cannot take a firmware above 1.44 MB over the air - `UpdateManager` refuses with `error_slot` and the user must reinstall once over USB).
- **`SlaveManager`** — Master-side counterpart of HyperBus: discovers slaves (ping/pong), assigns slave IDs/config, and streams per-frame LED data to them over UART and/or ESP-NOW.
- **`LogRing`** (`LogRing.h/.cpp`) — the last 60 seconds of everything the firmware prints, kept in an RTC-memory ring that survives a software restart, a crash and a watchdog reset. The project's own sources are compiled with `-include LogRing.h` (`build_src_flags` in `platformio.ini`), which turns `Serial` into `LogSerial`, a `Print` that copies each write into the ring before handing it to the real port; the libraries are untouched. After a restart that was not routine (panic, watchdog, `LogRing.noteRestart()` before the link/radio-dead restarts in `WiFiManager`) the minute before it is written once to `/lastlog.txt` (one file, overwritten each time; nothing is written while running). `GET /api/log` shows the running controller's minute, `GET /api/lastlog` the kept one. `LogRing.begin()` must run first in `setup()`.
- **`ButtonManager`** — physical push-button / toggle-switch input, pins configurable via WebUI.
- **`PluginManager`** (`PluginManager.h/.cpp`, with `PluginDef`, `PluginExpr`, `PluginJson`, `PluginHttp`, `PluginRun`) — user-installable plugins: one JSON file in `/plugins/` (kept across firmware updates because `BackupManager::listUserFiles` lists it) that polls a source on the network and shows the result on one segment through a `SegmentOverlay`. The overlay is applied only while drawing (`viewOf()` in `LEDManager`) and is never stored. Rules first; a plugin may also carry a Lua `script` (`PLUGIN_SCRIPT_LEVEL` 1): the plugin manager hands it the plugin's settings (never passwords) and values and either runs it on a Master segment (`MasterScripts`, one `Script::Task` per segment, `LEDManager::drawScriptSegment` copies the finished frame in) or sends it to the Slave that owns the segment (`SlaveManager::runScript`), re-sending after a Master restart; if the script cannot run, the plugin's own rules apply and `GET /api/plugins` says why (`script.note`). `PLUGIN_API_VERSION` / `PLUGIN_API_MIN` in `Config.h` decide compatibility. Documentation: `docs/en/09_Plugins_nutzen.md` (using) and `docs/en/10_Plugins_entwickeln.md` (file format). On-device checks: build env `esp32-s3-test` and `GET /api/plugin_selftest` (there is no host compiler).
- **`TextVariables`** (`TextVariables.h/.cpp`) — a small general store of text variables ("name.part" -> text, with its own lock and a generation counter) plus `TextTemplate::expand`, which fills `{name.part}` placeholders into a text (unknown ones show `--`, the result is cut to 64 bytes, filled-in text is never scanned again). `PluginManager::publishTextVariables()` publishes the known values of the running plugins as `<plugin-id>.<value>` on every pass of its loop; `LEDManager::shownText()` fills them into the text of Text and Lauftext elements when drawing and when building the Slave's widget packet (cached per widget until the template or a variable changes). The stored text stays the template; Slaves get the finished text, so nothing changes on the wire. `TextVariables.begin()` runs first in `setup()`; the core knows nothing about plugins, whoever has values just sets them. Documentation: `docs/en/09_Plugins_nutzen.md`, `10_Plugins_entwickeln.md`.
- **`ScriptHost`** (`ScriptHost.h/.cpp`, identical copies in the Master and the Slave repository, with Lua 5.4.7 in `lib/lua54/`) — one Lua state behind a small interface for plugin scripts: sandboxed (no files, network or system functions), with a memory ceiling and a time budget for every call; the time limit and a lower nesting depth (`LUAI_MAXCCALLS` 32, a deeper script gets "C stack overflow" instead of overflowing the stack) are small patches in the Lua sources (`lib/lua54/PATCHES.md`), re-apply them when Lua is updated. `ScriptTask.h/.cpp` (also identical in both repositories) runs one script in a task of its own with a 16 KB stack and draws it into a buffer; the Master keeps one per script-driven segment, the Slave one for its output (`ScriptRunner` assembles what the Master sends and hands it over). Documentation: `docs/en/11_Plugin_Skripte.md`.

### HyperBus (Master/Slave sync protocol)

Defined in `include/HyperBus.h` (framing: `0xAA` start byte, header, payload, CRC16) with two transport implementations sharing the same `BusInterface`:
- `HyperBusClass` — wired UART transport (`src/HyperBus.cpp`), used between Master and Slave over `Serial1`/`Serial0` (default pins: Master TX GPIO 17 → Slave RX GPIO 16).
- `EspNowBusClass` (`include/EspNowBus.h`) — wireless ESP-NOW transport, used as an auto-sensing fallback when no UART link is detected.

Commands (`HyperBusCommand` enum): `CMD_PING`/`CMD_PONG` (discovery), `CMD_SET_CONFIG` (assign slave ID/pins/LED count/name), `CMD_SET_LEDS`/`CMD_SET_LEDS_CHUNK` (per-frame RGBW pixel data), `CMD_UPDATE_KEY` + `CMD_TRIGGER_UPDATE_SEALED` (remote OTA trigger: X25519 key exchange, then SSID/password sealed with AES-256-GCM and the URL authenticated - see `include/UpdateSeal.h`, a separate copy in each project like `HyperBus.h`), `CMD_TRIGGER_UPDATE` (legacy plain-JSON OTA trigger, only sent to and accepted from pre-0.2.008 slaves over UART).

`CMD_SET_SCRIPT` / `CMD_REQUEST_SCRIPT` / `CMD_SCRIPT_CHUNK` / `CMD_SET_SCRIPT_VALUES` / `CMD_SCRIPT_STATUS` (0x11–0x15, Slaves 0.3.000 and later) and `CMD_SET_SCRIPT_DATA` (0x16, Slaves 0.3.001 and later: settings and values in as many packets as each needs, where 0.3.000 had one 240-byte packet for both; the Master sends it only to a Slave that reports 0.3.001 or later) hand a Slave a Lua script to run itself: the Master states what should run (refresh every 2 s), the Slave asks for a script it lacks and gets it in 224-byte pieces checked by CRC-32, then runs it in its own task (`ScriptRunner`) and reports its state back. The bytes of these commands are encoded in `include/ScriptWire.h`, an identical copy in both repositories like `HyperBus.h`.

`HyperLED_Slave/src/main.cpp` is a standalone firmware for slave boards: it auto-senses UART vs. ESP-NOW transport (locking in after a timeout, persisted via `Preferences`, with hardware-lockup recovery and auto-revert logic), forwards unrecognized/broadcast packets downstream (daisy-chaining slaves via `busUp`/`busDown`), and performs WiFi-on-demand OTA updates (disabling UART and freeing the LED buffer first to survive the SSL handshake on constrained RAM).

When changing the wire protocol, both `include/HyperBus.h` (root) and `HyperLED_Slave/include/HyperBus.h` must be kept in sync — they are separate copies, not a shared header.

## Frontend (`data/`)

Plain JS/HTML/CSS, no build step or bundler — files are uploaded to the device as-is via `pio run -t uploadfs`. `app.js` talks to the firmware's JSON API; `i18n.js` holds UI translations; `iro.min.js` is the vendored color-picker library. `plugins.js` holds the plugin pages (Einstellungen > Plugins and the "controlled by" badges on the light page). `app.js` hands it its helpers in `window.HyperUI` from inside its own `DOMContentLoaded` handler, so `plugins.js` starts from `DOMContentLoaded` too (registered later, so it runs later) - not at load. Everything a plugin delivers (names, labels, values, answers) is put into the page with `textContent` only. New texts go into `i18n.js` in de, en and ru; raise the `?v=` numbers in `index.html` when a file changes, or browsers keep the old one. For quick work on the web interface without flashing, a small server that serves `data/` from the working copy and forwards `/api/` to the device is enough (the browser then talks to `localhost`).

## Documentation

`docs/en/` and `docs/de/` (English/German) are the user-facing wiki, linked from `README.md`. Notably `docs/en/07_Master_Slave_Architektur.md` (HyperBus wiring/usage) and `docs/en/08_API_Referenz.md` (HTTP API) — keep these in sync with actual behavior when changing networking code or the API.

The plugin documentation is `docs/{de,en}/09_Plugins_nutzen.md` (for users), `10_Plugins_entwickeln.md` (file format, expression language) and `11_Plugin_Skripte.md` (Lua scripts), with working examples and a JSON Schema in `plugins/` (`beispiel-*.json`, `plugin.schema.json`). When the plugin format, the limits in `PluginDef.h`/`PluginExpr.h`/`ScriptHost`, or the effects plugins may use change, update the chapters, the schema and the examples together - and re-run the checks that keep them honest: every example installs through `POST /api/plugins/preview`, and the schema and the device agree on valid and invalid files (a differential test over about 50 mutated files was used when the schema was written). The device's own messages are German; the English chapters name them as they appear.

## Licensing

Code is EUPL-1.2 licensed (see `LICENSE`); source files carry an EUPL header comment block — preserve it when creating new source files that mirror existing ones.

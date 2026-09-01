# Features

HyperLED offers a huge range of features that make it suitable for anything from a simple LED strip behind the TV to large matrix installations or entire room lighting setups.

## Core Features
* **Extremely smooth effects:** Over 25 built-in animation effects (fire, rainbow, plasma, chase, confetti, and more) computed at up to 100 FPS, even with many LEDs.
* **Master/Slave Architecture (HyperBus):** Control multiple ESP32 boards perfectly in sync – wired over a reliable serial connection (TX/RX) or wireless via ESP-NOW.
* **Segments:** Split a single LED strip into multiple virtual zones. Each zone can have its own effect, color, and speed.
* **Presets & Playlists:** Save complete lighting moods as a preset and have several presets play back automatically in sequence.
* **Schedules:** Trigger effects or presets on a schedule (timezone-aware via NTP).

## Matrix & Widgets
* **2D matrix support:** Serpentine and progressive layout, including HUB75 scan-matrix panels.
* **Pixel Art Editor & Converter:** Draw directly in the browser or upload pixel art images – both end up as a background-image widget on the matrix.
* **Widget system:** Time, date, text, images, and an analog clock (multiple designs) can be freely placed on the matrix and scaled individually, with a live preview and no extra step.
* **Weather widget:** Shows the current temperature and weather icon (sun, clouds, rain, snow, thunderstorm) – powered by Open-Meteo, no API key required.

## Control
* **Modern web interface:** The UI uses "glassmorphism" and is heavily optimized for mobile devices. It runs as a fast single-page application served directly from the ESP32's LittleFS.
* **Physical button control:** Supports connecting push-buttons or switches directly to the ESP32 GPIO pins.

## Network & Smart Home
* **mDNS & NetBIOS support:** Reachable at `http://hyperled.local` (macOS/iOS/Linux/Android) or `http://hyperled` (Windows, via NetBIOS – works without extra software like Bonjour).
* **Wi-Fi Manager & Captive Portal:** No more hardcoded passwords. Set up Wi-Fi easily from your smartphone when the ESP32 starts in Access Point (AP) mode.
* **MQTT:** Full integration for Home Assistant, ioBroker, and other smart home hubs, including Home Assistant autodiscovery.
* **WLED-compatible JSON API:** Existing WLED automations and third-party tools mostly work without modification.
* **Over-the-Air (OTA) updates:** Update the firmware and web interface directly in the browser, without connecting the ESP32 to a PC.

## Power Supply
* **Automatic Brightness Limiting (ABL):** Estimates the live power draw of all LEDs and automatically limits brightness to protect the power supply from overload.

# Functions & Features

HyperLED offers a massive range of features, making the system perfect for both simple LED strips behind your TV as well as massive matrix installations or full-room ambient lighting.

## Core Features
* **Extremely smooth effects:** Up to 100 FPS calculation, even with many LEDs.
* **Master/Slave Architecture (HyperBus):** Control multiple ESP32 boards perfectly in sync via a highly reliable wired serial connection (TX/RX).
* **Segments:** Split a single LED strip into multiple virtual zones. Each zone can have its own effect, color, and speed.
* **Pixel Art Converter:** Upload Pixel Art images (PNG, JPG), and the ESP32 instantly converts them for LED matrices.

## Control
* **Modern Web Interface:** The UI uses "Glassmorphism" and is highly optimized for mobile devices (smartphones). It runs blazingly fast as a modern single-page application directly from the ESP32's LittleFS.
* **Android Companion App:** Automatically find all your HyperLED controllers in the network. Rename them, turn them on and off via quick buttons without having to type in IP addresses.
* **Button Control (Physical):** Supports connecting push buttons or switches directly to the ESP32 GPIO pins.

## Network & Smart Home
* **mDNS Support:** Reachable at `http://hyperled.local` (or your assigned name).
* **WLAN Manager & Captive Portal:** No more hardcoded passwords. Simply set up Wi-Fi via your smartphone when the ESP32 starts in Access Point (AP) mode.
* **MQTT:** Full integration for Home Assistant, ioBroker, and other smart home hubs. Send JSON payloads to the controller to fully remote control it.
* **Over-The-Air (OTA) Updates:** Update the firmware or the WebUI via an elegant loading animation directly in the browser, without connecting the ESP32 to a PC.

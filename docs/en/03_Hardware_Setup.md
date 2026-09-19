# Hardware Setup & Pinout

HyperLED runs on the powerful **ESP32-S3**. To achieve optimal results, proper wiring is essential.

> [!IMPORTANT]
> Always ensure that your power supply is powerful enough for your LEDs. The ESP32 CANNOT supply the LEDs directly with power (risk of damage!). Only connect the data line (DIN) and a common ground (GND) to the ESP32.

## Which ESP32 chips are suitable?

HyperLED is developed for the **ESP32-S3** and only tested on it (Master and Slave: Waveshare ESP32-S3-Zero). In theory other ESP32 chips work too – provided they bring what HyperLED needs:

- **Wi-Fi (2.4 GHz) and ESP-NOW** – for the web interface, MQTT, online updates and wireless Slaves
- **at least 4 MB flash** – two update slots of 1.44 MB each plus a 1.06 MB file system; the Master firmware already fills more than 90 % of its slot
- **enough RAM** for the web server, TLS and ESP-NOW at the same time (as Master)
- **for HUB75 panels:** a chip the panel library `esp-hub75` supports (ESP32, S2, S3, C6, P4)

### Suitable

| Chip | Cores / RAM | Master | Slave | HUB75 | Notes |
|---|---|---|---|---|---|
| **ESP32-S3** (N4R2, N8R2, N8R8, N16R8 …) | 2 × 240 MHz, 512 KB + PSRAM | ✅ | ✅ | ✅ | **The reference platform.** Best choice, especially with PSRAM. |
| **ESP32** (classic: WROOM-32, WROVER) | 2 × 240 MHz, 520 KB (WROVER + PSRAM) | ✅ | ✅ | ✅ | Fast enough. Different pinout (GPIO 34–39 input only, 6–11 taken), no native USB. |
| **ESP32-S2** (e.g. S2 Mini with PSRAM) | 1 × 240 MHz, 320 KB (+ PSRAM) | ⚠️ tight | ✅ | ✅ | Single core, little RAM – as Master only sensible with PSRAM. |
| **ESP32-C6** | 1 × 160 MHz, 512 KB | ⚠️ | ✅ | ✅ | Slower, no PSRAM. |
| **ESP32-C3** | 1 × 160 MHz, 400 KB | ⚠️ | ✅ | ❌ | LED strips only, no panel. Well suited as an inexpensive Slave. |
| **ESP32-C5** | 1 × 240 MHz, PSRAM possible | ⚠️ in theory | ⚠️ in theory | ❌ | New, Wi-Fi also on 5 GHz; only recently in the Arduino core, untested. |

✅ suitable · ⚠️ possible, with limitations · ❌ not possible

### Not suitable

| Chip | Reason |
|---|---|
| **ESP32-P4** | No Wi-Fi of its own; only conceivable with a radio companion chip (e.g. C6 via ESP-Hosted), ESP-NOW over it doubtful. |
| **ESP32-H2 / H4** | No Wi-Fi (only Bluetooth and Thread/Zigbee). |
| **ESP32-C2 (ESP8684)** | Little RAM, usually only 2–4 MB flash, not officially supported by the Arduino core. |
| **Modules with 2 MB flash** | Not enough room for two update slots. |
| **ESP8266** | Not an ESP32, a platform of its own. |

### What a different chip needs

1. **Pins:** The fixed pins in `include/Config.h` (HUB75, HyperBus UART, buttons, status LED) are laid out for the ESP32-S3.
2. **Build environment:** an `[env]` of its own in `platformio.ini`; the USB CDC settings only apply to chips with native USB.
3. **Single-core chips (S2, C3, C5, C6):** The background tasks for the MQTT connection, the connection test and the update check are pinned to core 1 and would have to move to the only core.
4. **RAM:** On single-core chips without PSRAM, running as Master with TLS (MQTTS, online update) and the web server gets tight; as a Slave that is no problem.

**Recommendation:** Master on an **ESP32-S3 with PSRAM**, or failing that a classic ESP32 (WROVER). Slaves without a panel can run inexpensively on an **ESP32-C3**, Slaves with a panel on an S3 or C6.

## Default Pinout

Certain pins are pre-configured as defaults in HyperLED. You can change these later in the WebUI.

| Function | ESP32-S3 Pin (Default) | Description |
|---|---|---|
| **LED Data Line (Data)** | **GPIO 4** | Connect this to `DIN` or `DI` of your LED strip. Freely selectable in the WebUI. |
| **Input 1 (Push button)** | **GPIO 39** | Hardwired, must be enabled in the WebUI. Switches to GND. |
| **Input 2 (Switch)** | **GPIO 40** | Hardwired, must be enabled in the WebUI. Intended for toggle switches. |

> [!NOTE]
> For a HUB75 matrix panel, the pinout is hardwired (14 signals needed at once) and shown directly in the WebUI as a reference when you select the **HUB75** LED type.

## Supported LED Types

The firmware uses the [NeoPixelBus](https://github.com/Makuna/NeoPixelBus) library to control a wide variety of digital LED chips.

> [!WARNING]
> **Check testing status:** Only the WS281X family below has been verified on real hardware. Everything else has been *code-reviewed* (2026-08-27): each chip's driver was checked against the matching NeoPixelBus feature/method classes and datasheet-documented protocol, but not physically tested. A code review can catch protocol-level mismatches (and did — see below) but cannot catch wiring- or timing-specific quirks of a real chip.

### Tested & 100% Recommended:
- **WS281X Family:**
  - **WS2812B** (The absolute standard, 5V)
  - **WS2813** (With backup data line)
  - **WS2815** (12V version, ideal for long runs without voltage drop)

### Code-reviewed (driver verified against NeoPixelBus/datasheet, not yet hardware-tested):
- **SK6812** (Often with dedicated white channel, e.g., RGBW)
- **APA102 / SK9822** (Requires both Data AND Clock lines - pin configurable in UI)
- **WS2801** (Older 4-pin standard, plain RGB-over-SPI)
- **LPD8806**
- **LPD6803** (16-bit-word SPI protocol)
- **P9813 / PP9813** (Own per-pixel checksum framing)
- **TM1814**, **TM1914** (Require a chip-specific settings header before pixel data)
- **TM1829, UCS8903, APA106, WS2811 (White), WS281X WWA** (Treated as WS2812-protocol-compatible clones)
- **FW1906, UCS8904** (RGBW clones)
- **WS2805, SM16825** (RGBCW clones with cold+warm white channels)
- **On/Off, PWM (1-5 channel analog/CCT)**

> [!NOTE]
> **Bug fixed 2026-08-27:** WS2801, LPD6803, and P9813 were previously wired to the APA102/DotStar protocol driver in the firmware — a genuine protocol mismatch (different SPI framing entirely) that would have prevented those three chip types from working on real hardware. TM1914 was missing its required settings header. Both are now fixed; if you have one of these strips, please re-flash and let us know whether it works.

## Wiring Best Practices
1. **Level Shifter:** The ESP32 outputs 3.3V logic on data pins. Many LEDs (like WS2812B) officially expect 5V logic. It often works without one, but if you experience flickering, you should use a logic level shifter (e.g., 74AHCT125).
2. **Capacitor:** Connect a large capacitor (e.g., 1000 µF, 6.3V or higher) across the power supply (VCC and GND) of the LED strip to buffer current spikes when turning it on.
3. **Resistor:** Place a resistor (approx. 330 to 470 Ohm) in the data line between the ESP32 pin (GPIO 4) and the first LED chip. This protects the pin from voltage spikes.

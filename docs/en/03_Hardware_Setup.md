# Hardware Setup & Pinout

HyperLED runs on the powerful **ESP32-S3**. To achieve optimal results, proper wiring is essential.

> [!IMPORTANT]
> Always ensure that your power supply is powerful enough for your LEDs. The ESP32 CANNOT supply the LEDs directly with power (risk of damage!). Only connect the data line (DIN) and a common ground (GND) to the ESP32.

## Default Pinout

Certain pins are pre-configured as defaults in HyperLED. You can change these later in the WebUI.

| Function | ESP32-S3 Pin (Default) | Description |
|---|---|---|
| **LED Data Line (Data)** | **GPIO 4** | Connect this to `DIN` or `DI` of your LED strip. |
| **Button 1 (Push)** | *Not active* | Can be activated in the UI to connect physical push buttons. Switches to GND. |
| **Button 2 (Switch)** | *Not active* | Intended for toggle switches. |

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

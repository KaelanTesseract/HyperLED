# Hardware Setup & Pinout

HyperLED runs on the powerful **ESP32-C6**. To achieve optimal results, proper wiring is essential.

> [!IMPORTANT]
> Always ensure that your power supply is powerful enough for your LEDs. The ESP32 CANNOT supply the LEDs directly with power (risk of damage!). Only connect the data line (DIN) and a common ground (GND) to the ESP32.

## Default Pinout

Certain pins are pre-configured as defaults in HyperLED. You can change these later in the WebUI.

| Function | ESP32-C6 Pin (Default) | Description |
|---|---|---|
| **LED Data Line (Data)** | **GPIO 4** | Connect this to `DIN` or `DI` of your LED strip. |
| **Button 1 (Push)** | *Not active* | Can be activated in the UI to connect physical push buttons. Switches to GND. |
| **Button 2 (Switch)** | *Not active* | Intended for toggle switches. |

## Supported LED Types

The firmware uses state-of-the-art libraries to control a wide variety of digital LED chips.

> [!WARNING]
> **Check testing status:** Since hardware tests are complex, not all chips have been physically verified yet.

### Tested & 100% Recommended:
- **WS281X Family:**
  - **WS2812B** (The absolute standard, 5V)
  - **WS2813** (With backup data line)
  - **WS2815** (12V version, ideal for long runs without voltage drop)

### Supported (But untested):
- **SK6812** (Often with dedicated white channel, e.g., RGBW)
- **APA102 / SK9822** (Requires both Data AND Clock lines - pin configurable in UI)
- **WS2801** (Older 4-pin standard)
- **LPD8806** 

## Wiring Best Practices
1. **Level Shifter:** The ESP32 outputs 3.3V logic on data pins. Many LEDs (like WS2812B) officially expect 5V logic. It often works without one, but if you experience flickering, you should use a logic level shifter (e.g., 74AHCT125).
2. **Capacitor:** Connect a large capacitor (e.g., 1000 µF, 6.3V or higher) across the power supply (VCC and GND) of the LED strip to buffer current spikes when turning it on.
3. **Resistor:** Place a resistor (approx. 330 to 470 Ohm) in the data line between the ESP32 pin (GPIO 4) and the first LED chip. This protects the pin from voltage spikes.

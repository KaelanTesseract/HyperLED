# The Web Interface & MQTT

The core of operating HyperLED is the modern glassmorphism web interface. It runs directly from the ESP32 and needs no internet connection.

## Main Screen

The main screen is organized into three areas:

1. **Segments:** List of all segments (Master and any Slaves). Select which segment you're currently editing here.
2. **Color & Brightness:** Color picker, brightness, speed, intensity, and palette for the currently selected segment. The **"Edit Elements"** button jumps straight to the matrix widgets (time, text, weather, etc.).
3. **Effects:** Click-to-select from over 25 built-in animation effects.

The gear icon in the top right opens Settings, with further tabs:

| Tab | Content |
|---|---|
| **LED** | LED type, count, data pin, physical buttons, Automatic Brightness Limiting (ABL). |
| **Matrix** | 2D matrix setup, live preview, pixel art editor/upload, HUB75 pinout reference. |
| **Segments** | Create, edit, and delete segments. |
| **Presets** | Save and apply lighting moods, and play several back automatically as a playlist. |
| **Schedules** | Trigger effects or presets on a schedule (timezone-aware via NTP). |
| **Slaves** | Name and configure automatically discovered Slave boards (LED type, pins, or HUB75 matrix size). |
| **WLAN / MQTT** | Network and smart home settings. |
| **System** | IP address, firmware version, OTA updates, factory reset. |

---

## MQTT Settings (Smart Home)

To integrate HyperLED into systems like Home Assistant, ioBroker, or Node-RED, the firmware features a robust MQTT client with Home Assistant autodiscovery.

### Configuration
Go to the **WLAN / MQTT** tab in the web interface and scroll down:
1. **Enable MQTT:** Check the box.
2. **Broker IP:** Enter the IP address of your MQTT server (e.g., Mosquitto).
3. **Port:** Default is `1883`.
4. **User / Password:** If your broker requires authentication.
5. **Topic:** Provide a unique name (e.g., `livingroom/hyperled`). The controller listens for commands on this topic.

> [!TIP]
> **Payloads:** The controller expects JSON payloads identical to the `/json/state` API (see the [API Reference](08_API_Referenz.md)). Sending a payload like `{"on": true, "bri": 255}` to the configured topic turns the controller on at full brightness.

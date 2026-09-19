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
| **System** | IP address, firmware version, OTA updates, backup and restore, factory reset. |

---

## MQTT Settings (Smart Home)

To integrate HyperLED into systems like Home Assistant, ioBroker, or Node-RED, the firmware features a robust MQTT client with Home Assistant autodiscovery.

### Configuration
Go to the **WLAN / MQTT** tab in the web interface and scroll down:
1. **Enable MQTT:** Check the box.
2. **Broker IP:** Enter the IP address of your MQTT server (e.g., Mosquitto).
3. **Port:** Default is `1883`.
4. **User / Password:** If your broker requires authentication.
5. **Base topic:** Leave empty for the default `hyperled/<MAC>`. To give several controllers readable names, enter e.g. `livingroom/hyperled` – each controller needs a topic of its own.

Saving restarts the controller, which then connects. Home Assistant finds it on its own through autodiscovery, also after Home Assistant restarts.

### What appears in Home Assistant
* **One light per segment** – segments on a Slave included. The colour controls follow the LED hardware: RGB, RGBW, colour temperature (strips with two white channels and CCT LEDs), brightness only, or on/off only.
* **Effects** with the same names as in the web interface. Clock/text, image and the panel effects are only offered for segments on a HUB75 panel (or a matrix).
* **"Alle Segmente"** switches and dims all segments together (only with more than one segment).
* **Per segment:** speed and intensity (in %) and the palette; for panels the background effect.
* **Texts:** every text and Lauftext element is a text entity, so an automation can put e.g. "washing machine done" on the panel.
* **Scene** (select) – shows the scene activated last until something changes afterwards. Plus **playlist** and **sync** as switches.
* **Buttons:** every enabled button is an event ("kurz", "lang" or "umgelegt") for automations. On the controller it keeps doing what it did.
* **Diagnostics:** Wi-Fi signal, free memory, IP address, start time, number and time of Wi-Fi failures, and per Slave whether it is connected (with its firmware version) – so Home Assistant can notify you of an outage.
* **Firmware:** shows a new release and installs it at the press of a button (scenes, schedules and images are kept). Plus a **restart** button and the **status LED** as a light of its own.
* **Availability:** When the controller is off or unreachable, Home Assistant shows its lights as unavailable.
* **Leaving:** Switching MQTT off (or resetting the controller) removes its lights from Home Assistant instead of leaving them behind as unavailable. Switching it on again brings them back with the same IDs.
* Every change – from the web interface, buttons, schedules, scenes or the playlist – reaches Home Assistant right away. New, renamed or deleted segments are picked up automatically.

> [!TIP]
> **Topics:** Commands go in JSON to `<base>/ha/set` (all segments) or `<base>/seg<N>/ha/set` (one segment), for example `{"state": "ON", "brightness": 255}`. The controller reports its state on the same paths with `/ha/state`, and its availability on `<base>/status` (`online` / `offline`).

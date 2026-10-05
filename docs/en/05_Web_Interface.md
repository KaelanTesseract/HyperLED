# The Web Interface & MQTT

The core of operating HyperLED is the modern glassmorphism web interface. It runs directly from the ESP32 and needs no internet connection.

## The five areas

The menu has five areas (at the bottom on a phone, on the left on a larger screen). The round power button at the top right switches all segments on or off.

| Area | Content |
|---|---|
| **Light** | The segments (Master and any Slaves) with the **Sync** option, brightness, speed, intensity and palette of the selected segment, the colour wheel and the over 25 built-in effects. If a plugin controls a segment, a notice at the top offers its **Settings** and **Pause**. |
| **Scenes** | **Presets** (save and apply lighting moods), the **playlist** (play several presets in turn) and **schedules** (switch on, off or apply a preset at set times, timezone-aware via NTP). |
| **Panel** | The live preview of the selected panel and its **elements** (time, date, text, scrolling text, image, analog clock, weather), the background effect and the pixel art editor/upload. |
| **Devices** | Three tabs: **Segments** (create, edit and delete segments), **Connected devices** (automatically found Slave boards: name and configure them: LED type, pins, or HUB75 matrix size) and **LED hardware** (LED type, count, data pin, physical buttons, Automatic Brightness Limiting, 2D matrix setup and canvas, HUB75 pinout as a reference). |
| **Settings** | Three tabs: **Wi-Fi & MQTT** (network and smart home), **Plugins** (add, set up, switch on and off, look at live values; see [Using Plugins](09_Plugins_nutzen.md)) and **System** (language, firmware version, online and local updates, backup and restore, factory reset). |

> A beginner-friendly tour with screenshots is in the [project wiki](https://github.com/KaelanTesseract/HyperLED/wiki).

---

## MQTT Settings (Smart Home)

To integrate HyperLED into systems like Home Assistant, ioBroker, or Node-RED, the firmware features a robust MQTT client with Home Assistant autodiscovery.

### Configuration
Go to the **WLAN / MQTT** tab in the web interface and scroll down:
1. **Enable MQTT:** Check the box.
2. **Broker IP:** Enter the IP address of your MQTT server (e.g., Mosquitto).
3. **Port:** Default is `1883`, with TLS usually `8883`.
4. **Encrypted (TLS):** Only switch this on if your broker offers TLS. The connection is then encrypted; HyperLED does not check the broker's certificate (home brokers mostly use self-signed ones).
5. **User / Password:** If your broker requires authentication. The web interface never shows the stored password – an empty field keeps it.
6. **Base topic:** Leave empty for the default `hyperled/<MAC>`. To give several controllers readable names, enter e.g. `livingroom/hyperled` – each controller needs a topic of its own.

**Test connection** tries the values in the form without saving them. Saving restarts the controller, which then connects. Home Assistant finds it on its own through autodiscovery, also after Home Assistant restarts.

Above the fields you see whether the controller is connected – and if not, why (e.g. "wrong user name or password" or "broker unreachable"). **Announce to Home Assistant again** sends all device information once more, in case something is missing in Home Assistant.

### Quick guide: adding HyperLED to Home Assistant
1. **Set up a broker:** In Home Assistant, install and start the **Mosquitto broker** add-on under *Settings → Add-ons → Add-on Store*. Home Assistant then suggests the **MQTT** integration – confirm it.
2. **Create a user:** Under *Settings → People → Users*, create a user of its own for HyperLED (e.g. `hyperled`). Mosquitto accepts Home Assistant users as MQTT logins.
3. **Connect HyperLED:** In the HyperLED web interface under *Settings → Wi-Fi & MQTT*, enable MQTT, enter Home Assistant's IP address as the server, port `1883`, and the user and password from step 2. **Test connection**, then **Save MQTT and restart**.
4. **Find the device:** After a few seconds the device **HyperLED-xxxxxx** appears under *Settings → Devices & Services → MQTT*, with all its lights and settings.
5. **Automations:** For example, put a text on the panel when the washing machine is done:

```yaml
action: text.set_value
target:
  entity_id: text.hyperled_xxxxxx_panel_lauftext
data:
  value: "Washing machine done"
```

Home Assistant shows the entity id with each text element. For a notification on radio failures, use the **WLAN-Ausfälle** sensor (it counts up).

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

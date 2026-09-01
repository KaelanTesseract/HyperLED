# API Reference

HyperLED offers a comprehensive HTTP JSON API for automation systems, MQTT, and scripts. The core is **WLED-compatible** – existing WLED integrations (e.g. Home Assistant, Node-RED) mostly work without modification.

## WLED-compatible Control

### `GET /json/state`
Returns the Master controller's current state.

*Example response (abridged):*
```json
{
  "on": true,
  "bri": 128,
  "transition": 7,
  "ps": -1,
  "pl": -1,
  "seg": [
    {
      "name": "Master",
      "start": 0,
      "stop": 30,
      "on": true,
      "bri": 128,
      "effect": 0,
      "speed": 128,
      "intensity": 128,
      "color": 16711680,
      "color2": 0,
      "color2Enabled": false,
      "palette": 0,
      "widgets": []
    }
  ]
}
```

### `POST /json/state`
Changes the state. Expects strict JSON with a correct `Content-Length` header.

*Example payloads:*
- Turn off: `{"on": false}`
- Turn on at full brightness: `{"on": true, "bri": 255}`
- **Toggle:** `{"on": "t"}` (switches between on/off, useful for physical buttons)
- Adjust a segment: `{"seg": [{"id": 0, "effect": 5, "col": [[255,0,0]]}]}`

### `GET /json`
Combines `state`, `info`, `effects` (list of all effect names), and `palettes` (list of all palette names) into one response – handy for the initial fetch when an integration connects.

### `POST /json`
Accepts the same payload as `POST /json/state` (WLED-compatible alias).

### `GET /json/info`
Device information (firmware version, free heap, uptime, MAC address, name, etc.), matching the `info` object from `GET /json`.

### `GET /json/eff` / `GET /json/pal`
Return the plain list of effect or palette names as a JSON array (the index matches the `effect`/`palette` number in the state).

---

## Segments & Configuration

| Endpoint | Method | Description |
|---|---|---|
| `/api/segments` | GET / POST | Read segments, or create/edit/delete them. |
| `/api/state` | GET / POST | Compact state endpoint (alternative to `/json/state`). |
| `/api/config` | GET / POST | Base LED configuration (type, count, pin, ABL). |
| `/api/buttons` | GET / POST | Physical button/switch configuration. |

---

## Matrix & Widgets

| Endpoint | Method | Description |
|---|---|---|
| `/api/matrix_preview` | GET | Live preview of the Master matrix's current pixel content. |
| `/api/matrix_config` | POST | 2D matrix setup (width, height, layout). |
| `/api/canvas_panels` | GET / POST | Multi-panel canvas (combine several panels into one larger area). |
| `/api/text_widgets` | POST | Set widgets (time, date, text, image, analog clock, weather) for a segment. |
| `/api/text_widget_image` | POST | Upload image data for an image widget (pixel art editor/converter). |
| `/api/weather_status` | GET | Current status of the weather widget (temperature, icon). |
| `/api/weather_location` | POST | Set the location for the weather widget (geocoded via Open-Meteo). |

---

## Slaves (HyperBus)

| Endpoint | Method | Description |
|---|---|---|
| `/api/slaves` | GET | List of all currently reachable Slaves (ID, name, LED count, version, wired/wireless). |
| `/api/slaves/config` | POST | Configure a Slave (name, LED type, pins, or HUB75 matrix size/driver). |
| `/api/slaves/update` | POST | Trigger a remote firmware update for a Slave. |

See [Master/Slave Architecture](07_Master_Slave_Architektur.md) for how this works.

---

## Presets, Playlists & Schedules

| Endpoint | Method | Description |
|---|---|---|
| `/presets.json` | GET | All saved presets as raw data. |
| `/api/presets/save` | POST | Save the current state as a preset (`{"id": 0, "name": "Living Room"}` – `id: 0` auto-assigns the next free ID). |
| `/api/presets/apply` | POST | Apply a preset (`{"id": 1}`). |
| `/api/presets/delete` | POST | Delete a preset (`{"id": 1}`). |
| `/api/playlist` | GET / POST | Read or set the playlist (sequence of several presets). |
| `/api/schedules` | GET / POST | Read or set schedules. |
| `/api/time` | GET | Current NTP time and sync status. |

---

## Network, System & OTA

| Endpoint | Method | Description |
|---|---|---|
| `/api/scan` | GET | Starts an asynchronous Wi-Fi scan (mostly used in AP mode). |
| `/api/scan_results` | GET | Results of the scan (SSID, signal strength, etc.). |
| `/api/save_wifi` | POST | Save Wi-Fi credentials. |
| `/api/wifi/status` | GET | Current connection status. |
| `/api/status` | GET | Compact system status. |
| `/api/version` | GET | Firmware version. |
| `/api/info` | GET | Device information (compact variant of `/json/info`). |
| `/api/update_online` | POST | Checks for a new release and starts the update. |
| `/api/update_progress` | GET | Progress of an ongoing OTA update. |
| `/update` | POST | Manual firmware/filesystem upload (multipart form, same as flashing via the WebUI). |
| `/api/mqtt` | GET / POST | Read or save MQTT settings. |
| `/api/factory_reset` | POST | Resets all settings (LED pins, buttons, Wi-Fi) to factory defaults and forces a reboot. |

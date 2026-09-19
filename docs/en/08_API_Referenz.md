# API Reference

HyperLED offers an HTTP JSON API for scripts and its own web interface. For Home Assistant and other smart home hubs, MQTT with Home Assistant autodiscovery is the intended way.

## State

### `GET /api/state`
Returns the current state: `on` (at least one segment is lit), `sync` (synchronised segments) and all segments.

*Example response (abridged):*
```json
{
  "sync": false,
  "on": true,
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

### `POST /api/state`
Changes the state. Every field is optional.

*Example payloads:*
- Turn everything off: `{"on": false}`
- **Toggle:** `{"on": "t"}` (off if any segment is lit, otherwise everything on - handy for buttons)
- Turn on synchronised segments: `{"sync": true}`
- Adjust a segment: `{"seg": [{"id": 0, "on": true, "bri": 200, "effect": 5, "color": "#ff0000"}]}`

Per segment, `on`, `bri`, `effect`, `speed`, `intensity`, `palette`, `color`, `color2`, `color2Enabled`, `white`, `whiteOnly` and `cct` are understood.

---

## Segments & Configuration

| Endpoint | Method | Description |
|---|---|---|
| `/api/segments` | GET / POST | Read segments, or create/edit/delete them. |
| `/api/state` | GET / POST | Read and change the state (see above). |
| `/api/config` | GET / POST | Base LED configuration (type, count, pin, ABL). |
| `/api/buttons` | GET / POST | Physical button/switch configuration. |

---

## Matrix & Widgets

| Endpoint | Method | Description |
|---|---|---|
| `/api/matrix_preview` | GET | Live preview of the Master matrix's current pixel content. With `?seg=N` the live pixels of segment N instead; `&s=K` returns only every K-th pixel, and with `&w=<panel width>` the step applies to rows and columns (result: a grid of ceil(w/K) columns). |
| `/api/matrix_config` | POST | 2D matrix setup (width, height, layout). |
| `/api/canvas_panels` | GET / POST | Multi-panel canvas (combine several panels into one larger area). |
| `/api/text_widgets` | POST | Set widgets (time, date, text, image, analog clock, weather, marquee/scrolling text) for a segment. Each widget may carry `bri` (0–255, default 255), its own brightness on top of the segment's, and `legib`, how it stays readable in front of a background effect (0 = none, 1 = dark outline, the default, 2 = dark box). |
| `/api/panel_background` | POST | Set the background effect behind a segment's widgets: `{seg, effect, bri, speed, intensity, palette, color, color2, color2Enabled}`. `effect` 255 turns it off; `bri` is relative to the segment's brightness. `/api/segments` reports the setting as `bg`. |
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
| `/api/info` | GET | Device information and diagnostics (firmware, memory, uptime, Wi-Fi, last outage). |
| `/api/update_online` | POST | Checks for a new release and starts the update. |
| `/api/update_progress` | GET | Progress of an ongoing OTA update. |
| `/update` | POST | Manual firmware/filesystem upload (multipart form, same as flashing via the WebUI). |
| `/api/mqtt` | GET / POST | Read or save MQTT settings. An empty `topic` means the default, which `GET` returns as `defaultTopic` (`hyperled/<mac>`). `GET` never returns the password, only `passSet` (whether one is stored); an empty `pass` on `POST` keeps the stored one, an empty `user` removes both. Saving restarts the controller. |
| `/api/backup` | POST / GET | Backup of all settings: `POST` starts it, `GET` collects the JSON file (`202` while it is still being written). Contains every stored setting including the Wi-Fi and MQTT passwords, plus scenes, playlist, schedules and the images of image elements. |
| `/api/restore` | POST | Upload a backup file (`multipart/form-data`). The controller checks the whole file, then replaces all settings and files and restarts. `?wifi=1` also takes the Wi-Fi credentials from the file; without it the controller keeps its own. Works on another controller too. |
| `/api/factory_reset` | POST | Resets all settings (LED pins, buttons, Wi-Fi) to factory defaults and forces a reboot. |

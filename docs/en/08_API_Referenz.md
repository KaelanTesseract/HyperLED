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
| `/api/slaves` | GET | List of all currently reachable Slaves (ID, name, LED count, version, wired/wireless, `scripts`: whether the Slave runs Lua scripts - firmware 0.3.000 and later). While a script runs on a Slave, `script` shows its state (`state`: 0 none, 1 loading, 2 running, 3 failed), `result`, `fps`, `frameMs`, `frameCrc`, `memoryKb`, `message` and `ageMs`. |
| `/api/slaves/config` | POST | Configure a Slave (name, LED type, pins, or HUB75 matrix size/driver). |
| `/api/slaves/update` | POST | Trigger a remote firmware update of the slaves (`{"url": "https://…"}`, at most 116 characters, optionally `"id"` to update only that Slave instead of all; without `url` the newest Slave release the Master knows of – `409` with `no_release` while it knows none). Response: `sealed` (credentials handed over encrypted, slave 0.2.008 and later), `wired` (older slaves over the cable), `skipped` (older slaves over radio – they no longer get the Wi-Fi password over the air and need one update over USB). |

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

## Plugins

A plugin is one JSON file that reads a value from the network and shows it on one segment (see the plugin documentation). The web interface (Settings > Plugins) uses these routes; they can also be called from scripts. Bodies are JSON (`Content-Type: application/json`); errors come as `{"error": "…"}` with a message in German and the place where the problem is.

| Endpoint | Method | Description |
|---|---|---|
| `/api/plugins` | GET | All plugins: `id`, `state` (`off`, `waiting`, `running`, `no_connection`, `incompatible`, `invalid`), `reason`, `enabled`, `name`, `version`, `author`, `license`, `description`, `force`, `allow_power`, `wants_power`, `script_level`, `warning`, `settings` (a password is returned as `***`, or empty when unset, and never otherwise), `data` (the values read). A plugin with a script also has `script`: `mode` (`script` or `rules`), `note` (why the rules apply instead of the script), `state`, `message`, `fps`, `frame_ms`, `memory_kb`. Also `api` (the interface level of this firmware), `max` (how many plugins fit). |
| `/api/plugins/preview` | POST | Checks a plugin file completely (format, expressions, script, compatibility) **without saving anything**. The body is the plugin file. Returns `name`, `version`, `author`, `license`, `description`, `source_url` (the address template, with `{setting}` placeholders), `segment_setting`, `wants_power`, `has_script`, `script_level`, `compatible`, `compat_note`, `replaces` (and `installed_version`), `limit_reached`. |
| `/api/plugins/install` | POST | Installs (or replaces) a plugin. The body is the plugin file (at most 16 KB). Settings of an installed version are kept as far as they still fit. A script is compiled first; a syntax error is refused with its line. Returns `{"id": "…"}`. |
| `/api/plugins/fetch` | POST | `{"url": "https://…"}`: the controller loads a plugin file from an address (it follows redirects, up to 16 KB, 10 s). Returns `202`; `409` while another load is running. |
| `/api/plugins/fetch_result` | GET | How that load is going: `{"state": "idle" \| "running" \| "done" \| "error", "text": "…", "error": "…"}`. A finished result is handed over once. |
| `/api/plugins/definition` | GET | `?id=…`: the settings of a plugin for building a form (`type`, `label` and `hint` per language, `default`, `min`, `max`, `optional`, `options`), the `effects` a plugin may use, `segment_setting`, `wants_power`, `has_script`. |
| `/api/plugins/settings` | POST | `{"id": "…", "values": {"key": value}}`: sets some settings. The device checks every value; an empty password keeps the stored one. |
| `/api/plugins/enable` | POST | `{"id": "…", "enabled": true}`: switches a plugin on or off. Refused with the reason when a setting is still empty, or when another plugin controls the segment (one segment, one plugin). |
| `/api/plugins/options` | POST | `{"id": "…", "force": bool, "allow_power": bool}`: `force` runs a plugin although it needs another interface level; `allow_power` lets it switch the segment on and off. |
| `/api/plugins/values` | GET | `?id=…`: what the plugin reads right now: `values`, `source` (who draws: `script`, `rules`, `on_error`, `none`), `rule` (`index`, `when`), `raw` (the beginning of the last answer of the source, at most 1 KB), and for a script its `script` state. |
| `/api/plugins/remove` | POST | `{"id": "…"}`: removes a plugin with its settings. |

While a plugin controls a segment, `GET /api/state` shows `plugin: {id, name}` on that segment. This is never stored: it does not reach presets, `/api/segments` or MQTT.

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
| `/api/log` | GET | The last minute of everything the controller prints on its serial port, as text (each line with "how many seconds ago"). The controller keeps this itself in memory; no connected computer is needed. |
| `/api/lastlog` | GET | The minute before the last restart that was **not routine** (a crash, a watchdog, a restart because Wi-Fi or the radio had died), with the reason and the uptime. It lives in one file that the next such restart overwrites; `404` if there has been none yet. An update or a restart after changing settings leaves it alone. |
| `/api/update_online` | POST | Starts the online update to the given version (`{"version": "0.2.002"}`). Scenes, playlist, schedules and images are kept. |
| `/api/update_status` | GET | What the Master's own release check found: `installed`, `latest`, `slaveLatest` (empty while unknown), `checking`, `checkedAgo` (seconds, `-1` = never), `updating`. Checked a minute after start and then twice a day. |
| `/api/update_check` | POST | Starts that check right away (at most once a minute). |
| `/api/update_progress` | GET | Progress of an ongoing OTA update. |
| `/update` | POST | Manual firmware/filesystem upload (multipart form, same as flashing via the WebUI). |
| `/api/mqtt` | GET / POST | Read or save MQTT settings. An empty `topic` means the default, which `GET` returns as `defaultTopic` (`hyperled/<mac>`). `GET` never returns the password, only `passSet` (whether one is stored); an empty `pass` on `POST` keeps the stored one, an empty `user` removes both. `tls` switches on MQTTS. `GET` returns the running connection's state under `status` (`connected`, `error`, `since` in seconds). Saving restarts the controller. |
| `/api/mqtt/test` | POST / GET | `POST` tries the form values (as for saving) with a connection of its own, without saving them; `GET` returns `result`: `running`, `ok` or the reason it failed. |
| `/api/mqtt/resync` | POST | Announces the controller to Home Assistant again with every entity and state (`409` when not connected). |
| `/api/backup` | POST / GET | Backup of all settings: `POST` starts it, `GET` collects the JSON file (`202` while it is still being written). Contains every stored setting including the Wi-Fi and MQTT passwords, plus scenes, playlist, schedules and the images of image elements. |
| `/api/restore` | POST | Upload a backup file (`multipart/form-data`). The controller checks the whole file, then replaces all settings and files and restarts. `?wifi=1` also takes the Wi-Fi credentials from the file; without it the controller keeps its own. Works on another controller too. |
| `/api/factory_reset` | POST | Resets all settings (LED pins, buttons, Wi-Fi) to factory defaults and forces a reboot. |

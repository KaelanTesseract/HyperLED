# API Reference

The HyperLED system provides various HTTP endpoints for automation systems and the Android app.

### Status & Control (`/json/state`)

**`GET /json/state`**
Returns the current state.
*Example Response:*
```json
{
  "on": true,
  "bri": 255,
  "transition": 7
}
```

**`POST /json/state`**
Allows changing the state. The controller strictly expects JSON and a defined Content-Length in the header!
*Payloads:*
- Turn Off: `{"on": false}`
- Turn On: `{"on": true}`
- **Toggle:** `{"on": "t"}` (Switches between on and off, useful for physical buttons).

### System & Network (`/api/`)

**`GET /api/scan`**
Starts an asynchronous Wi-Fi scan (mostly used in AP mode).

**`GET /api/scan_results`**
Returns the results of the scan (SSID, signal strength, etc.).

**`POST /api/factory_reset`**
Resets all settings (LED Pins, Buttons, Wi-Fi) to factory defaults and forces a reboot.

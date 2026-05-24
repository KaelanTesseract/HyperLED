# API Referenz

Das HyperLED-System bietet diverse HTTP-Endpoints für Automatisierungssysteme und die Android-App.

### Status & Steuerung (`/json/state`)

**`GET /json/state`**
Gibt den aktuellen Zustand zurück.
*Beispiel-Antwort:*
```json
{
  "on": true,
  "bri": 255,
  "transition": 7
}
```

**`POST /json/state`**
Erlaubt es, den Zustand zu verändern. Der Controller erwartet striktes JSON und eine definierte Content-Length im Header!
*Payloads:*
- Ausschalten: `{"on": false}`
- Einschalten: `{"on": true}`
- **Toggle (Umschalten):** `{"on": "t"}` (Wechselt zwischen an und aus, nützlich für physische Buttons).

### System & Netzwerk (`/api/`)

**`GET /api/scan`**
Startet einen asynchronen WLAN-Scan (wird meistens im AP-Modus genutzt).

**`GET /api/scan_results`**
Liefert die Resultate des Scans zurück (SSID, Signalstärke etc.).

**`POST /api/factory_reset`**
Setzt alle Einstellungen (LED Pins, Buttons, WLAN) auf den Werkszustand zurück und erzwingt einen Neustart.

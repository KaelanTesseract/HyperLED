# API Referenz

HyperLED bietet eine umfangreiche HTTP-JSON-API für Automatisierungssysteme, MQTT und Skripte. Der Kern ist **WLED-kompatibel** – bestehende WLED-Integrationen (z. B. Home Assistant, Node-RED) funktionieren größtenteils ohne Anpassung.

## WLED-kompatible Steuerung

### `GET /json/state`
Gibt den aktuellen Zustand des Master-Controllers zurück.

*Beispiel-Antwort (gekürzt):*
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
Ändert den Zustand. Erwartet striktes JSON mit korrektem `Content-Length`-Header.

*Beispiel-Payloads:*
- Ausschalten: `{"on": false}`
- Einschalten mit voller Helligkeit: `{"on": true, "bri": 255}`
- **Toggle:** `{"on": "t"}` (wechselt zwischen an/aus, nützlich für physische Buttons)
- Segment anpassen: `{"seg": [{"id": 0, "effect": 5, "col": [[255,0,0]]}]}`

### `GET /json`
Kombiniert `state`, `info`, `effects` (Liste aller Effektnamen) und `palettes` (Liste aller Palettennamen) in einer Antwort – praktisch für den ersten Abruf beim Verbindungsaufbau einer Integration.

### `POST /json`
Nimmt denselben Payload wie `POST /json/state` entgegen (WLED-kompatibler Alias).

### `GET /json/info`
Geräteinformationen (Firmware-Version, freier Heap, Uptime, MAC-Adresse, Name usw.), analog zum `info`-Objekt aus `GET /json`.

### `GET /json/eff` / `GET /json/pal`
Liefern jeweils die reine Liste der Effekt- bzw. Palettennamen als JSON-Array (Index entspricht der `effect`/`palette`-Nummer im State).

---

## Segmente & Konfiguration

| Endpunkt | Methode | Beschreibung |
|---|---|---|
| `/api/segments` | GET / POST | Segmente auslesen bzw. anlegen bearbeiten oder löschen. |
| `/api/state` | GET / POST | Kompakter Zustands-Endpunkt (Alternative zu `/json/state`). |
| `/api/config` | GET / POST | LED-Grundkonfiguration (Typ, Anzahl, Pin, ABL). |
| `/api/buttons` | GET / POST | Konfiguration der physischen Taster/Schalter. |

---

## Matrix & Widgets

| Endpunkt | Methode | Beschreibung |
|---|---|---|
| `/api/matrix_preview` | GET | Aktueller Pixelinhalt der Master-Matrix als Live-Vorschau. |
| `/api/matrix_config` | POST | 2D-Matrix-Setup (Breite, Höhe, Layout). |
| `/api/canvas_panels` | GET / POST | Multi-Panel-Canvas (mehrere Panels zu einer großen Fläche zusammenschalten). |
| `/api/text_widgets` | POST | Widgets (Uhrzeit, Datum, Text, Bild, Analoguhr, Wetter) für ein Segment setzen. |
| `/api/text_widget_image` | POST | Bilddaten für ein Bild-Widget hochladen (Pixel-Art-Editor/-Converter). |
| `/api/weather_status` | GET | Aktueller Status des Wetter-Widgets (Temperatur, Symbol). |
| `/api/weather_location` | POST | Standort für das Wetter-Widget festlegen (Geocoding über Open-Meteo). |

---

## Slaves (HyperBus)

| Endpunkt | Methode | Beschreibung |
|---|---|---|
| `/api/slaves` | GET | Liste aller aktuell erreichbaren Slaves (ID, Name, LED-Anzahl, Version, kabelgebunden/kabellos). |
| `/api/slaves/config` | POST | Slave konfigurieren (Name, LED-Typ, Pins bzw. HUB75-Matrixgröße/Treiber). |
| `/api/slaves/update` | POST | Firmware-Update eines Slaves aus der Ferne anstoßen. |

Details zur Funktionsweise siehe [Master/Slave Architektur](07_Master_Slave_Architektur.md).

---

## Presets, Playlists & Zeitpläne

| Endpunkt | Methode | Beschreibung |
|---|---|---|
| `/presets.json` | GET | Alle gespeicherten Presets als Rohdaten. |
| `/api/presets/save` | POST | Aktuellen Zustand als Preset speichern (`{"id": 0, "name": "Wohnzimmer"}` – `id: 0` vergibt automatisch die nächste freie ID). |
| `/api/presets/apply` | POST | Preset anwenden (`{"id": 1}`). |
| `/api/presets/delete` | POST | Preset löschen (`{"id": 1}`). |
| `/api/playlist` | GET / POST | Playlist (Abfolge mehrerer Presets) auslesen bzw. festlegen. |
| `/api/schedules` | GET / POST | Zeitpläne auslesen bzw. festlegen. |
| `/api/time` | GET | Aktuelle NTP-Zeit und Sync-Status. |

---

## Netzwerk, System & OTA

| Endpunkt | Methode | Beschreibung |
|---|---|---|
| `/api/scan` | GET | Startet einen asynchronen WLAN-Scan (v. a. im AP-Modus genutzt). |
| `/api/scan_results` | GET | Ergebnisse des Scans (SSID, Signalstärke usw.). |
| `/api/save_wifi` | POST | WLAN-Zugangsdaten speichern. |
| `/api/wifi/status` | GET | Aktueller Verbindungsstatus. |
| `/api/status` | GET | Kompakter System-Status. |
| `/api/version` | GET | Firmware-Version. |
| `/api/info` | GET | Geräteinformationen (kompakte Variante von `/json/info`). |
| `/api/update_online` | POST | Prüft auf ein neues Release und startet das Update. |
| `/api/update_progress` | GET | Fortschritt eines laufenden OTA-Updates. |
| `/update` | POST | Manueller Firmware-/Dateisystem-Upload (Multipart-Formular, wie beim Flashen über die WebUI). |
| `/api/mqtt` | GET / POST | MQTT-Einstellungen auslesen bzw. speichern. |
| `/api/factory_reset` | POST | Setzt alle Einstellungen (LED-Pins, Taster, WLAN) auf den Werkszustand zurück und erzwingt einen Neustart. |

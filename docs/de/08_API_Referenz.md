# API Referenz

HyperLED bietet eine HTTP-JSON-API für Skripte und die eigene Weboberfläche. Für Home Assistant und andere Smart-Home-Zentralen ist MQTT mit Home-Assistant-Autodiscovery der vorgesehene Weg.

## Zustand

### `GET /api/state`
Gibt den aktuellen Zustand zurück: `on` (mindestens ein Segment leuchtet), `sync` (Gleichlauf) und alle Segmente.

*Beispiel-Antwort (gekürzt):*
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
Ändert den Zustand. Alle Felder sind optional.

*Beispiel-Payloads:*
- Alles ausschalten: `{"on": false}`
- **Umschalten:** `{"on": "t"}` (aus, wenn irgendein Segment leuchtet, sonst alles an – praktisch für Taster)
- Gleichlauf einschalten: `{"sync": true}`
- Segment anpassen: `{"seg": [{"id": 0, "on": true, "bri": 200, "effect": 5, "color": "#ff0000"}]}`

Pro Segment werden `on`, `bri`, `effect`, `speed`, `intensity`, `palette`, `color`, `color2`, `color2Enabled`, `white`, `whiteOnly` und `cct` verstanden.

---

## Segmente & Konfiguration

| Endpunkt | Methode | Beschreibung |
|---|---|---|
| `/api/segments` | GET / POST | Segmente auslesen bzw. anlegen bearbeiten oder löschen. |
| `/api/state` | GET / POST | Zustand lesen und ändern (siehe oben). |
| `/api/config` | GET / POST | LED-Grundkonfiguration (Typ, Anzahl, Pin, ABL). |
| `/api/buttons` | GET / POST | Konfiguration der physischen Taster/Schalter. |

---

## Matrix & Widgets

| Endpunkt | Methode | Beschreibung |
|---|---|---|
| `/api/matrix_preview` | GET | Aktueller Pixelinhalt der Master-Matrix als Live-Vorschau. Mit `?seg=N` stattdessen die aktuellen Pixel von Segment N; `&s=K` liefert nur jeden K-ten Pixel, mit `&w=<Panelbreite>` gilt der Schritt für Zeilen und Spalten (Ergebnis: ein Raster mit ceil(w/K) Spalten). |
| `/api/matrix_config` | POST | 2D-Matrix-Setup (Breite, Höhe, Layout). |
| `/api/canvas_panels` | GET / POST | Multi-Panel-Canvas (mehrere Panels zu einer großen Fläche zusammenschalten). |
| `/api/text_widgets` | POST | Widgets (Uhrzeit, Datum, Text, Bild, Analoguhr, Wetter, Lauftext) für ein Segment setzen. Jedes Widget kann mit `bri` (0–255, Standard 255) eine eigene Helligkeit bekommen, die zur Segment-Helligkeit hinzukommt, und mit `legib` festlegen, wie es vor einem Hintergrund-Effekt lesbar bleibt (0 = nichts, 1 = dunkler Umriss, Standard, 2 = dunkler Kasten). |
| `/api/panel_background` | POST | Hintergrund-Effekt hinter den Widgets eines Segments setzen: `{seg, effect, bri, speed, intensity, palette, color, color2, color2Enabled}`. `effect` 255 schaltet ihn aus; `bri` gilt im Verhältnis zur Segment-Helligkeit. In `/api/segments` steht die Einstellung unter `bg`. |
| `/api/text_widget_image` | POST | Bilddaten für ein Bild-Widget hochladen (Pixel-Art-Editor/-Converter). |
| `/api/weather_status` | GET | Aktueller Status des Wetter-Widgets (Temperatur, Symbol). |
| `/api/weather_location` | POST | Standort für das Wetter-Widget festlegen (Geocoding über Open-Meteo). |

---

## Slaves (HyperBus)

| Endpunkt | Methode | Beschreibung |
|---|---|---|
| `/api/slaves` | GET | Liste aller aktuell erreichbaren Slaves (ID, Name, LED-Anzahl, Version, kabelgebunden/kabellos). |
| `/api/slaves/config` | POST | Slave konfigurieren (Name, LED-Typ, Pins bzw. HUB75-Matrixgröße/Treiber). |
| `/api/slaves/update` | POST | Firmware-Update der Slaves aus der Ferne anstoßen (`{"url": "https://…"}`, höchstens 116 Zeichen). Antwort: `sealed` (verschlüsselt übergeben, ab Slave 0.2.008), `wired` (ältere Slaves per Kabel), `skipped` (ältere Slaves per Funk – bekommen das WLAN-Passwort nicht mehr über Funk und brauchen einmal ein Update per USB). |

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
| `/api/info` | GET | Geräteinformationen und Diagnose (Firmware, Speicher, Laufzeit, WLAN, letzter Ausfall). |
| `/api/update_online` | POST | Prüft auf ein neues Release und startet das Update. |
| `/api/update_progress` | GET | Fortschritt eines laufenden OTA-Updates. |
| `/update` | POST | Manueller Firmware-/Dateisystem-Upload (Multipart-Formular, wie beim Flashen über die WebUI). |
| `/api/mqtt` | GET / POST | MQTT-Einstellungen auslesen bzw. speichern. `topic` leer bedeutet den Standard, den `GET` als `defaultTopic` mitliefert (`hyperled/<mac>`). Das Passwort gibt `GET` nie heraus, nur `passSet` (ob eines gespeichert ist); ein leeres `pass` beim `POST` behält das gespeicherte, ein leerer `user` löscht beide. Nach dem Speichern startet der Controller neu. |
| `/api/backup` | POST / GET | Sicherung aller Einstellungen: `POST` stößt sie an, `GET` holt die JSON-Datei ab (`202`, solange sie noch entsteht). Enthält alle gespeicherten Einstellungen samt WLAN- und MQTT-Passwort sowie Szenen, Playlist, Zeitpläne und die Bilder der Bild-Elemente. |
| `/api/restore` | POST | Sicherungsdatei hochladen (`multipart/form-data`). Der Controller prüft die Datei vollständig, ersetzt dann alle Einstellungen und Dateien und startet neu. `?wifi=1` übernimmt auch die WLAN-Zugangsdaten aus der Datei; ohne behält der Controller seine eigenen. Funktioniert auch auf einem anderen Controller. |
| `/api/factory_reset` | POST | Setzt alle Einstellungen (LED-Pins, Taster, WLAN) auf den Werkszustand zurück und erzwingt einen Neustart. |

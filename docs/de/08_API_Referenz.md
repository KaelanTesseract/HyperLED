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
- Gleichlauf ein- oder ausschalten: `{"sync": true}` / `{"sync": false}`
- Segment anpassen: `{"seg": [{"id": 0, "on": true, "bri": 200, "effect": 5, "color": "#ff0000"}]}`

Pro Segment werden `on`, `bri`, `effect`, `speed`, `intensity`, `palette`, `color`, `color2`, `color2Enabled`, `white`, `whiteOnly` und `cct` verstanden.

---

## Segmente & Konfiguration

| Endpunkt | Methode | Beschreibung |
|---|---|---|
| `/api/segments` | GET / POST | Segmente auslesen bzw. anlegen bearbeiten oder löschen. Ein Slave-Segment trägt `isSlave`, `slaveId`, `sharesPower` und `ablMa` (die Strombegrenzung in mA für einen Slave mit eigenem Netzteil, 0 = keine). |
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
| `/api/slaves` | GET | Liste aller aktuell erreichbaren Slaves (ID, Name, LED-Anzahl, Version, kabelgebunden/kabellos, `scripts`: ob der Slave Lua-Skripte ausführt – Firmware 0.3.000 und später, `chip`: der Chip, für den der Slave gebaut ist, `esp32s3`, `esp32c6` oder `esp32`, leer bei einem Slave vor 0.3.006). Läuft auf einem Slave ein Skript, zeigt `script` dessen Zustand (`state`: 0 keins, 1 lädt, 2 läuft, 3 fehlgeschlagen), `result`, `fps`, `frameMs`, `frameCrc`, `memoryKb`, `message` und `ageMs`. |
| `/api/slaves/config` | POST | Slave konfigurieren (Name, LED-Typ, Pins bzw. HUB75-Matrixgröße/Treiber). |
| `/api/slaves/update` | POST | Firmware-Update der Slaves aus der Ferne anstoßen (`{"url": "https://…"}`, höchstens 116 Zeichen; das Wort `{chip}` darin steht für den Chip des Slaves, `…/firmware-{chip}.bin` ist also für einen ESP32-S3 `firmware-esp32s3.bin`: Ein Slave ab 0.3.005 setzt es selbst ein, der Master tut es für einen älteren; optional `"id"`, um nur diesen einen Slave statt aller zu aktualisieren; ohne `url` das neueste Slave-Release, das der Master kennt – `409` mit `no_release`, solange er keines kennt). Antwort: `sealed` (verschlüsselt übergeben, ab Slave 0.2.008), `wired` (ältere Slaves per Kabel), `skipped` (ältere Slaves per Funk – bekommen das WLAN-Passwort nicht mehr über Funk und brauchen einmal ein Update per USB). |

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

## Plugins

Ein Plugin ist eine JSON-Datei, die einen Wert aus dem Netzwerk liest und auf einem Segment anzeigt (siehe die Plugin-Dokumentation). Die Weboberfläche (Einstellungen > Plugins) benutzt diese Routen; sie lassen sich auch aus Skripten aufrufen. Die Bodys sind JSON (`Content-Type: application/json`); Fehler kommen als `{"error": "…"}` mit einer deutschen Meldung und der Stelle des Problems.

| Endpunkt | Methode | Beschreibung |
|---|---|---|
| `/api/plugins` | GET | Alle Plugins: `id`, `state` (`off`, `waiting`, `running`, `no_connection`, `incompatible`, `invalid`), `reason`, `enabled`, `name`, `version`, `author`, `license`, `description`, `force`, `allow_power`, `wants_power`, `script_level`, `warning`, `settings` (ein Passwort kommt als `***`, leer wenn nicht gesetzt, sonst nie), `data` (die gelesenen Werte). Ein Plugin mit Skript hat außerdem `script`: `mode` (`script` oder `rules`), `note` (warum statt des Skripts die Regeln gelten), `state`, `message`, `fps`, `frame_ms`, `memory_kb`. Dazu `api` (Schnittstellenstufe dieser Firmware) und `max` (wie viele Plugins passen). |
| `/api/plugins/preview` | POST | Prüft eine Plugin-Datei vollständig (Format, Ausdrücke, Skript, Verträglichkeit) und **speichert nichts**. Der Body ist die Plugin-Datei. Liefert `name`, `version`, `author`, `license`, `description`, `source_url` (die Adressvorlage mit `{einstellung}`-Platzhaltern), `segment_setting`, `wants_power`, `has_script`, `script_level`, `compatible`, `compat_note`, `replaces` (und `installed_version`), `limit_reached`. |
| `/api/plugins/install` | POST | Installiert (oder ersetzt) ein Plugin. Der Body ist die Plugin-Datei (höchstens 16 KB). Einstellungen einer installierten Fassung bleiben erhalten, soweit sie noch passen. Ein Skript wird zuerst übersetzt; ein Syntaxfehler wird mit Zeile abgelehnt. Liefert `{"id": "…"}`. |
| `/api/plugins/fetch` | POST | `{"url": "https://…"}`: Das Gerät lädt eine Plugin-Datei von einer Adresse (folgt Weiterleitungen, bis 16 KB, 10 s). Antwort `202`; `409`, solange ein anderer Ladevorgang läuft. |
| `/api/plugins/fetch_result` | GET | Stand dieses Ladevorgangs: `{"state": "idle" \| "running" \| "done" \| "error", "text": "…", "error": "…"}`. Ein fertiges Ergebnis wird einmal ausgeliefert. |
| `/api/plugins/definition` | GET | `?id=…`: die Einstellungen eines Plugins zum Bauen eines Formulars (`type`, `label` und `hint` je Sprache, `default`, `min`, `max`, `optional`, `options`), die `effects`, die ein Plugin benutzen darf, `segment_setting`, `wants_power`, `has_script`. |
| `/api/plugins/settings` | POST | `{"id": "…", "values": {"schluessel": wert}}`: setzt einzelne Einstellungen. Das Gerät prüft jeden Wert; ein leeres Passwort behält das gespeicherte. |
| `/api/plugins/enable` | POST | `{"id": "…", "enabled": true}`: schaltet ein Plugin ein oder aus. Wird mit Grund abgelehnt, wenn eine Einstellung noch leer ist oder ein anderes Plugin das Segment steuert (ein Segment, ein Plugin). |
| `/api/plugins/options` | POST | `{"id": "…", "force": bool, "allow_power": bool}`: `force` führt ein Plugin aus, obwohl es eine andere Schnittstellenstufe braucht; `allow_power` erlaubt ihm, das Segment ein- und auszuschalten. |
| `/api/plugins/values` | GET | `?id=…`: was das Plugin gerade liest: `values`, `source` (wer zeichnet: `script`, `rules`, `on_error`, `none`), `rule` (`index`, `when`), `raw` (der Anfang der letzten Antwort der Quelle, höchstens 1 KB) und bei einem Skript dessen Zustand unter `script`. |
| `/api/plugins/remove` | POST | `{"id": "…"}`: entfernt ein Plugin samt Einstellungen. |

Solange ein Plugin ein Segment steuert, zeigt `GET /api/state` an diesem Segment `plugin: {id, name}`. Das wird nie gespeichert: Es gelangt weder in Voreinstellungen noch in `/api/segments` oder MQTT.

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
| `/api/log` | GET | Die letzte Minute von allem, was der Controller auf der seriellen Schnittstelle ausgibt, als Text (jede Zeile mit „vor wie vielen Sekunden“). Der Controller führt das selbst im Speicher mit, ein angeschlossener Computer ist dafür nicht nötig. |
| `/api/lastlog` | GET | Die letzte Minute vor dem letzten Neustart, der **kein Routineschritt** war (Absturz, Watchdog, Neustart, weil WLAN oder Funk ausgefallen waren), mit Grund und Laufzeit. Liegt in einer Datei, die der nächste solche Neustart überschreibt; `404`, wenn es noch keinen gab. Ein Update oder ein Neustart nach Einstellungen lässt sie unberührt. |
| `/api/update_online` | POST | Startet das Online-Update auf die angegebene Version (`{"version": "0.2.002"}`). Szenen, Playlist, Zeitpläne und Bilder bleiben erhalten. |
| `/api/update_status` | GET | Was die eigene Release-Prüfung des Masters gefunden hat: `installed`, `latest`, `slaveLatest` (leer, solange unbekannt), `checking`, `checkedAgo` (Sekunden, `-1` = noch nie), `updating`. Geprüft wird eine Minute nach dem Start und dann zweimal am Tag. |
| `/api/update_check` | POST | Stößt diese Prüfung sofort an (höchstens einmal pro Minute). |
| `/api/update_progress` | GET | Fortschritt eines laufenden OTA-Updates. |
| `/update` | POST | Manueller Firmware-/Dateisystem-Upload (Multipart-Formular, wie beim Flashen über die WebUI). |
| `/api/mqtt` | GET / POST | MQTT-Einstellungen auslesen bzw. speichern. `topic` leer bedeutet den Standard, den `GET` als `defaultTopic` mitliefert (`hyperled/<mac>`). Das Passwort gibt `GET` nie heraus, nur `passSet` (ob eines gespeichert ist); ein leeres `pass` beim `POST` behält das gespeicherte, ein leerer `user` löscht beide. `tls` schaltet MQTTS ein. `GET` liefert unter `status` den Zustand der laufenden Verbindung (`connected`, `error`, `since` in Sekunden). Nach dem Speichern startet der Controller neu. |
| `/api/mqtt/test` | POST / GET | `POST` probiert die Formularwerte (wie beim Speichern) mit einer eigenen Verbindung aus, ohne sie zu speichern; `GET` liefert `result`: `running`, `ok` oder den Fehlergrund. |
| `/api/mqtt/resync` | POST | Meldet den Controller mit allen Entitäten und Zuständen erneut bei Home Assistant an (`409`, wenn nicht verbunden). |
| `/api/backup` | POST / GET | Sicherung aller Einstellungen: `POST` stößt sie an, `GET` holt die JSON-Datei ab (`202`, solange sie noch entsteht). Enthält alle gespeicherten Einstellungen samt WLAN- und MQTT-Passwort sowie Szenen, Playlist, Zeitpläne und die Bilder der Bild-Elemente. |
| `/api/restore` | POST | Sicherungsdatei hochladen (`multipart/form-data`). Der Controller prüft die Datei vollständig, ersetzt dann alle Einstellungen und Dateien und startet neu. `?wifi=1` übernimmt auch die WLAN-Zugangsdaten aus der Datei; ohne behält der Controller seine eigenen. Funktioniert auch auf einem anderen Controller. |
| `/api/factory_reset` | POST | Setzt alle Einstellungen (LED-Pins, Taster, WLAN) auf den Werkszustand zurück und erzwingt einen Neustart. |

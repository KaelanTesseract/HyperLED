# Update-Funktion für mehrere Chips (ESP32-S3 und ESP32-C6)

Ziel: Das Online-Update erkennt selbst, welche Firmware zum Chip gehört, und HyperLED läuft später
auch auf dem ESP32-C6. Zuerst wird der Dateiname der S3-Firmware umgestellt (Phase 1), danach kommt
die Absicherung (Phase 2), dann die Slaves (Phase 3) und zuletzt der Chip selbst (Phase 4).

**Regel:** Erledigtes wird abgehakt (`[x]`), nicht gelöscht.

## Entscheidungen

- Dateiname der Firmware im Release: `firmware-<chip>.bin`, also `firmware-esp32s3.bin` und später
  `firmware-esp32c6.bin`. Der Chip steht beim Bauen fest (`HYPERLED_CHIP` in `include/Config.h`).
- **Der alte Name `firmware.bin` entfällt** (Entscheidung vom 2026-10-06): Alle laufenden Geräte werden
  einmal von Hand gebracht. Ab 0.3.003 liefert ein Release nur `firmware-esp32s3.bin`
  und `littlefs-esp32s3.bin`. Folge: Ein Gerät bis 0.3.001, das sich aus einem Release ab 0.3.002 selbst
  aktualisieren will, scheitert (es sucht `firmware.bin`), und weil seine alte Fassung das Dateisystem
  vor der Firmware ersetzt, bliebe es mit neuer Weboberfläche auf alter Firmware zurück. Solche Geräte
  müssen von Hand aktualisiert werden (USB oder Lokales Update).
- **Auch das Dateisystem heißt nach dem Chip: `littlefs-<chip>.bin`** (Entscheidung vom 2026-10-06, ab
  Release 0.3.003; es gibt keinen Rückfall auf `littlefs.bin`). Solange Partitionstabelle (4 MB,
  Dateisystem 704 KB) und Weboberfläche gleich sind, ist `littlefs-esp32c6.bin` dieselbe Datei wie
  `littlefs-esp32s3.bin`, nur unter zweitem Namen. Das Image trägt keine Chip-Kennung, eine Prüfung wie bei
  der Firmware (Phase 2) ist dort nicht möglich. Die Weboberfläche nennt feste S3-Pins (zum Beispiel
  „Eingang 1 (Pin 39)“, HUB75-Pinbelegung); beim C6 muss sie dafür angepasst werden, die Datei kann dann
  wirklich abweichen.
- Fehlt im Release die Datei für den Chip, bricht das Update ab, **bevor** etwas überschrieben wird
  (Status `error_nofw`), damit nie eine neue Weboberfläche auf einer alten Firmware landet.
- Slaves: Der Master baut die Download-Adresse der Slave-Firmware auf `firmware-esp32s3.bin`
  (alle Slaves sind bisher S3); ab einem Slave mit anderem Chip siehe Phase 3.

## Phase 1: Dateiname der S3-Firmware

- [x] Namensschema festgelegt (siehe Entscheidungen), alter Name entfällt
- [x] `HYPERLED_CHIP` in `include/Config.h` (`esp32s3`, `esp32c6`, `esp32`; unbekannter Chip: Build-Fehler)
- [x] `UpdateManager::performUpdate` nimmt `firmware-<chip>.bin`
- [x] Fehlt die Datei, bricht das Update vor dem ersten Schreiben ab (`error_nofw`)
- [x] Meldung dafür in der Weboberfläche (`dyn_update_no_firmware` in de, en, ru; `app.js` und `i18n.js` hochgezählt)
- [x] Master-Download der Slave-Firmware auf `firmware-esp32s3.bin` umgestellt (`src/AppWebServer.cpp`)
- [x] Texte des Lokalen Updates nennen `firmware-esp32s3.bin`
- [x] Versionen Master und Slave auf 0.3.002; beide bauen für `esp32-s3`
- [x] Commit und Push (Master, Slave, Wiki)
- [x] Releases **0.3.002**: Master (`firmware-esp32s3.bin`, `littlefs.bin`) und Slave (`firmware-esp32s3.bin`)
- [x] Alle drei Geräte auf 0.3.002: Slave HUB75, Slave 1 UART, Master (Firmware und Weboberfläche, vorher Sicherung)
- [ ] Prüfung auf dem Gerät: Ein Gerät mit 0.3.002 aktualisiert sich aus dem **nächsten** Release über
      `firmware-esp32s3.bin`; Slaves über „Geräte jetzt aktualisieren“
- [ ] Prüfung: Release ohne passende Datei lässt das Gerät unverändert (`error_nofw`)
- [x] Doku und Wiki: Namen der Release-Dateien („Updates, Sicherung, Reset“)

## Phase 1b: Dateisystem nach Chip benannt (Release 0.3.003)

- [x] `UpdateManager` nimmt `littlefs-<chip>.bin`; fehlt eine der beiden Dateien, bricht das Update vor dem ersten Schreiben ab
- [x] Texte des Lokalen Updates und Wiki nennen `littlefs-esp32s3.bin`; Master auf 0.3.003 (Slaves bleiben auf 0.3.002, ihr Code ist unverändert)
- [x] Commit und Push, Release **0.3.003** (nur Master: `firmware-esp32s3.bin`, `littlefs-esp32s3.bin`)
- [x] Master auf 0.3.003 (von Hand, weil 0.3.002 noch `littlefs.bin` sucht; Sicherung vor- und zurückspielen)
- [ ] Echter Test: Release 0.3.004 (ohne alten Namen) aktualisiert den Master über `firmware-esp32s3.bin` und `littlefs-esp32s3.bin` selbst

## Release-Ablauf

1. `pio run -e esp32-s3` und `pio run -e esp32-s3 -t buildfs` im Master, `pio run -e esp32-s3` im Slave-Ordner.
2. Aus `.pio/build/esp32-s3/` kopieren und umbenennen: `firmware.bin` → `firmware-esp32s3.bin`,
   `littlefs.bin` → `littlefs-esp32s3.bin` (Slave: `firmware.bin` → `firmware-esp32s3.bin`).
3. Tag gleich der Version in `include/Config.h`; Master-Release mit beiden Dateien, Slave-Release mit seiner.
4. Zuerst das Slave-Release, dann das des Masters (der Master holt die Slave-Firmware aus dem Slave-Release).

## Phase 2: Chip-Prüfung im Firmware-Image

Ein Image für den falschen Chip kann ein Gerät lahmlegen. Der Kopf der Datei nennt den Chip
(Byte 12 und 13, Little Endian; S3 `0x0009`, C6 `0x000D`, ESP32 `0x0000`), Byte 0 ist `0xE9`.

- [x] `firmwareImageIsForThisChip` (`include/UpdateManager.h`) und `HYPERLED_CHIP_ID` (`include/Config.h`)
- [x] Online-Update: Vorab-Prüfung der ersten 16 Bytes per Range-Anfrage, **bevor** das Dateisystem ersetzt wird
      (Status `error_chip`), und noch einmal beim Download selbst (bricht ab, bevor `Update.end()` etwas aktiviert)
- [x] Manueller Upload (`POST /update`): falscher Chip, falsche Kennung oder keine Firmware wird mit `400 wrong_chip` abgewiesen, bevor etwas geschrieben wird
- [x] Meldung für „falscher Chip“ in de, en, ru; Weboberfläche behandelt beide Wege (`app.js`/`i18n.js` hochgezählt)
- [x] Am Gerät geprüft (0.3.004): Image mit C6-Kennung, Image mit falschem Byte 0 und eine Zufallsdatei wurden abgewiesen (HTTP 400),
      der Master lief unverändert weiter und startete danach wieder mit 0.3.004; ein echtes S3-Image wird angenommen
- [ ] Online-Pfad am Gerät prüfen: braucht ein kurzlebiges Test-Release mit absichtlich falscher Kennung (danach löschen) oder
      das Release 0.3.005 mit dem echten Update-Test. Bis dahin ist dieser Weg nur gebaut, nicht erprobt

## Phase 3: Slaves

- [ ] Frage klären: Wie erfährt der Master den Chip eines Slaves? Zwei Wege: (a) der Slave meldet ihn
      im Ping/Pong (neues Feld, Protokolländerung in beiden Repos), (b) der Master schickt die URL der
      `firmware-esp32s3.bin`, und der Slave ersetzt den Namen selbst durch `firmware-<chip>.bin` (keine
      Protokolländerung). Empfehlung: (b)
- [x] Release-Datei der Slave-Firmware heißt `firmware-esp32s3.bin` (ab 0.3.002, kein alter Name)
- [x] Master baut die Slave-URL (`src/AppWebServer.cpp`, `/api/slaves/update`) auf diesen Namen (alle Slaves sind bisher S3)
- [ ] Für Slaves mit anderem Chip: den Dateinamen nach Weg (b) setzen lassen
- [ ] Chip-Prüfung (Phase 2) auch im Slave

## Phase 4: ESP32-C6

Nicht lauffähig, solange diese Stellen nicht angepasst sind (aus dem Quelltext gelesen, noch nicht
für den C6 gebaut):

- Hintergrund-Tasks sind fest an **Kern 1** gebunden (`MqttManager`, `PluginManager`, `UpdateManager`);
  der C6 hat nur einen Kern.
- Das Skript-System legt Bildpuffer im **PSRAM** an (`ScriptTask.cpp`, `ScriptHost.cpp`), und der
  `UpdateManager` sichert Dateien dort; der C6 hat keinen PSRAM.
- `NeoPixelBus` (RMT-Treiber) und `esp-hub75` auf dem C6 prüfen; die HUB75-Speicherprüfung
  rechnet mit etwa 250 KB freiem Speicher.
- USB-Konfiguration (`ARDUINO_USB_*`), Pinbelegung (`include/Config.h`, HUB75, HyperBus-UART,
  Taster, Status-LED), Partitionstabelle.
- Ein Kern mit 160 MHz teilt sich Webserver, Funk, Effekte und Lua; der Master ist hier am
  schwächsten. **Deshalb zuerst der Slave.**

Schritte:

- [ ] Klären: Steht ein ESP32-C6-Board zum Testen zur Verfügung?
- [ ] Dateisystem: gleiche Partitionstabelle und gleiche Weboberfläche wie beim S3? Sonst `littlefs-<chip>.bin`
- [ ] Build-Umgebung `esp32-c6` in `platformio.ini` (pioarduino-Plattform)
- [ ] Slave für den C6 **nur kompilieren**, Fehlerliste festhalten
- [ ] Master für den C6 **nur kompilieren**, Fehlerliste festhalten
- [ ] Entscheiden, ob und wie weit der Master lohnt (Speicher und Leistung am Gerät messen)
- [ ] Stellen oben anpassen (Kerne, PSRAM, Treiber, Pins), jeweils auf dem S3 gegenprüfen
- [ ] Slave auf dem C6 am Gerät testen (Strip, HUB75, Skripte, Funk, Online-Update)
- [ ] Master auf dem C6 am Gerät testen (falls entschieden)
- [ ] Releases liefern `firmware-esp32c6.bin` (Master und Slave); Beschreibung in README, `docs/*/03`,
      Wiki („Was du brauchst“) und Chip-Tabelle anpassen

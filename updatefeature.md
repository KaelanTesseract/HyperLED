# Update-Funktion für mehrere Chips (ESP32-S3 und ESP32-C6)

Ziel: Das Online-Update erkennt selbst, welche Firmware zum Chip gehört, und HyperLED läuft später
auch auf dem ESP32-C6. Zuerst wird der Dateiname der S3-Firmware umgestellt (Phase 1), danach kommt
die Absicherung (Phase 2), dann die Slaves (Phase 3), die Frage, welche Chips überhaupt in Frage kommen
(Phase 3.5), und zuletzt der Chip selbst (Phase 4).

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
- [x] Prüfung auf dem Gerät: Das Online-Update über `firmware-esp32s3.bin` läuft (Master, Test-Release); Slaves über „Geräte jetzt aktualisieren“ sind noch offen (Phase 3)
- [ ] Prüfung: Release ohne passende Datei lässt das Gerät unverändert (`error_nofw`)
- [x] Doku und Wiki: Namen der Release-Dateien („Updates, Sicherung, Reset“)

## Phase 1b: Dateisystem nach Chip benannt (Release 0.3.003)

- [x] `UpdateManager` nimmt `littlefs-<chip>.bin`; fehlt eine der beiden Dateien, bricht das Update vor dem ersten Schreiben ab
- [x] Texte des Lokalen Updates und Wiki nennen `littlefs-esp32s3.bin`; Master auf 0.3.003 (Slaves bleiben auf 0.3.002, ihr Code ist unverändert)
- [x] Commit und Push, Release **0.3.003** (nur Master: `firmware-esp32s3.bin`, `littlefs-esp32s3.bin`)
- [x] Master auf 0.3.003 (von Hand, weil 0.3.002 noch `littlefs.bin` sucht; Sicherung vor- und zurückspielen)
- [x] Echter Test des Online-Updates mit `firmware-esp32s3.bin` und `littlefs-esp32s3.bin` (siehe Phase 2, Test-Release mit den Dateien von 0.3.004)

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
- [x] Online-Pfad am Gerät geprüft (2026-10-06, Master 0.3.004) mit kurzlebigen Test-Releases (Vor-Release, danach gelöscht):
      ein Release mit absichtlich falscher Kennung ließ das Update bei `error_chip` enden, **bevor** das Dateisystem ersetzt wurde
      (der Master lief unverändert weiter); ein Release mit den echten Dateien lief durch (Dateisystem, Firmware, Neustart),
      alle Nutzerdaten waren danach unverändert

## Phase 3: Slaves

Entscheidung (2026-10-06): Weg (b). Der Master schickt die Adresse mit dem Wort `{chip}`
(`…/firmware-{chip}.bin`), und der Slave setzt seinen eigenen Chip ein; das Funkprotokoll bleibt, wie es ist.
Ein Slave unter 0.3.005 kennt das Wort nicht. Für ihn schreibt der Master `esp32s3` ein (alle alten Slaves sind
S3). Gemeinsame Datei in beiden Repositories: `include/ChipId.h` (Chip-Name, Kennung und die Prüfung).

- [x] Release-Datei der Slave-Firmware heißt `firmware-esp32s3.bin` (ab 0.3.002, kein alter Name)
- [x] Master baut die Slave-Adresse mit `{chip}` (`src/AppWebServer.cpp`) und löst sie je Slave auf (`SlaveManager::triggerSlaveUpdate`:
      Slave ab 0.3.005 setzt selbst ein, ein älterer bekommt `esp32s3`; auch beim alten Kabelweg)
- [x] Slave setzt `{chip}` ein und prüft vor dem Schreiben die ersten 16 Bytes der Datei (`performOtaUpdate`); bei falschem Chip
      oder nicht lesbarer Datei startet er ohne Update neu
- [x] `include/ChipId.h` in beiden Repositories (identisch), Master-Code darauf umgestellt; beide Firmwares 0.3.005
- [x] Am Gerät geprüft (Slave HUB75 und Slave 1 UART, Master 0.3.005): `{chip}` wird eingesetzt; ein Image mit falscher Kennung wird
      abgewiesen (der Slave fragt die Datei nur zweimal an, lädt sie aber nicht); mit passender Kennung läuft das Update (dreimal);
      der Master löst `{chip}` für den alten Slave 185 selbst auf, und beide Slaves kommen auf 0.3.005
- [x] Commit, Push und Releases 0.3.005 (Slave zuerst, dann Master)
- [x] Prüfung über „Geräte jetzt aktualisieren“ aus dem veröffentlichten Slave-Release (der Master holt `firmware-{chip}.bin`): beide Slaves verschwanden nach dem Aufruf für etwa zehn Sekunden vom Funk und kamen mit 0.3.005 zurück (Download selbst nicht einsehbar, die Slaves haben keine Konsole am Rechner)
- [ ] Für einen Slave mit anderem Chip (C6): in Phase 4 mit dem ersten C6-Slave durchspielen

## Phase 3.5: Welche ESP32-Chips kommen in Frage

Stand 2026-10-06. Die Chip-Daten stammen aus den Vergleichen von
[esp32.co.uk](https://esp32.co.uk/esp32-c-versions-compared-2026-guide/),
[espboards.dev](https://www.espboards.dev/blog/esp32-soc-options/) und dem Stand des
[Arduino-Kerns](https://github.com/espressif/arduino-esp32); die Spalten „HUB75-Bibliothek“ und „Rolle“ sind
aus unserem Quelltext und `docs/*/03_Hardware_Setup.md` abgeleitet. Zahlen mit „ca.“ vor einer Entscheidung am
Datenblatt gegenprüfen.

### Was HyperLED von einem Chip braucht

- **Wi-Fi 2.4 GHz und ESP-NOW** (Master: Weboberfläche, MQTT, Update; Slave: Funk, Update). Ohne Wi-Fi geht nur
  ein Slave am Kabel (HyperBus über UART) und er ist nicht über die Luft zu aktualisieren.
- **Mindestens 4 MB Flash** für zwei Update-Plätze (je 1,625 MB) und das Dateisystem (704 KB).
- **Speicher:** Der Master braucht Platz für Webserver, TLS (MQTTS, GitHub), Plugins und Lua. Ein HUB75-Panel
  64 × 64 braucht ca. 82 KB DMA-Speicher im internen RAM.
- **Ein LED-Ausgang** (Datenpin über RMT oder SPI, ein Kanal genügt) und für Panels die Bibliothek `esp-hub75`.
- Aus dem Quelltext (Phase 4): Aufgaben sind an **Kern 1** gebunden (nur Zwei-Kern-Chips laufen so), Bildpuffer der
  Skripte liegen im **PSRAM** (Chips ohne PSRAM brauchen einen Rückfall), der Funkkanal des Masters ist der Kanal
  seines WLANs (ein Master im 5-GHz-Band könnte 2,4-GHz-Slaves nicht mitnehmen).

### Die Chips

| Chip | Kerne und Takt | SRAM | PSRAM | Funk | Arduino / PlatformIO | HUB75-Bibliothek |
|---|---|---|---|---|---|---|
| **ESP32** (WROOM, WROVER) | Xtensa, 1–2 × 240 MHz | 520 KB | nur WROVER, 4–8 MB | Wi-Fi 4, BT + BLE | ja | ja |
| **ESP32-S2** | Xtensa, 1 × 240 MHz | 320 KB | optional, ca. 2 MB | Wi-Fi 4 (kein BT) | ja | ja |
| **ESP32-S3** (Referenz) | Xtensa, 2 × 240 MHz | 512 KB | optional, 2–8 MB | Wi-Fi 4, BLE 5 | ja | ja |
| **ESP32-C2** (ESP8684) | RISC-V, 1 × 120 MHz | ca. 272 KB | nein | Wi-Fi 4, BLE 5 | nur als IDF-Komponente | nein |
| **ESP32-C3** | RISC-V, 1 × 160 MHz | 400 KB | nein | Wi-Fi 4, BLE 5 | ja | nein |
| **ESP32-C5** | RISC-V, 1 × 240 MHz | ca. 384 KB | optional | Wi-Fi 6 **2,4 und 5 GHz**, BLE 5, 802.15.4 | ja (neu) | nein |
| **ESP32-C6** | RISC-V, 1 × 160 MHz | 512 KB | nein | Wi-Fi 6 (2,4 GHz), BLE 5, 802.15.4 | ja (pioarduino) | ja |
| **ESP32-C61** | RISC-V, 1 × 160 MHz | 320 KB | optional | Wi-Fi 6 (2,4 GHz), BLE 5 | nur als IDF-Komponente | unklar |
| **ESP32-H2** | RISC-V, 1 × 96 MHz | 320 KB | nein | **kein Wi-Fi**, BLE 5, 802.15.4 | ja | nein |
| **ESP32-P4** | RISC-V, 2 × 400 MHz | 768 KB | 16–32 MB im Gehäuse | **kein Wi-Fi** (Zusatzchip nötig) | ja | ja |

Nicht gemeint: ESP8266 (andere Plattform) und Module mit 2 MB Flash (zu klein für zwei Update-Plätze).

### Wofür sie taugen

✅ geeignet · ⚠️ mit Einschränkung oder noch nicht erprobt · ❌ nicht sinnvoll

| Chip | Master | Slave mit Streifen | Slave mit HUB75-Panel | Anmerkung |
|---|---|---|---|---|
| **ESP32** | ✅ (WROVER) / ⚠️ (WROOM ohne PSRAM) | ✅ | ✅ | `esp32dev` steht schon in `platformio.ini`; andere Pins, kein natives USB; Einkern-Varianten ⚠️ |
| **ESP32-S2** | ⚠️ (ein Kern, wenig RAM) | ✅ | ✅ | ein Kern: Aufgabenbindung anpassen |
| **ESP32-S3** | ✅ | ✅ | ✅ | Referenz, alles getestet |
| **ESP32-C2** | ❌ | ⚠️ (nur 4-MB-Variante) | ❌ | wenig RAM, Arduino nur als IDF-Komponente |
| **ESP32-C3** | ⚠️ (RAM knapp) | ✅ (günstig) | ❌ | gut als einfacher Streifen-Slave |
| **ESP32-C5** | ⚠️ (5-GHz-Funkkanal, neu) | ⚠️ (neu, nicht erprobt) | ❌ (Bibliothek) | Im 5-GHz-Band könnten 2,4-GHz-Slaves dem Master nicht folgen |
| **ESP32-C6** | ⚠️ (ein Kern, kein PSRAM) | ✅ | ✅ | erster Kandidat für Phase 4 (Slave) |
| **ESP32-C61** | ❌ | ❌ | ❌ | Arduino nur als IDF-Komponente, mit PlatformIO nicht praktikabel |
| **ESP32-H2** | ❌ | ⚠️ (nur Kabel, kein Update über die Luft) | ❌ | kein Wi-Fi, kein ESP-NOW |
| **ESP32-P4** | ❌ (kein Wi-Fi) | ⚠️ (nur Kabel) | ⚠️ (nur Kabel; stark, braucht Funk-Zusatzchip für Funk) | Zwei Kerne, viel PSRAM, aber Funk nur über einen Zusatzchip |

### Reihenfolge, die sich daraus ergibt

1. **ESP32-S3** bleibt die Referenz für Master und Slave.
2. **ESP32-C6 als Slave** (Streifen und HUB75) ist der naheliegende erste neue Chip: gleiche Funkart, HUB75
   unterstützt, Arduino-Unterstützung da.
3. **ESP32-C3 als Streifen-Slave** ist der günstigste Zusatz, ohne Panels.
4. **Klassischer ESP32** (WROVER) für Master und Slave, weil die Umgebung schon da ist; WROOM ohne PSRAM braucht den
   PSRAM-Rückfall aus Phase 4.
5. Alles andere (C2, C5, C61, H2, P4, S2 als Master) erst nach einem konkreten Bedarf.

### Schritte

- [x] Chips aufgelistet und gegenübergestellt (diese Tabellen)
- [ ] Entscheidung: Welche Chips sollen unterstützt werden, und in welcher Reihenfolge?
- [ ] `docs/*/03_Hardware_Setup.md` und das Wiki („Was du brauchst“) auf diese Tabellen angleichen
- [ ] Zahlen mit „ca.“ am Datenblatt prüfen, wenn ein Chip in die engere Wahl kommt

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

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

Entscheidung (2026-10-07): **zuerst der Slave**, der Master später und nur, wenn er lohnt (ein Kern, kein PSRAM).
Der Slave baut für den C6; was geprüft, geändert und noch nicht am Gerät erprobt ist:

**Slave: Befunde und Änderungen** (Repository `HyperLED_Slave`, Umgebung `esp32-c6`, Board `esp32-c6-devkitc-1`)

- [x] Umgebung `esp32-c6` in `HyperLED_Slave/platformio.ini`
- [x] **NeoPixelBus 2.8.4 kann den C6 nicht** (`NeoMethods.h` bindet RMT, I2S und DMA-SPI für C6 und H2 aus; nur Bit-Banging bleibt,
      und das sperrt auf einem Kern die Interrupts, also auch den Funk). Dazu der offene
      [Issue #917](https://github.com/Makuna/NeoPixelBus/issues/917). Darum:
- [x] Erster Ansatz `BusRmt` (nur einige Typen) wurde ersetzt durch `NeoC6RmtMethod` (siehe „Alle LED-Typen“ unten), über die RMT-Funktionen
      des Arduino-Kerns 3.3.11 (`rmtInit`, `rmtWrite`)
- [x] Statuslicht über `rgbLedWrite()` statt NeoPixelBus (`StatusLedManager.cpp`), Pin GPIO8
- [x] Pins für den C6 in `Config.h` und `main.cpp`: Statuslicht 8, Standard-LED-Pin 2 (GPIO4 ist beim C6 ein Strapping-Pin),
      Uplink RX 17 / TX 16, Downlink RX 18 / TX 19, HUB75 vorläufig (0 bis 7, 14, 15, 20 bis 23; der Super Mini hat GPIO 10/11 nicht)
- [x] Der Slave **kompiliert** für den C6 (1 380 816 Bytes, Chip-Kennung `0x000D`, RAM 16 %, Flash 67 % des 1,94-MB-Platzes)
- [x] Der S3-Build ist unverändert (1 216 688 Bytes wie vorher)
- [x] PSRAM-Stellen der Skripte haben schon einen Rückfall auf internen Speicher (`ScriptTask.cpp`, `ScriptHost.cpp`, `main.cpp`)
- [x] **Am Gerät** (2026-10-07, ESP32-C6 Super Mini): Flashen per USB, Funk (ESP-NOW) findet den Master, Statuslicht grün, Streifen
      mit 37 WS2812B auf GPIO2 läuft (Statusleiste für den Snapmaker U1), Strombegrenzung des Slaves wirkt. **Nicht geprüft:** Skripte auf dem C6,
      Online-Update über `firmware-esp32c6.bin` (kommt mit diesem Release)
- [x] **Alle LED-Typen** (Entscheidung 2026-10-07): Statt eines eigenen Treibers je Typ gibt es eine eigene NeoPixelBus-„Methode“
      für den C6 (`HyperLED_Slave/include/NeoC6RmtMethod.h`, RMT mit den Zeiten der Bibliothek selbst), die Farbformate der Bibliothek
      bleiben; TM1814, TM1914, WS2805, SM16825 und die SPI-Streifen laufen wie auf dem S3 (kompiliert, Streifen am Gerät offen)
- [x] Der Slave meldet seinen Chip im PONG (ein Byte, ab 0.3.006), der Master gibt ihn in `/api/slaves` als `chip` weiter; die Weboberfläche
      bietet für einen C6-Slave seine Pins an (GPIO 0 bis 7, 14, 15, 20 bis 23, Standard 2) und seine HUB75-Belegung
- [x] Am Gerät (ESP32-C6 Super Mini, 4 MB): läuft, findet den Master über ESP-NOW, Master zeigt Chip `esp32c6`, Statuslicht grün;
      ältere S3-Slaves (0.3.005, ohne Chip) werden weiter richtig gelesen
- [x] **Fehler gefunden und behoben (2026-10-07): Statuslicht wurde rot, obwohl der Master den Slave sah.** Ursache: Zeitvergleiche mit
      `millis() - Zeitstempel > N`, wobei der Funk-Task den Zeitstempel zwischen Uhrlesen und Subtraktion setzt; die vorzeichenlose
      Differenz wird dann riesig. Auf dem Ein-Kern-C6 passierte das alle 15 bis 60 s („lost the Master, scanning“ im Log, Diagnose zeigte
      `now == last`). Behoben mit vorzeichenbehafteten Vergleichen (`EspNowBus.cpp`, `main.cpp`: Statuslicht, UART-Wächter, Kanalwechsel;
      Master: Aufräumen der Slave-Liste). Auf dem C6 sieben Minuten ohne ein Ereignis. Der Fehler steckt auch in den S3-Slaves (selten),
      die Korrektur kommt mit dem nächsten Slave-Release dorthin
- [ ] **Kabelverbindung (HyperBus über UART) am C6** (bewusst nicht getestet, 2026-10-07, wird als ungetestet dokumentiert): Uplink-Pins beim C6 sind RX = GPIO17, TX = GPIO16 (so beschriftet), der Master-TX
      geht also an GPIO17 des C6 und nicht an GPIO16 wie beim S3; Downlink GPIO18 (RX) und GPIO19 (TX). Am Gerät prüfen
- [ ] HUB75 am C6: Pins sind vorläufig (nur kompiliert, nie an einem Panel gesehen); Speicher und Takt am Panel messen
- [x] Release 0.3.006: `firmware-esp32c6.bin` für den Slave im Slave-Release, `HyperLED_Slave` baut dafür mit `-e esp32-c6`
- [x] Nebenher geliefert (2026-10-07): eigene Strombegrenzung (mA) für Slaves mit eigenem Netzteil, je Segment (`Segment::ablMa`), vom Master
      gerechnet, keine Änderung am Funkprotokoll; am Gerät geprüft (das Dimmen wirkt)
- [ ] Master-Seite: Er kennt den Chip eines Slaves nicht, sondern lässt ihn `{chip}` einsetzen (Phase 3). Zu prüfen, dass der Master
      beim Anzeigen und Aktualisieren eines C6-Slaves nichts S3-spezifisches annimmt (z. B. Pin-Auswahl für Datenpin in der Oberfläche)

**Master für den C6** (später)

Aus dem Quelltext des Masters, noch nicht für den C6 gebaut:

- Hintergrund-Tasks sind fest an **Kern 1** gebunden (`MqttManager`, `PluginManager`, `UpdateManager`); der C6 hat nur einen Kern.
- Das Skript-System und der `UpdateManager` nutzen PSRAM (mit Rückfall, beim Master zu prüfen).
- `NeoPixelBus`: derselbe Befund wie beim Slave, auch der Master braucht `BusRmt`.
- `esp-hub75`, USB-Konfiguration, Pinbelegung, Partitionstabelle prüfen.
- Ein Kern mit 160 MHz teilt sich Webserver, Funk, Effekte und Lua.

- [ ] Master für den C6 nur kompilieren, Fehlerliste festhalten
- [ ] Entscheiden, ob und wie weit der Master lohnt (Speicher und Leistung am Gerät messen)
- [ ] Dateisystem: gleiche Partitionstabelle und gleiche Weboberfläche wie beim S3? Sonst `littlefs-<chip>.bin`
- [ ] Releases liefern `firmware-esp32c6.bin` (Master und Slave); README, `docs/*/03`, Wiki („Was du brauchst“) und Chip-Tabelle anpassen

# Das Web-Interface & MQTT

Das Herzstück zur Bedienung von HyperLED ist das moderne Glassmorphism-Webinterface. Es läuft direkt vom ESP32 und benötigt keine Internetverbindung.

## Hauptbildschirm

Der Hauptbildschirm ist in drei Bereiche gegliedert:

1. **Segmente:** Liste aller Segmente (Master und ggf. Slaves). Wähle hier aus, welches Segment du gerade bearbeitest.
2. **Farbe & Helligkeit:** Farbwähler, Helligkeit, Geschwindigkeit, Intensität und Palette für das aktuell ausgewählte Segment. Der Button **„Elemente bearbeiten“** führt direkt zu den Matrix-Widgets (Uhrzeit, Text, Wetter usw.).
3. **Effekte:** Klick-Auswahl aus über 25 eingebauten Animationseffekten.

Über das Zahnrad-Symbol oben rechts öffnest du die Einstellungen mit weiteren Tabs:

| Tab | Inhalt |
|---|---|
| **LED** | LED-Typ, Anzahl, Datenpin, physische Taster, automatische Strombegrenzung (ABL). |
| **Matrix** | 2D-Matrix-Setup, Live-Vorschau, Pixel-Art-Editor/-Upload, HUB75-Pinbelegung als Referenz. |
| **Segmente** | Segmente anlegen, bearbeiten und löschen. |
| **Presets** | Lichtstimmungen speichern, anwenden und als Playlist automatisch abspielen lassen. |
| **Zeitpläne** | Effekte oder Presets zeitgesteuert auslösen (zeitzonenbewusst über NTP). |
| **Slaves** | Automatisch gefundene Slave-Boards benennen und konfigurieren (LED-Typ, Pins bzw. HUB75-Matrixgröße). |
| **WLAN / MQTT** | Netzwerk- und Smart-Home-Einstellungen. |
| **System** | IP-Adresse, Firmware-Version, OTA-Updates, Sicherung und Wiederherstellung, Werkseinstellungen. |

---

## MQTT Einstellungen (Smart Home)

Damit du HyperLED in Systeme wie Home Assistant, ioBroker oder Node-RED einbinden kannst, besitzt die Firmware einen robusten MQTT-Client mit Home-Assistant-Autodiscovery.

### Konfiguration
Gehe im Web-Interface auf den Tab **WLAN / MQTT** und scrolle nach unten:
1. **MQTT aktivieren:** Setze das Häkchen.
2. **Broker IP:** Trage die IP-Adresse deines MQTT-Servers ein (z. B. Mosquitto).
3. **Port:** Standard ist `1883`, mit TLS meist `8883`.
4. **Verschlüsselt (TLS):** Nur einschalten, wenn dein Broker TLS anbietet. Die Verbindung wird dann verschlüsselt; das Zertifikat des Brokers prüft HyperLED nicht (Heim-Broker nutzen meist selbst ausgestellte Zertifikate).
5. **Benutzer / Passwort:** Falls dein Broker eine Authentifizierung benötigt. Das gespeicherte Passwort zeigt die Weboberfläche nie an – ein leeres Feld behält es.
6. **Basis-Thema (Topic):** Leer lassen für den Standard `hyperled/<MAC>`. Wer mehrere Controller sprechend benennen will, trägt z. B. `wohnzimmer/hyperled` ein – jeder Controller braucht ein eigenes Thema.

Mit **Verbindung testen** probierst du die eingetragenen Werte aus, ohne sie zu speichern. Beim Speichern startet der Controller neu und verbindet sich. Home Assistant findet ihn danach über die Autodiscovery von selbst, auch nach einem Neustart von Home Assistant.

Über den Feldern steht, ob der Controller verbunden ist – und wenn nicht, warum (z. B. „Benutzername oder Passwort falsch“ oder „Broker nicht erreichbar“). **In Home Assistant neu anmelden** schickt alle Geräte-Informationen noch einmal, falls in Home Assistant etwas fehlt.

### Kurzanleitung: HyperLED in Home Assistant einbinden
1. **Broker einrichten:** In Home Assistant unter *Einstellungen → Add-ons → Add-on-Store* das Add-on **Mosquitto broker** installieren und starten. Home Assistant schlägt danach die **MQTT**-Integration vor – bestätigen.
2. **Benutzer anlegen:** Unter *Einstellungen → Personen → Benutzer* einen eigenen Benutzer für HyperLED anlegen (z. B. `hyperled`). Mosquitto akzeptiert die Home-Assistant-Benutzer als MQTT-Zugang.
3. **HyperLED verbinden:** In der HyperLED-Weboberfläche unter *Einstellungen → WLAN & MQTT* MQTT aktivieren, als Server die IP-Adresse von Home Assistant eintragen, Port `1883`, dazu Benutzer und Passwort aus Schritt 2. **Verbindung testen**, dann **MQTT speichern und neu starten**.
4. **Gerät finden:** Nach wenigen Sekunden erscheint unter *Einstellungen → Geräte & Dienste → MQTT* das Gerät **HyperLED-xxxxxx** mit allen Lichtern und Einstellungen.
5. **Automationen:** Zum Beispiel einen Text aufs Panel schreiben, wenn die Waschmaschine fertig ist:

```yaml
action: text.set_value
target:
  entity_id: text.hyperled_xxxxxx_panel_lauftext
data:
  value: "Waschmaschine fertig"
```

Die Entity-ID steht in Home Assistant beim jeweiligen Text-Element. Für eine Benachrichtigung bei Funk-Ausfällen eignet sich der Sensor **WLAN-Ausfälle** (er zählt hoch).

### Was in Home Assistant erscheint
* **Ein Licht pro Segment** – auch Segmente auf einem Slave. Die Farbwahl richtet sich nach der LED-Hardware: RGB, RGBW, Farbtemperatur (bei Streifen mit zwei Weißkanälen und CCT-LEDs), nur Helligkeit oder nur Ein/Aus.
* **Effekte** mit denselben Namen wie in der Weboberfläche. Uhr/Text, Bild und die Panel-Effekte stehen nur bei Segmenten auf einem HUB75-Panel (bzw. einer Matrix) zur Auswahl.
* **„Alle Segmente“** schaltet und dimmt alle Segmente zusammen (nur bei mehr als einem Segment).
* **Pro Segment:** Geschwindigkeit und Intensität (in %) und die Palette; bei Panels der Hintergrund-Effekt.
* **Texte:** Jedes Text- und Lauftext-Element ist ein Textfeld. So lässt sich z. B. „Waschmaschine fertig“ aus einer Automation aufs Panel schreiben.
* **Szene** (Auswahl) – zeigt die zuletzt aktivierte Szene, bis sich danach etwas ändert. Dazu **Playlist** und **Gleichlauf** als Schalter.
* **Taster:** Jeder aktivierte Taster ist ein Ereignis („kurz“, „lang“ bzw. „umgelegt“) für Automationen. Am Controller macht er weiter, was er bisher tut.
* **Diagnose:** WLAN-Signal, freier Speicher, IP-Adresse, Startzeit, Zahl und Zeitpunkt der WLAN-Ausfälle, dazu je Slave, ob er verbunden ist (mit Firmware-Version). So kann Home Assistant bei einem Ausfall benachrichtigen.
* **Firmware:** zeigt ein neues Release und installiert es auf Knopfdruck (Szenen, Zeitpläne und Bilder bleiben erhalten). Dazu ein **Neustart**-Knopf und die **Status-LED** als eigenes Licht.
* **Verfügbarkeit:** Ist der Controller aus oder nicht erreichbar, zeigt Home Assistant die Lichter als „nicht verfügbar“.
* **Abmelden:** Schaltest du MQTT aus (oder setzt den Controller zurück), entfernt er seine Lichter aus Home Assistant, statt sie als „nicht verfügbar“ zurückzulassen. Beim Wiedereinschalten erscheinen sie mit denselben IDs wieder.
* Jede Änderung – aus der Weboberfläche, über Taster, Zeitpläne, Szenen oder die Playlist – kommt sofort in Home Assistant an. Neue, umbenannte oder gelöschte Segmente übernimmt Home Assistant automatisch.

> [!TIP]
> **Themen:** Befehle gehen im JSON-Format an `<Basis>/ha/set` (alle Segmente) oder `<Basis>/seg<N>/ha/set` (ein Segment), zum Beispiel `{"state": "ON", "brightness": 255}`. Den Zustand meldet der Controller unter denselben Pfaden mit `/ha/state`, die Erreichbarkeit unter `<Basis>/status` (`online` / `offline`).

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
| **System** | IP-Adresse, Firmware-Version, OTA-Updates, Werkseinstellungen. |

---

## MQTT Einstellungen (Smart Home)

Damit du HyperLED in Systeme wie Home Assistant, ioBroker oder Node-RED einbinden kannst, besitzt die Firmware einen robusten MQTT-Client mit Home-Assistant-Autodiscovery.

### Konfiguration
Gehe im Web-Interface auf den Tab **WLAN / MQTT** und scrolle nach unten:
1. **MQTT aktivieren:** Setze das Häkchen.
2. **Broker IP:** Trage die IP-Adresse deines MQTT-Servers ein (z. B. Mosquitto).
3. **Port:** Standard ist `1883`.
4. **Benutzer / Passwort:** Falls dein Broker eine Authentifizierung benötigt.
5. **Topic:** Gib einen eindeutigen Namen an (z. B. `wohnzimmer/hyperled`). Über dieses Topic lauscht der Controller auf Befehle.

> [!TIP]
> **Payloads:** Der Controller erwartet JSON-Pakete, die identisch zur `/json/state`-API sind (siehe [API Referenz](08_API_Referenz.md)). Ein Payload wie `{"on": true, "bri": 255}` an das eingestellte Topic schaltet den Controller mit voller Helligkeit ein.

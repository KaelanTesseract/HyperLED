# Das Web-Interface & MQTT

Das Herzstück zur Bedienung von HyperLED ist das moderne Glassmorphism-Webinterface. 

## Aufbau der UI
Die Oberfläche ist in 5 Haupt-Tabs unterteilt, die sich perfekt an Desktop und Smartphone (ohne horizontales Scrollen) anpassen:

1. **Effekte:** Wähle aus verschiedenen Animationen (Solid, Breathe, Rainbow, etc.) und passe die Helligkeit an.
2. **Farben:** Wähle Farbpaletten für deine Effekte aus.
3. **LEDs:** 
   - Konfiguriere deine LED-Segmente.
   - Wechsle in den **Pixel Art Converter** für LED-Matrixen.
   - Konfiguriere **Tasten (Buttons)**, um die LEDs lokal hardware-seitig ein- oder auszuschalten.
4. **WLAN / MQTT:** Konfiguriere Netzwerk- und Smart-Home-Schnittstellen.
5. **System:** Zeigt IP-Adresse, Firmware-Version und erlaubt OTA-Updates.

---

## MQTT Einstellungen (Smart Home)

Damit du HyperLED in Systeme wie Home Assistant, ioBroker oder Node-RED einbinden kannst, besitzt die Firmware einen robusten MQTT-Client.

### Konfiguration
Gehe im Web-Interface auf den Tab **WLAN / MQTT** und scrolle nach unten:
1. **MQTT aktivieren:** Setze das Häkchen.
2. **Broker IP:** Trage die IP-Adresse deines MQTT-Servers (z.B. Mosquitto) ein.
3. **Port:** Standard ist `1883`.
4. **Benutzer / Passwort:** Falls dein Broker eine Authentifizierung benötigt.
5. **Topic:** Gib einen eindeutigen Namen an (z.B. `wohnzimmer/hyperled`). Über dieses Topic lauscht der Controller auf Befehle.

> [!TIP]
> **Payloads:** Der Controller erwartet JSON-Pakete, die fast identisch zu unseren API-Aufrufen sind. Ein Payload wie `{"on": true, "bri": 255}` an das eingestellte Topic schaltet den Controller mit voller Helligkeit ein.

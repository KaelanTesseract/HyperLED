# HyperLED MQTT / Home Assistant – Optimierungsplan

Grundlage: Durchsicht der MQTT-Anbindung (`src/MqttManager.cpp`) im September 2026. HyperLED soll
über MQTT mit Home Assistant verbunden werden (Autodiscovery, JSON-Schema).

**Regel:** Ist ein Punkt erledigt, wird er aus dieser Datei gelöscht – hier steht nur, was noch
offen ist. Neu entdeckte Folgearbeiten kommen ebenfalls hierher.

## C. Weboberfläche

1. **MQTT-Verbindungsstatus anzeigen** – verbunden / nicht verbunden mit Fehlergrund, dazu ein
   Knopf „Verbindung testen“.
2. **Knopf „In Home Assistant neu anmelden“** – sendet Discovery und Zustand erneut.
3. **Kurzanleitung in der Doku** – Einrichtung in Home Assistant Schritt für Schritt (de/en).

## Optional

- **Verschlüsselte Verbindung (MQTTS, Port 8883)** – kostet Speicher und ist im Heimnetz meist
  unnötig; nur umsetzen, wenn der Broker es verlangt.

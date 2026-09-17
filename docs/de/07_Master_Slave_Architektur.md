# Master/Slave Architektur

Eines der mächtigsten Features von HyperLED ist die Fähigkeit, mehrere ESP32-Controller extrem zuverlässig und synchron miteinander zu verbinden. Das hochperformante Protokoll dafür heißt **HyperBus** und funktioniert wahlweise kabelgebunden oder kabellos.

## Wie es funktioniert

Ein Controller agiert als **Master**, andere Controller in der Kette agieren als **Slave**. Der Master verteilt die Segmente und sagt jedem Slave, was er zeigen soll – das Bild selbst berechnet der Slave, wo immer es geht, auf eigene Rechnung:

- **Effekte:** Der Master schickt nur die Einstellungen (Effekt, Farbe, Helligkeit, Tempo …), sobald sich etwas ändert, und der Slave berechnet die Animation selbst.
- **„Uhr / Text" auf einem HUB75-Panel:** Ab Slave-Firmware 0.2.004 zeichnet der Slave alle Elemente selbst – Uhrzeit, Datum, Analoguhr, Text, Lauftext, Wetter und Bild. Der Master schickt die Elementliste, alle paar Sekunden die Uhrzeit, das Wetter bei Änderung und Bilder genau einmal: Fehlt einem Slave ein Bild (etwa nach einem Neustart), fordert er es selbst an und prüft es per Prüfsumme.
- **Pixelstrom als Rückfall:** Nur was ein Slave nicht selbst kann (ältere Firmware, der Effekt „Bild", ein Element, das nicht mehr in ein Paket passt), berechnet der Master und überträgt die geänderten Pixel.

So bleibt der Master frei für die Weboberfläche und die Koordination der Slaves, und die Funkverbindung wird nicht mit Bilddaten verstopft.

Ein Slave übernimmt selbst keine eigene Steuerlogik – Name, LED-Typ, Pinbelegung bzw. Matrixgröße werden ausschließlich über die Web-Oberfläche des Masters eingestellt.

### Zwei Übertragungswege

- **Kabelgebunden (UART):** Höchste Zuverlässigkeit und Geschwindigkeit. Master und Slave werden über eine Datenleitung plus gemeinsamen Ground verbunden (Standard: Master TX GPIO 17 → Slave RX GPIO 16). Kabelgebundene Slaves können weitere Slaves an ihrem eigenen Downlink-Port weiterreichen (Daisy-Chaining).
- **Kabellos (ESP-NOW):** Für Slaves ohne praktikable Verkabelung. Der Slave erkennt beim Start automatisch, ob eine kabelgebundene Verbindung vorliegt – ist das nicht der Fall, wechselt er selbstständig in den ESP-NOW-Modus und rastet dort ein.

Ein Slave muss vor dem ersten Betrieb nicht manuell auf einen Übertragungsweg festgelegt werden: Er erkennt und speichert die passende Betriebsart automatisch.

> [!NOTE]
> Im ESP-NOW-Betrieb müssen Master und Slave auf demselben WLAN-Kanal funken. Der Slave sucht die Kanäle nacheinander ab und übernimmt anschließend den Kanal, den der Master ihm mitteilt – der Kanal ergibt sich also aus dem WLAN, mit dem der Master verbunden ist, und muss nirgends eingestellt werden. Wechselt der Master den Kanal, beginnt der Slave nach wenigen Sekunden Funkstille automatisch von vorn zu suchen.

## Einrichtung

1. Verbinde die Hardware (bei kabelgebundenem Betrieb wie oben beschrieben) bzw. versorge den Slave einfach mit Strom (bei ESP-NOW).
2. Öffne das Web-Interface des Master-Controllers und gehe in den Einstellungen auf den Tab **Slaves**.
3. Neu gefundene Slaves melden sich hier automatisch (Ping/Pong-Protokoll) und lassen sich benennen und einrichten.
4. Wähle den passenden LED-Typ – für ein HUB75-Panel wählst du **HUB75** und gibst Breite, Höhe und ggf. den Treiber-Chip des Panels an. Die Pinbelegung selbst ist auf dem Slave fest verdrahtet (siehe [Hardware Setup](03_Hardware_Setup.md)).
5. Nach dem Speichern erscheint der Slave als eigenes Segment im Hauptbildschirm und kann wie jedes andere Segment gesteuert werden.

> [!NOTE]
> Große HUB75-Panels benötigen unter Umständen mehr RAM, als auf dem ESP32-S3 verfügbar ist. Die Firmware erkennt das und verweigert die Initialisierung sicher, statt abzustürzen – wähle in dem Fall ein kleineres Panel oder eine geringere Farbtiefe.

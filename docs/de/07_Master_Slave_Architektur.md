# Master/Slave Architektur

Eines der mächtigsten Features von HyperLED ist die Fähigkeit, mehrere ESP32-Controller extrem zuverlässig und synchron miteinander zu verbinden. Das hochperformante Protokoll dafür heißt **HyperBus** und funktioniert wahlweise kabelgebunden oder kabellos.

## Wie es funktioniert

Ein Controller agiert als **Master**, andere Controller in der Kette agieren als **Slave**. Der Master verteilt die Segmente und sagt jedem Slave, was er zeigen soll – das Bild selbst berechnet der Slave, wo immer es geht, auf eigene Rechnung:

- **Effekte:** Der Master schickt nur die Einstellungen (Effekt, Farbe, Helligkeit, Tempo …), sobald sich etwas ändert, und der Slave berechnet die Animation selbst.
- **„Uhr / Text" auf einem HUB75-Panel:** Ab Slave-Firmware 0.2.004 zeichnet der Slave alle Elemente selbst – Uhrzeit, Datum, Analoguhr, Text, Lauftext, Wetter und Bild. Der Master schickt die Elementliste, alle paar Sekunden die Uhrzeit, das Wetter bei Änderung und Bilder genau einmal: Fehlt einem Slave ein Bild (etwa nach einem Neustart), fordert er es selbst an und prüft es per Prüfsumme.
- **Hintergrund-Effekt hinter den Elementen:** Ab Slave-Firmware 0.2.007 kann hinter den Elementen ein Effekt laufen. Der Slave zeichnet ihn selbst, der Master schickt nur die Einstellungen. Damit die Elemente lesbar bleiben, bekommt jedes einen dunklen Umriss (oder einen abgedunkelten Kasten); die Helligkeit des Hintergrunds gilt wie die der Elemente im Verhältnis zur Segment-Helligkeit. Der Hintergrund läuft nur, wenn der Slave alle Elemente selbst zeichnet – ein bewegter Hintergrund lässt sich nicht als Pixelstrom übertragen.
- **Lua-Skripte:** Ab Slave-Firmware 0.3.000 kann der Master einem Slave ein kurzes Lua-Skript übergeben, das das Bild des Slaves Pixel für Pixel auf dem Slave selbst zeichnet. Es wird nichts gestreamt: Der Master schickt das Skript einmal (in kleinen Stücken, die der Slave per Prüfsumme kontrolliert) und danach nur noch die Werte, die das Skript liest. Siehe [Skripte auf Slaves](#skripte-auf-slaves).
- **Pixelstrom als Rückfall:** Nur was ein Slave nicht selbst kann (ältere Firmware, der Effekt „Bild", ein Element, das nicht mehr in ein Paket passt), berechnet der Master und überträgt die geänderten Pixel.

So bleibt der Master frei für die Weboberfläche und die Koordination der Slaves, und die Funkverbindung wird nicht mit Bilddaten verstopft.

Ein Slave übernimmt selbst keine eigene Steuerlogik – Name, LED-Typ, Pinbelegung bzw. Matrixgröße werden ausschließlich über die Web-Oberfläche des Masters eingestellt.

### Zwei Übertragungswege

- **Kabelgebunden (UART):** Höchste Zuverlässigkeit und Geschwindigkeit. Master und Slave werden über eine Datenleitung plus gemeinsamen Ground verbunden (Standard: Master TX GPIO 17 → Slave RX GPIO 16 bei einem ESP32-S3-Slave; bei einem **ESP32-C6**-Slave geht der Master-TX an **GPIO 17** des Slaves und der Master-RX an dessen **GPIO 16**, siehe [Hardware Setup](03_Hardware_Setup.md)). Kabelgebundene Slaves können weitere Slaves an ihrem eigenen Downlink-Port weiterreichen (Daisy-Chaining).
- **Kabellos (ESP-NOW):** Für Slaves ohne praktikable Verkabelung. Der Slave erkennt beim Start automatisch, ob eine kabelgebundene Verbindung vorliegt – ist das nicht der Fall, wechselt er selbstständig in den ESP-NOW-Modus und rastet dort ein.

Ein Slave muss vor dem ersten Betrieb nicht manuell auf einen Übertragungsweg festgelegt werden: Er erkennt und speichert die passende Betriebsart automatisch.

> [!NOTE]
> Im ESP-NOW-Betrieb müssen Master und Slave auf demselben WLAN-Kanal funken. Der Slave sucht die Kanäle nacheinander ab und übernimmt anschließend den Kanal, den der Master ihm mitteilt – der Kanal ergibt sich also aus dem WLAN, mit dem der Master verbunden ist, und muss nirgends eingestellt werden. Wechselt der Master den Kanal, beginnt der Slave nach wenigen Sekunden Funkstille automatisch von vorn zu suchen.

## Skripte auf Slaves

Ein Skript ist Lua-Text von höchstens 8 KB. Es definiert eine Funktion `frame(t, dt)`, die mit `px(x, y, r, g, b)`, `fill`, `clear` und `hsv` zeichnet, und kann die Einstellungen (`settings`) und die Werte (`v`) lesen, die der Master ihm gibt. Das Bild wird auf dem Slave berechnet, in einem eigenen Task, sodass ein langsames Skript weder den Funk noch das Panel aufhält.

- **Übertragung:** Der Master sagt dem Slave alle 2 s, was laufen soll (`CMD_SET_SCRIPT`: an/aus, Helligkeit, Größe und Prüfsumme des Skripts) und welche Einstellungen und Werte es lesen soll. Ab Slave-Firmware **0.3.001** gehen sie in `CMD_SET_SCRIPT_DATA`-Paketen, so vielen, wie jede Liste braucht (bis zu 8 je Liste): die Einstellungen bei Änderung und alle 5 s, die Werte bei Änderung und alle 2 s. Vorher teilten sich beide Listen ein Paket von 240 Bytes (`CMD_SET_SCRIPT_VALUES`), das ein Plugin mit vielen Einstellungen überlief, sodass sein Skript keine Werte sah; für einen Slave mit 0.3.000 benutzt der Master dieses Paket weiter. Hat der Slave das Skript nicht, fragt er danach (`CMD_REQUEST_SCRIPT`) und bekommt es in Stücken zu höchstens 224 Bytes, eines alle 20 ms (`CMD_SCRIPT_CHUNK`); er prüft die Prüfsumme des ganzen Textes, bevor er es ausführt, und fragt erneut, wenn ein Stück gefehlt hat. Der Slave meldet seinen Zustand bei Änderung und alle 5 s (`CMD_SCRIPT_STATUS`).
- **Zustand:** `GET /api/slaves` zeigt je Slave, ob er Skripte kann (`scripts`), und solange eines läuft dessen Zustand, Bildrate, Bildzeit, Speicher, letzte Meldung und eine Prüfsumme des letzten Bildes (`script`).
- **Grenzen:** Jeder Aufruf eines Skripts hat einen Speicherdeckel (auf einem Slave 32 KB) und ein Zeitbudget (40 ms). Ein Skript, das eines überschreitet oder drei Bilder in Folge (zehn in einer Minute) nicht schafft, wird gestoppt und gemeldet. Ein Slave, dessen Master 10 s lang schweigt, beendet sein Skript von selbst und dunkelt das Panel ab. Solange ein Skript läuft, ignoriert der Slave Befehle für Effekte, Elemente und Pixel.
- **Sicherheit:** Ein Skript sieht nur sein eigenes Bild, die Einstellungen und die Werte. Es kommt weder an Dateien noch ans Netzwerk noch an sonstigen Zustand des Geräts.
- **Geschwindigkeit (64×64-Panel):** rund 20 ms pro Bild für eine Flächenfüllung oder einen Verlauf (50 Bilder pro Sekunde), rund 70 ms für drei `sin`-Aufrufe je Pixel (14 Bilder pro Sekunde); ein Streifen mit 124 LEDs braucht unter 1 ms.

## Einrichtung

1. Verbinde die Hardware (bei kabelgebundenem Betrieb wie oben beschrieben) bzw. versorge den Slave einfach mit Strom (bei ESP-NOW).
2. Öffne das Web-Interface des Master-Controllers und gehe in den Einstellungen auf den Tab **Slaves**.
3. Neu gefundene Slaves melden sich hier automatisch (Ping/Pong-Protokoll) und lassen sich benennen und einrichten.
4. Wähle den passenden LED-Typ – für ein HUB75-Panel wählst du **HUB75** und gibst Breite, Höhe und ggf. den Treiber-Chip des Panels an. Die Pinbelegung selbst ist auf dem Slave fest verdrahtet (siehe [Hardware Setup](03_Hardware_Setup.md)).
5. Nach dem Speichern erscheint der Slave als eigenes Segment im Hauptbildschirm und kann wie jedes andere Segment gesteuert werden.

> [!NOTE]
> Große HUB75-Panels benötigen unter Umständen mehr RAM, als auf dem ESP32-S3 verfügbar ist. Die Firmware erkennt das und verweigert die Initialisierung sicher, statt abzustürzen – wähle in dem Fall ein kleineres Panel oder eine geringere Farbtiefe.

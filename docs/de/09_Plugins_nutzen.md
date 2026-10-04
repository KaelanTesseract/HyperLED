# Plugins nutzen

Ein **Plugin** ist eine kleine Datei, die einen Wert aus dem Netzwerk holt und auf einem Segment anzeigt – zum Beispiel den Druckfortschritt eines 3D-Druckers, die Außentemperatur oder den Zustand eines Dienstes im Heimnetz. HyperLED selbst weiß nichts über diese Dinge: Was angezeigt wird, steht allein im Plugin.

Plugins gehören nicht zur Firmware. Sie lassen sich hinzufügen, einstellen und wieder entfernen, ohne die Firmware zu aktualisieren, und sie überleben Firmware-Updates und die Sicherung.

> Wer ein eigenes Plugin schreiben möchte, findet alles in [Plugins entwickeln](10_Plugins_entwickeln.md) und [Plugin-Skripte](11_Plugin_Skripte.md). Fertige Beispiele liegen im Ordner [`plugins/`](../../plugins/).

---

## Ein Plugin hinzufügen

Öffne **Einstellungen → Plugins** und tippe auf **+ Plugin hinzufügen**. Es gibt zwei Wege:

- **Datei wählen:** eine Plugin-Datei (`.json`) von deinem Gerät.
- **Von einer Adresse laden:** die Adresse einer Plugin-Datei, zum Beispiel ein Release-Anhang auf GitHub oder eine Rohdatei. Das Laden übernimmt der Controller selbst; Weiterleitungen (wie bei GitHub-Releases) folgt er.

Danach prüft HyperLED das Plugin **vollständig, bevor etwas gespeichert wird**, und zeigt dir eine **Vorschau**:

| Zeile in der Vorschau | Bedeutung |
|---|---|
| Name, Version, Autor, Lizenz | Von wem das Plugin ist und unter welchen Bedingungen es weitergegeben wird. |
| **Fragt diese Adresse ab** | Die Adresse, die das Plugin später abfragt. Teile in `{ }` (zum Beispiel `{host}`) trägst du nach der Installation selbst ein. **Lies diese Zeile:** Ein Plugin darf diese Adresse und nur diese abfragen. |
| Zeigt etwas auf einem Segment an | Das Plugin überlagert ein Segment, das du aussuchst. |
| Möchte das Segment ein- und ausschalten | Das Plugin würde das Segment gern ein- oder ausschalten. Das darf es nur, wenn du es später ausdrücklich erlaubst. |
| Enthält ein Skript | Das Plugin zeichnet das Segment mit einem kleinen Programm, Bild für Bild (siehe unten). |
| Ersetzt die installierte Version | Ein Plugin mit derselben Kennung ist schon da. Deine Einstellungen bleiben erhalten, soweit sie noch passen. |
| Läuft auf diesem Gerät nicht | Das Plugin braucht eine neuere Firmware oder Plugin-Schnittstelle. Es wird nach der Installation stillgelegt (siehe [Zustände](#zustände)). |

Erst **Installieren** speichert das Plugin. Danach öffnet sich gleich die Einstellungsseite.

Ist die Datei fehlerhaft, bekommst du eine **Meldung mit der Stelle**, an der das Problem liegt (zum Beispiel „Regel 2 when: …“ oder bei einem Skript „Skript: script:7: … near 'end'“). Es wird nichts gespeichert.

---

## Einstellen und einschalten

Auf der Einstellungsseite eines Plugins steht, was das Plugin von dir wissen möchte – zum Beispiel die Adresse deines Druckers, ein Segment und Farben. Pflichtfelder haben einen Stern (`*`).

- **Segment:** Die Liste zeigt deine Segmente mit Namen. Pro Segment kann nur **ein** Plugin laufen.
- **Passwörter und Schlüssel** werden nie angezeigt. Ein leeres Feld bedeutet „unverändert“.
- **Speichern** schickt nur das, was du geändert hast. Das Gerät prüft jeden Wert und meldet Fehler direkt am Feld (zum Beispiel „muss zwischen 1 und 65535 liegen“).

Mit **Plugin eingeschaltet** startet das Plugin. Ist eine Pflichteinstellung noch leer oder das Segment belegt, erscheint der Grund direkt an der Stelle, und das Plugin bleibt aus.

Zwei Schalter erscheinen nur, wenn sie gebraucht werden:

- **Darf das Segment ein- und ausschalten:** nur bei Plugins, die das möchten (zum Beispiel „Drucker fertig → Licht aus“). Ohne diese Erlaubnis ändert das Plugin nur das Aussehen des Segments.
- **Trotzdem ausführen:** nur bei einem Plugin, das für eine andere Plugin-Schnittstelle geschrieben ist (siehe unten). Davor fragt HyperLED noch einmal nach.

---

## Zustände

Die Liste zeigt für jedes Plugin einen Zustand:

| Zustand | Bedeutung | Was tun? |
|---|---|---|
| **Aus** | Das Plugin ist ausgeschaltet. Das Segment gehört dir. | Einschalten, wenn gewünscht. |
| **Wartet auf Antwort** | Eingeschaltet, die Quelle hat noch nicht geantwortet. | Ein paar Sekunden warten. Dauert es, prüfe die Adresse in den Einstellungen. |
| **Läuft** | Antworten kommen an, das Plugin zeigt etwas an. | – |
| **Keine Verbindung** | Die Quelle hat dreimal hintereinander nicht geantwortet (oder ihre Antwort passt nicht zum Plugin). Das Plugin zeigt, was für diesen Fall vorgesehen ist, sonst bleibt das Segment bei dir. Es versucht es weiter. | Ist die Quelle (zum Beispiel der Drucker) eingeschaltet und erreichbar? Stimmt die Adresse? Der Grund steht unter dem Zustand. |
| **Nicht kompatibel** | Das Plugin braucht eine neuere Plugin-Schnittstelle, Firmware oder Skript-Stufe, als dieses Gerät kann. Es bleibt gespeichert und läuft nach einem passenden Firmware-Update von selbst. | Firmware aktualisieren, oder eine ältere Fassung des Plugins verwenden. Wer es trotzdem versuchen will, schaltet in den Einstellungen **Trotzdem ausführen** ein; erzeugt es dann wiederholt Fehler, schaltet es sich selbst ab. |
| **Ungültig** | Die gespeicherte Datei ist beschädigt oder passt nicht zu dieser Firmware. | Entfernen und neu installieren. |

Bei Plugins mit Skript steht eine zusätzliche Zeile darunter:

- **„Skript läuft · 20,5 ms je Bild“:** Das Skript zeichnet das Segment.
- **„Regeln statt Skript: …“:** Das Skript kann gerade nicht laufen (zum Beispiel ein Slave ohne Skript-Unterstützung oder ein Fehler im Skript). Das Plugin zeigt stattdessen, was seine Regeln vorsehen, und nennt den Grund.

---

## Im Licht-Bereich

Steuert ein Plugin ein Segment, steht oben im Bereich **Licht**:

> **Gesteuert von Plugin-Name · Segment** [Pausieren]

**Pausieren** schaltet das Plugin aus; das Segment ist sofort wieder so, wie du es eingestellt hast. Das liegt an der Arbeitsweise: Ein Plugin **überlagert** ein Segment nur beim Zeichnen und **speichert nichts**. Deine Einstellungen (Effekt, Farbe, Helligkeit …) bleiben unverändert erhalten – nach einem Neustart, nach dem Ausschalten oder Entfernen des Plugins ist alles wieder da. Änderst du Effekt, Farbe, Geschwindigkeit oder Intensität eines gesteuerten Segments, merkt sich HyperLED das – sichtbar wird es aber erst, sobald das Plugin pausiert ist.

Die **Helligkeit** bestimmst immer du, und sie wirkt sofort; ein Plugin kann sie nicht ändern.

---

## Live-Werte

**Live-Werte** an einem Plugin zeigt, was es gerade tut – ohne etwas zu ändern:

- den **Zustand** und seinen Grund,
- die **gelesenen Werte** (oder „unbekannt“),
- **wer gerade zeichnet**: eine Regel (mit Nummer und Bedingung), der Fehlerzustand, das Skript, oder niemand („Keine Regel passt – das Segment bleibt, wie du es eingestellt hast“),
- den **Anfang der Antwort** der Quelle.

Das hilft, wenn ein Plugin nicht tut, was du erwartest: Siehst du den Wert, den du erwartest? Passt die Bedingung?

---

## Aktualisieren, entfernen, sichern

- **Aktualisieren:** Installiere die neue Fassung wie ein neues Plugin. Ist die Kennung dieselbe, ersetzt sie die alte; deine Einstellungen bleiben, soweit sie noch passen.
- **Entfernen:** löscht das Plugin samt Einstellungen. Das Segment gehört danach wieder dir.
- **Sicherung und Firmware-Updates:** Plugins und ihre Einstellungen (auch Passwörter!) sind in der Sicherung enthalten und bleiben bei Firmware-Updates erhalten. Die Sicherungsdatei enthält deshalb Passwörter im Klartext, genau wie die WLAN- und MQTT-Daten – gib sie nicht weiter.

---

## Was ein Plugin darf – und was nicht

Ein Plugin ist **Daten, kein Programm**, mit einer Ausnahme (Skripte, siehe unten). Es kann nur:

- die **eine** in der Vorschau gezeigte Adresse mit **GET** abfragen (keine anderen Anfragen, nichts senden außer den Kopfzeilen, die in der Datei stehen und mit deinen Einstellungen gefüllt werden),
- das **eine** Segment überlagern, das du ausgesucht hast, und zwar nur Effekt, Farbe, Geschwindigkeit und Intensität; Ein- und Ausschalten nur mit deiner Erlaubnis,
- Werte lesen und anzeigen.

Es hat **keinen Zugriff** auf WLAN-Zugangsdaten, MQTT-Zugang, andere Einstellungen oder das Dateisystem.

Ein **Skript** darf zusätzlich Pixel seines Segments zeichnen und kennt nur die Einstellungen und Werte seines Plugins (**ohne Passwörter**: Sie würden sonst unverschlüsselt per Funk zu einem Slave gehen). Es kann weder Netzwerk noch Dateien benutzen, und seine Rechenzeit und sein Speicher sind begrenzt. Ein fehlerhaftes oder endloses Skript wird abgebrochen und beendet sich nach mehreren Fehlern selbst; der Controller läuft weiter.

> **Trotzdem gilt:** Installiere nur Plugins aus Quellen, denen du vertraust. Ein Plugin kann Adressen in deinem Heimnetz abfragen (zum Beispiel die Oberfläche deines Routers) und deine Einstellungen für dieses Plugin – auch Passwörter – an *die Adresse* schicken, die in der Vorschau steht. Darum zeigt die Vorschau sie dir.

---

## Grenzen

| Was | Grenze |
|---|---|
| Plugins gleichzeitig installiert | 8 |
| Größe einer Plugin-Datei | 16 KB (Skript darin: 8 KB) |
| Plugins pro Segment | 1 |
| Abfrageabstand | frühestens alle 2 Sekunden (je nach Plugin, oft Minuten) |
| Größe der Antwort einer Quelle | 8 KB |
| Wartezeit je Abfrage | höchstens 10 Sekunden (je nach Plugin 1–10) |

Die Quellen werden **nacheinander** abgefragt, nie gleichzeitig: Eine verschlüsselte Verbindung braucht viel Arbeitsspeicher.

---

## Hilfe bei Problemen

| Meldung | Bedeutung und Abhilfe |
|---|---|
| **Keine Verbindung zur Quelle** | Die Adresse antwortet nicht. Läuft das Gerät (zum Beispiel der Drucker) und ist es im selben Netzwerk? Stimmen Adresse und Port? |
| **Die Quelle antwortet mit Fehler 404 / 401 / 500** | Die Quelle hat die Anfrage abgelehnt. 401/403 bedeutet meist: ein Schlüssel oder Passwort fehlt oder ist falsch. 404: der Pfad stimmt nicht, prüfe die Adresse in der Vorschau. |
| **Die Antwort der Quelle ist kein JSON** | Die Adresse liefert etwas anderes (zum Beispiel eine Webseite). Prüfe die Adresse. |
| **Die Antwort der Quelle ist größer als 8 KB** | Das Plugin oder die Quelle passt nicht zu diesem Gerät. |
| **Die Antwort enthält nichts von dem, was das Plugin liest – passt die Quelle zum Plugin?** | Die Quelle antwortet, aber nicht in dem Aufbau, den das Plugin erwartet (zum Beispiel ein anderes Gerät oder eine andere Version). Sieh dir den Anfang der Antwort bei **Live-Werte** an. |
| **Die Einstellung 'host' ist noch leer** | Eine Pflichteinstellung fehlt. |
| **Das Segment 2 wird schon vom Plugin 'X' gesteuert** | Pro Segment läuft nur ein Plugin. Wähle ein anderes Segment oder schalte das andere Plugin aus. |
| **Das Plugin braucht die Plugin-Schnittstelle 2, diese Firmware bietet 1** | Das Plugin ist neuer als die Firmware. Firmware aktualisieren. |
| **Der Slave kann noch keine Skripte** | Das Segment liegt auf einem Slave mit älterer Firmware (Skripte brauchen Slave-Firmware 0.3.000 oder neuer). Bis zum Update zeigt das Plugin, was seine Regeln vorsehen. |
| **Das Skript ist fehlgeschlagen: …** | Ein Fehler im Skript. Die Meldung nennt die Zeile. Das Plugin zeigt bis zur Behebung, was seine Regeln vorsehen; der Autor sollte das Skript korrigieren. |
| **Der Slave meldet sich nicht** | Das Skript läuft auf einem Slave, der seit Sekunden nichts meldet (Funk gestört, Slave aus). Das Plugin zeigt solange, was seine Regeln vorsehen. |

Tut ein Plugin nichts und es steht „Läuft“ da: **Live-Werte** öffnen. Steht dort „Keine Regel passt“, erfüllt der gelesene Wert keine Bedingung des Plugins; der Autor kann helfen.

# HyperLED WebUI – Optimierungsplan

Grundlage: Design-Review der WebUI (`data/`) nach Apples Human Interface Guidelines, gemessen an der
Live-UI bei 375 px (Handy) und 1280 px (Desktop).

**Regel:** Die Schritte werden der Reihe nach abgearbeitet. Ist ein Schritt fertig, wird er aus
dieser Datei gelöscht – hier steht nur, was noch offen ist.

## Arbeitsregeln für jeden Schritt

- Nach Änderungen an `data/` die zugehörigen `.gz`-Dateien neu erzeugen (`gzip -9 -k -f <datei>`),
  sonst liefert das Gerät die alte Version aus.
- `?v=` für `style.css`, `app.js` und `i18n.js` in `index.html` hochzählen (Browser-Cache).
- WebUI flashen: `PYTHONIOENCODING=utf-8 pio run -e esp32-s3 -t uploadfs --upload-port COM11`.
  Achtung: `uploadfs` ersetzt das ganze Dateisystem des Masters – auch die hochgeladenen Bilder
  der Bild-Elemente (`/img/w<id>.rgb`). Danach müssen Bilder neu hochgeladen werden.
- Prüfen bei 375 px und 1280 px, einmal mit Tastatur (Tab/Escape) durchgehen.

## Entscheidungen aus erledigten Schritten (nicht erneut aufrollen)

- „GPIO 4" bleibt als Auswahltext in den Pin-Listen stehen: so steht es auf der Platine. Die
  Beschriftungen darüber heißen „Daten-Pin" und „Takt-Pin".
- Master/Slave heißt in der Oberfläche „Dieses Gerät" und „Gerät"; im Code, in den API-Feldern und
  in den Schlüsselnamen von `i18n.js` bleibt es Master/Slave.
- Rückmeldungen laufen über `showToast()` (kurze Meldung unten), `flashButton()` (der Auslöser sagt
  kurz, wie es ausging) und `askConfirm()` (ein Dialog für echte Rückfragen, „Abbrechen" ist
  Standard). Neue Aktionen nutzen diese drei, kein `alert()`/`confirm()`.
- Es gibt kein Einstellungs-Modal mehr. Die Oberfläche hat fünf Bereiche (`.area`, umgeschaltet
  mit `showArea()`): Licht (`#mainControls`), Szenen, Panel, Geräte, Einstellungen. „Geräte" hat die
  Unteransichten Segmente, Verbundene Geräte und LED-Hardware (alles, was die Hardware beschreibt);
  „Einstellungen" hat WLAN & MQTT und System (wie dieser Controller selbst eingerichtet ist).
  Unteransichten schalten nur innerhalb ihres Bereichs um (`.tab-btn` / `.tab-content`, geladen
  über `loadDeviceView()`), `showDeviceView(id)` springt in den Bereich, der die Ansicht enthält.
  Element-IDs sind unverändert. Mehr als fünf Einträge verträgt die untere Leiste am Handy nicht.
- Das Farbrad bleibt eine normale, immer offene Karte hinter den Effekten – nicht einklappbar,
  weil iro.js in einem versteckten Container keine Größe bekommt.
- Effekte haben genau die zwei Gruppen, die die Firmware kennt (`EFFECT_HUB75_SHOWCASE_START`):
  „Streifen und Panels" und „Nur Panels". Keine weitere Einteilung ohne Grundlage in der Firmware.
- Überschriften: `h1` = Bereich, `h2` = Karte, `h3` = Abschnitt in einer Karte.
- Gemeinsame Muster sind Klassen (`.field`, `.list-row`, `.day-btn`, `.btn-chip`, `.hint`,
  `.subsection`, `.card-inset`, `.device-card`, …). Übrig gebliebene Inline-Styles sind
  Einzelfälle (Anzeigen/Ausblenden per JS, einzelne Abstände/Breiten) und bleiben bewusst stehen.
  Schalter (50×30) sind die einzige Bedienfläche unter 44 px – so groß ist ein Schalter. Die
  Feldregel in `.form-group` gilt deshalb nur für Textfelder und Auswahllisten, nicht für
  Schalter, Regler und Farbfelder.
- Live-Streifen (Bereich „Licht"): eine ausgedünnte Anfrage pro Sekunde
  (`/api/matrix_preview?seg=N&s=K`, bei Panels `&w=<Breite>`, 16×16-Vorschaubild), nur solange
  „Licht" sichtbar und der Tab im Vordergrund ist. Rechnet ein Slave den Effekt selbst, ist die
  Kopie auf dem Master dunkel – nach drei dunklen Antworten zeigt der Streifen die eingestellten
  Farben und sagt das dazu. Mehr Last auf dem Master für den Streifen ist nicht vorgesehen.
- Akzentfarbe = Farbe des gewählten Segments, wenn es an ist. Ausnahmen mit Rückfall auf
  `#34D399`: Weiß/Grau (Sättigung < 0,3), Rot (Farbton ≤ 20° oder ≥ 340°, wäre die Fehlerfarbe),
  und Farben, die auch aufgehellt keine 4,5:1 zur dunklen Beschriftung erreichen. Zu dunkle Farben
  werden im selben Farbton aufgehellt. Die Umrisse im Widget-Editor bleiben bewusst grün.
- Panel-Vorschau: ganze Pixel pro Panel-Pixel, so groß wie die Karte erlaubt, höchstens 640 px
  (64×64 → 10 px am Desktop). Mittel-Hilfslinien sind rosa (`#f472b6`), damit sie sich von den
  grünen Umrissen abheben; sie erscheinen beim Ziehen und kurz nach „mittig"-Knöpfen. Mittig heißt
  `|2·x + Breite − Panelbreite| ≤ 1` – passt die Größe nicht auf gerade/ungerade, zählen beide
  Positionen mit einem Pixel Versatz als mittig. Kein magnetisches Einrasten, damit jede Position
  erreichbar bleibt.
- Hintergrund-Effekt hinter den Elementen: Abschnitt „Hintergrund-Effekt“ im Element-Editor, alle
  Effekte wählbar, ruhige zuerst („Gut als Hintergrund“). Lesbarkeit pro Element, Standard ist der
  dunkle Umriss (auch für bestehende Elemente). Die Helligkeit gilt im Verhältnis zur
  Segment-Helligkeit, unabhängig von den Elementen – ausdrücklich so gewünscht, keine automatische
  Kopplung. Bildpixel mit allen Kanälen unter 12 sind durchsichtig. Auf Gerät-Panels läuft der
  Hintergrund nur, wenn das Gerät alle Elemente selbst zeichnet – nie als Pixelstrom.
- Jeder Regler zeigt seinen Wert (`attachSliderValue()`, „Name · 50 %“; in Element-Karten der Wert
  rechts). Jedes Eingabefeld hat eine verknüpfte Beschriftung (`for`/`aria-labelledby`), jeder
  Symbolknopf eine `aria-label` und `type="button"`.
- Pro Bereich höchstens ein gefüllter grüner Knopf; weitere Speichern-Knöpfe sind sekundär.
  Werkseinstellungen stehen in einem eigenen Abschnitt „Controller zurücksetzen“.
- „Gleichlauf“ ist ein Umschaltknopf (`.toggle-chip`, Checkbox bleibt für Tastatur und Screenreader),
  kein freistehender Schalter. Der Ein/Aus-Knopf des gewählten Segments ist eingeschaltet nur
  umrandet, damit er sich vom globalen Ein/Aus-Knopf unterscheidet.
- „Licht“ am Desktop: Die Effekt-Karte überspannt drei Rasterzeilen, damit „Farbe“ direkt unter
  „Segmente“ steht. „Uhr / Text“ und „Bild“ blenden Geschwindigkeit, Intensität und Palette aus.
- Element-Karten tragen die Nummer ihres Rahmens in der Vorschau; jede Einstellung ist eine
  beschriftete Zeile (`widgetRowHtml()`).
- Die einzige Bewegung ist das Ab-/Aufblenden des Streifens beim Ausschalten (0,35 s).
- Helles und dunkles Erscheinungsbild folgen der Systemeinstellung (`prefers-color-scheme`); einen
  Umschalter in der App gibt es bewusst nicht. Farben stehen nur als Rollen-Token in `:root`
  (`--fill`, `--fill-subtle`, `--fill-strong`, `--hover`, `--hairline-strong`, `--scrim`,
  `--shadow` neben den bisherigen) und bekommen im hellen Block ihre zweiten Werte. Neue Stile
  verwenden diese Token, keine festen `rgba(255,255,255,…)`/`rgba(0,0,0,…)`-Werte.
- Hell ist die Beschriftung auf dem Akzent weiß; `accentFromLight()` dunkelt die Lichtfarbe dann im
  selben Farbton ab, bis 4,5:1 erreicht sind (Standard hell: `#1D845F`). Die LED-Vorschauen bleiben
  in beiden Erscheinungsbildern schwarz – ein ausgeschaltetes Pixel ist schwarz.

---

Alle Schritte aus dem Design-Review sind erledigt. Neue Punkte kommen als neuer Schritt hierher.

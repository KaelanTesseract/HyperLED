# Using Plugins

A **plugin** is a small file that extends HyperLED with a function of its own. It can read data from the network, derive rules from it, control a segment accordingly or draw it itself with a script of its own - for example show the progress of a 3D printer, colour a segment by the outdoor temperature or make the state of a service in your home network visible. HyperLED itself knows nothing about these things: what happens is defined by the plugin alone.

Plugins are not part of the firmware. You can add, set up and remove them without updating the firmware, and they survive firmware updates and backups.

> If you want to write a plugin yourself, see [Developing Plugins](10_Plugins_entwickeln.md) and [Plugin Scripts](11_Plugin_Skripte.md). Ready-made examples are in the [`plugins/`](../../plugins/) folder, and the [Klipper Status Display](https://github.com/KaelanTesseract/HyperLED-Plugin-Klipper-Status) is a complete plugin of its own.

---

## Adding a plugin

Open **Settings → Plugins** and tap **+ Add plugin**. There are two ways:

- **Choose a file:** a plugin file (`.json`) from your device.
- **Load from an address:** the address of a plugin file, for example a release attachment on GitHub or a raw file. The controller does the loading itself and follows redirects (as GitHub releases use).

HyperLED then checks the plugin **completely before anything is saved** and shows you a **preview**:

| Line in the preview | Meaning |
|---|---|
| Name, version, author, licence | Who the plugin is from and under which terms it may be passed on. |
| **Asks this address** | The address the plugin will query later. Parts in `{ }` (for example `{host}`) you fill in yourself after installing. **Read this line:** a plugin may query this address and only this one. |
| Shows something on a segment | The plugin lays itself over a segment you choose. |
| Wants to switch the segment on and off | The plugin would like to switch the segment on or off. It may do so only if you explicitly allow it later. |
| Contains a script | The plugin draws the segment with a small program, frame by frame (see below). |
| Replaces the installed version | A plugin with the same id is already installed. Your settings are kept as far as they still fit. |
| Does not run on this device | The plugin needs a newer firmware or plugin interface. It is shut down after the installation (see [States](#states)). |

Only **Install** saves the plugin. The settings page opens right afterwards.

If the file is faulty you get a **message with the place** where the problem is (for example "Rule 2 when: …", or for a script "Skript: script:7: … near 'end'"). Nothing is saved.

---

## Setting up and switching on

The settings page of a plugin shows what the plugin wants to know from you - for example the address of your printer, a segment and colours. Required fields have a star (`*`).

- **Segment:** the list shows your segments by name. Only **one** plugin can run per segment.
- **Passwords and keys** are never shown. An empty field means "unchanged".
- **Save** sends only what you changed. The device checks every value and reports errors right at the field (for example "must be between 1 and 65535").

With **Plugin switched on** the plugin starts. If a required setting is still empty or the segment is taken, the reason appears right there and the plugin stays off.

Two switches appear only when they are needed:

- **May switch the segment on and off:** only for plugins that want this (for example "printer finished → light off"). Without this permission the plugin only changes how the segment looks.
- **Run anyway:** only for a plugin written for another plugin interface (see below). HyperLED asks once more before.

---

## States

The list shows a state for every plugin:

| State | Meaning | What to do |
|---|---|---|
| **Off** | The plugin is switched off. The segment is yours. | Switch it on if you want it. |
| **Waiting for an answer** | Switched on, the source has not answered yet. | Wait a few seconds. If it lasts, check the address in the settings. |
| **Running** | Answers arrive, the plugin shows something. | - |
| **No connection** | The source did not answer three times in a row (or its answer does not fit the plugin). The plugin shows what it provides for this case; otherwise the segment stays yours. It keeps trying. | Is the source (for example the printer) switched on and reachable? Is the address right? The reason is shown under the state. |
| **Not compatible** | The plugin needs a newer plugin interface, firmware or script level than this device offers. It stays stored and runs by itself after a suitable firmware update. | Update the firmware, or use an older version of the plugin. If you want to try anyway, switch on **Run anyway** in the settings; if it then fails repeatedly, it switches itself off. |
| **Invalid** | The stored file is damaged or does not fit this firmware. | Remove it and install it again. |

For plugins with a script an extra line appears below:

- **"Script running · 20.5 ms per frame":** the script draws the segment.
- **"Rules instead of the script: …":** the script cannot run at the moment (for example a Slave without script support, or an error in the script). The plugin shows what its rules provide instead, and says why.

---

## On the Light page

When a plugin controls a segment, the **Light** page shows at the top:

> **Controlled by plugin name · segment** [Settings] [Pause]

**Settings** opens the plugin's settings right there, so you can change a colour or the brightness of a status display without going to *Settings → Plugins*. **Pause** switches the plugin off; the segment is immediately the way you set it. This is how plugins work: a plugin only **lays itself over** a segment while drawing and **stores nothing**. Your settings (effect, colour, brightness …) stay as they are - after a restart, after switching the plugin off or removing it, everything is back. If you change effect, colour, speed or intensity of a controlled segment, HyperLED remembers it - but you only see it once the plugin is paused.

**Brightness** is always yours and takes effect immediately; a plugin cannot change it.

---

## Live values

**Live values** on a plugin shows what it is doing right now - without changing anything:

- the **state** and its reason,
- the **values read** (or "unknown"),
- **who is drawing**: a rule (with number and condition), the error state, the script, or nobody ("No rule matches. The segment stays as you set it."),
- the **beginning of the answer** of the source.

This helps when a plugin does not do what you expect: do you see the value you expect? Does the condition fit?

---

## Showing plugin values in a text

The values of a plugin can also be shown in a **Text** or **Lauftext** (scrolling text) element on a panel, for example "Outside {beispiel-wetter.temp} °C".

1. The plugin must be switched on and in the state *running*.
2. Open the **Panel** area, choose a segment with the effect "Uhr / Text" (clock / text) and add an element of type *Text* or *Lauftext*.
3. Type `{plugin-id.value}` into the text field, or pick a value under the field at **Insert a plugin value …**. The list shows the values of the running plugins with their current state and writes the placeholder at the cursor for you.

The controller fills in the current value and sends the finished text to Slaves as well. When the value changes, the text changes by itself.

- **`--`** is shown while a value is unknown: the plugin is off, paused, still waiting for its first answer, has no connection, is removed, or the value is missing from the answer. A stale value is never shown.
- **What is saved is the text with the placeholder**, never the filled-in value; no plugin changes it. If you remove the plugin the placeholder stays and shows `--`.
- Anything in braces that does not look like `{id.value}` is left as it is. A literal `{id.value}` cannot be written.
- Whole numbers appear without decimals, other numbers with two; true/false as `true` and `false`. Rounding and units belong in the plugin's value (see [Developing Plugins](10_Plugins_entwickeln.md)).
- The text shown is at most **64 bytes** long, umlauts count twice; anything longer is cut off. This applies to the finished text with the values filled in.
- A value that is there but empty gives no text (a Lauftext then disappears); only an *unknown* value gives `--`.
- A **Lauftext** does not restart when its text changes. It does jump when the *length* of the text changes.
- If the list of elements with the filled-in values no longer fits into one radio packet to the Slave, the Master draws the affected element itself and streams it.

---

## Updating, removing, backing up

- **Updating:** install the new version like a new plugin. If the id is the same, it replaces the old one; your settings are kept as far as they still fit.
- **Removing:** deletes the plugin together with its settings. The segment is yours again afterwards.
- **Backup and firmware updates:** plugins and their settings (passwords too!) are part of the backup and are kept across firmware updates. The backup file therefore contains passwords in plain text, just like the Wi-Fi and MQTT data - do not hand it on.

---

## What a plugin may do - and what not

A plugin is **data, not a program**, with one exception (scripts, see below). It can only:

- query the **one** address shown in the preview with **GET** (no other requests, nothing sent except the headers that are in the file and filled with your settings),
- lay itself over the **one** segment you chose, and only change effect, colour, speed and intensity; switching on and off only with your permission,
- read values and show them.

It has **no access** to Wi-Fi credentials, MQTT access, other settings or the file system.

A **script** may additionally draw pixels of its segment and knows only the settings and values of its plugin (**without passwords**: they would otherwise travel unencrypted over the air to a Slave). It can use neither the network nor files, and its computing time and memory are limited. A faulty or endless script is aborted and ends itself after several errors; the controller keeps running.

> **Still:** install plugins only from sources you trust. A plugin can query addresses in your home network (for example the interface of your router) and send your settings for this plugin - passwords included - to *the address* shown in the preview. That is why the preview shows it to you.

---

## Limits

| What | Limit |
|---|---|
| Plugins installed at the same time | 8 |
| Size of a plugin file | 16 KB (script in it: 8 KB) |
| Plugins per segment | 1 |
| Interval between requests | at the earliest every 2 seconds (depending on the plugin, often minutes) |
| Size of a source's answer | 8 KB |
| Waiting time per request | at most 10 seconds (1-10, depending on the plugin) |

Sources are queried **one after the other**, never at the same time: an encrypted connection needs a lot of memory.

---

## Troubleshooting

| Message | Meaning and remedy |
|---|---|
| **Keine Verbindung zur Quelle** (no connection to the source) | The address does not answer. Is the device (for example the printer) running and in the same network? Are address and port right? |
| **Die Quelle antwortet mit Fehler 404 / 401 / 500** (the source answers with error …) | The source refused the request. 401/403 usually means a key or password is missing or wrong. 404: the path is wrong, check the address in the preview. |
| **Die Antwort der Quelle ist kein JSON** (the answer is not JSON) | The address delivers something else (for example a web page). Check the address. |
| **Die Antwort der Quelle ist größer als 8 KB** (the answer is larger than 8 KB) | The plugin or the source does not fit this device. |
| **Die Antwort enthält nichts von dem, was das Plugin liest** (the answer contains nothing the plugin reads) | The source answers, but not in the structure the plugin expects (for example another device or version). Look at the beginning of the answer under **Live values**. |
| **Die Einstellung 'host' ist noch leer** (the setting is still empty) | A required setting is missing. |
| **Das Segment 2 wird schon vom Plugin 'X' gesteuert** (the segment is already controlled by plugin 'X') | Only one plugin runs per segment. Choose another segment or switch the other plugin off. |
| **Das Plugin braucht die Plugin-Schnittstelle 2, diese Firmware bietet 1** (the plugin needs interface 2, this firmware offers 1) | The plugin is newer than the firmware. Update the firmware. |
| **Der Slave kann noch keine Skripte** (the Slave cannot run scripts yet) | The segment lives on a Slave with older firmware (scripts need Slave firmware 0.3.000 or later). Until it is updated, the plugin shows what its rules provide. |
| **Das Skript ist fehlgeschlagen: …** (the script failed) | An error in the script. The message names the line. Until it is fixed, the plugin shows what its rules provide; the author should correct the script. |
| **Der Slave meldet sich nicht** (the Slave does not respond) | The script runs on a Slave that has said nothing for a while (radio disturbed, Slave off). Meanwhile the plugin shows what its rules provide. |

The device's messages are in German, with the place of the problem; this table names them as you will see them.

If a plugin does nothing although it says "Running": open **Live values**. If it says "No rule matches", the value read meets no condition of the plugin; the author can help.

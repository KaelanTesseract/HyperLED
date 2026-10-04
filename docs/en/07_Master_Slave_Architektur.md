# Master/Slave Architecture

One of HyperLED's most powerful features is its ability to link multiple ESP32 controllers together extremely reliably and in perfect sync. The high-performance protocol behind this is called **HyperBus**, and it works either wired or wireless.

## How It Works

One controller acts as the **Master**, other controllers in the chain act as **Slaves**. The Master assigns the segments and tells each Slave what to show – the picture itself is computed by the Slave wherever it can:

- **Effects:** The Master only sends the settings (effect, colour, brightness, speed …) when something changes, and the Slave runs the animation itself.
- **"Clock / Text" on a HUB75 panel:** From Slave firmware 0.2.004 on, the Slave draws every element itself – time, date, analog clock, text, scrolling text, weather and image. The Master sends the element list, the time every few seconds, the weather when it changes, and images exactly once: a Slave that lacks an image (after a restart, for example) requests it on its own and verifies it by checksum.
- **Background effect behind the elements:** From Slave firmware 0.2.007 on, an effect can run behind the elements. The Slave draws it itself; the Master only sends the settings. To keep the elements readable each one gets a dark outline (or a darkened box), and the background brightness is relative to the segment's, like the elements' own. The background only runs while the Slave draws every element itself – a moving background cannot be streamed as pixels.
- **Lua scripts:** From Slave firmware 0.3.000 on, the Master can hand a Slave a short Lua script that draws the Slave's picture, pixel by pixel, on the Slave itself. Nothing is streamed: the Master sends the script once (in small pieces, which the Slave checks by checksum), then only the values the script reads. See [Scripts on Slaves](#scripts-on-slaves).
- **Pixel stream as a fallback:** Only what a Slave cannot do itself (older firmware, the "Image" effect, an element that no longer fits into one packet) is computed by the Master, which then sends the changed pixels.

This keeps the Master free for the web interface and for coordinating the Slaves, and keeps the radio link from being flooded with picture data.

A Slave doesn't run its own control logic – its name, LED type, pinout, or matrix size are configured entirely through the Master's web interface.

### Two Transport Options

- **Wired (UART):** The highest reliability and speed. Master and Slave are connected via a data line plus a shared ground (default: Master TX GPIO 17 → Slave RX GPIO 16). Wired Slaves can pass further Slaves along on their own downlink port (daisy-chaining).
- **Wireless (ESP-NOW):** For Slaves where wiring isn't practical. On boot, a Slave automatically detects whether a wired connection is present – if not, it switches to ESP-NOW mode on its own and locks in.

A Slave doesn't need to be manually set to a transport mode before first use: it detects and remembers the right mode automatically.

> [!NOTE]
> In ESP-NOW mode, Master and Slave have to be on the same Wi-Fi channel. The Slave scans the channels in turn and then adopts the channel the Master tells it – the channel follows from whichever network the Master is connected to, and never needs to be configured. If the Master changes channel, the Slave starts scanning again after a few seconds of silence.

## Scripts on Slaves

A script is Lua text of at most 8 KB. It defines a function `frame(t, dt)` that draws with `px(x, y, r, g, b)`, `fill`, `clear` and `hsv`, and can read the settings (`settings`) and the values (`v`) the Master gives it. The picture is computed on the Slave, in a task of its own, so a slow script holds up neither the radio nor the panel.

- **Transfer:** The Master tells the Slave every 2 s what should run (`CMD_SET_SCRIPT`: on/off, brightness, size and checksum of the script) and which values it should read (`CMD_SET_SCRIPT_VALUES`). A Slave that does not have the script asks for it (`CMD_REQUEST_SCRIPT`) and receives it in pieces of at most 224 bytes, one every 20 ms (`CMD_SCRIPT_CHUNK`); it checks the checksum of the whole text before it runs it, and asks again if a piece was lost. The Slave reports its state on change and every 5 s (`CMD_SCRIPT_STATUS`).
- **State:** `GET /api/slaves` shows for each Slave whether it runs scripts (`scripts`) and, while one is running, its state, frame rate, frame time, memory, last message and a checksum of the last frame (`script`).
- **Limits:** Every call into a script has a memory ceiling (32 KB on a Slave) and a time budget (40 ms). A script that exceeds one, or fails three frames in a row (ten in a minute), is stopped and reported. A Slave whose Master falls silent for 10 s stops its script by itself and darkens the panel. While a script runs, the Slave ignores effect, element and pixel commands.
- **Safety:** A script sees only its own picture, the settings and the values. It cannot reach files, the network or anything else of the device.
- **Speed (64x64 panel):** about 20 ms per frame for a full-screen fill or gradient (50 frames per second), about 70 ms for three `sin` calls per pixel (14 frames per second); a strip of 124 LEDs needs under 1 ms.

## Setup

1. Wire the hardware (as described above for wired mode), or simply power the Slave (for ESP-NOW).
2. Open the Master controller's web interface and go to the **Slaves** tab in Settings.
3. Newly found Slaves announce themselves here automatically (ping/pong protocol) and can be named and configured.
4. Pick the matching LED type – for a HUB75 panel, choose **HUB75** and enter the panel's width, height, and driver chip if needed. The pinout itself is hardwired on the Slave (see [Hardware Setup](03_Hardware_Setup.md)).
5. After saving, the Slave shows up as its own segment on the main screen and can be controlled like any other segment.

> [!NOTE]
> Large HUB75 panels may need more RAM than is available on the ESP32-S3. The firmware detects this and safely refuses to initialize instead of crashing – in that case, choose a smaller panel or a lower color depth.

# Master/Slave Architecture

One of HyperLED's most powerful features is its ability to link multiple ESP32 controllers together extremely reliably and in perfect sync. The high-performance protocol behind this is called **HyperBus**, and it works either wired or wireless.

## How It Works

One controller acts as the **Master**, other controllers in the chain act as **Slaves**. On every frame, the Master sends the raw color data for the assigned LEDs as a packet to the Slaves. The Slaves listen to this stream and output it 1:1 to their own LED strip or HUB75 panel.

A Slave doesn't run its own control logic – its name, LED type, pinout, or matrix size are configured entirely through the Master's web interface.

### Two Transport Options

- **Wired (UART):** The highest reliability and speed. Master and Slave are connected via a data line plus a shared ground (default: Master TX GPIO 17 → Slave RX GPIO 16). Wired Slaves can pass further Slaves along on their own downlink port (daisy-chaining).
- **Wireless (ESP-NOW):** For Slaves where wiring isn't practical. On boot, a Slave automatically detects whether a wired connection is present – if not, it switches to ESP-NOW mode on its own and locks in.

A Slave doesn't need to be manually set to a transport mode before first use: it detects and remembers the right mode automatically.

> [!NOTE]
> In ESP-NOW mode, Master and Slave have to be on the same Wi-Fi channel. The Slave scans the channels in turn and then adopts the channel the Master tells it – the channel follows from whichever network the Master is connected to, and never needs to be configured. If the Master changes channel, the Slave starts scanning again after a few seconds of silence.

## Setup

1. Wire the hardware (as described above for wired mode), or simply power the Slave (for ESP-NOW).
2. Open the Master controller's web interface and go to the **Slaves** tab in Settings.
3. Newly found Slaves announce themselves here automatically (ping/pong protocol) and can be named and configured.
4. Pick the matching LED type – for a HUB75 panel, choose **HUB75** and enter the panel's width, height, and driver chip if needed. The pinout itself is hardwired on the Slave (see [Hardware Setup](03_Hardware_Setup.md)).
5. After saving, the Slave shows up as its own segment on the main screen and can be controlled like any other segment.

> [!NOTE]
> Large HUB75 panels may need more RAM than is available on the ESP32-S3. The firmware detects this and safely refuses to initialize instead of crashing – in that case, choose a smaller panel or a lower color depth.

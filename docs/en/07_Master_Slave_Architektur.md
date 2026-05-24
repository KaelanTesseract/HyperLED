# Master/Slave Architecture

One of the most powerful features of HyperLED is the ability to connect multiple ESP32 controllers together extremely reliably and in perfect sync. This is done **wired** via a highly performant serial protocol called **HyperBus** (using the UART pins).

## How it works
One controller acts as the **Master** (Sender). Other controllers in the chain act as **Slaves**.
During every frame calculation, the Master sends the raw color data of all LEDs as a high-speed serial packet (HyperBus) to the slaves. The slaves listen to this stream and forward it 1:1 to their own LED strips.

### Wiring (UART)
For communication, the Master and Slaves must be connected with a data line (+ common ground):
- **Master TX** (Default: GPIO 21) connects to **Slave RX** (Default: GPIO 20).
- All connected boards MUST share the same **Ground (GND)**!

## Setup
1. Connect the hardware pins as described above.
2. Open the web interface of your main controller.
3. Slaves on the HyperBus will automatically register with the Master (Ping/Pong protocol).
4. Go to the **LEDs** tab.
5. Create so-called **Slave Segments**. These are virtual sections whose calculated data is exclusively sent over the HyperBus to the corresponding Slave IDs.

> [!TIP]
> **Freeze Function:** A slave can be "frozen" via an API command (`/api/slaves?cmd=freeze`). In this state, it temporarily ignores the master stream. This allows you, for example, to sync an entire room but temporarily decouple individual display cases for a different action.

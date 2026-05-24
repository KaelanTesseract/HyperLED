# Installation & Flashing

This project uses **PlatformIO** (often as an extension in Visual Studio Code) instead of the Arduino IDE to perfectly manage dependencies and memory configurations (partitions).

## Prerequisites
1. Install Visual Studio Code.
2. Install the **PlatformIO IDE** extension in VS Code.
3. Download or clone the HyperLED source code.

## Step 1: Flash Firmware (C++ Code)
The C++ code handles Wi-Fi connectivity, LED control, and hosts the web server.

1. Open the `HyperLED` project folder in VS Code.
2. Connect your ESP32-C6 via USB to the PC.
3. Click the small **Checkmark Icon (Build)** in the bottom blue bar (or in the left PlatformIO menu) to ensure everything compiles correctly.
4. Click the **Right Arrow (Upload)**. The firmware will now be written to the ESP32.

## Step 2: Flash Web Interface (LittleFS)
> [!WARNING]
> If you forget this step, the ESP32 will boot and join Wi-Fi, but the browser will only show "Not Found" or a blank white screen because it lacks the HTML/CSS files!

The web frontend is located in the `/data/` folder and must be flashed into a special memory partition (LittleFS) on the ESP32.

1. Click the **PlatformIO Icon** (Alien head) in the left sidebar.
2. Under "Project Tasks", expand your environment (e.g., `esp32-c6-devkitc-1`).
3. Expand the **Platform** section.
4. First, click **Build Filesystem Image**.
5. Then, click **Upload Filesystem Image**.

## Step 3: First Boot (AP Mode)
After both steps are successful:
1. Search for new Wi-Fi networks on your smartphone.
2. Connect to the **HyperLED-AP** network.
3. A "Captive Portal" window should open automatically. If not, open your browser and go to `http://192.168.4.1`.
4. Enter the Wi-Fi credentials for your home network. The ESP will restart and connect to your router.

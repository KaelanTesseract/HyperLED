# Android Companion App

HyperLED features a custom-built Android app (written in Kotlin). It serves as a wrapper for the web interface but adds profound native features.

## App Features
1. **Auto-Discovery (mDNS):** You no longer need to know the IP address of the ESP32! The app automatically scans your Wi-Fi for devices providing the `_wled._tcp.` service.
2. **AP Mode Quick Start:** A blue button at the very top allows you to configure a completely new device (which isn't in your Wi-Fi yet) with one click when you are connected to its hotspot.
3. **Device List with Status:**
   - **Name:** Displays the internal hostname by default. Clicking it opens the full-screen web interface.
   - **Power Toggle:** A small icon shows the live status (Green = On, Gray = Off). Click to turn the LEDs on or off – instantly, without loading the UI.
   - **Rename (Pencil Icon):** Click the pencil to give the device a custom name (e.g., "Gaming Room"). This is saved persistently on your smartphone.

## Compiling & Installing the App
The app is located in the `HyperLED_Android` folder.
1. Open **Android Studio**.
2. Select *File -> Open* and navigate to the `HyperLED_Android` folder.
3. Wait for Gradle to finish syncing (the elephant icon in the top right).
4. Connect your Android smartphone (USB debugging must be enabled on the phone) or start an emulator.
5. Press the green **Run** (Play) button.

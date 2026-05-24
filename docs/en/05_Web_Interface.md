# Web Interface & MQTT

The core of operating HyperLED is the modern glassmorphism web interface.

## UI Structure
The interface is divided into 5 main tabs that perfectly adapt to desktop and smartphone screens (without horizontal scrolling):

1. **Effects:** Choose from various animations (Solid, Breathe, Rainbow, etc.) and adjust the brightness.
2. **Colors:** Select color palettes for your effects.
3. **LEDs:** 
   - Configure your LED segments.
   - Switch to the **Pixel Art Converter** for LED matrices.
   - Configure **Buttons** to physically turn LEDs on or off locally via hardware.
4. **WLAN / MQTT:** Configure network and smart home interfaces.
5. **System:** Displays IP address, firmware version, and allows OTA updates.

---

## MQTT Settings (Smart Home)

To integrate HyperLED into systems like Home Assistant, ioBroker, or Node-RED, the firmware features a robust MQTT client.

### Configuration
Go to the **WLAN / MQTT** tab in the web interface and scroll down:
1. **Enable MQTT:** Check the box.
2. **Broker IP:** Enter the IP address of your MQTT server (e.g., Mosquitto).
3. **Port:** Default is `1883`.
4. **User / Password:** If your broker requires authentication.
5. **Topic:** Provide a unique name (e.g., `livingroom/hyperled`). The controller listens for commands on this topic.

> [!TIP]
> **Payloads:** The controller expects JSON payloads that are almost identical to our API calls. Sending a payload like `{"on": true, "bri": 255}` to the configured topic turns the controller on at full brightness.

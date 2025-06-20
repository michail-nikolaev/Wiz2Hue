# Win2Hue - WiZ to Zigbee Bridge

> **Note**: This project was developed with assistance from AI (Claude Code)

Win2Hue is an ESP32-based IoT bridge that converts WiZ smart lights into Zigbee-compatible devices for Philips Hue ecosystems. The project enables WiZ WiFi lights to be controlled through Hue bridges, apps, and smart home systems that support Zigbee devices.

## Features

- **Universal Compatibility**: Makes WiZ lights appear as native Hue devices in Zigbee networks
- **Intelligent Discovery**: Automatic WiZ light detection on local network with persistent caching
- **Smart Capabilities**: Detects and respects bulb capabilities (RGB, RGBW, Tunable White, Dimmable, etc.)
- **Persistent Storage**: Caches discovered lights for fast startup using LittleFS filesystem
- **Visual Reset System**: Fast LED blinking during 3+ second button hold for system reset
- **Robust Reset**: Reset available during WiFi connection, Zigbee pairing, and normal operation

## Hardware Requirements

- **Seeed XIAO ESP32-C6** development board
- LED indicators connected to pins D0-D3 (Red, Blue, Green, Yellow)
- Built-in boot button for reset functionality

## Quick Start

1. **Configure WiFi**: Create `src/secrets.h` with your network credentials:
   ```cpp
   #define SSID "YourWiFiNetwork"
   #define PASSWORD "YourWiFiPassword"
   ```

2. **Flash Firmware**: Upload to Seeed XIAO ESP32-C6 using Arduino IDE or PlatformIO

3. **Discover Lights**: Device automatically scans for WiZ lights on startup

4. **Pair with Hue**: Join device to your Zigbee network using standard Hue pairing process

5. **Control**: WiZ lights now appear as Hue devices in your smart home system

## Architecture

- **WiZ Protocol**: UDP communication on port 38899 with JSON commands
- **Zigbee Emulation**: ESP32-C6 Zigbee stack with Hue device profiles
- **Persistent Caching**: LittleFS storage for discovered light configurations
- **Multi-Phase Reset**: System reset available throughout operation phases

## System Reset

Hold the boot button for 3+ seconds while observing fast LED blinking:
- Clears cached light configurations
- Resets Zigbee network settings
- Forces device restart for clean state

## Project Structure

```
src/
├── main.cpp        # Main application logic and system control
├── wiz.cpp         # WiZ light discovery and communication
├── lights.cpp      # Zigbee/Hue device emulation
├── wifi.cpp        # WiFi connection management
├── fs.cpp          # Filesystem operations and caching
└── wiz2hue.h       # Shared data structures and declarations
```

## Development

See [CLAUDE.md](CLAUDE.md) for detailed architecture documentation, development notes, and technical implementation details.

## License

This project is provided as-is for educational and personal use.

---

## Resources & References

The following resources were instrumental in developing this project:

- **Zigbee Hue Tutorial**: https://wejn.org/2025/01/zigbee-hue-llo-world/
- **ESP Zigbee SDK Issues**: https://github.com/espressif/esp-zigbee-sdk/issues/358
- **ZLL Lights Reference**: https://github.com/peeveeone/ZLL_Lights
- **Zigbee Lamp Implementation**: https://github.com/ks0777/zigbee-lamp
- **Hue Device Specifications**: https://github.com/ebaauw/homebridge-hue/wiki/ZigBee-Devices#hue-lights
- **WiZ Protocol Library**: https://github.com/sbidy/pywizlight
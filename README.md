# Win2Hue - WiZ to Zigbee Bridge

> **Note**: This project was developed with assistance from AI (Claude Code)

Win2Hue is an ESP32-based IoT bridge that converts WiZ smart lights into Zigbee-compatible devices for Philips Hue ecosystems. The project enables WiZ WiFi lights to be controlled through Hue bridges, apps, and smart home systems that support Zigbee devices.

## Features

- **Automatic Discovery**: Finds and configures WiZ lights on your network
- **Dynamic IP Updates**: Automatically updates cached light IP addresses when they change on the network
- **Dynamic Zigbee Bridge**: Creates appropriate Zigbee device types based on WiZ bulb capabilities  
- **Connection Monitoring**: Automatic monitoring and recovery from WiFi, Zigbee, or WiZ communication failures
- **Persistent Storage**: Caches discovered lights for fast startup with intelligent IP address management
- **Multi-threaded Architecture**: Individual FreeRTOS tasks for each light enable responsive, non-blocking communication
- **Thread-Safe Operations**: Mutex protection prevents race conditions during state updates and filesystem operations
- **Individual Rate Limiting**: Per-light command throttling to prevent overload
- **Smart Color Handling**: Intelligent RGB vs temperature mode detection and switching

## Hardware Requirements

- **ESP32-C6** development board (Seeed XIAO ESP32-C6 recommended, other ESP32-C6 models may work)

## System Reset & Recovery

**Manual Reset:**
Hold the boot button for 3+ seconds while observing fast LED blinking:
- Clears cached light configurations
- Resets Zigbee network settings
- Forces device restart for clean state

**Automatic Recovery:**
The system continuously monitors connections and automatically restarts when needed:
- **WiFi monitoring** every 30 seconds with reconnection attempts
- **Zigbee monitoring** every 60 seconds  
- **WiZ communication health** tracking with failure threshold
- **System restart** triggered on connection loss or critical failures

## Development

See [CLAUDE.md](CLAUDE.md) for detailed architecture documentation, development notes, and technical implementation details.


## Resources & References

The following resources were instrumental in developing this project:

- **Zigbee Hue Tutorial** by Michal J. ([@wejn](https://github.com/wejn)): https://wejn.org/2025/01/zigbee-hue-llo-world/
- **ESP Zigbee SDK Issue** by m4nu-el and all involved ([@m4nu-el](https://github.com/m4nu-el)): https://github.com/espressif/esp-zigbee-sdk/issues/358
- **ZLL Lights Reference** by [@peeveeone](https://github.com/peeveeone): https://github.com/peeveeone/ZLL_Lights
- **Zigbee Lamp Implementation** by [@ks0777](https://github.com/ks0777): https://github.com/ks0777/zigbee-lamp
- **Hue Device Specifications** by Erik Baauw ([@ebaauw](https://github.com/ebaauw)): https://github.com/ebaauw/homebridge-hue/wiki/ZigBee-Devices#hue-lights
- **WiZ Protocol Library** by [@sbidy](https://github.com/sbidy) and contributors: https://github.com/sbidy/pywizlight
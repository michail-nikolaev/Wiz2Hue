# Wiz2Hue - WiZ to Zigbee (Hue-compatible) Bridge

> **Note**: This project was developed with assistance from AI (Claude Code)

Wiz2Hue is an ESP32-based IoT bridge that converts WiZ smart lights into Zigbee-compatible devices for Philips Hue ecosystems. The project enables WiZ WiFi lights to be controlled through Hue bridges, apps, and smart home systems that support Zigbee devices.

## Features

- **Automatic Discovery**: Finds and configures WiZ lights on your network automatically
- **Dynamic IP Updates**: Automatically updates cached light IP addresses when they change on the network
- **Dual-Mode Leader System**: Intelligent bidirectional synchronization between WiZ and Zigbee devices
  - **WiZ-Leader Mode**: WiZ bulb controls state, system polls every 5 seconds for changes
  - **Hue-Leader Mode**: Hue commands temporarily control WiZ bulbs with 5-second timeout
- **Dynamic Zigbee Bridge**: Creates appropriate Zigbee device types based on WiZ bulb capabilities
- **Resilient Operation**: No system restart on WiZ communication failures (bulbs can be physically turned off)
- **Persistent Storage**: Caches discovered lights for fast startup with intelligent IP address management
- **Multi-threaded Architecture**: Individual FreeRTOS tasks for each light enable responsive, non-blocking communication
- **Thread-Safe Operations**: Mutex protection prevents race conditions during state updates and filesystem operations
- **Adaptive Rate Limiting**: Per-light command throttling with mode-specific timing intervals
- **Smart Color Handling**: Intelligent RGB vs temperature mode detection and switching

## How It Works

The bridge implements intelligent **dual-mode leader synchronization**:

**WiZ-Leader Mode (Default)**:
- WiZ bulbs control the lighting state
- System polls WiZ bulbs every 5 seconds to detect physical button presses or app changes
- Zigbee state automatically updates to match WiZ bulb changes

**Hue-Leader Mode (Temporary)**:
- Activated when Hue/Zigbee commands are received
- Bridge forces WiZ bulbs to match the commanded state
- Automatically returns to WiZ-Leader mode after 5 seconds
- Prevents conflicts during state transitions

This design provides seamless bidirectional control while preventing feedback loops and maintaining responsiveness.

## Hardware Requirements

- **ESP32-C6** development board (Seeed XIAO ESP32-C6 recommended, other ESP32-C6 models may work)

## System Reset & Recovery

**Manual Reset:**
Hold the boot button for 3+ seconds while observing fast LED blinking:
- Clears cached light configurations
- Resets Zigbee network settings
- Forces device restart for clean state

**Automatic Recovery:**
The system continuously monitors connections with selective recovery:
- **WiFi monitoring** every 30 seconds with reconnection attempts
- **Zigbee monitoring** every 60 seconds with automatic restart on failure
- **WiZ communication health** tracking for logging only (no automatic restart)
- **System restart** triggered only on WiFi/Zigbee connection loss
- **Resilient design** allows bulbs to be physically turned off without affecting system stability

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

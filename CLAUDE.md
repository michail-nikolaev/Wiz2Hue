# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Win2Hue is an ESP32-based IoT bridge that converts WiZ smart lights into Zigbee-compatible devices for Philips Hue ecosystems. The project runs on Seeed XIAO ESP32-C6 hardware and bridges WiFi-based WiZ lights to Zigbee networks, enabling control through Hue bridges and apps.

## Architecture Overview

### Core Components

**WiZ Light Management (`src/wiz.cpp`, `src/wiz2hue.h`)**
- **Discovery System**: Broadcasts UDP packets to find WiZ lights on local network
- **Capability Detection**: Parses module names to determine bulb types (RGB, RGBW, TW, DW, Socket, Fan)
- **State Management**: Reads/writes bulb state with capability-aware filtering
- **JSON Serialization**: Complete bidirectional conversion for debugging and data persistence

**Zigbee Bridge (`src/lights.cpp`)**
- **Hue Emulation**: Makes WiZ lights appear as native Hue devices
- **Zigbee Protocol**: ESP32-C6 Zigbee stack integration for network communication
- **Device Registration**: Manages device pairing and network joining

**System Control (`src/main.cpp`)**
- **Main Application Logic**: Setup, loop, and system coordination
- **Reset System**: Unified reset with visual feedback and reliable filesystem clearing
- **Button Handling**: Reset available during all connection phases (WiFi, Zigbee, setup, main loop)

**Network Layer (`src/wifi.cpp`)**
- **WiFi Management**: Connects to local network for WiZ discovery
- **Broadcast IP Discovery**: Automatically determines network broadcast address

**Filesystem Management (`src/fs.cpp`)**
- **Persistent Storage**: LittleFS operations for light caching and configuration
- **Cache Management**: Smart loading/saving of discovered light configurations
- **Reliable Reset**: Explicit filesystem synchronization to ensure data persistence

### Key Data Structures

**`WizBulbInfo`**: Complete device profile including IP, MAC, capabilities, and feature detection
**`WizBulbState`**: Current device state with -1 indicating "unknown" values
**`Features`**: Capability flags (brightness, color, color_tmp, effect, fan) with Kelvin ranges
**`BulbClass`**: Enum categorizing bulb types for appropriate command filtering

### State Management Pattern

1. **Discovery**: `discoverOrLoadLights()` tries cache first, falls back to network scan
2. **Persistent Storage**: Discovered lights cached in `/lights.json` on LittleFS
3. **State Reading**: `getBulbState()` queries current device state via UDP "getPilot"
4. **State Setting**: `setBulbState()` uses capability-aware filtering to send only supported parameters
5. **JSON Debug**: All operations output structured JSON for debugging

### File System Operations

- **Cache Management**: Lights stored in `/lights.json` for fast startup
- **Smart Discovery**: Uses cached lights if available, network discovery as fallback
- **Unified Reset**: `resetSystem()` clears both LittleFS cache and Zigbee network
- **Reliable Persistence**: `LittleFS.end()` forces write buffer flush before system reset
- **Filesystem Sync**: Explicit unmount/remount cycle ensures data integrity during resets

## Hardware Configuration

**Target**: Seeed XIAO ESP32-C6 (board: `seeed_xiao_esp32c6`)
**Framework**: Arduino ESP32 
**Platform**: Custom ESP32 platform with Zigbee support
**Partition**: Uses `zigbee_spiffs.csv` partition table (build flag: `-DZIGBEE_MODE_ED`)
**Filesystem**: LittleFS for persistent storage (uses "spiffs" partition name for Arduino compatibility)

**Pin Assignments:**
- D0: Red LED indicator
- D1: Blue LED indicator  
- D2: Green LED indicator
- D3: Yellow LED indicator
- BOOT_PIN: Reset button for Zigbee network reset

## Dependencies

**Core Libraries:**
- `bblanchon/ArduinoJson@^7.0.0`: JSON parsing and generation
- ESP32 Arduino LittleFS: Built-in filesystem for persistent storage
- ESP32 Zigbee SDK (via managed components)
- ESP WiFi/UDP stack

**Managed Components** (auto-downloaded):
- espressif__esp-zigbee-lib: Zigbee 3.0 protocol stack
- Various ESP-IDF components for networking and device management

## Development Notes

**WiZ Protocol**: Uses UDP port 38899 with JSON-based commands ("getPilot", "setPilot", "getSystemConfig")
**Capability Filtering**: Only sends supported parameters to prevent device errors (e.g., no RGB commands to white-only bulbs)
**State Defaults**: Use -1 for unknown/unset values throughout `WizBulbState` structure
**JSON Output**: All debug information uses structured JSON format for parsing and analysis
**Error Handling**: Comprehensive error messages with retry logic for network operations
**Reset System**: Visual feedback with fast LED blinking (100ms intervals) during button hold
**Filesystem Sync**: Explicit LittleFS flush operations prevent data loss during resets

## Configuration

**Network Settings**: Configure WiFi credentials in `src/secrets.h` (gitignored)
**Zigbee Mode**: Currently configured as End Device (`-DZIGBEE_MODE_ED`)
**Serial Monitor**: 115200 baud for debug output and state monitoring
**File System**: LittleFS mounted automatically, uses "spiffs" partition for Arduino compatibility
**Reset Behavior**: 3+ second button hold with fast LED blinking clears both filesystem cache and Zigbee network
**Reset Availability**: Reset function active during WiFi connection, Zigbee connection, setup, and main loop

## Partition Table (`zigbee_spiffs.csv`)

**Memory Layout** (ESP32-C6 4MB Flash):
- **Application**: 1.6MB (main firmware)
- **LittleFS**: 896KB (persistent storage in "spiffs" partition)
- **Zigbee Storage**: 20KB (network config + factory data)
- **Core Dump**: 64KB (crash debugging)
- **System**: 32KB (NVS + PHY calibration)

The system performs automatic WiZ discovery with intelligent caching - uses stored lights for fast startup, network discovery as fallback, and outputs comprehensive JSON logs for monitoring and debugging.
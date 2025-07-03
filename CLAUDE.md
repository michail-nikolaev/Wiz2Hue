# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Win2Hue is an ESP32-based IoT bridge that converts WiZ smart lights into Zigbee-compatible devices for Philips Hue ecosystems. The project runs on Seeed XIAO ESP32-C6 hardware and bridges WiFi-based WiZ lights to Zigbee networks, enabling control through Hue bridges and apps.

## Architecture Overview

### Core Components

**WiZ Light Management (`src/wiz.cpp`, `src/wiz2hue.h`)**
- **Discovery System**: Broadcasts UDP packets to find WiZ lights on local network
- **IP Address Management**: Automatically updates cached light IP addresses by MAC address matching
- **Capability Detection**: Parses module names to determine bulb types (RGB, RGBW, TW, DW, Socket, Fan)
- **State Management**: Reads/writes bulb state with capability-aware filtering
- **JSON Serialization**: Complete bidirectional conversion for debugging and data persistence
- **Health Monitoring**: Tracks communication failures and triggers system restart on excessive failures

**Zigbee Bridge (`src/lights.cpp`)**
- **Dynamic Light Creation**: Creates Zigbee lights dynamically based on discovered WiZ bulbs
- **ZigbeeWizLight Class**: Encapsulates each Zigbee-WiZ light pair with individual state management
- **Rate Limiting**: Individual 100ms rate limiting per light to prevent WiZ bulb overload
- **State Synchronization**: Reads actual WiZ bulb state on startup for accurate initial state
- **Zigbee Protocol**: ESP32-C6 Zigbee stack integration for network communication
- **Device Registration**: Manages device pairing and network joining
- **Network Monitoring**: Periodic Zigbee connection status checks with automatic restart on failure

**System Control (`src/main.cpp`)**
- **Main Application Logic**: Setup, loop, and system coordination
- **Reset System**: Unified reset with visual feedback and reliable filesystem clearing
- **Button Handling**: Reset available during all connection phases (WiFi, Zigbee, setup, main loop)
- **Connection Monitoring**: Automatic monitoring and restart on WiFi, Zigbee, or WiZ failures

**Network Layer (`src/wifi.cpp`)**
- **WiFi Management**: Connects to local network for WiZ discovery
- **Broadcast IP Discovery**: Automatically determines network broadcast address
- **Connection Monitoring**: Periodic WiFi status checks with automatic reconnection attempts

**Filesystem Management (`src/fs.cpp`)**
- **Persistent Storage**: LittleFS operations for light caching and configuration
- **Cache Management**: Smart loading/saving of discovered light configurations
- **Reliable Reset**: Explicit filesystem synchronization to ensure data persistence

### Key Data Structures

**`WizBulbInfo`**: Complete device profile including IP, MAC, capabilities, and feature detection
**`WizBulbState`**: Current device state with -1 indicating "unknown" values
**`Features`**: Capability flags (brightness, color, color_tmp, effect, fan) with Kelvin ranges
**`BulbClass`**: Enum categorizing bulb types for appropriate command filtering
**`ZigbeeWizLight`**: Class managing individual Zigbee-WiZ light pairs with state and rate limiting

### State Management Pattern

1. **Discovery**: `discoverOrLoadLights()` tries cache first, performs IP update check, falls back to network scan
2. **IP Address Updates**: `updateBulbIPs()` matches cached and discovered bulbs by MAC address and updates IPs
3. **Persistent Storage**: Discovered lights cached in `/lights.json` on LittleFS with automatic IP updates
4. **Dynamic Zigbee Creation**: `setup_lights()` creates ZigbeeWizLight objects for each discovered WiZ bulb
5. **Endpoint Assignment**: Consistent endpoint IDs (starting from 10) based on MAC address sorting
6. **Initial State Sync**: Each ZigbeeWizLight reads actual WiZ bulb state on startup
7. **State Reading**: `getBulbState()` queries current device state via UDP "getPilot"
8. **State Setting**: `setBulbState()` uses capability-aware filtering to send only supported parameters
9. **Rate Limited Updates**: Individual 100ms rate limiting per light with 10-second periodic refresh  
10. **Smart Color Handling**: Detects RGB vs temperature parameter changes and sends only relevant commands
11. **JSON Debug**: All operations output structured JSON for debugging

### File System Operations

- **Cache Management**: Lights stored in `/lights.json` for fast startup with automatic IP address updates
- **Smart Discovery**: Uses cached lights if available, performs IP update check, network discovery as fallback
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
**Error Handling**: Comprehensive error messages with retry logic for network operations (20 attempts for system config)
**Reset System**: Visual feedback with fast LED blinking (100ms intervals) during button hold
**Filesystem Sync**: Explicit LittleFS flush operations prevent data loss during resets

### Zigbee Light Management

**Dynamic Creation**: Zigbee lights are created dynamically based on discovered WiZ bulbs rather than hardcoded
**Endpoint Mapping**: Endpoints start from 10 and increment sequentially for each bulb (sorted by MAC)
**Individual Rate Limiting**: Each ZigbeeWizLight has independent 100ms rate limiting to prevent WiZ overload
**State Synchronization**: On startup, each light reads the actual WiZ bulb state for accurate initial values
**Capability Mapping**: WiZ bulb types automatically map to appropriate Zigbee light types:
- RGB → ESP_ZB_HUE_LIGHT_TYPE_EXTENDED_COLOR (supports full color + tunable white)
- TW → ESP_ZB_HUE_LIGHT_TYPE_TEMPERATURE
- DW → ESP_ZB_HUE_LIGHT_TYPE_DIMMABLE (or ON_OFF if no brightness)
- Socket/Fan → ESP_ZB_HUE_LIGHT_TYPE_ON_OFF

**Smart Color Mode Handling**:
- **Change Detection**: Monitors RGB vs temperature parameter changes using previous state comparison
- **Mode Isolation**: RGB changes exclude temperature parameters, temperature changes exclude RGB parameters  
- **Conflict Prevention**: Uses signed integers (int16_t) with -1 values to properly exclude parameters
- **Fallback Logic**: When no specific change detected, prioritizes RGB if any color values present

**WiZ Module Recognition**: Comprehensive support for WiZ module naming (ESP01/ESP14/ESP24/ESP25_SH/DH/LED_RGB/TW/DW)
**Periodic Updates**: Every 10 seconds, last known state is resent to ensure bulbs stay synchronized
**Optimized Callbacks**: Minimal Serial output in callbacks to reduce processing lag (optional debug mode available)
**Color Temperature**: Proper mireds/Kelvin conversions with validation and range clamping to prevent invalid values
**Test Mode Enhancement**: During Zigbee connection, cycles through random colors and temperatures for visual feedback

### IP Address Management

**Dynamic IP Updates**: The system automatically handles WiZ lights that change IP addresses after router restarts or DHCP renewals:

- **MAC-based Matching**: Uses MAC addresses as unique identifiers to track lights across IP changes
- **Boot-time Discovery**: When cached lights exist, performs network discovery to check for IP changes
- **Selective Updates**: Only updates IP addresses that have actually changed, preserving other cached data
- **Conditional Saving**: Cache file is only updated when IP address changes are detected
- **Fallback Protection**: If discovery fails, uses cached lights as-is to maintain system operation

**Implementation Details**:
- `updateBulbIPs()` function compares cached vs discovered bulbs by MAC address (`src/wiz.cpp:901-939`)
- `discoverOrLoadLights()` orchestrates the IP update process during system startup (`src/wiz.cpp:941-984`)
- Detailed logging shows which bulbs had IP updates and which remained unchanged
- No unnecessary file system writes when no changes are detected

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

## Connection Monitoring & Recovery

**Automatic Monitoring System**:
- **WiFi Monitoring**: Checks connection status every 30 seconds with automatic reconnection attempts (10-second timeout)
- **Zigbee Monitoring**: Verifies network connectivity every 60 seconds  
- **WiZ Health Tracking**: Monitors communication failures with 10-failure threshold before restart
- **System Recovery**: Automatic ESP.restart() on connection loss or critical health issues
- **Non-blocking Design**: All monitoring uses millis() timing to avoid interfering with light command processing

**Failure Recovery Patterns**:
1. **WiFi Disconnection**: Attempts reconnection, restarts system if failed
2. **Zigbee Network Loss**: Immediate system restart to rejoin network
3. **WiZ Communication Issues**: Tracks consecutive failures, restarts after threshold exceeded
4. **Manual Reset**: 3+ second button hold for immediate factory reset and restart

The system performs automatic WiZ discovery with intelligent caching - uses stored lights for fast startup, network discovery as fallback, creates dynamic Zigbee lights with individual rate limiting and state management, comprehensive connection monitoring with automatic recovery, and outputs structured JSON logs for monitoring and debugging.
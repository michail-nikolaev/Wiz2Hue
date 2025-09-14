# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Wiz2Hue is an ESP32-based IoT bridge that converts WiZ smart lights into Zigbee-compatible devices for Philips Hue ecosystems. The project runs on Seeed XIAO ESP32-C6 hardware and bridges WiFi-based WiZ lights to Zigbee networks, enabling control through Hue bridges and apps.

## Architecture Overview

### Core Components

**WiZ Light Management (`src/wiz.cpp`, `src/wiz2hue.h`)**
- **Discovery System**: Broadcasts UDP packets to find WiZ lights on local network using AsyncUDP
- **IP Address Management**: Automatically updates cached light IP addresses by MAC address matching
- **Capability Detection**: Parses module names to determine bulb types (RGB, RGBW, TW, DW, Socket, Fan)
- **State Management**: Reads/writes bulb state with capability-aware filtering using AsyncUDP for improved performance
- **JSON Serialization**: Complete bidirectional conversion for debugging and data persistence
- **Health Monitoring**: Tracks communication failures for logging (no automatic restart)

**Zigbee Bridge (`src/lights.cpp`)**
- **Dynamic Light Creation**: Creates Zigbee lights dynamically based on discovered WiZ bulbs
- **ZigbeeWizLight Class**: Encapsulates each Zigbee-WiZ light pair with dual-mode leader system
- **Dual-Mode Synchronization**: Intelligent bidirectional sync with WIZ_LEADER/HUE_LEADER modes
- **Multi-threaded Architecture**: Uses FreeRTOS tasks for asynchronous communication with individual WiZ bulbs
- **Thread-Safe State Management**: FreeRTOS mutexes protect shared state and filesystem operations
- **Adaptive Rate Limiting**: 100ms command interval, 5s WiZ-leader polling, 10s Hue-leader resend
- **State Synchronization**: Reads actual WiZ bulb state on startup and during mode transitions
- **Zigbee Protocol**: ESP32-C6 Zigbee stack integration for network communication (ZIGBEE_ROUTER mode)
- **Device Registration**: Manages device pairing and network joining
- **Network Monitoring**: Periodic Zigbee connection status checks with automatic restart on failure

**System Control (`src/main.cpp`)**
- **Main Application Logic**: Setup, loop, and system coordination
- **Reset System**: Unified reset with visual feedback and reliable filesystem clearing
- **Button Handling**: Reset available during all connection phases (WiFi, Zigbee, setup, main loop)
- **Connection Monitoring**: Automatic monitoring and restart on WiFi/Zigbee failures (WiZ failures logged only)
- **FreeRTOS Integration**: Uses FreeRTOS delay functions for proper task scheduling

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
**`ZigbeeWizLight`**: Class managing individual Zigbee-WiZ light pairs with dual-mode leader system and FreeRTOS communication tasks
**`LeaderMode`**: Enum with three states: WIZ_LEADER (default), HUE_LEADER (temporary), and IN_SYNC (synchronization state)

### State Management Pattern

1. **Discovery**: `discoverOrLoadLights()` tries cache first, performs IP update check, falls back to network scan
2. **IP Address Updates**: `updateBulbIPs()` matches cached and discovered bulbs by MAC address and updates IPs
3. **Persistent Storage**: Discovered lights cached in `/lights.json` on LittleFS with automatic IP updates
4. **Dynamic Zigbee Creation**: `setup_lights()` creates ZigbeeWizLight objects for each discovered WiZ bulb
5. **Endpoint Assignment**: Consistent endpoint IDs (starting from 10) based on MAC address sorting
6. **Initial State Sync**: Each ZigbeeWizLight reads actual WiZ bulb state on startup
7. **State Reading**: `getBulbState()` queries current device state via AsyncUDP "getPilot" with callback-based response handling
8. **State Setting**: `setBulbState()` uses capability-aware filtering to send only supported parameters via AsyncUDP
9. **Dual-Mode Leader System**: Implements intelligent bidirectional synchronization between WiZ and Zigbee
   - **WIZ_LEADER Mode**: Default state where WiZ broadcasts control Zigbee state updates
   - **HUE_LEADER Mode**: Temporary mode (5s timeout) triggered by Hue commands, forces WiZ to follow Zigbee
   - **IN_SYNC State**: Prevents feedback loops during state synchronization operations
10. **Multi-threaded Communication**: Each bulb has a dedicated FreeRTOS task for asynchronous communication
11. **Thread-Safe Operations**: State updates and filesystem operations protected by FreeRTOS mutexes
12. **Rate Limited Updates**: Individual 100ms rate limiting per light with 5-second periodic refresh in WIZ_LEADER mode
13. **Smart Color Handling**: Detects RGB vs temperature parameter changes and sends only relevant commands
14. **JSON Debug**: All operations output structured JSON for debugging

### File System Operations

- **Cache Management**: Lights stored in `/lights.json` for fast startup with automatic IP address updates
- **Smart Discovery**: Uses cached lights if available, performs IP update check, network discovery as fallback
- **Unified Reset**: `resetSystem()` clears both LittleFS cache and Zigbee network
- **Reliable Persistence**: `LittleFS.end()` forces write buffer flush before system reset
- **Filesystem Sync**: Explicit unmount/remount cycle ensures data integrity during resets
- **Thread-Safe Filesystem**: Global filesystem mutex prevents concurrent access during settings saves

## Hardware Configuration

**Target**: Seeed XIAO ESP32-C6 (board: `seeed_xiao_esp32c6`)
**Framework**: Arduino ESP32 
**Platform**: Custom ESP32 platform with Zigbee support
**Partition**: Uses `zigbee_spiffs.csv` partition table (build flag: `-DZIGBEE_MODE_ZCZR`)
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
- `esphome/AsyncUDP-ESP32@^2.0.0`: Asynchronous UDP communication library for improved performance
- ESP32 Arduino LittleFS: Built-in filesystem for persistent storage
- ESP32 Zigbee SDK (via managed components)
- ESP WiFi/UDP stack

**Managed Components** (auto-downloaded):
- espressif__esp-zigbee-lib: Zigbee 3.0 protocol stack
- Various ESP-IDF components for networking and device management

## Development Notes

**WiZ Protocol**: Uses UDP port 38899 with JSON-based commands ("getPilot", "setPilot", "getSystemConfig") via AsyncUDP for improved performance
**Capability Filtering**: Only sends supported parameters to prevent device errors (e.g., no RGB commands to white-only bulbs)
**State Defaults**: Use -1 for unknown/unset values throughout `WizBulbState` structure
**JSON Output**: All debug information uses structured JSON format for parsing and analysis
**Error Handling**: Comprehensive error messages with retry logic for network operations (20 attempts for system config)
**Reset System**: Visual feedback with fast LED blinking (100ms intervals) during button hold
**Filesystem Sync**: Explicit LittleFS flush operations prevent data loss during resets
**Multi-threading**: FreeRTOS tasks handle individual bulb communication asynchronously
**Thread Safety**: Mutexes protect shared state and filesystem operations from race conditions
**Task Management**: Communication tasks created per bulb with proper cleanup in destructors

### Zigbee Light Management

**Dynamic Creation**: Zigbee lights are created dynamically based on discovered WiZ bulbs rather than hardcoded
**Endpoint Mapping**: Endpoints start from 10 and increment sequentially for each bulb (sorted by MAC)
**Individual Rate Limiting**: Each ZigbeeWizLight has independent 100ms rate limiting to prevent WiZ overload
**State Synchronization**: On startup, each light reads the actual WiZ bulb state for accurate initial values
**Asynchronous Communication**: Each bulb has a dedicated FreeRTOS task for non-blocking communication
**Thread-Safe State Updates**: State changes protected by mutexes to prevent race conditions
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

**Dual-Mode Leader System**:
- **WIZ_LEADER Mode (Default)**: WiZ bulb controls state, system polls bulb every 5 seconds to detect changes
- **HUE_LEADER Mode (Temporary)**: Triggered by Hue commands, forces WiZ to follow Zigbee state with 5-second timeout
- **IN_SYNC State**: Temporary state during synchronization to prevent feedback loops
- **Mode Transitions**: Automatic timeout-based switching between leader modes
- **Polling-Based Monitoring**: Regular state polling instead of broadcast listening for simplicity

**WiZ Module Recognition**: Comprehensive support for WiZ module naming (ESP01/ESP14/ESP24/ESP25_SH/DH/LED_RGB/TW/DW)
**Adaptive Periodic Updates**: 5-second WiZ state polling in WIZ_LEADER mode, 10-second Hue command resend in HUE_LEADER mode
**Resilient Operation**: No automatic system restart on WiZ communication failures (bulbs can be physically turned off)
**Color Temperature**: Proper mireds/Kelvin conversions with validation and range clamping to prevent invalid values
**Test Mode Enhancement**: During Zigbee connection, cycles through random colors and temperatures for visual feedback
**Task-based Communication**: FreeRTOS tasks handle periodic updates and state changes asynchronously
**Filesystem Coordination**: Removed individual light settings files, global mutex ensures thread-safe operations

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
**Zigbee Mode**: Currently configured as Coordinator (`-DZIGBEE_MODE_ZCZR`)
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
- **WiZ Health Tracking**: Monitors communication failures for logging purposes only (no automatic restart)
- **System Recovery**: Automatic ESP.restart() on WiFi/Zigbee connection loss only
- **Non-blocking Design**: All monitoring uses millis() timing to avoid interfering with light command processing

**Failure Recovery Patterns**:
1. **WiFi Disconnection**: Attempts reconnection, restarts system if failed
2. **Zigbee Network Loss**: Immediate system restart to rejoin network
3. **WiZ Communication Issues**: Logs failures but continues operation (bulbs may be physically off)
4. **Manual Reset**: 3+ second button hold for immediate factory reset and restart

## Dual-Mode Leader Operation

The system implements intelligent bidirectional synchronization between WiZ and Zigbee devices:

**Default Operation (WIZ_LEADER Mode)**:
- WiZ bulb state is monitored via periodic polling
- Periodic 5-second state polling to detect WiZ changes
- Updates Zigbee light to match WiZ state changes

**Hue Command Handling (HUE_LEADER Mode)**:
- Triggered by any Zigbee/Hue command
- Sends commands to WiZ bulb to match Zigbee state
- 5-second timeout automatically returns to WIZ_LEADER mode
- Prevents feedback loops with IN_SYNC state during updates

**Resilient Design**:
- No system restart on WiZ communication failures
- Bulbs can be physically turned off without affecting system stability
- Continues operation even when some bulbs are unreachable
- Comprehensive logging for debugging without system disruption

The system performs automatic WiZ discovery with intelligent caching, creates dynamic Zigbee lights with dual-mode synchronization, uses individual FreeRTOS communication tasks with thread-safe state management, and provides comprehensive monitoring with selective recovery for maximum uptime.
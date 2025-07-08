#include "wiz2hue.h"
#include <WiFiUdp.h>
#include <AsyncUDP.h>
#include <ArduinoJson.h>
#include <vector>

const int WIZ_PORT = 38899;
const int DISCOVERY_TIMEOUT = 10000;       // 10 seconds total discovery time
const int RESPONSE_TIMEOUT = 3000;         // 3 seconds per device config request
const int BROADCAST_ATTEMPTS = 3;          // Number of broadcast attempts
const int BROADCAST_DELAY = 500;           // Delay between broadcasts in ms
const int SOCKET_TIMEOUT = 1000;           // Socket receive timeout in ms
const int RETRY_BROADCAST_INTERVAL = 3000; // Retry broadcast every 3 seconds

// Global UDP transmission rate limiting to prevent buffer overflow
static unsigned long lastGlobalUdpSend = 0;
const int GLOBAL_UDP_DELAY = 20;           // 20ms minimum between any UDP sends

// Helper function to enforce global UDP rate limiting
static void enforceGlobalUdpDelay() {
    unsigned long timeSinceLastSend = millis() - lastGlobalUdpSend;
    if (timeSinceLastSend < GLOBAL_UDP_DELAY) {
        delay(GLOBAL_UDP_DELAY - timeSinceLastSend);
    }
    lastGlobalUdpSend = millis();
}

std::vector<WizBulbInfo> scanForWiz(IPAddress broadcastIP)
{
    std::vector<WizBulbInfo> discoveredBulbs;
    WiFiUDP udp;

    // Use a different port for listening to avoid conflicts
    if (!udp.begin(38900))
    {
        Serial.println("Failed to start UDP for Wiz discovery");
        return discoveredBulbs;
    }

    Serial.println("=== Wiz Lights Discovery Tool ===");
    Serial.println("Scanning for Wiz devices...");
    Serial.printf("Broadcasting to: %s:%d\n", broadcastIP.toString().c_str(), WIZ_PORT);

    // Wiz discovery message - getPilot command
    String discoveryMessage = "{\"method\":\"getPilot\",\"params\":{}}";

    // Track discovered devices to avoid duplicates
    std::vector<IPAddress> discoveredIPs;

    Serial.printf("Making %d broadcast attempts...\n", BROADCAST_ATTEMPTS);

    // Send multiple broadcasts to increase chance of discovery
    for (int attempt = 1; attempt <= BROADCAST_ATTEMPTS; attempt++)
    {
        Serial.printf("Broadcast attempt %d of %d\n", attempt, BROADCAST_ATTEMPTS);

        udp.beginPacket(broadcastIP, WIZ_PORT);
        udp.print(discoveryMessage);
        bool sent = udp.endPacket();
        
        if (!sent) {
            Serial.printf("  Warning: Broadcast attempt %d failed (TX buffer full)\n", attempt);
            delay(100); // Extra delay on failure
        }

        // Add delay between broadcasts (except last one)
        if (attempt < BROADCAST_ATTEMPTS)
        {
            delay(BROADCAST_DELAY);
        }
    }

    unsigned long startTime = millis();
    unsigned long lastRetryBroadcast = millis();
    int deviceCount = 0;

    Serial.printf("Listening for responses for %d seconds...\n", DISCOVERY_TIMEOUT / 1000);
    Serial.println("(Waiting for Wiz lights to respond...)");

    // Listen for responses with retry logic
    while (millis() - startTime < DISCOVERY_TIMEOUT)
    {
        int packetSize = udp.parsePacket();

        if (packetSize)
        {
            IPAddress deviceIP = udp.remoteIP();

            Serial.printf("Raw packet received from %s:%d, size: %d bytes\n",
                          deviceIP.toString().c_str(), udp.remotePort(), packetSize);

            // Check for duplicates
            bool isDuplicate = false;
            for (const IPAddress &ip : discoveredIPs)
            {
                if (ip == deviceIP)
                {
                    Serial.printf("Received duplicate response from %s, skipping...\n", deviceIP.toString().c_str());
                    isDuplicate = true;
                    break;
                }
            }

            if (isDuplicate)
            {
                // Still need to read the packet to clear the buffer
                char dummyBuffer[512];
                udp.read(dummyBuffer, sizeof(dummyBuffer));
                continue;
            }

            // Add to discovered devices
            discoveredIPs.push_back(deviceIP);
            deviceCount++;

            // Read the response with better buffer handling
            char incomingPacket[512];
            int len = udp.read(incomingPacket, sizeof(incomingPacket) - 1);
            if (len > 0)
            {
                incomingPacket[len] = '\0';
            }
            else
            {
                Serial.println("Warning: Failed to read packet data");
                strcpy(incomingPacket, "{}");
            }

            Serial.printf("\n----- Discovered Wiz Light #%d -----\n", deviceCount);
            Serial.printf("IP Address: %s\n", deviceIP.toString().c_str());
            Serial.printf("Port: %d\n", udp.remotePort());
            Serial.printf("Response length: %d bytes\n", len);

            // Parse initial response for quick info
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, incomingPacket);

            if (!error && doc["result"].is<JsonObject>())
            {
                JsonObject result = doc["result"];
                if (result["mac"].is<const char *>())
                {
                    Serial.printf("MAC: %s\n", result["mac"].as<const char *>());
                }
                if (result["rssi"].is<int>())
                {
                    Serial.printf("RSSI: %d dBm\n", result["rssi"].as<int>());
                }
                Serial.printf("Debug: Successfully processed response from device with MAC: %s\n",
                              result["mac"].is<const char *>() ? result["mac"].as<const char *>() : "Unknown");
            }
            else
            {
                Serial.printf("Failed to parse initial response: %s\n", incomingPacket);
                if (error)
                {
                    Serial.printf("JSON error: %s\n", error.c_str());
                }
            }

            // No delay here - process packets as fast as possible
        }
        else
        {
            // Check if we should send another retry broadcast
            if (millis() - lastRetryBroadcast > RETRY_BROADCAST_INTERVAL)
            {
                unsigned long remaining = (DISCOVERY_TIMEOUT - (millis() - startTime)) / 1000;
                if (remaining > 0)
                {
                    Serial.printf("Socket timeout, sending retry broadcast... (%lu seconds remaining)\n", remaining);

                    udp.beginPacket(broadcastIP, WIZ_PORT);
                    udp.print(discoveryMessage);
                    bool sent = udp.endPacket();
                    
                    if (!sent) {
                        Serial.println("  Warning: Retry broadcast failed (TX buffer full)");
                    }

                    lastRetryBroadcast = millis();
                }
            }

            delay(10); // Smaller delay to catch more responses
        }
    }

    if (deviceCount > 0)
    {
        Serial.println("\n=== Discovery completed successfully ===");
        Serial.printf("Found %d Wiz light(s) on your network.\n", deviceCount);

        Serial.println("\nDiscovered devices:");
        for (const IPAddress &ip : discoveredIPs)
        {
            Serial.printf("- %s\n", ip.toString().c_str());
        }

        // Now get system configuration for each device
        Serial.println("\n=== Getting device capabilities ===");
        for (size_t i = 0; i < discoveredIPs.size(); i++)
        {
            Serial.printf("\nDevice %d/%d: %s\n", i + 1, discoveredIPs.size(), discoveredIPs[i].toString().c_str());
            WizBulbInfo bulbInfo = getSystemConfig(discoveredIPs[i]);
            
            if (!bulbInfo.isValid) {
                Serial.printf("  Failed to get configuration: %s\n", bulbInfo.errorMessage.c_str());
            } else {
                // Add bulb to results regardless of configuration success
                discoveredBulbs.push_back(bulbInfo);
            }            

            // Add delay between config requests to prevent overwhelming devices
            if (i < discoveredIPs.size() - 1)
            {
                delay(500);
            }
        }

        Serial.println("\n=== All device information collected ===");
        Serial.printf("Successfully discovered %d Wiz light(s) with capabilities.\n", discoveredBulbs.size());
    }
    else
    {
        Serial.println("\n=== Discovery completed with no results ===");
        Serial.println("No Wiz lights found on your network. Possible reasons:");
        Serial.println("- No Wiz lights are powered on");
        Serial.println("- Wiz lights are on a different network");
        Serial.println("- Firewall is blocking UDP traffic on port 38899");
        Serial.println("- Network doesn't allow UDP broadcasts");
        Serial.println("\nTroubleshooting tips:");
        Serial.println("1. Make sure your Wiz lights are powered on and connected to your WiFi network");
        Serial.println("2. Check if lights are on the same network segment");
        Serial.println("3. Verify firewall settings allow UDP broadcasts");
    }
    
    udp.stop();
    return discoveredBulbs;
}

BulbClass determineBulbClass(const String& moduleName) {
    String moduleNameUpper = moduleName;
    moduleNameUpper.toUpperCase();
    
    // Based on WiZ module naming: ESP##_[SH/DH/LED][RGB/TW/DW][#C]_##
    // Where: ESP## = hardware platform, SH/DH/LED = form factor, RGB/TW/DW = capability
    
    // Special device types first
    if (moduleNameUpper.indexOf("SOCKET") >= 0) {
        return BulbClass::SOCKET;
    }
    else if (moduleNameUpper.indexOf("FAN") >= 0) {
        return BulbClass::FAN;
    }
    // RGB bulbs (Full color + tunable white + effects)
    // Matches: ESP01_SHRGB1C_31, ESP01_SHRGB_03, ESP14_SHRGB1C_01, ESP24_SHRGB_01, etc.
    else if (moduleNameUpper.indexOf("SHRGB") >= 0 || 
             moduleNameUpper.indexOf("DHRGB") >= 0 || 
             moduleNameUpper.indexOf("LEDRGB") >= 0) {
        return BulbClass::RGB;
    }
    // Tunable White (CCT control + dimming, 2700K-6500K)
    // Matches: ESP01_SHTW1C_31, etc.
    else if (moduleNameUpper.indexOf("SHTW") >= 0 || 
             moduleNameUpper.indexOf("DHTW") >= 0 || 
             moduleNameUpper.indexOf("LEDTW") >= 0) {
        return BulbClass::TW;
    }
    // Dimmable White (brightness only, some ~1800K filaments)
    // Matches: ESP01_SHDW1_31, etc.
    else if (moduleNameUpper.indexOf("SHDW") >= 0 || 
             moduleNameUpper.indexOf("DHDW") >= 0 || 
             moduleNameUpper.indexOf("LEDDW") >= 0) {
        return BulbClass::DW;
    }
    
    return BulbClass::UNKNOWN;
}

Features determineBulbFeatures(BulbClass bulbClass) {
    Features features;
    
    switch (bulbClass) {
        case BulbClass::RGB:
            // RGB bulbs: Full color + tunable white + brightness + effects
            // Examples: ESP01_SHRGB1C_31, ESP01_SHRGB_03, ESP14_SHRGB1C_01, ESP24_SHRGB_01
            features.brightness = true;
            features.color = true;
            features.color_tmp = true;  // RGB bulbs support full color AND tunable white
            features.effect = true;
            features.kelvin_range = {2200, 6500}; // Full range based on documentation
            break;
            
            
        case BulbClass::TW:
            // Tunable White: CCT control + dimming only (2700K-6500K)
            // Examples: ESP01_SHTW1C_31
            features.brightness = true;
            features.color_tmp = true;
            features.kelvin_range = {2700, 6500}; // Standard tunable white range
            break;
            
        case BulbClass::DW:
            // Dimmable White: brightness only, some ~1800K filaments
            // Examples: ESP01_SHDW1_31
            features.brightness = true;
            // No color temperature control - fixed color
            features.kelvin_range = {1800, 1800}; // Some filaments are ~1800K
            break;
            
        case BulbClass::FAN:
            // Fan lights - assume similar to RGB but with fan control
            features.brightness = true;
            features.color = true;
            features.color_tmp = true;
            features.effect = true;
            features.fan = true;
            features.kelvin_range = {2700, 6500};
            break;
            
        case BulbClass::SOCKET:
            // Smart plugs/sockets: on/off only (some have power monitoring)
            // Examples: ESP25_SOCKET_01
            // No lighting features - pure switch functionality
            break;
            
        default:
            // Unknown modules - assume basic brightness only for safety
            features.brightness = true;
            features.kelvin_range = {2700, 2700}; // Safe default
            break;
    }
    
    return features;
}

WizBulbInfo getSystemConfig(IPAddress deviceIP)
{
    WizBulbInfo bulbInfo;
    bulbInfo.ip = deviceIP.toString();
    
    AsyncUDP udp;

    // System config request
    String configMessage = "{\"method\":\"getSystemConfig\",\"params\":{}}";
    const int CONFIG_ATTEMPTS = 20;     // Number of attempts to get system config
    const int CONFIG_RETRY_DELAY = 500; // Delay between retries

    bool configReceived = false;
    bool responseReceived = false;

    // Set up callback for received packets
    udp.onPacket([&](AsyncUDPPacket packet) {
        if (configReceived) return; // Already got response
        
        // Limit response size to prevent crashes
        const int MAX_RESPONSE_SIZE = 800;
        char response[MAX_RESPONSE_SIZE];
        int len = min((int)packet.length(), MAX_RESPONSE_SIZE - 1);
        memcpy(response, packet.data(), len);
        response[len] = '\0';

        Serial.println("System Configuration:");

        // Use JsonDocument to save memory
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, response);

        if (!error)
        {
            if (doc["result"].is<JsonObject>())
            {
                JsonObject result = doc["result"];

                // Extract key information safely
                bulbInfo.moduleName = result["moduleName"] | "Unknown";
                bulbInfo.fwVersion = result["fwVersion"] | "Unknown";
                bulbInfo.mac = result["mac"] | "Unknown";
                bulbInfo.rssi = result["rssi"] | 0;
                bulbInfo.src = result["src"] | "Unknown";
                bulbInfo.homeId = result["homeId"] | "Unknown";
                bulbInfo.roomId = result["roomId"] | "Unknown";

                // Determine bulb class and features
                bulbInfo.bulbClass = determineBulbClass(bulbInfo.moduleName);
                bulbInfo.features = determineBulbFeatures(bulbInfo.bulbClass);
                bulbInfo.isValid = true;

                // Print information as JSON
                Serial.printf("%s\n", wizBulbInfoToJson(bulbInfo).c_str());

                // Only print full response if it's reasonably sized
                if (strlen(response) < 400)
                {
                    Serial.printf("  Full capabilities: %s\n", response);
                }
                else
                {
                    Serial.println("  Full capabilities: [Response too large to display]");
                }

                configReceived = true;
            }
            else
            {
                Serial.println("  Response doesn't contain 'result' field");
                Serial.printf("  Raw response: %s\n", response);
                bulbInfo.errorMessage = "Invalid response format";
                configReceived = true; // Still count as received
            }
        }
        else
        {
            Serial.printf("  Failed to parse JSON response: %s\n", error.c_str());
            bulbInfo.errorMessage = "JSON parse error: " + String(error.c_str());
            // Only print response if it's not too large
            if (strlen(response) < 1024)
            {
                Serial.printf("  Raw response: %s\n", response);
            }
            else
            {
                Serial.println("  Raw response: [Too large to display]");
            }
            configReceived = true; // Still count as received
        }
        
        responseReceived = true;
    });

    // Start listening on a random port
    if (!udp.listen(0)) {
        Serial.println("Failed to start AsyncUDP for system config request");
        bulbInfo.errorMessage = "Failed to start AsyncUDP";
        return bulbInfo;
    }

    for (int attempt = 1; attempt <= CONFIG_ATTEMPTS && !configReceived; attempt++)
    {
        if (attempt > 1)
        {
            Serial.printf("  Retrying system config request (attempt %d/%d)...\n", attempt, CONFIG_ATTEMPTS);
            delay(CONFIG_RETRY_DELAY);
        }

        // Send request to specific device with error checking
        size_t sentBytes = udp.writeTo((const uint8_t*)configMessage.c_str(), configMessage.length(), deviceIP, WIZ_PORT);
        
        if (sentBytes == 0) {
            Serial.printf("  Warning: Config request attempt %d failed (AsyncUDP send error)\n", attempt);
            continue; // Skip to next attempt
        }

        unsigned long startTime = millis();
        responseReceived = false;

        // Wait for response with shorter timeout per attempt
        while (millis() - startTime < (RESPONSE_TIMEOUT / CONFIG_ATTEMPTS) && !responseReceived)
        {
            delay(10);
        }

        if (configReceived) {
            break;
        }
    }

    if (!configReceived)
    {
        Serial.println("  System Configuration: Timeout - no response received");
        Serial.printf("  Failed to get system config after %d attempts\n", CONFIG_ATTEMPTS);
        bulbInfo.errorMessage = "Timeout - no response";
    }

    udp.close();
    return bulbInfo;
}

WizBulbState getBulbState(IPAddress deviceIP)
{
    WizBulbState bulbState;
    AsyncUDP udp;

    // State request - getPilot command
    String stateMessage = "{\"method\":\"getPilot\",\"params\":{}}";
    const int STATE_ATTEMPTS = 2;
    const int STATE_RETRY_DELAY = 300;

    bool stateReceived = false;
    bool responseReceived = false;

    // Set up callback for received packets
    udp.onPacket([&](AsyncUDPPacket packet) {
        if (stateReceived) return; // Already got response
        
        const int MAX_RESPONSE_SIZE = 512;
        char response[MAX_RESPONSE_SIZE];
        int len = min((int)packet.length(), MAX_RESPONSE_SIZE - 1);
        memcpy(response, packet.data(), len);
        response[len] = '\0';

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, response);

        if (!error)
        {
            if (doc["result"].is<JsonObject>())
            {
                JsonObject result = doc["result"];

                // Extract state information
                bulbState.state = result["state"] | false;
                bulbState.dimming = result["dimming"] | -1;
                
                // Color values
                bulbState.r = result["r"] | -1;
                bulbState.g = result["g"] | -1;
                bulbState.b = result["b"] | -1;
                bulbState.c = result["c"] | -1;
                bulbState.w = result["w"] | -1;
                
                // Color temperature
                bulbState.temp = result["temp"] | -1;
                
                // Scene and effects
                bulbState.sceneId = result["sceneId"] | -1;
                bulbState.speed = result["speed"] | -1;
                
                // Fan speed (if present)
                bulbState.fanspd = result["fanspd"] | -1;

                bulbState.isValid = true;
                bulbState.lastUpdated = millis();

                Serial.printf(" Bulb State raw response: %s\n", response);

                stateReceived = true;
            }
            else
            {
                Serial.println("  State response doesn't contain 'result' field");
                bulbState.errorMessage = "Invalid state response format";
                stateReceived = true;
            }
        }
        else
        {
            Serial.printf("  Failed to parse state JSON: %s\n", error.c_str());
            bulbState.errorMessage = "JSON parse error: " + String(error.c_str());
            stateReceived = true;
        }
        
        responseReceived = true;
    });

    // Start listening on a random port
    if (!udp.listen(0)) {
        Serial.println("Failed to start AsyncUDP for bulb state request");
        bulbState.errorMessage = "Failed to start AsyncUDP";
        return bulbState;
    }

    for (int attempt = 1; attempt <= STATE_ATTEMPTS && !stateReceived; attempt++)
    {
        if (attempt > 1)
        {
            Serial.printf("  Retrying state request (attempt %d/%d)...\n", attempt, STATE_ATTEMPTS);
            delay(STATE_RETRY_DELAY);
        }

        // Send request to specific device with error checking
        size_t sentBytes = udp.writeTo((const uint8_t*)stateMessage.c_str(), stateMessage.length(), deviceIP, WIZ_PORT);
        
        if (sentBytes == 0) {
            Serial.printf("  Warning: State request attempt %d failed (AsyncUDP send error)\n", attempt);
            continue; // Skip to next attempt
        }

        unsigned long startTime = millis();
        responseReceived = false;

        // Wait for response
        while (millis() - startTime < (RESPONSE_TIMEOUT / STATE_ATTEMPTS) && !responseReceived)
        {
            delay(10);
        }

        if (stateReceived) {
            break;
        }
    }

    if (!stateReceived)
    {
        Serial.println("  Bulb State: Timeout - no response received");
        bulbState.errorMessage = "Timeout - no state response";
    }

    udp.close();
    return bulbState;
}

bool setBulbStateInternal(IPAddress deviceIP, const WizBulbState& state, const Features& features)
{
    AsyncUDP udp;

    // Build setPilot command JSON with capability checking
    JsonDocument doc;
    doc["method"] = "setPilot";
    JsonObject params = doc["params"].to<JsonObject>();
    
    // Basic state - always supported
    params["state"] = state.state;
    
    // Brightness - only if supported and not unknown
    if (features.brightness && state.dimming >= 0 && state.dimming <= 100) {
        params["dimming"] = state.dimming;
    }
    
    // Color information - only for bulbs that support color and values are not unknown
    if (features.color) {
        if (state.r >= 0 && state.r <= 255) params["r"] = state.r;
        if (state.g >= 0 && state.g <= 255) params["g"] = state.g;
        if (state.b >= 0 && state.b <= 255) params["b"] = state.b;
        // Cold/warm white for RGBW bulbs
        if (state.c >= 0 && state.c <= 255) params["c"] = state.c;
        if (state.w >= 0 && state.w <= 255) params["w"] = state.w;
    }
    
    // Color temperature - only if supported, not unknown, and within range
    if (features.color_tmp && state.temp >= 0 && 
        state.temp >= features.kelvin_range.min && state.temp <= features.kelvin_range.max) {
        params["temp"] = state.temp;
    }
    
    // Scene and effects - only if supported and not unknown
    if (features.effect) {
        if (state.sceneId >= 0) {
            params["sceneId"] = state.sceneId;
        }
        if (state.speed >= 0 && state.speed <= 100) {
            params["speed"] = state.speed;
        }
    }
    
    // Fan control - only if supported and not unknown
    if (features.fan && state.fanspd >= 0 && state.fanspd <= 100) {
        params["fanspd"] = state.fanspd;
    }

    // Serialize to string
    String controlMessage;
    serializeJson(doc, controlMessage);

    // Serial.printf("Setting bulb state for %s\n", deviceIP.toString().c_str());
    // Serial.printf("  Requested state: %s\n", wizBulbStateToJson(state).c_str());
    // Serial.printf("  Control message: %s\n", controlMessage.c_str());

    // Send control command with retry mechanism for buffer overflow protection
    const int MAX_UDP_RETRIES = 5;
    const int UDP_RETRY_DELAY = 50; // 50ms delay between retries
    bool packetSent = false;
    
    for (int attempt = 1; attempt <= MAX_UDP_RETRIES && !packetSent; attempt++) {
        // Enforce global rate limiting before sending
        enforceGlobalUdpDelay();
        
        // AsyncUDP sends packets asynchronously - returns size of sent packet or 0 if failed
        size_t sentBytes = udp.writeTo((const uint8_t*)controlMessage.c_str(), controlMessage.length(), deviceIP, WIZ_PORT);
        packetSent = (sentBytes > 0);
        
        if (!packetSent) {
            Serial.printf("  AsyncUDP send failed (attempt %d/%d) - retrying...\n", 
                         attempt, MAX_UDP_RETRIES);
            if (attempt < MAX_UDP_RETRIES) {
                delay(UDP_RETRY_DELAY);
            }
        }
    }
    
    if (packetSent) {
        //Serial.printf("  Control command sent to %s\n", deviceIP.toString().c_str());                        
        return true;
    } else {
        Serial.printf("  Failed to send control command to %s after %d attempts\n", 
                     deviceIP.toString().c_str(), MAX_UDP_RETRIES);
        return false;
    }
}

bool setBulbState(const WizBulbInfo& bulbInfo, const WizBulbState& state)
{
    IPAddress deviceIP;
    if (!deviceIP.fromString(bulbInfo.ip))
    {
        Serial.printf("Invalid IP address in bulb info: %s\n", bulbInfo.ip.c_str());
        return false;
    }
    
    // Use the bulb's known capabilities directly
    bool success = setBulbStateInternal(deviceIP, state, bulbInfo.features);
    
    // Track failures for health monitoring
    
    if (!success) {
        wizBulbFailureCount++;
        Serial.printf("WiZ bulb command failed. Failure count: %d/%d\n", wizBulbFailureCount, MAX_WIZ_FAILURES);
    } else {
        wizBulbFailureCount = 0; // Reset on success
    }
    
    return success;
}

WizBulbState getBulbState(const WizBulbInfo& bulbInfo)
{
    IPAddress deviceIP;
    if (!deviceIP.fromString(bulbInfo.ip))
    {
        WizBulbState invalidState;
        invalidState.errorMessage = "Invalid IP address: " + bulbInfo.ip;
        Serial.printf("Invalid IP address in bulb info: %s\n", bulbInfo.ip.c_str());
        return invalidState;
    }
    
    return getBulbState(deviceIP);
}

String wizBulbStateToJson(const WizBulbState& state)
{
    JsonDocument doc;
    
    doc["state"] = state.state;
    if (state.dimming >= 0) doc["dimming"] = state.dimming;
    if (state.r >= 0) doc["r"] = state.r;
    if (state.g >= 0) doc["g"] = state.g;
    if (state.b >= 0) doc["b"] = state.b;
    if (state.c >= 0) doc["c"] = state.c;
    if (state.w >= 0) doc["w"] = state.w;
    if (state.temp >= 0) doc["temp"] = state.temp;
    if (state.sceneId >= 0) doc["sceneId"] = state.sceneId;
    if (state.speed >= 0) doc["speed"] = state.speed;
    if (state.fanspd >= 0) doc["fanspd"] = state.fanspd;
    
    doc["isValid"] = state.isValid;
    if (!state.errorMessage.isEmpty()) doc["errorMessage"] = state.errorMessage;
    if (state.lastUpdated > 0) doc["lastUpdated"] = state.lastUpdated;
    
    String json;
    serializeJson(doc, json);
    return json;
}

String bulbClassToString(BulbClass bulbClass)
{
    switch (bulbClass) {
        case BulbClass::RGB: return "RGB";
        case BulbClass::TW: return "TW";
        case BulbClass::DW: return "DW";
        case BulbClass::SOCKET: return "SOCKET";
        case BulbClass::FAN: return "FAN";
        default: return "UNKNOWN";
    }
}

BulbClass bulbClassFromString(const String& str)
{
    if (str == "RGB") return BulbClass::RGB;
    if (str == "TW") return BulbClass::TW;
    if (str == "DW") return BulbClass::DW;
    if (str == "SOCKET") return BulbClass::SOCKET;
    if (str == "FAN") return BulbClass::FAN;
    return BulbClass::UNKNOWN;
}

String wizBulbInfoToJson(const WizBulbInfo& bulbInfo)
{
    JsonDocument doc;
    
    // Device identification
    doc["ip"] = bulbInfo.ip;
    doc["mac"] = bulbInfo.mac;
    doc["moduleName"] = bulbInfo.moduleName;
    doc["fwVersion"] = bulbInfo.fwVersion;
    
    // Network info
    doc["rssi"] = bulbInfo.rssi;
    doc["homeId"] = bulbInfo.homeId;
    doc["roomId"] = bulbInfo.roomId;
    doc["src"] = bulbInfo.src;
    
    // Capabilities
    doc["bulbClass"] = bulbClassToString(bulbInfo.bulbClass);
    
    JsonObject features = doc["features"].to<JsonObject>();
    features["brightness"] = bulbInfo.features.brightness;
    features["color"] = bulbInfo.features.color;
    features["color_tmp"] = bulbInfo.features.color_tmp;
    features["effect"] = bulbInfo.features.effect;
    features["fan"] = bulbInfo.features.fan;
    
    JsonObject kelvinRange = features["kelvin_range"].to<JsonObject>();
    kelvinRange["min"] = bulbInfo.features.kelvin_range.min;
    kelvinRange["max"] = bulbInfo.features.kelvin_range.max;
    
    // Additional info
    doc["isValid"] = bulbInfo.isValid;
    if (!bulbInfo.errorMessage.isEmpty()) doc["errorMessage"] = bulbInfo.errorMessage;
    
    String json;
    serializeJson(doc, json);
    return json;
}

WizBulbState wizBulbStateFromJson(const String& json)
{
    WizBulbState state;
    JsonDocument doc;
    
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        state.errorMessage = "JSON parse error: " + String(error.c_str());
        return state;
    }
    
    state.state = doc["state"] | false;
    state.dimming = doc["dimming"] | -1;
    state.r = doc["r"] | -1;
    state.g = doc["g"] | -1;
    state.b = doc["b"] | -1;
    state.c = doc["c"] | -1;
    state.w = doc["w"] | -1;
    state.temp = doc["temp"] | -1;
    state.sceneId = doc["sceneId"] | -1;
    state.speed = doc["speed"] | -1;
    state.fanspd = doc["fanspd"] | -1;
    
    state.isValid = doc["isValid"] | false;
    state.errorMessage = doc["errorMessage"] | "";
    state.lastUpdated = doc["lastUpdated"] | 0;
    
    return state;
}

WizBulbInfo wizBulbInfoFromJson(const String& json)
{
    WizBulbInfo bulbInfo;
    JsonDocument doc;
    
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        bulbInfo.errorMessage = "JSON parse error: " + String(error.c_str());
        return bulbInfo;
    }
    
    // Device identification
    bulbInfo.ip = doc["ip"] | "";
    bulbInfo.mac = doc["mac"] | "";
    bulbInfo.moduleName = doc["moduleName"] | "";
    bulbInfo.fwVersion = doc["fwVersion"] | "";
    
    // Network info
    bulbInfo.rssi = doc["rssi"] | 0;
    bulbInfo.homeId = doc["homeId"] | "";
    bulbInfo.roomId = doc["roomId"] | "";
    bulbInfo.src = doc["src"] | "";
    
    // Capabilities
    String bulbClassStr = doc["bulbClass"] | "UNKNOWN";
    bulbInfo.bulbClass = bulbClassFromString(bulbClassStr);
    
    if (doc["features"].is<JsonObject>()) {
        JsonObject features = doc["features"];
        bulbInfo.features.brightness = features["brightness"] | false;
        bulbInfo.features.color = features["color"] | false;
        bulbInfo.features.color_tmp = features["color_tmp"] | false;
        bulbInfo.features.effect = features["effect"] | false;
        bulbInfo.features.fan = features["fan"] | false;
        
        if (features["kelvin_range"].is<JsonObject>()) {
            JsonObject kelvinRange = features["kelvin_range"];
            bulbInfo.features.kelvin_range.min = kelvinRange["min"] | 2200;
            bulbInfo.features.kelvin_range.max = kelvinRange["max"] | 6500;
        }
    }
    
    // Additional info
    bulbInfo.isValid = doc["isValid"] | false;
    bulbInfo.errorMessage = doc["errorMessage"] | "";
    
    return bulbInfo;
}


std::vector<WizBulbInfo> updateBulbIPs(const std::vector<WizBulbInfo>& cachedBulbs, const std::vector<WizBulbInfo>& discoveredBulbs)
{
    std::vector<WizBulbInfo> updatedBulbs = cachedBulbs;
    bool anyUpdated = false;
    
    Serial.printf("Updating IP addresses for %d cached bulbs using %d discovered bulbs\n", cachedBulbs.size(), discoveredBulbs.size());
    
    for (size_t i = 0; i < updatedBulbs.size(); i++) {
        WizBulbInfo& cached = updatedBulbs[i];
        
        // Find matching bulb by MAC address
        for (const WizBulbInfo& discovered : discoveredBulbs) {
            if (discovered.mac == cached.mac && !discovered.mac.isEmpty()) {
                if (discovered.ip != cached.ip) {
                    Serial.printf("Updating IP for MAC %s: %s -> %s\n", 
                                 cached.mac.c_str(), cached.ip.c_str(), discovered.ip.c_str());
                    cached.ip = discovered.ip;
                    anyUpdated = true;
                } else {
                    Serial.printf("IP unchanged for MAC %s: %s\n", cached.mac.c_str(), cached.ip.c_str());
                }
                break;
            }
        }
    }
    
    if (anyUpdated) {
        Serial.println("IP addresses updated, saving to cache");
        if (saveLightsToFile(updatedBulbs)) {
            Serial.println("Successfully updated and saved lights cache");
        } else {
            Serial.println("Failed to save updated lights cache");
        }
    } else {
        Serial.println("No IP address changes detected");
    }
    
    return updatedBulbs;
}

std::vector<WizBulbInfo> discoverOrLoadLights(IPAddress broadcastIP, bool* fromCache)
{
    Serial.println("=== Smart Light Discovery ===");
    
    // Try to load from file first
    std::vector<WizBulbInfo> cachedBulbs = loadLightsFromFile();
    
    if (cachedBulbs.size() > 0) {
        Serial.printf("Found %d cached lights, checking for IP updates...\n", cachedBulbs.size());
        if (fromCache) *fromCache = true;
        
        // Perform discovery to check for IP changes
        std::vector<WizBulbInfo> discoveredBulbs = scanForWiz(broadcastIP);
        
        if (discoveredBulbs.size() > 0) {
            // Update cached bulbs with any new IP addresses
            std::vector<WizBulbInfo> updatedBulbs = updateBulbIPs(cachedBulbs, discoveredBulbs);                        
            return updatedBulbs;
        } else {
            Serial.println("No bulbs discovered during IP update check, using cached lights as-is");
            return cachedBulbs;
        }
    }
    
    // No cached lights, perform discovery
    Serial.println("No cached lights found, performing network discovery...");
    std::vector<WizBulbInfo> bulbs = scanForWiz(broadcastIP);
    if (fromCache) *fromCache = false;
    
    if (bulbs.size() > 0) {
        // Save discovered lights to file
        if (saveLightsToFile(bulbs)) {
            Serial.println("Successfully saved discovered lights to cache");
        } else {
            Serial.println("Failed to save lights to cache");
        }
    }
    
    return bulbs;
}


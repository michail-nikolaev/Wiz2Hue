#include "wiz2hue.h"
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <vector>

const int WIZ_PORT = 38899;
const int DISCOVERY_TIMEOUT = 10000;       // 10 seconds total discovery time
const int RESPONSE_TIMEOUT = 3000;         // 3 seconds per device config request
const int BROADCAST_ATTEMPTS = 3;          // Number of broadcast attempts
const int BROADCAST_DELAY = 500;           // Delay between broadcasts in ms
const int SOCKET_TIMEOUT = 1000;           // Socket receive timeout in ms
const int RETRY_BROADCAST_INTERVAL = 3000; // Retry broadcast every 3 seconds

void scanForWiz(IPAddress broadcastIP)
{
    WiFiUDP udp;

    // Use a different port for listening to avoid conflicts
    if (!udp.begin(38900))
    {
        Serial.println("Failed to start UDP for Wiz discovery");
        return;
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
        udp.endPacket();

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
                    udp.endPacket();

                    lastRetryBroadcast = millis();
                }
            }

            delay(10); // Smaller delay to catch more responses
        }
    }

    udp.stop();

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
            getSystemConfig(discoveredIPs[i]);

            // Add delay between config requests to prevent overwhelming devices
            if (i < discoveredIPs.size() - 1)
            {
                delay(500);
            }
        }

        Serial.println("\n=== All device information collected ===");
        Serial.println("You can now control these lights using their IP addresses.");
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
}

void getSystemConfig(IPAddress deviceIP)
{
    WiFiUDP udp;

    if (!udp.begin(0))
    { // Use random port
        Serial.println("Failed to start UDP for system config request");
        return;
    }

    // System config request
    String configMessage = "{\"method\":\"getSystemConfig\",\"params\":{}}";
    const int CONFIG_ATTEMPTS = 2;      // Number of attempts to get system config
    const int CONFIG_RETRY_DELAY = 500; // Delay between retries

    bool configReceived = false;

    for (int attempt = 1; attempt <= CONFIG_ATTEMPTS && !configReceived; attempt++)
    {
        if (attempt > 1)
        {
            Serial.printf("  Retrying system config request (attempt %d/%d)...\n", attempt, CONFIG_ATTEMPTS);
            delay(CONFIG_RETRY_DELAY);
        }

        // Send request to specific device
        udp.beginPacket(deviceIP, WIZ_PORT);
        udp.print(configMessage);
        udp.endPacket();

        unsigned long startTime = millis();

        // Wait for response with shorter timeout per attempt
        while (millis() - startTime < (RESPONSE_TIMEOUT / CONFIG_ATTEMPTS))
        {
            int packetSize = udp.parsePacket();

            if (packetSize)
            {
                // Limit response size to prevent crashes
                const int MAX_RESPONSE_SIZE = 800;
                char response[MAX_RESPONSE_SIZE];
                int len = udp.read(response, sizeof(response) - 1);
                if (len >= MAX_RESPONSE_SIZE - 1)
                {
                    Serial.println("  Warning: Response truncated due to size");
                }
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
                        const char *moduleName = result["moduleName"] | "Unknown";
                        const char *fwVersion = result["fwVersion"] | "Unknown";
                        const char *mac = result["mac"] | "Unknown";
                        int rssi = result["rssi"] | 0;
                        const char *src = result["src"] | "Unknown";
                        const char *homeId = result["homeId"] | "Unknown";
                        const char *roomId = result["roomId"] | "Unknown";

                        Serial.printf("  Model: %s\n", moduleName);
                        Serial.printf("  Firmware: %s\n", fwVersion);
                        Serial.printf("  MAC: %s\n", mac);
                        Serial.printf("  RSSI: %d dBm\n", rssi);
                        Serial.printf("  Source: %s\n", src);
                        Serial.printf("  Home ID: %s\n", homeId);
                        Serial.printf("  Room ID: %s\n", roomId);

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
                        configReceived = true; // Still count as received
                    }
                }
                else
                {
                    Serial.printf("  Failed to parse JSON response: %s\n", error.c_str());
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

                break;
            }

            delay(10);
        }
    }

    if (!configReceived)
    {
        Serial.println("  System Configuration: Timeout - no response received");
        Serial.printf("  Failed to get system config after %d attempts\n", CONFIG_ATTEMPTS);
    }

    udp.stop();
}
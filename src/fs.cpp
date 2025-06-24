#include "wiz2hue.h"
#include <ArduinoJson.h>
#include <vector>

bool initFileSystem()
{
    Serial.println("Initializing LittleFS...");
    
    // Use default LittleFS mount (looks for "spiffs" partition by default)
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed - attempting to format");
        
        // Try to format and mount again
        if (!LittleFS.format()) {
            Serial.println("LittleFS format failed");
            return false;
        }
        
        if (!LittleFS.begin()) {
            Serial.println("LittleFS mount failed after format");
            return false;
        }
    }
    
    Serial.println("LittleFS mounted successfully");
    
    // Print filesystem info for debugging
    Serial.printf("LittleFS total bytes: %zu\n", LittleFS.totalBytes());
    Serial.printf("LittleFS used bytes: %zu\n", LittleFS.usedBytes());
    
    return true;
}

std::vector<WizBulbInfo> loadLightsFromFile()
{
    std::vector<WizBulbInfo> bulbs;
    
    if (!LittleFS.exists("/lights.json")) {
        Serial.println("No lights.json file found");
        return bulbs;
    }
    
    File file = LittleFS.open("/lights.json", "r");
    if (!file) {
        Serial.println("Failed to open lights.json for reading");
        return bulbs;
    }
    
    String jsonContent = file.readString();
    file.close();
    
    Serial.printf("Loading lights from file: %s\n", jsonContent.c_str());
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonContent);
    
    if (error) {
        Serial.printf("Failed to parse lights.json: %s\n", error.c_str());
        return bulbs;
    }
    
    if (doc["lights"].is<JsonArray>()) {
        JsonArray lightsArray = doc["lights"];
        for (JsonVariant lightVariant : lightsArray) {
            String lightJson;
            serializeJson(lightVariant, lightJson);
            WizBulbInfo bulb = wizBulbInfoFromJson(lightJson);
            if (bulb.isValid) {
                bulbs.push_back(bulb);
            }
        }
    }
    
    Serial.printf("Loaded %d lights from file\n", bulbs.size());
    return bulbs;
}

bool saveLightsToFile(const std::vector<WizBulbInfo>& bulbs)
{
    JsonDocument doc;
    JsonArray lightsArray = doc["lights"].to<JsonArray>();
    
    for (const WizBulbInfo& bulb : bulbs) {
        if (bulb.isValid) {
            String bulbJson = wizBulbInfoToJson(bulb);
            JsonDocument bulbDoc;
            DeserializationError error = deserializeJson(bulbDoc, bulbJson);
            if (!error) {
                lightsArray.add(bulbDoc);
            }
        }
    }
    
    String jsonContent;
    serializeJson(doc, jsonContent);
    
    File file = LittleFS.open("/lights.json", "w");
    if (!file) {
        Serial.println("Failed to open lights.json for writing");
        return false;
    }
    
    size_t bytesWritten = file.print(jsonContent);
    file.close();
    
    Serial.printf("Saved %d lights to file (%d bytes)\n", bulbs.size(), bytesWritten);
    Serial.printf("Saved content: %s\n", jsonContent.c_str());
    
    return bytesWritten > 0;
}

void clearFileSystemCache()
{
    Serial.println("Clearing LittleFS cache...");
    if (LittleFS.exists("/lights.json")) {
        if (LittleFS.remove("/lights.json")) {
            Serial.println("Successfully removed lights.json");
        } else {
            Serial.println("Failed to remove lights.json");
        }
    }
    
    // Clear any other cached files if they exist
    if (LittleFS.exists("/config.json")) {
        LittleFS.remove("/config.json");
        Serial.println("Removed config.json");
    }
    
    // Clear ZigbeeWizLight settings files (light_*.json)
    File root = LittleFS.open("/");
    if (root) {
        File file = root.openNextFile();
        int removedLightFiles = 0;
        while (file) {
            String fileName = file.name();
            if (fileName.startsWith("light_") && fileName.endsWith(".json")) {
                file.close();
                if (LittleFS.remove("/" + fileName)) {
                    Serial.printf("Successfully removed %s\n", fileName.c_str());
                    removedLightFiles++;
                } else {
                    Serial.printf("Failed to remove %s\n", fileName.c_str());
                }
            } else {
                file.close();
            }
            file = root.openNextFile();
        }
        root.close();
        Serial.printf("Removed %d ZigbeeWizLight settings files\n", removedLightFiles);
    } else {
        Serial.println("Failed to open root directory for cleanup");
    }
    
    // Force filesystem to flush all pending operations to flash
    Serial.println("Syncing filesystem changes to flash...");
    LittleFS.end();
    delay(100); // Give time for flash write operations to complete
    
    // Reinitialize filesystem to ensure clean state
    if (!LittleFS.begin()) {
        Serial.println("Warning: Failed to remount LittleFS after cache clear");
    } else {
        Serial.println("Filesystem cache cleared and synced successfully");
    }
}
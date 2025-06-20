
#ifndef WIZ2HUE_H
#define WIZ2HUE_H

#include <Arduino.h>
#include <vector>
#include <LittleFS.h>

const int RED_PIN = D0;
const int BLUE_PIN = D1;
const int GREEN_PIN = D2;
const int YELLOW_PIN = D3;

// Bulb capability and feature structures
enum class BulbClass {
    UNKNOWN,
    RGB,        // Full color RGB
    RGBW,       // RGB + White
    TW,         // Tunable White (warm/cool)
    DW,         // Dimmable White only
    SOCKET,     // Smart socket (on/off only)
    FAN         // Fan with light
};

struct KelvinRange {
    int min = 2200;
    int max = 6500;
};

struct Features {
    bool brightness = false;
    bool color = false;
    bool color_tmp = false;
    bool effect = false;
    bool fan = false;
    KelvinRange kelvin_range;
};

struct WizBulbState {
    // Basic state
    bool state = false;                 // on/off
    int dimming = -1;                   // brightness 0-100 (-1 = unknown)
    
    // Color information
    int r = -1;                         // red 0-255 (-1 = unknown)
    int g = -1;                         // green 0-255 (-1 = unknown)
    int b = -1;                         // blue 0-255 (-1 = unknown)
    int c = -1;                         // cold white 0-255 (-1 = unknown)
    int w = -1;                         // warm white 0-255 (-1 = unknown)
    
    // Color temperature (Kelvin)
    int temp = -1;                      // color temperature (-1 = unknown)
    
    // Scene and effects
    int sceneId = -1;                   // scene ID (-1 = unknown, 0 = no scene)
    int speed = -1;                     // effect/transition speed 0-100 (-1 = unknown)
    
    // Fan control (for fan lights)
    int fanspd = -1;                    // fan speed 0-100 (-1 = unknown)
    
    // Additional state info
    bool isValid = false;               // whether state was successfully read
    String errorMessage;                // error description if read failed
    unsigned long lastUpdated = 0;     // timestamp of last state read
};

struct WizBulbInfo {
    // Device identification
    String ip;
    String mac;
    String moduleName;
    String fwVersion;
    
    // Network info
    int rssi = 0;
    String homeId;
    String roomId;
    String src;
    
    // Capabilities
    BulbClass bulbClass = BulbClass::UNKNOWN;
    Features features;
    
    // Additional info
    bool isValid = false;
    String errorMessage;
};

IPAddress wifi_connect(int pin_to_blink, int button);
IPAddress broadcastIP();

void setup_lights();
void hue_connect(int pin_to_blink, int button);
void hue_reset();

// System reset functions
void checkForReset(int button);
void resetSystem();

void ledDigital(int* left, int period, int pin, int sleep);
void ledAnalog(int* left, int period, int pin, int sleep);

std::vector<WizBulbInfo> scanForWiz(IPAddress broadcastIP);
WizBulbInfo getSystemConfig(IPAddress deviceIP);

// State management functions
WizBulbState getBulbState(IPAddress deviceIP);
bool setBulbState(IPAddress deviceIP, const WizBulbState& state);
bool setBulbState(const WizBulbInfo& bulbInfo, const WizBulbState& state);

// Convenience functions for WizBulbInfo state management
WizBulbState getBulbState(const WizBulbInfo& bulbInfo);

// JSON serialization/deserialization functions
String wizBulbStateToJson(const WizBulbState& state);
String wizBulbInfoToJson(const WizBulbInfo& bulbInfo);
WizBulbState wizBulbStateFromJson(const String& json);
WizBulbInfo wizBulbInfoFromJson(const String& json);

// File management functions
bool initFileSystem();
std::vector<WizBulbInfo> loadLightsFromFile();
bool saveLightsToFile(const std::vector<WizBulbInfo>& bulbs);
std::vector<WizBulbInfo> discoverOrLoadLights(IPAddress broadcastIP);
void clearFileSystemCache();

#endif
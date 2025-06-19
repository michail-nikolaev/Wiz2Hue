
#ifndef WIZ2HUE_H
#define WIZ2HUE_H

#include <Arduino.h>
#include <vector>

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

IPAddress wifi_connect(int pin_to_blink);
IPAddress broadcastIP();

void setup_lights();
void hue_connect(int pin_to_blink);
void zigbee_check_for_reset(int button);
void hue_reset();

void ledDigital(int* left, int period, int pin, int sleep);
void ledAnalog(int* left, int period, int pin, int sleep);

std::vector<WizBulbInfo> scanForWiz(IPAddress broadcastIP);
WizBulbInfo getSystemConfig(IPAddress deviceIP);

#endif
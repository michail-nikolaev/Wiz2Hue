#include <Arduino.h>
#include "wiz2hue.h"

#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include <Zigbee.h>
#include <ArduinoJson.h>
#include <vector>
#include <map>

// Forward declarations
class ZigbeeWizLight;
static void staticLightChangeCallback(bool state, uint8_t endpoint, uint8_t red, uint8_t green, uint8_t blue, uint8_t level, uint16_t temperature);
static void staticIdentifyCallback(uint16_t time);

// Global mapping
static std::map<uint8_t, ZigbeeWizLight*> endpointToLight;

// Class to manage Zigbee-WiZ light pair
class ZigbeeWizLight {
private:
  ZigbeeHueLight* zigbeeLight;
  WizBulbInfo wizBulb;
  uint8_t endpoint;
  
  // Current Hue light state
  bool currentState;
  int16_t currentRed;      // Using signed int to allow -1
  int16_t currentGreen; 
  int16_t currentBlue;
  uint8_t currentLevel;
  int16_t currentTemperature; // Using signed int to allow -1
  
  // Previous state for change detection
  uint8_t prevRed;
  uint8_t prevGreen;
  uint8_t prevBlue;
  uint16_t prevTemperature;
  
  
  // Rate limiting
  unsigned long lastCommandTime;
  unsigned long lastPeriodicUpdate;
  bool hasPendingUpdate;
  
  // Settings save rate limiting
  unsigned long lastSettingsSave;
  bool hasPendingSettingsSave;
  
  static const unsigned long COMMAND_INTERVAL = 100;   // 100ms between commands
  static const unsigned long PERIODIC_INTERVAL = 10000; // 10 seconds periodic update
  static const unsigned long SETTINGS_SAVE_INTERVAL = 10000; // 10 seconds between settings saves
  
  // Settings file management
  String getSettingsFilePath() const {
    String macForFilename = wizBulb.mac;
    macForFilename.replace(":", "");
    return "/light_" + macForFilename + ".json";
  }
  
  void saveSettings() {
    JsonDocument doc;
    doc["mac"] = wizBulb.mac;
    doc["endpoint"] = endpoint;
    doc["state"] = currentState;
    doc["red"] = currentRed;
    doc["green"] = currentGreen;
    doc["blue"] = currentBlue;
    doc["level"] = currentLevel;
    doc["temperature"] = currentTemperature;
    
    String jsonContent;
    serializeJson(doc, jsonContent);
    
    String filepath = getSettingsFilePath();
    File file = LittleFS.open(filepath, "w");
    if (file) {
      file.print(jsonContent);
      file.close();
      Serial.printf("Saved settings for light %s: %s\n", wizBulb.mac.c_str(), jsonContent.c_str());
    } else {
      Serial.printf("Failed to save settings for light %s\n", wizBulb.mac.c_str());
    }
  }
  
  bool loadSettings() {
    String filepath = getSettingsFilePath();
    if (!LittleFS.exists(filepath)) {
      return false;
    }
    
    File file = LittleFS.open(filepath, "r");
    if (!file) {
      return false;
    }
    
    String jsonContent = file.readString();
    file.close();
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonContent);
    if (error) {
      Serial.printf("Failed to parse settings for light %s: %s\n", wizBulb.mac.c_str(), error.c_str());
      return false;
    }
    
    // Load settings
    if (doc["state"].is<bool>()) currentState = doc["state"];
    if (doc["red"].is<int>()) currentRed = doc["red"];
    if (doc["green"].is<int>()) currentGreen = doc["green"];
    if (doc["blue"].is<int>()) currentBlue = doc["blue"];
    if (doc["level"].is<uint8_t>()) currentLevel = doc["level"];
    if (doc["temperature"].is<int>()) currentTemperature = doc["temperature"];
    
    // Set previous state for change detection
    prevRed = currentRed >= 0 ? currentRed : 0;
    prevGreen = currentGreen >= 0 ? currentGreen : 0;
    prevBlue = currentBlue >= 0 ? currentBlue : 0;
    prevTemperature = currentTemperature >= 0 ? currentTemperature : 0;
    
    Serial.printf("Loaded settings for light %s: %s\n", wizBulb.mac.c_str(), jsonContent.c_str());
    return true;
  }
  
  void restoreSettingsToWizBulb() {
    if (!currentState && currentRed < 0 && currentGreen < 0 && currentBlue < 0 && currentTemperature < 0) {
      Serial.printf("No valid settings to restore for light %s\n", wizBulb.mac.c_str());
      return;
    }
    
    Serial.printf("Restoring settings to WiZ bulb %s\n", wizBulb.mac.c_str());
    
    WizBulbState wizState;
    wizState.state = currentState;
    
    if (currentState && wizBulb.features.brightness) {
      wizState.dimming = map(currentLevel, 0, 255, 0, 100);
    }
    
    if (currentState && wizBulb.features.color && 
        currentRed >= 0 && currentGreen >= 0 && currentBlue >= 0) {
      wizState.r = currentRed;
      wizState.g = currentGreen;
      wizState.b = currentBlue;
    } else if (currentState && wizBulb.features.color_tmp && currentTemperature > 0) {
      int kelvin = 1000000 / currentTemperature;
      if (kelvin < wizBulb.features.kelvin_range.min) {
        kelvin = wizBulb.features.kelvin_range.min;
      } else if (kelvin > wizBulb.features.kelvin_range.max) {
        kelvin = wizBulb.features.kelvin_range.max;
      }
      wizState.temp = kelvin;
    }
    
    setBulbState(wizBulb, wizState);
  }
  
public:
  ZigbeeWizLight(uint8_t ep, const WizBulbInfo& bulb, es_zb_hue_light_type_t zigbeeType) 
    : wizBulb(bulb), endpoint(ep), currentState(false), currentRed(-1), currentGreen(-1), 
      currentBlue(-1), currentLevel(0), currentTemperature(-1), prevRed(0), prevGreen(0),
      prevBlue(0), prevTemperature(0), lastCommandTime(0), lastPeriodicUpdate(0), 
      hasPendingUpdate(false), lastSettingsSave(0), hasPendingSettingsSave(false) {
    
    // Try to load settings from JSON file first
    bool settingsLoaded = loadSettings();
    
    if (!settingsLoaded) {
      // No saved settings, read actual WiZ bulb state and use as initial state
      WizBulbState actualState = getBulbState(wizBulb);
      if (actualState.isValid) {
        Serial.printf("Reading initial state for bulb %s: %s\n", 
                      wizBulb.ip.c_str(), actualState.state ? "ON" : "OFF");
        
        currentState = actualState.state;
        
        if (actualState.dimming >= 0) {
          currentLevel = map(actualState.dimming, 0, 100, 0, 255);
        }
        
        if (actualState.r >= 0 && actualState.g >= 0 && actualState.b >= 0) {
          currentRed = actualState.r;
          currentGreen = actualState.g;
          currentBlue = actualState.b;
          prevRed = currentRed;
          prevGreen = currentGreen;
          prevBlue = currentBlue;
        }
        
        if (actualState.temp >= 0) {
          // Convert Kelvin to mireds
          currentTemperature = 1000000 / actualState.temp;
          prevTemperature = currentTemperature;
        }
        
        // Save the initial state for future use
        saveSettings();
      } else {
        Serial.printf("Failed to read initial state for bulb %s: %s\n", 
                      wizBulb.ip.c_str(), actualState.errorMessage.c_str());
      }
    } else {
      Serial.printf("Loaded saved settings for bulb %s\n", wizBulb.mac.c_str());
      // Restore the saved settings to the WiZ bulb
      restoreSettingsToWizBulb();
    }
    
    // Convert Kelvin range to mireds for ZigbeeHueLight constructor
    uint16_t min_mireds = 1000000 / bulb.features.kelvin_range.max;  // Higher Kelvin = lower mireds
    uint16_t max_mireds = 1000000 / bulb.features.kelvin_range.min;  // Lower Kelvin = higher mireds
    zigbeeLight = new ZigbeeHueLight(endpoint, zigbeeType, min_mireds, max_mireds);

    
    // Configure the light
    zigbeeLight->onLightChange(staticLightChangeCallback);
    zigbeeLight->onIdentify(staticIdentifyCallback);
    
    String modelName = getHueModelName(bulb);
    zigbeeLight->setManufacturerAndModel("nkey", modelName.c_str());
    zigbeeLight->setSwBuild("0.0.1");
    zigbeeLight->setOnOffOnTime(0);
    zigbeeLight->setOnOffGlobalSceneControl(false);
  }
  
  ~ZigbeeWizLight() {
    delete zigbeeLight;
  }
  
  ZigbeeHueLight* getZigbeeLight() {
    return zigbeeLight;
  }
  
  uint8_t getEndpoint() const {
    return endpoint;
  }
  
  const WizBulbInfo& getWizBulb() const {
    return wizBulb;
  }
  
  void onLightChangeCallback(bool state, uint8_t ep, uint8_t red, uint8_t green, uint8_t blue, uint8_t level, uint16_t temperature) {
    // Optional debug output - uncomment to enable detailed logging
    // Serial.printf("onLightChange EP:%d State:%s RGB:(%d,%d,%d) Level:%d Temp:%d mireds\n", 
    //               ep, state ? "ON" : "OFF", red, green, blue, level, temperature);
    
    // Detect what changed to send only relevant parameters
    bool rgbChanged = (red != prevRed || green != prevGreen || blue != prevBlue);
    bool tempChanged = (temperature != prevTemperature);
    
    // Update previous state for next comparison
    prevRed = red;
    prevGreen = green;
    prevBlue = blue;
    prevTemperature = temperature;
    
    // Update current state
    currentState = state;
    currentLevel = level;
    
    // Smart parameter selection: prioritize the parameter group that changed
    if (rgbChanged && wizBulb.features.color) {
      // RGB changed - use RGB mode, reset temperature
      currentRed = red;
      currentGreen = green;
      currentBlue = blue;
      currentTemperature = -1; // Reset temperature to avoid conflicts
      Serial.printf("  → RGB mode: RGB(%d,%d,%d), temp set to -1\n", red, green, blue);
    } else if (tempChanged && wizBulb.features.color_tmp) {
      // Temperature changed - use temperature mode, reset RGB
      currentTemperature = temperature;
      currentRed = -1;   // Reset RGB to avoid conflicts
      currentGreen = -1;
      currentBlue = -1;
      Serial.printf("  → Temperature mode: %d mireds, RGB set to -1\n", temperature);
    }
    
    // Mark settings as needing to be saved (rate limited)
    hasPendingSettingsSave = true;
    
    hasPendingUpdate = true;
  }
  
  void onIdentifyCallback(uint16_t time) {
    // Identify callback - could implement LED blinking here if needed
  }
  
  void processCommands() {
    unsigned long currentTime = millis();
    
    // Check for periodic update
    if (currentTime - lastPeriodicUpdate >= PERIODIC_INTERVAL) {
      hasPendingUpdate = true;
      lastPeriodicUpdate = currentTime;
    }
    
    // Send command if pending and rate limit allows
    if (hasPendingUpdate && (currentTime - lastCommandTime >= COMMAND_INTERVAL)) {
      sendToWizBulb();
      hasPendingUpdate = false;
      lastCommandTime = currentTime;
    }
    
    // Save settings if pending and rate limit allows
    if (hasPendingSettingsSave && (currentTime - lastSettingsSave >= SETTINGS_SAVE_INTERVAL)) {
      saveSettings();
      hasPendingSettingsSave = false;
      lastSettingsSave = currentTime;
    }
  }
  
private:
  void sendToWizBulb() {
    // Convert current Hue state to WiZ state
    WizBulbState wizState;
    wizState.state = currentState;
    
    Serial.printf("  sendToWizBulb: current RGB(%d,%d,%d), temp %d\n", 
                  currentRed, currentGreen, currentBlue, currentTemperature);
    
    if (currentState && wizBulb.features.brightness) {
      wizState.dimming = map(currentLevel, 0, 255, 0, 100);
    }
    
    // Smart parameter sending: check what mode we're in based on values
    if (currentState && wizBulb.features.color && 
        currentRed >= 0 && currentGreen >= 0 && currentBlue >= 0) {
      // RGB mode - send RGB values, exclude temperature
      wizState.r = currentRed;
      wizState.g = currentGreen;
      wizState.b = currentBlue;      
      //Serial.printf("  → Sending RGB mode: RGB(%d,%d,%d), temp excluded\n", (int)currentRed, (int)currentGreen, (int)currentBlue);
    } else if (currentState && wizBulb.features.color_tmp && currentTemperature > 0) {
      // Temperature mode - send temperature, exclude RGB
      int kelvin = 1000000 / currentTemperature;
      // Clamp to bulb's supported range
      if (kelvin < wizBulb.features.kelvin_range.min) {
        kelvin = wizBulb.features.kelvin_range.min;
      } else if (kelvin > wizBulb.features.kelvin_range.max) {
        kelvin = wizBulb.features.kelvin_range.max;
      }
      wizState.temp = kelvin;      
      Serial.printf("  → Sending temp mode: %dK, RGB excluded\n", kelvin);
    }
    
    setBulbState(wizBulb, wizState);
  }
  
  String getHueModelName(const WizBulbInfo& bulb) {
    switch (bulb.bulbClass) {
      case BulbClass::RGB:
        return "WizHue(LCA001)";  // Extended color light (RGB + tunable white)
      case BulbClass::TW:
        return "WizHue(LWO003)";  // Color temperature light
      case BulbClass::DW:
        return bulb.features.brightness ? "WizHue(LTA005)" : "WizHue(OnOff)";
      case BulbClass::SOCKET:
        return "WizHue(Socket)";
      case BulbClass::FAN:
        return "WizHue(Fan)";
      default:
        return "WizHue(Unknown)";
    }
  }
};

// Dynamic light management
static std::vector<ZigbeeWizLight*> zigbeeWizLights;

const uint8_t FIRST_ENDPOINT = 10;

void hue_connect(int pin_to_blink, int button, const std::vector<WizBulbInfo>& bulbs)
{
  uint8_t phillips_hue_key[] = {0x81, 0x45, 0x86, 0x86, 0x5D, 0xC6, 0xC8, 0xB1, 0xC8, 0xCB, 0xC4, 0x2E, 0x5D, 0x65, 0xD3, 0xB9};
  Zigbee.setEnableJoiningToDistributed(true);
  Zigbee.setStandardDistributedKey(phillips_hue_key);

  // When all EPs are registered, start Zigbee in End Device mode
  if (!Zigbee.begin())
  {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  }

  digitalWrite(GREEN_PIN, HIGH);
  Serial.println("Connecting Zigbee to network");
  
  // Test mode variables
  unsigned long lastTestTime = 0;
  const unsigned long TEST_INTERVAL = 500;
  
  while (!Zigbee.connected())
  {
    Serial.print(".");
    digitalWrite(pin_to_blink, HIGH);
    delay(100);
    digitalWrite(pin_to_blink, LOW);
    delay(100);
    checkForReset(button);
    
    if (millis() - lastTestTime >= TEST_INTERVAL) {
      Serial.println("\n=== Test Mode: Changing bulb states ===");
      
      for (size_t i = 0; i < bulbs.size(); i++) {
        WizBulbState newState;
        newState.state = true; // Always turn on
        
        if (bulbs[i].features.brightness) {
          // Random brightness 10-100%
          newState.dimming = random(10, 101);
          Serial.printf("Setting bulb %s brightness to %d%%", bulbs[i].ip.c_str(), newState.dimming);
        } else {
          // Toggle on/off for bulbs without brightness support
          static bool toggleState = true;
          newState.state = toggleState;
          toggleState = !toggleState;
          Serial.printf("Toggling bulb %s %s", bulbs[i].ip.c_str(), newState.state ? "ON" : "OFF");
        }
        
        // Set random color if supported
        if (bulbs[i].features.color) {
          newState.r = random(0, 256);
          newState.g = random(0, 256);
          newState.b = random(0, 256);
          Serial.printf(", RGB(%d,%d,%d)", newState.r, newState.g, newState.b);
        }
        
        // Set random color temperature if supported (and not setting color)
        if (bulbs[i].features.color_tmp && !bulbs[i].features.color) {
          newState.temp = random(bulbs[i].features.kelvin_range.min, bulbs[i].features.kelvin_range.max + 1);
          Serial.printf(", temp %dK", newState.temp);
        }
        
        Serial.println();
        
        setBulbState(bulbs[i], newState);
        delay(100); // Small delay between bulb commands
      }
      
      lastTestTime = millis();
      Serial.println("=== Test complete, resuming Zigbee connection ===");
    }
  }
  digitalWrite(pin_to_blink, HIGH);
}

// Sort bulbs by MAC address for consistent endpoint assignment
std::vector<WizBulbInfo> sortBulbsByMac(const std::vector<WizBulbInfo>& bulbs) {
  std::vector<WizBulbInfo> sorted = bulbs;
  std::sort(sorted.begin(), sorted.end(), [](const WizBulbInfo& a, const WizBulbInfo& b) {
    return a.mac < b.mac;
  });
  return sorted;
}

// Map WiZ bulb capabilities to Zigbee light type
es_zb_hue_light_type_t mapBulbToZigbeeType(const WizBulbInfo& bulb) {
  switch (bulb.bulbClass) {
    case BulbClass::RGB:
      // RGB bulbs support full color + tunable white - use extended color
      return ESP_ZB_HUE_LIGHT_TYPE_EXTENDED_COLOR;
    case BulbClass::TW:
      return ESP_ZB_HUE_LIGHT_TYPE_TEMPERATURE;
    case BulbClass::DW:
      return bulb.features.brightness ? ESP_ZB_HUE_LIGHT_TYPE_DIMMABLE : ESP_ZB_HUE_LIGHT_TYPE_ON_OFF;
    case BulbClass::SOCKET:
    case BulbClass::FAN:
    default:
      return ESP_ZB_HUE_LIGHT_TYPE_ON_OFF;
  }
}

// Static callback implementations
static void staticLightChangeCallback(bool state, uint8_t endpoint, uint8_t red, uint8_t green, uint8_t blue, uint8_t level, uint16_t temperature) {
  auto it = endpointToLight.find(endpoint);
  if (it != endpointToLight.end()) {
    it->second->onLightChangeCallback(state, endpoint, red, green, blue, level, temperature);
  } else {
    Serial.printf("ERROR: No ZigbeeWizLight found for endpoint %d\n", endpoint);
  }
}

static void staticIdentifyCallback(uint16_t time) {
  // Static identify callback - implementation could be added if needed
}

// Process all light commands
void processLightCommands()
{
  for (auto* light : zigbeeWizLights) {
    light->processCommands();
  }
}


void hue_reset()
{
  Zigbee.factoryReset();
}

void setup_lights(const std::vector<WizBulbInfo>& bulbs)
{
  Serial.printf("\n=== Setting up Zigbee lights for %d WiZ bulbs ===\n", bulbs.size());
  
  // Clear existing lights
  for (auto* light : zigbeeWizLights) {
    delete light;
  }
  zigbeeWizLights.clear();
  endpointToLight.clear();
  
  // Sort bulbs by MAC address for consistent endpoint assignment
  std::vector<WizBulbInfo> sortedBulbs = sortBulbsByMac(bulbs);
  
  // Create ZigbeeWizLight for each discovered WiZ bulb
  uint8_t endpoint = FIRST_ENDPOINT;
  for (const auto& bulb : sortedBulbs) {
    if (!bulb.isValid) {
      Serial.printf("Skipping invalid bulb: %s\n", bulb.ip.c_str());
      continue;
    }
    
    es_zb_hue_light_type_t zigbeeType = mapBulbToZigbeeType(bulb);
    
    Serial.printf("Creating ZigbeeWiz light - IP: %s, MAC: %s, Type: %d, Endpoint: %d\n", 
                  bulb.ip.c_str(), bulb.mac.c_str(), zigbeeType, endpoint);
    
    // Create new ZigbeeWizLight
    ZigbeeWizLight* zigbeeWizLight = new ZigbeeWizLight(endpoint, bulb, zigbeeType);
    
    // Add to Zigbee stack
    Zigbee.addEndpoint(zigbeeWizLight->getZigbeeLight());
    
    // Store references
    zigbeeWizLights.push_back(zigbeeWizLight);
    endpointToLight[endpoint] = zigbeeWizLight;
    
    Serial.printf("Successfully created ZigbeeWiz light (endpoint %d)\n", endpoint);
    
    endpoint++; // Next endpoint for next bulb
  }
  
  Serial.printf("=== Setup complete: %d ZigbeeWiz lights created ===\n\n", zigbeeWizLights.size());
}


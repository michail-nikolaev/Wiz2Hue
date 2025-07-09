#include <Arduino.h>
#include <vector>
#include "wiz2hue.h"

const int RED_PERIOD = 2000;
const int BLUE_PERIOD = 1000;
const int GREEN_PERIOD = 1000;
const int YELLOW_PERIOD = 500;
const int LED_BUILTIN_PERIOD = 1000;

const int SLEEP = 10;

int redPinLeft = RED_PERIOD;
int bluePinLeft = BLUE_PERIOD;
int greenPinLeft = GREEN_PERIOD;
int yellowPinLeft = YELLOW_PERIOD;
int ledBuiltinLeft = LED_BUILTIN_PERIOD;

uint8_t button = BOOT_PIN;

// Global storage for discovered bulbs
std::vector<WizBulbInfo> globalDiscoveredBulbs;

// Connection monitoring variables
unsigned long lastConnectionCheck = 0;
unsigned long lastWiFiCheck = 0;
unsigned long lastZigbeeCheck = 0;
const unsigned long CONNECTION_CHECK_INTERVAL = 30000;  // 30 seconds
const unsigned long WIFI_CHECK_INTERVAL = 30000;       // 30 seconds  
const unsigned long ZIGBEE_CHECK_INTERVAL = 60000;     // 60 seconds

void setup()
{
  Serial.begin(115200);
  // delay(5000);
  // hue_reset();
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);
  pinMode(button, INPUT_PULLUP);

  // Initialize only the pins we've configured as outputs
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(RED_PIN, LOW);
  digitalWrite(BLUE_PIN, LOW);
  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(YELLOW_PIN, LOW);

  wifi_connect(RED_PIN, button);

  // Initialize filesystem
  if (!initFileSystem()) {
    Serial.println("Failed to initialize filesystem - continuing without caching");
  }

  delay(1000);
  bool lightsFromCache = false;
  globalDiscoveredBulbs = discoverOrLoadLights(broadcastIP(), &lightsFromCache);
  
  if (lightsFromCache) {
    Serial.printf("Light discovery completed. Loaded %d Wiz bulbs from cache with full capability information.\n", globalDiscoveredBulbs.size());
  } else {
    Serial.printf("Light discovery completed. Discovered %d Wiz bulbs via network scan with full capability information.\n", globalDiscoveredBulbs.size());
  }
  
  // Read and print current state of each discovered bulb
  if (globalDiscoveredBulbs.size() > 0) {
    Serial.println("\n=== Reading current bulb states ===");
    for (size_t i = 0; i < globalDiscoveredBulbs.size(); i++) {
      Serial.printf("\nReading state of bulb %d/%d: %s\n", i + 1, globalDiscoveredBulbs.size(), globalDiscoveredBulbs[i].ip.c_str());
      
      WizBulbState currentState = getBulbState(globalDiscoveredBulbs[i]);
      
      if (currentState.isValid) {
        Serial.printf("Current state: %s\n", wizBulbStateToJson(currentState).c_str());
      } else {
        Serial.printf("Failed to read state: %s\n", currentState.errorMessage.c_str());
      }
      
      // Add small delay between state requests
      if (i < globalDiscoveredBulbs.size() - 1) {
        delay(300);
      }
    }
    Serial.println("\n=== All bulb states collected ===");
  }

  setup_lights(globalDiscoveredBulbs);
  hue_connect(YELLOW_PIN, button, globalDiscoveredBulbs);
  Serial.println();

  delay(500);
  
  // Check for reset button during setup
  checkForReset(button);
}

void checkForReset(int button)
{
  // Checking button for factory reset
  if (digitalRead(button) == LOW)
  { // Push button pressed
    // Key debounce handling
    vTaskDelay(pdMS_TO_TICKS(100));
    int startTime = millis();
    bool ledState = false;
    unsigned long lastBlink = millis();
    const int BLINK_INTERVAL = 100; // Fast blink every 100ms
    
    while (digitalRead(button) == LOW)
    {
      // Fast blink built-in LED while button is held
      if (millis() - lastBlink >= BLINK_INTERVAL) {
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
        lastBlink = millis();
      }
      
      vTaskDelay(pdMS_TO_TICKS(10));; // Short delay to prevent excessive CPU usage
      
      if ((millis() - startTime) > 3000)
      {
        // If key pressed for more than 3secs, perform unified system reset
        Serial.println("Button held for 3+ seconds - performing full system reset");
        digitalWrite(LED_BUILTIN, LOW); // Turn off LED before reset
        resetSystem();
      }
    }
    
    // Button released - turn off LED
    digitalWrite(LED_BUILTIN, LOW);
  }
}

void resetSystem()
{
    Serial.println("=== System Reset ===");
    
    // Clear LittleFS cache
    clearFileSystemCache();
    
    // Reset Zigbee network
    Serial.println("Resetting Zigbee network...");
    hue_reset();
    
    Serial.println("System reset complete - device will restart");
    delay(500); // Additional delay to ensure all operations complete
    ESP.restart();
}

void checkConnections() {
  unsigned long currentTime = millis();
  
  // Check WiFi connection
  if (currentTime - lastWiFiCheck >= WIFI_CHECK_INTERVAL) {
    if (!checkWiFiConnection()) {
      Serial.println("WiFi monitoring detected connection loss - restarting system");
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP.restart();
    }
    lastWiFiCheck = currentTime;
  }
  
  // Check Zigbee connection
  if (currentTime - lastZigbeeCheck >= ZIGBEE_CHECK_INTERVAL) {
    if (!checkZigbeeConnection()) {
      Serial.println("Zigbee monitoring detected connection loss - restarting system");
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP.restart();
    }
    lastZigbeeCheck = currentTime;
  }
  
  // Check WiZ bulb health
  if (currentTime - lastConnectionCheck >= CONNECTION_CHECK_INTERVAL) {
    if (!checkWizBulbHealth()) {
      Serial.println("WiZ bulb health critical - restarting system");
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP.restart();
    }
    lastConnectionCheck = currentTime;
  }
}

void loop()
{
  ledDigital(&ledBuiltinLeft, LED_BUILTIN_PERIOD, LED_BUILTIN, SLEEP);
  vTaskDelay(pdMS_TO_TICKS(SLEEP));

  // Monitor connections and restart if needed
  checkConnections();

  checkForReset(button);
}

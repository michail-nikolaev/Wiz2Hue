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

void setup()
{
  Serial.begin(115200);
  delay(5000);
  // hue_reset();
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);
  pinMode(button, INPUT_PULLUP);

  for (int p = 0; p < D9; p++)
  {
    digitalWrite(p, LOW);
  }

  wifi_connect(RED_PIN);

  delay(1000);
  std::vector<WizBulbInfo> discoveredBulbs = scanForWiz(broadcastIP());
  
  Serial.printf("Scan completed. Found %d Wiz bulbs with full capability information.\n", discoveredBulbs.size());
  
  // Read and print current state of each discovered bulb
  if (discoveredBulbs.size() > 0) {
    Serial.println("\n=== Reading current bulb states ===");
    for (size_t i = 0; i < discoveredBulbs.size(); i++) {
      Serial.printf("\nReading state of bulb %d/%d: %s\n", i + 1, discoveredBulbs.size(), discoveredBulbs[i].ip.c_str());
      
      WizBulbState currentState = getBulbState(discoveredBulbs[i]);
      
      if (currentState.isValid) {
        Serial.printf("Current state: %s\n", wizBulbStateToJson(currentState).c_str());
      } else {
        Serial.printf("Failed to read state: %s\n", currentState.errorMessage.c_str());
      }
      
      // Add small delay between state requests
      if (i < discoveredBulbs.size() - 1) {
        delay(300);
      }
    }
    Serial.println("\n=== All bulb states collected ===");
  }

  setup_lights();
  hue_connect(YELLOW_PIN);
  Serial.println();

  delay(500);
}

void loop()
{
  ledDigital(&ledBuiltinLeft, LED_BUILTIN_PERIOD, LED_BUILTIN, SLEEP);
  delay(SLEEP);

  zigbee_check_for_reset(button);
}

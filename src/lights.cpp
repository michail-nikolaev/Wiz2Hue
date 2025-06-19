#include <Arduino.h>
#include "wiz2hue.h"

#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include <Zigbee.h>

static ZigbeeHueLight *zbTemperatureLight = nullptr;
static ZigbeeHueLight *zbColorLight = nullptr;
static ZigbeeHueLight *zbDimmableLight = nullptr;
static ZigbeeHueLight *zbColorOnOffLight = nullptr;
static ZigbeeHueLight *zbColorTemperatureLight = nullptr;

void hue_connect(int pin_to_blink)
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

  /*esp_zb_ieee_addr_t custom_mac = {0x34, 0x12, 0xED, 0xF2, 0xEF, 0xBE, 0xAD, 0xD1};
  esp_err_t err = esp_zb_set_long_address(custom_mac);
  if (err == ESP_OK) {
      Serial.print("Successfully set custom long address.");
  } else {
      Serial.print("Failed to set custom long address/");
  }*/

  digitalWrite(GREEN_PIN, HIGH);
  Serial.println("Connecting Zigbee to network");
  while (!Zigbee.connected())
  {
    Serial.print(".");
    digitalWrite(pin_to_blink, HIGH);
    delay(100);
    digitalWrite(pin_to_blink, LOW);
    delay(100);
  }
  digitalWrite(pin_to_blink, HIGH);
}

void setLight(bool state, uint8_t red, uint8_t green, uint8_t blue, uint8_t level, uint16_t temperature)
{
  if (!state)
  {
    analogWrite(BLUE_PIN, 0);
    analogWrite(RED_PIN, 0);
    analogWrite(GREEN_PIN, 0);
    digitalWrite(YELLOW_PIN, 0);
  }
  else
  {
    Serial.print("\nlevel ");
    Serial.print(level);
    Serial.print("\nblue ");
    Serial.print(blue);
    Serial.print("\nred ");
    Serial.print(red);
    Serial.print("\ngreen ");
    Serial.print(green);
    Serial.print("\ntemperature ");
    Serial.print(temperature);
    Serial.print("\n");

    analogWrite(BLUE_PIN, blue);
    analogWrite(RED_PIN, red);
    analogWrite(GREEN_PIN, green);

    digitalWrite(YELLOW_PIN, 1);
  }
}

// Create a task on identify call to handle the identify function
void identify(uint16_t time)
{
  log_d("Identify called for %d seconds", time);
}

void zigbee_check_for_reset(int button)
{
  // Checking button for factory reset
  if (digitalRead(button) == LOW)
  { // Push button pressed
    // Key debounce handling
    delay(100);
    int startTime = millis();
    while (digitalRead(button) == LOW)
    {
      delay(50);
      if ((millis() - startTime) > 3000)
      {
        // If key pressed for more than 3secs, perform unified system reset
        Serial.println("Button held for 3+ seconds - performing full system reset");
        resetSystem();
      }
    }
  }
}

void hue_reset()
{
  Zigbee.factoryReset();
}

void setup_lights()
{

  zbTemperatureLight = new ZigbeeHueLight(0x0B, ESP_ZB_HUE_LIGHT_TYPE_TEMPERATURE);
  zbColorLight = new ZigbeeHueLight(0x0C, ESP_ZB_HUE_LIGHT_TYPE_COLOR);
  zbDimmableLight = new ZigbeeHueLight(0x0D, ESP_ZB_HUE_LIGHT_TYPE_DIMMABLE);
  zbColorTemperatureLight = new ZigbeeHueLight(0x0E, ESP_ZB_HUE_LIGHT_TYPE_EXTENDED_COLOR);
  zbColorOnOffLight = new ZigbeeHueLight(0x0F, ESP_ZB_HUE_LIGHT_TYPE_ON_OFF);

  zbColorLight->onLightChange(setLight);
  zbColorLight->onIdentify(identify);
  zbColorLight->setManufacturerAndModel("nkey", "WizHue(LCT015)");
  zbColorLight->setSwBuild("0.0.1");
  zbColorLight->setOnOffOnTime(0);
  zbColorLight->setOnOffGlobalSceneControl(false);
  Zigbee.addEndpoint(zbColorLight);

  zbDimmableLight->onLightChange(setLight);
  zbDimmableLight->onIdentify(identify);
  zbDimmableLight->setManufacturerAndModel("nkey", "WizHue(LTA005)");
  zbDimmableLight->setSwBuild("0.0.1");
  zbDimmableLight->setOnOffOnTime(0);
  zbDimmableLight->setOnOffGlobalSceneControl(false);
  Zigbee.addEndpoint(zbDimmableLight);

  zbTemperatureLight->onLightChange(setLight);
  zbTemperatureLight->onIdentify(identify);
  zbTemperatureLight->setManufacturerAndModel("nkey", "WizHue(LWO003)");
  zbTemperatureLight->setSwBuild("0.0.1");
  zbTemperatureLight->setOnOffOnTime(0);
  zbTemperatureLight->setOnOffGlobalSceneControl(false);
  Zigbee.addEndpoint(zbTemperatureLight);

  zbColorTemperatureLight->onLightChange(setLight);
  zbColorTemperatureLight->onIdentify(identify);
  zbColorTemperatureLight->setManufacturerAndModel("nkey", "WizHue(LCA001)");
  zbColorTemperatureLight->setSwBuild("0.0.1");
  zbColorTemperatureLight->setOnOffOnTime(0);
  zbColorTemperatureLight->setOnOffGlobalSceneControl(false);
  Zigbee.addEndpoint(zbColorTemperatureLight);

  zbColorOnOffLight->onLightChange(setLight);
  zbColorOnOffLight->onIdentify(identify);
  zbColorOnOffLight->setManufacturerAndModel("nkey", "WizHue(OnOff)");
  zbColorOnOffLight->setSwBuild("0.0.1");
  zbColorOnOffLight->setOnOffOnTime(0);
  zbColorOnOffLight->setOnOffGlobalSceneControl(false);
  Zigbee.addEndpoint(zbColorOnOffLight);
}

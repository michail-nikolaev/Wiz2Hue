
#ifndef WIZ2HUE_H
#define WIZ2HUE_H

#include <Arduino.h>

const int RED_PIN = D0;
const int BLUE_PIN = D1;
const int GREEN_PIN = D2;
const int YELLOW_PIN = D3;

IPAddress wifi_connect(int pin_to_blink);
IPAddress broadcastIP();

void setup_lights();
void hue_connect(int pin_to_blink);
void zigbee_check_for_reset(int button);
void hue_reset();

void ledDigital(int* left, int period, int pin, int sleep);
void ledAnalog(int* left, int period, int pin, int sleep);

void scanForWiz(IPAddress broadcastIP);
void getSystemConfig(IPAddress deviceIP);

#endif
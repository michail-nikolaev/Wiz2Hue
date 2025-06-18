#include <WiFi.h>
#include "secrets.h"
#include "wiz2hue.h"

const char *ssid = SSID;
const char *password = PASSWORD;

IPAddress broadcastIP()
{
  return WiFi.calculateBroadcast(WiFi.localIP(), WiFi.subnetMask());
}

IPAddress wifi_connect(int pin_to_blink)
{
  WiFi.disconnect(true);
  Serial.printf("\n******************************************************Connecting to %s\n", ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(pin_to_blink, HIGH);
    delay(100);
    digitalWrite(pin_to_blink, LOW);
    delay(100);
    Serial.print(".");
  }

  Serial.printf("\nWiFi connected\nIP address: %s, Broadcast: %s\n", WiFi.localIP().toString().c_str(), broadcastIP().toString().c_str());
  digitalWrite(pin_to_blink, HIGH);

  return WiFi.localIP();
}

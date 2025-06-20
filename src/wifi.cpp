#include <WiFi.h>
#include "secrets.h"
#include "wiz2hue.h"

const char *ssid = SSID;
const char *password = PASSWORD;

IPAddress broadcastIP()
{
  return WiFi.calculateBroadcast(WiFi.localIP(), WiFi.subnetMask());
}

IPAddress wifi_connect(int pin_to_blink, int button)
{
  // Comprehensive WiFi reset to handle post-upload state issues
  Serial.println("Initializing WiFi...");
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_STA);
  delay(100);
  WiFi.disconnect(true);
  delay(100);
  
  Serial.printf("\n******************************************************Connecting to %s\n", ssid);

  WiFi.begin(ssid, password);

  // Add timeout to prevent infinite loop
  unsigned long startTime = millis();
  const unsigned long WIFI_TIMEOUT = 30000; // 30 seconds timeout
  
  while (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(pin_to_blink, HIGH);
    delay(100);
    digitalWrite(pin_to_blink, LOW);
    delay(100);
    Serial.print(".");
    checkForReset(button);
    
    // Check for timeout and retry connection
    if (millis() - startTime > WIFI_TIMEOUT) {
      Serial.println("\nWiFi connection timeout, retrying...");
      WiFi.disconnect(true);
      delay(1000);
      WiFi.begin(ssid, password);
      startTime = millis(); // Reset timeout counter
    }
  }

  Serial.printf("\nWiFi connected\nIP address: %s, Broadcast: %s\n", WiFi.localIP().toString().c_str(), broadcastIP().toString().c_str());
  digitalWrite(pin_to_blink, HIGH);

  return WiFi.localIP();
}
 
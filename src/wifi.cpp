#include <WiFi.h>
#include <esp_wifi.h>
#include "secrets.h"
#include "wiz2hue.h"

const char *ssid = SSID;
const char *password = PASSWORD;
const int CHANNEL = 11; // TODO

IPAddress broadcastIP()
{
  return WiFi.calculateBroadcast(WiFi.localIP(), WiFi.subnetMask());
}

IPAddress wifi_connect(int pin_to_blink, int button)
{
  // Aggressive WiFi reset with debug information
  Serial.println("Initializing WiFi...");
  Serial.printf("SSID: %s\n", ssid);
  Serial.printf("Initial WiFi Status: %d\n", WiFi.status());
  
  // Force complete WiFi shutdown
  WiFi.mode(WIFI_OFF);
  delay(1000);
  Serial.println("WiFi turned OFF");
  
  // Restart WiFi in station mode
  WiFi.mode(WIFI_STA);
  delay(1000);
  Serial.println("WiFi set to STA mode");
  
  // Disconnect any existing connections
  WiFi.disconnect(true);
  delay(1000);
  Serial.println("WiFi disconnected");
  
  Serial.printf("\n******************************************************Connecting to %s\n", ssid);

  // Try connection with explicit parameters
  WiFi.begin(ssid, password, CHANNEL);
  Serial.printf("WiFi.begin() called, status: %d\n", WiFi.status());

  // Add timeout to prevent infinite loop
  unsigned long startTime = millis();
  const unsigned long WIFI_TIMEOUT = 5000; // 15 seconds timeout
  int retryCount = 0;
  
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
      retryCount++;
      Serial.printf("\nWiFi connection timeout (attempt %d), status: %d\n", retryCount, WiFi.status());
      
      if (retryCount >= 1) {
        Serial.println("Multiple WiFi failures - performing complete reset");
        ESP.restart();
      }
      
      Serial.println("Retrying WiFi connection...");
      WiFi.disconnect(true);
      delay(2000);
      WiFi.begin(ssid, password);
      startTime = millis(); // Reset timeout counter
    }
  }

  Serial.printf("\nWiFi connected\nIP address: %s, Broadcast: %s\n", WiFi.localIP().toString().c_str(), broadcastIP().toString().c_str());
  
  // Configure WiFi power save for ESP32-C6 UDP reliability
  esp_wifi_set_ps(WIFI_PS_NONE);
  
  digitalWrite(pin_to_blink, HIGH);

  return WiFi.localIP();
}

bool checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost - attempting reconnection");
    
    // Try quick reconnection first
    WiFi.disconnect(true);
    delay(1000);
    WiFi.begin(ssid, password, CHANNEL);
    
    // Wait up to 10 seconds for reconnection
    unsigned long startTime = millis();
    const unsigned long RECONNECT_TIMEOUT = 10000;
    
    while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < RECONNECT_TIMEOUT) {
      delay(500);
      Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("\nWiFi reconnected - IP: %s\n", WiFi.localIP().toString().c_str());
      // Re-disable power save after reconnection
      esp_wifi_set_ps(WIFI_PS_NONE);
      return true;
    } else {
      Serial.println("\nWiFi reconnection failed - system restart required");
      return false;
    }
  }
  return true;
}
 
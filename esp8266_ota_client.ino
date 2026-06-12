/*
  SkyPatch OTA - ESP8266 Client (Public Version)
  Professional Over-the-Air Update System
*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

// ==================== CONFIGURATION ====================
// REPLACE WITH YOUR CREDENTIALS
const char* SSID = "YOUR_WIFI_SSID";
const char* PASSWORD = "YOUR_WIFI_PASSWORD";
const char* SERVER_IP = "YOUR_SERVER_IP"; // e.g. "192.168.1.15"
const int SERVER_PORT = 8000;

const char* DEVICE_ID = "ESP8266_01";
const char* DEVICE_NAME = "SkyPatch_Node_01";
const char* FIRMWARE_VERSION = "1.0.0";

// EEPROM Addresses
#define EEPROM_SIZE 512
#define EEPROM_VERSION_ADDR 0
#define EEPROM_API_KEY_ADDR 50

const unsigned long UPDATE_CHECK_INTERVAL = 300000; 
unsigned long lastUpdateCheck = 0;

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n\nSkyPatch OTA System Starting...");
  
  EEPROM.begin(EEPROM_SIZE);
  connectToWiFi();
  
  String savedApiKey = readApiKeyFromEEPROM();
  if (savedApiKey.length() == 0) {
    registerDevice();
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectToWiFi();
  
  if (millis() - lastUpdateCheck > UPDATE_CHECK_INTERVAL) {
    lastUpdateCheck = millis();
    checkForUpdates();
  }
  delay(1000);
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
}

void registerDevice() {
  if (WiFi.status() != WL_CONNECTED) return;
  String url = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + 
               "/api/register-device?device_id=" + String(DEVICE_ID) + 
               "&device_name=" + String(DEVICE_NAME);
  
  WiFiClient client;
  HTTPClient http;
  http.begin(client, url);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String response = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);
    saveApiKeyToEEPROM(doc["api_key"].as<String>());
    Serial.println("Device Registered Successfully!");
  }
  http.end();
}

void checkForUpdates() {
  if (WiFi.status() != WL_CONNECTED) return;
  String apiKey = readApiKeyFromEEPROM();
  if (apiKey.length() == 0) return;
  
  String currentVersion = readVersionFromEEPROM();
  String url = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + 
               "/api/check-update?device_id=" + String(DEVICE_ID) + 
               "&current_version=" + currentVersion;
  
  WiFiClient client;
  HTTPClient http;
  http.begin(client, url);
  http.addHeader("device_key", apiKey);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String response = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);
    if (doc["update_available"].as<bool>()) {
      performUpdate(doc["latest_version"].as<String>(), apiKey);
    }
  }
  http.end();
}

void performUpdate(String newVersion, String apiKey) {
  String updateUrl = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + 
                     "/api/download-firmware?version=" + newVersion + 
                     "&device_id=" + String(DEVICE_ID);
  
  WiFiClient client;
  t_httpUpdate_return ret = ESPhttpUpdate.update(client, updateUrl, apiKey);
  
  if (ret == HTTP_UPDATE_OK) {
    saveVersionToEEPROM(newVersion);
    ESP.restart();
  }
}

String readVersionFromEEPROM() {
  char buffer[20] = {0};
  for (int i = 0; i < 20; i++) buffer[i] = EEPROM.read(EEPROM_VERSION_ADDR + i);
  String v = String(buffer);
  return (v.length() == 0) ? FIRMWARE_VERSION : v;
}

void saveVersionToEEPROM(String v) {
  for (int i = 0; i < v.length(); i++) EEPROM.write(EEPROM_VERSION_ADDR + i, v[i]);
  EEPROM.write(EEPROM_VERSION_ADDR + v.length(), '\0');
  EEPROM.commit();
}

String readApiKeyFromEEPROM() {
  char buffer[100] = {0};
  for (int i = 0; i < 100; i++) buffer[i] = EEPROM.read(EEPROM_API_KEY_ADDR + i);
  return String(buffer);
}

void saveApiKeyToEEPROM(String k) {
  for (int i = 0; i < k.length() && i < 100; i++) EEPROM.write(EEPROM_API_KEY_ADDR + i, k[i]);
  EEPROM.write(EEPROM_API_KEY_ADDR + k.length(), '\0');
  EEPROM.commit();
}

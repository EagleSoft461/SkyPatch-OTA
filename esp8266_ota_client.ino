/*
  SkyPatch OTA - ESP8266 Client v2.1
  Güvenlik Özellikli Over-the-Air Güncelleme Sistemi (Hata Düzeltmeleri Uygulandı)
*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

// ==================== KONFIGÜRASYON ====================
const char* SSID = "YOUR_SSID";
const char* PASSWORD = "YOUR_PASSWORD";
const char* SERVER_IP = "192.168.1.100";
const int SERVER_PORT = 8000;
const char* DEVICE_ID = "ESP8266_01";
const char* DEVICE_NAME = "SalonSensor";
const char* FIRMWARE_VERSION = "1.0.0";

// EEPROM Adresleri
#define EEPROM_SIZE 512
#define EEPROM_VERSION_ADDR 0
#define EEPROM_API_KEY_ADDR 50

// Güncelleme Kontrol Aralığı (ms)
const unsigned long UPDATE_CHECK_INTERVAL = 300000; // 5 dakika
unsigned long lastUpdateCheck = 0;

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║  SkyPatch OTA v2.1 Sistemi             ║");
  Serial.println("║  Hata Düzeltmeleri Uygulandı           ║");
  Serial.println("╚════════════════════════════════════════╝");
  
  EEPROM.begin(EEPROM_SIZE);
  
  String savedVersion = readVersionFromEEPROM();
  Serial.println("📦 EEPROM'dan okunan versiyon: " + savedVersion);
  
  String savedApiKey = readApiKeyFromEEPROM();
  if (savedApiKey.length() > 0) {
    Serial.println("🔐 Kaydedilmiş API Anahtarı bulundu");
  }
  
  connectToWiFi();
  
  if (savedApiKey.length() == 0) {
    registerDevice();
  }
  
  Serial.println("\n✓ Kurulum tamamlandı!");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }
  
  if (millis() - lastUpdateCheck > UPDATE_CHECK_INTERVAL) {
    lastUpdateCheck = millis();
    checkForUpdates();
  }
  
  delay(1000);
}

void connectToWiFi() {
  Serial.println("\n📡 WiFi'ye bağlanılıyor: " + String(SSID));
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi'ye bağlandı! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n✗ WiFi bağlantısı başarısız!");
  }
}

void registerDevice() {
  Serial.println("\n📝 Cihaz backend'e kaydediliyor...");
  
  if (WiFi.status() != WL_CONNECTED) return;
  
  String url = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + 
               "/api/register-device?device_id=" + String(DEVICE_ID) + 
               "&device_name=" + String(DEVICE_NAME);
  
  WiFiClient client;
  HTTPClient http;
  
  // Eski sürümler için setConnectTimeout yerine sadece setTimeout kullanıyoruz
  http.setTimeout(10000); 
  
  Serial.println("POST: " + url);
  
  if (http.begin(client, url)) {
    // Admin key gerekiyorsa buraya eklenmeli, ancak v2'de default admin key kullandık
    http.addHeader("admin_key", "skypatch_admin_key_2024_secure");
    int httpCode = http.POST("");
    
    if (httpCode == 200) {
      String response = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, response);
      
      if (doc.containsKey("api_key")) {
        String apiKey = doc["api_key"].as<String>();
        saveApiKeyToEEPROM(apiKey);
        Serial.println("✓ API Anahtarı kaydedildi!");
      }
    } else {
      Serial.println("✗ Kayıt başarısız. Kod: " + String(httpCode));
      Serial.println("Yanıt: " + http.getString());
    }
    http.end();
  }
}

void checkForUpdates() {
  Serial.println("\n🔍 Güncelleme kontrol ediliyor...");
  
  if (WiFi.status() != WL_CONNECTED) return;
  
  String apiKey = readApiKeyFromEEPROM();
  if (apiKey.length() == 0) return;
  
  String currentVersion = readVersionFromEEPROM();
  String url = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + 
               "/api/check-update?device_id=" + String(DEVICE_ID) + 
               "&current_version=" + currentVersion;
  
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(10000);
  
  if (http.begin(client, url)) {
    http.addHeader("device_key", apiKey);
    int httpCode = http.GET();
    
    if (httpCode == 200) {
      String response = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, response);
      
      if (doc["update_available"].as<bool>()) {
        Serial.println("⚠ YENİ GÜNCELLEME BULUNDU: v" + doc["latest_version"].as<String>());
        performUpdate(doc["latest_version"].as<String>(), apiKey);
      } else {
        Serial.println("✓ Sistem güncel.");
      }
    }
    http.end();
  }
}

void performUpdate(String newVersion, String apiKey) {
  Serial.println("\n📥 Firmware indiriliyor...");
  
  String updateUrl = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + 
                     "/api/download-firmware?version=" + newVersion + 
                     "&device_id=" + String(DEVICE_ID);
  
  WiFiClient client;
  String currentVersion = readVersionFromEEPROM();
  
  // ESP8266HTTPUpdate ayarları
  ESPhttpUpdate.rebootOnUpdate(false); // Manuel reboot için
  
  t_httpUpdate_return ret = ESPhttpUpdate.update(client, updateUrl, apiKey);
  
  switch(ret) {
    case HTTP_UPDATE_FAILED:
      Serial.println("✗ HATA: " + ESPhttpUpdate.getLastErrorString());
      reportUpdateStatus(currentVersion, newVersion, "failed", ESPhttpUpdate.getLastError());
      break;
      
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("✓ Güncelleme yok");
      break;
      
    case HTTP_UPDATE_OK:
      Serial.println("✓ BAŞARILI!");
      saveVersionToEEPROM(newVersion);
      reportUpdateStatus(currentVersion, newVersion, "success", 0); // int parametresi 0 yapıldı
      Serial.println("🔄 Yeniden başlatılıyor...");
      delay(2000);
      ESP.restart();
      break;
  }
}

void reportUpdateStatus(String fromVersion, String toVersion, String status, int errorCode) {
  if (WiFi.status() != WL_CONNECTED) return;
  
  String apiKey = readApiKeyFromEEPROM();
  String url = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + 
               "/api/report-update-status?device_id=" + String(DEVICE_ID) + 
               "&from_version=" + fromVersion + 
               "&to_version=" + toVersion + 
               "&status=" + status + 
               "&error_message=" + String(errorCode);
  
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(10000);
  
  if (http.begin(client, url)) {
    http.addHeader("device_key", apiKey);
    http.POST("");
    http.end();
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

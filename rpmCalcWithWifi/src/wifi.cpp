// wifi.cpp

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "../include/wifi_utils.h"
#include "../include/servo.h"
#include "../include/nvs_utils.h"
#include "../include/rpm.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"


const char* ssid = "ESP8266_AP";
const char* password = "12345678";

WebServer server(80);
IPAddress espAPIP;

bool testCheck = false;  
bool syncStatus = false; 

void handleRanges() {
  if (server.method() == HTTP_POST) {
    String jsonData = server.arg("plain");
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, jsonData);
    if (error) {
      server.send(400, "text/plain", "Invalid JSON");
      return;
    }
    if (doc.containsKey("ranges")) {
      JsonArray boundaries = doc["ranges"].as<JsonArray>();
      int count = boundaries.size();

      if (count < 2) {
        server.send(400, "text/plain", "Not enough boundaries provided");
        return;
      }

      int newNumRanges = count - 1;
      if (newNumRanges > MAX_RANGES) {
        server.send(400, "text/plain", "Too many ranges");
        return;
      }

      for (int i = 0; i < newNumRanges; i++) {
        modeRanges[i][0] = boundaries[i].as<int>();
        modeRanges[i][1] = boundaries[i + 1].as<int>();
      }
      generateServoPositions(newNumRanges);
      numRanges = newNumRanges;
      storeRanges();
      server.send(200, "text/plain", "Ranges updated successfully");
    }
  }
}

void handleSendRanges() {
  if (server.method() == HTTP_GET) {
    DynamicJsonDocument doc(256);
    JsonArray ranges = doc.createNestedArray("ranges");

    // Loop through the valid ranges
    for (int i = 0; i < numRanges; i++) {
      JsonArray range = ranges.createNestedArray();
      range.add(modeRanges[i][0]);
      range.add(modeRanges[i][1]);
    }
    String jsonData;
    serializeJson(doc, jsonData);
    server.send(200, "application/json", jsonData);
  }
}

void handleRPMData() {
  DynamicJsonDocument doc(256);
  doc["rpm"] = getRPMUnified();
  String jsonData;
  serializeJson(doc, jsonData);
  server.send(200, "application/json", jsonData);
}

void sendCurrentMode() {
  StaticJsonDocument<200> doc;
  doc["current_position"] = determineMode(getRPMUnified());
  String jsonData;
  serializeJson(doc, jsonData);
  server.send(200, "application/json", jsonData);
}

void handleTargetPosition() {
  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  if (error || !doc.containsKey("targetPosition")) {
    server.send(400, "application/json", "{\"error\":\"Invalid or missing targetPosition\"}");
    return;
  }

  int targetMode = doc["targetPosition"];
  targetMode = constrain(targetMode, 1, numRanges);

  DynamicJsonDocument response(256);
  JsonArray modePath = response.createNestedArray("mode_path");
  int currentMode = determineMode(getRPMUnified());

  if (targetMode > currentMode) {
    for (int i = currentMode; i <= targetMode; i++) {
      modePath.add(i);
      st.WritePosEx(SERVO_ID, modeServoPositions[i - 1], 0, 20);
      delay(2000);
    }
  } else {
    for (int i = currentMode; i >= targetMode; i--) {
      modePath.add(i);
      st.WritePosEx(SERVO_ID, modeServoPositions[i - 1], 0, 20);
      delay(2000);
    }
  }

  String jsonData;
  serializeJson(response, jsonData);
  server.send(200, "application/json", jsonData);
}

void handleTestResult() {
  DynamicJsonDocument doc(256);
  JsonArray modePath = doc.createNestedArray("mode_path");
  for (int i = 1; i <= numRanges; i++) {
    modePath.add(i);
    st.WritePosEx(SERVO_ID, modeServoPositions[i - 1], 0, 20);
    delay(2000);
  }
  for (int i = numRanges - 1; i >= 1; i--) {
    modePath.add(i);
    st.WritePosEx(SERVO_ID, modeServoPositions[i - 1], 0, 20);
    delay(2000);
  }
  String jsonData;
  serializeJson(doc, jsonData);
  server.send(200, "application/json", jsonData);
}

void handleTestCheck() {
  String jsonData = server.arg("plain");
  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, jsonData);
  if (!error && doc.containsKey("test_check")) {
    testCheck = doc["test_check"];
    server.send(200, "text/plain", "Test check updated successfully");
  } else {
    server.send(400, "text/plain", "Invalid JSON");
  }
}

void handleSync() {
  DynamicJsonDocument doc(256);
  doc["sync"] = syncStatus;
  String jsonData;
  serializeJson(doc, jsonData);
  server.send(200, "application/json", jsonData);
}

void setupWiFiRoutes() {
  server.on("/data", HTTP_GET, handleRPMData);
  server.on("/test_result", HTTP_GET, handleTestResult);
  server.on("/current_position", HTTP_GET, sendCurrentMode);
  server.on("/save_test_check", HTTP_POST, handleTestCheck);
  server.on("/receive_target_position", HTTP_GET, handleTargetPosition);
  server.on("/save_ranges", HTTP_POST, handleRanges);
  server.on("/sync", HTTP_POST, handleSync);
  server.on("/ranges", HTTP_GET, handleSendRanges);
}

void startWiFi() {
  WiFi.softAPConfig(IPAddress(192, 168, 4, 6), IPAddress(192, 168, 4, 6), IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid, password, 1, false, 1);
  WiFi.setSleep(WIFI_PS_NONE);
  setupWiFiRoutes();
  server.begin();
  espAPIP = WiFi.softAPIP();
  Serial.println("HTTP server started");
}

void enableWiFi() {
  Serial.println("📶 Turning WiFi ON – allowing client connections");
  startWiFi();
}

void disableWiFi() {
  Serial.println("📴 Turning WiFi OFF – hiding SSID, no client access");
  WiFi.softAPdisconnect(true);
  esp_wifi_stop();
}

IPAddress getIpAddress() {
    return espAPIP;
}

void listConnectedClients() {
    wifi_sta_list_t sta_list;

    if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
        Serial.printf("📡 %d client(s) connected:\n", sta_list.num);
        for (int i = 0; i < sta_list.num; ++i) {
            const uint8_t* mac = sta_list.sta[i].mac;
            Serial.printf("  → MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2],
                mac[3], mac[4], mac[5]);
        }
    } else {
        Serial.println("❌ Failed to retrieve client list.");
    }
}

void showTxPower() {
    int8_t power_quarter_dbm;
    esp_wifi_get_max_tx_power(&power_quarter_dbm);
    float power_dbm = power_quarter_dbm / 4.0;
    Serial.printf("📡 Current TX Power: %.2f dBm\n", power_dbm);
}

void setTxPower(float dBm) {
    if (dBm < 8.0 || dBm > 20.0) {
        Serial.println("❌ Invalid TX power. Must be between 8.0 and 20.0 dBm.");
        return;
    }

    int8_t quarter_dbm = static_cast<int8_t>(dBm * 4);
    esp_wifi_set_max_tx_power(quarter_dbm);
    Serial.printf("✅ TX Power set to %.2f dBm\n", dBm);
}

void printWifiMac() {
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac);  // or WIFI_IF_STA depending on mode
    Serial.printf("📡 MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2],
                  mac[3], mac[4], mac[5]);
}
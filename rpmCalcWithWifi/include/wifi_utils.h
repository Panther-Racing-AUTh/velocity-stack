#pragma once

#include <WiFi.h>
#include <WebServer.h>
// Prevent C headers from being misparsed in C++
#ifdef __cplusplus
extern "C" {
#endif

#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_interface.h"

#ifdef __cplusplus
}  // extern "C"
#endif


extern WebServer server;
extern IPAddress espAPIP;  // Global declaration of the Access Point IP

extern const char* ssid;
extern const char* password;

void startWiFi();
void enableWiFi();
void disableWiFi();

void handleRPMData();
void handleRanges();
void handleSendRanges();
void handleTargetPosition();
void handleTestResult();
void handleTestCheck();
void sendCurrentMode();
void handleSync();
IPAddress getIpAddress();
void listConnectedClients();
void showTxPower();
void setTxPower(float dBm);
void printWifiMac();


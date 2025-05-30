#include <Arduino.h>
#include <WiFi.h>  // Use WiFi library for ESP32
#include <WebServer.h>
#include <ArduinoJson.h>  // Include the ArduinoJson library
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include <SCServo.h> 

// include rpm data file
#include "include/rpm_data.h"

#include "include/cli.h"
#include "include/nvs_utils.h"
#include "include/rpm.h"
#include "include/servo.h"



// Button declarations
// #define STATUS_BUTTON 9 
// #define POWER_BUTTON 1

int STATUS_BUTTON = 9 ;
int POWER_BUTTON = 1;
// Pin declarations
const int interruptPin = 18;  // Digital GPIO pin for signal detection

unsigned long lastServoUpdate = 0;

unsigned long lastPrintTime = 0;  // For periodic RPM updates
bool functionStatus = true;       // Toggle isDiagnosticsMode (ON/OFF)
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
bool wifiEnabled = true;  // Track WiFi state, diagnostics sto true kai race mode sto false
bool printOnceEnable=false;
String status = "diagnostics";

//Possible modes:

// off          - standby mode
// diagnostics  - default mode 
// race         - race mode (rpm calculation enabled)
// wifi         - diagnostics mode (WIFI enabled)

// Variables for mode change from button press
volatile bool waitingForSecondPress = false;
volatile bool doublePressDetected = false;
volatile unsigned long lastPressTime = 0;
unsigned long singlePressTimer = 0;
bool singlePressPending = false;


bool printOnceDisable=false;
bool lastWifiButtonState = HIGH;  // Previous button state
bool wifiButtonState = HIGH;

// ============ Interrupt Service Routines (ISRs) ===================

// ISRs for button presses

void IRAM_ATTR handleStatusInterrupt() {
  static unsigned long lastTrigger = 0;
  if (millis() - lastTrigger > 200) {
    lastTrigger = millis();
    Serial.println("============ SYSTEM STATUS ============");
    Serial.print("Current mode: ");
    Serial.println(status);
  }
}

void IRAM_ATTR handlePowerInterrupt() {
  static unsigned long lastTrigger = 0;
  unsigned long now = millis();

  if (now - lastTrigger < 150) return;  // Debounce
  lastTrigger = now;

  if (waitingForSecondPress && (now - lastPressTime < 500)) {
    doublePressDetected = true;
    waitingForSecondPress = false;
  } else {
    waitingForSecondPress = true;
    lastPressTime = now;
    singlePressTimer = now;
    singlePressPending = true;
  }
}













// ============================================================ STATUS FUNCTIONS ================================================
// void diagnosticsMode() {
//   printOnceDisable=false;
//   if(!printOnceEnable){
//     enableWiFi();
//     printOnceEnable= true;
//   }
//     server.handleClient();

//     if(!testCheck){
//       rpm = simulateMoto3RPM();

//       mode = determineMode(rpm);
//       if(lastMode != mode){
//         st.WritePosEx(SERVO_ID, modeServoPositions[mode-1], 0, 100);  // Move to 360° (full rotation)
//         Serial.print("Servo position: ");
//         Serial.println(modeServoPositions[mode-1]);
        
//         delay(500);
//       }
//       lastMode = mode;
//     }
//     yield();  // Prevent watchdog timer reset

// }

// void raceMode() {
//   printOnceEnable=false;
//   if(!printOnceDisable){
//       disableWiFi();
//       printOnceDisable=true;
//   }
//     Serial.println("Race Mode Active: WiFi Disabled");
    
//     if (period > 0 && (millis() - lastPrintTime >= 100)) {
//         lastPrintTime = millis();
//         freq = 1000000.0 / period;
//         rpm = (freq * 120) / 36;
//         Serial.print("RPM: ");
//         Serial.println(rpm);
//     }

//     //Safe Mode Section, μενει να δουμε τον αισθητηρα του servo και αναλογως να rebootαρει
//     //Serial.println("Rebooting ESP32 due to error...");
//     //delay(1000);  // Give time for the message to print
//     //ESP.restart();
// }

void changeStatus(){
  if (doublePressDetected) {
    doublePressDetected = false;
    singlePressPending = false;
    waitingForSecondPress = false;

    if (status == "diagnostics") {
      status = "race";
      Serial.println("Switched to RACE mode");
      // raceMode();
    } else if (status == "race"){
      status = "wifi";
      Serial.println("Switched to WIFI mode");
    }
    else {
      status = "diagnostics";
      Serial.println("Switched to DIAGNOSTICS mode");
      // diagnosticsMode();
    }

    return;  // Done this loop
  }

  // Handle single press after timeout
  if (singlePressPending && millis() - singlePressTimer > 1000) {
    singlePressPending = false;
    waitingForSecondPress = false;

    if (status == "off") {
      status = "diagnostics";
      Serial.println("Turned ON: DIAGNOSTICS mode");
      // diagnosticsMode();
    } else {
      status = "off";
      Serial.printf("Turned OFF\n");
    }
  }
}


void setup() { 
  Serial.begin(115200);

  // NVS Initialization
  initNVS();

  // Servo Initialization
  servo_defaultInit();


  attachInterrupt(digitalPinToInterrupt(interruptPin), rpmSensorISR, RISING);
  pinMode(interruptPin, INPUT);



  // button initialization
  pinMode(STATUS_BUTTON, INPUT_PULLUP);  
  pinMode(POWER_BUTTON, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(STATUS_BUTTON), handleStatusInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(POWER_BUTTON), handlePowerInterrupt, FALLING);


}




void loop() {
  
  handleCLI();
  // wifiButtonState = digitalRead(wifiButtonPin);

  // if (wifiButtonState == LOW && lastWifiButtonState == HIGH) {
  //   delay(50);

  //   wifiEnabled = !wifiEnabled;
  //   Serial.print("Mode Toggled - Now: ");
  //   Serial.println(wifiEnabled ? "Diagnostics Mode" : "Race Mode");
  //   delay(300);
  //   isDiagnosticsMode = !isDiagnosticsMode;
  // }
  // lastWifiButtonState = wifiButtonState;

  // if (status == "diagnostics") {
  //   diagnosticsMode();
  // } else if (status == "race") {
  //     raceMode();
  // } else if (status == "off") {
  //     // do nothing
  // }
  
  // Serial.println(isDiagnosticsMode ? "WiFi is ON (SSID visible)" : "WiFi is OFF (hidden)");

  //random rpm values to check fetching rpm,determine the mode we are at that moment,etc.(testing...)
  
  changeStatus();

  // Check servo update every 200 ms
  updateServoIfFollowing();
  

  delay(50);
}

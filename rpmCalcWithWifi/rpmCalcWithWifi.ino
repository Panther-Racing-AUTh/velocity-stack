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
#include "include/state.h"
#include "include/pin_utils.h"



int STATUS_BUTTON = 9 ;

// ============ Interrupt Service Routines (ISRs) ===================

// ISRs for button presses

void IRAM_ATTR handleStatusInterrupt() {
  static unsigned long lastTrigger = 0;
  if (millis() - lastTrigger > 200) {
    lastTrigger = millis();
    printMenu();
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


void setup() { 
  Serial.begin(115200);
  gpio_install_isr_service(0); 
  while (!Serial) delay(10); // <-- For ESP32-C3 USB Serial
  delay(500);
  // NVS Initialization
  initNVS();

  // Servo Initialization
  servo_defaultInit();

  // Mode Switch Button Initialization
  initModeButtonInterrupt();

  // Init State
  initState();

  // RPM Sensor Pin Initialization
  initRpmSensorInterrupt();

  // Mark Pin Initialization
  initMarkPin();

  // button initialization
  pinMode(STATUS_BUTTON, INPUT_PULLUP);  

  attachInterrupt(digitalPinToInterrupt(STATUS_BUTTON), handleStatusInterrupt, FALLING);


}




void loop() {
  
  handleCLI();
  
  changeStatus();

  // Check servo update every 200 ms
  updateServoIfFollowing();
  
  toggleMovementPin();

  delay(50);
}

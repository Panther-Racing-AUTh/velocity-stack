#include <WiFi.h>  // Use WiFi library for ESP32
#include <WebServer.h>
#include <ArduinoJson.h>  // Include the ArduinoJson library
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include <Arduino.h>
#include <SCServo.h>

// include rpm data file
#include "rpm_data.h"


// **Access Point credentials**
const char* ssid = "ESP8266_AP";
const char* password = "12345678";

// Create a web server on port 80
WebServer server(80);

// Button declarations
#define STATUS_BUTTON 9 
#define POWER_BUTTON 1

// Pin declarations
const int analogPin = 0;      // ADC pin
const int interruptPin = 18;  // Digital GPIO pin for signal detection
const int wifiButtonPin = 4;
const int wifiButtonPinOutput = 9;

// Servo pins declaration
#define RX 6        // ESP32 RX pin
#define TX 7        // ESP32 TX pin
#define SERVO_ID 1  // Default servo ID
SMS_STS st;

// Variables for RPM simulation
volatile unsigned long t1 = 0, t2 = 0;               // Time variables
volatile float period = 0.0, freq = 1000.0, rpm = 0.0;  // Period, frequency, and RPM variables
volatile bool risingEdgeDetected = false;
volatile int mode=0;
volatile int lastMode=0;

unsigned long lastPrintTime = 0;  // For periodic RPM updates
bool functionStatus = true;       // Toggle isDiagnosticsMode (ON/OFF)
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
bool wifiEnabled = true;  // Track WiFi state, diagnostics sto true kai race mode sto false
bool printOnceEnable=false;
String status = "diagnostics";

//Possible modes:

// off          - standby mode
// diagnostics  - diagnostics mode (WIFI enabled)
// race         - race mode (rpm calculation enabled)

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


// ISR for rpm calculation
void IRAM_ATTR onRise() {
  static unsigned long last_rise_time = 0;
  unsigned long current_time = micros();

  // Debounce: Ignore edges that occur within 300 microseconds
  if (current_time - last_rise_time > period/2) {
    last_rise_time = current_time;

    if (risingEdgeDetected) {
      t2 = current_time;
      if (t1 < t2) {
        period = t2 - t1;  // Calculate period
      }
      risingEdgeDetected = false;
    } else {
      t1 = current_time;
      risingEdgeDetected = true;
    }
  }
}

// Mode ranges storage
const int MAX_RANGES=12;
const int INITIAL_RANGES = 4;

int modeRanges[MAX_RANGES][2] = {
  {0,3000},
  {3001, 5000},
  {5001, 8000},
  {8001, 14000}
};

int modeServoPositions[MAX_RANGES];

int numRanges = INITIAL_RANGES;

bool testCheck = false;  
bool syncStatus = false; 




// ============================================================ STATUS FUNCTIONS ================================================
void diagnosticsMode() {
  printOnceDisable=false;
  if(!printOnceEnable){
    enableWiFi();
    printOnceEnable= true;
  }
    server.handleClient();

    if(!testCheck){
      rpm = simulateMoto3RPM();

      mode = determineMode(rpm);
      if(lastMode != mode){
        st.WritePosEx(SERVO_ID, modeServoPositions[mode-1], 0, 100);  // Move to 360° (full rotation)
        Serial.print("Servo position: ");
        Serial.println(modeServoPositions[mode-1]);
        
        delay(500);
      }
      lastMode = mode;
    }
    yield();  // Prevent watchdog timer reset

}

void raceMode() {
  printOnceEnable=false;
  if(!printOnceDisable){
      disableWiFi();
      printOnceDisable=true;
  }
    Serial.println("Race Mode Active: WiFi Disabled");
    
    if (period > 0 && (millis() - lastPrintTime >= 100)) {
        lastPrintTime = millis();
        freq = 1000000.0 / period;
        rpm = (freq * 120) / 36;
        Serial.print("RPM: ");
        Serial.println(rpm);
    }

    //Safe Mode Section, μενει να δουμε τον αισθητηρα του servo και αναλογως να rebootαρει
    //Serial.println("Rebooting ESP32 due to error...");
    //delay(1000);  // Give time for the message to print
    //ESP.restart();
}

void changeStatus(){
  if (doublePressDetected) {
    doublePressDetected = false;
    singlePressPending = false;
    waitingForSecondPress = false;

    if (status == "diagnostics") {
      status = "race";
      Serial.println("Switched to RACE mode");
      // raceMode();
    } else {
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

// ================================================ NON-VOLATILE STORAGE (NVS) FUNCTIONS ============================================================
void eraseNVS() {
  esp_err_t err = nvs_flash_erase();
  if (err == ESP_OK) {
    Serial.println("NVS erased successfully.");
    err = nvs_flash_init();
    if (err == ESP_OK) {
      Serial.println("NVS initialized.");
    } else {
      Serial.print("Error initializing NVS: ");
      Serial.println(err);
    }
  } else {
    Serial.print("Error erasing NVS: ");
    Serial.println(err);
  }
}

void storeRanges() {
  nvs_handle_t handle;
  if (nvs_open("storage", NVS_READWRITE, &handle) == ESP_OK) {
    // Store the number of ranges
    nvs_set_i32(handle, "numRanges", numRanges);

    for (int i = 0; i < numRanges; i++) {
      char key[16];
      int32_t value0 = modeRanges[i][0];
      int32_t value1 = modeRanges[i][1];

      int32_t storedValue0, storedValue1;
      
      // Check and update the first value if needed
      sprintf(key, "range_%d_0", i);
      if (nvs_get_i32(handle, key, &storedValue0) != ESP_OK || storedValue0 != value0) {
        nvs_set_i32(handle, key, value0);
      }
      
      // Check and update the second value if needed
      sprintf(key, "range_%d_1", i);
      if (nvs_get_i32(handle, key, &storedValue1) != ESP_OK || storedValue1 != value1) {
        nvs_set_i32(handle, key, value1);
      }
    }

    nvs_commit(handle);
    nvs_close(handle);
  }
}

void loadRanges() {
  nvs_handle_t handle;
  if (nvs_open("storage", NVS_READONLY, &handle) == ESP_OK) {
    int32_t tempNumRanges;  // Temporary storage for int32_t
    nvs_get_i32(handle, "numRanges", &tempNumRanges);
    numRanges = tempNumRanges;  // Direct assignment

    for (int i = 0; i < numRanges; i++) {  
      char key[16];

      int32_t value0, value1;

      sprintf(key, "range_%d_0", i);
      if (nvs_get_i32(handle, key, &value0) == ESP_OK) {
        modeRanges[i][0] = value0;
      }

      sprintf(key, "range_%d_1", i);
      if (nvs_get_i32(handle, key, &value1) == ESP_OK) {
        modeRanges[i][1] = value1;
      }
    }

    nvs_close(handle);
  }
}


// ============================================================== SERVO FUNCTIONS ==================================================
void generateServoPositions(int numSteps) {
  if (numSteps > MAX_RANGES || numSteps < 2) {
    Serial.println("Error: numSteps must be between 2 and MAX_RANGES.");
    return;
  }

  Serial.println(numSteps);
  for (int i = 0; i < numSteps; i++) {
    float angleDeg = (360.0 / (numSteps - 1)) * i;
    int position = (int)(4096.0 * angleDeg / 360.0);
    modeServoPositions[i] = position;

    Serial.print("Step ");
    Serial.print(i);
    Serial.print(": ");
    Serial.print(angleDeg, 2);
    Serial.print("° => Position ");
    Serial.println(position);
  }
  delay(3000);
}

int degreesToPos(float degrees) {
  return int(4096 * (degrees / 360.0));
}


// ================================================================ RPM FUNCTIONS ==============================================================
int simulateMoto3RPM() {
  static unsigned long lastUpdate = 0;
  static int index = 0;

  unsigned long now = millis();

  if (now - lastUpdate > 100) {
    lastUpdate = now;

    index++;
    if (index >= rpmLength) {
      index = 0;  // loop back to the beginning
    }
  }

  return rpm1[index];
}

int determineMode(int rpm) {
    for (int i = 0; i < numRanges; i++) { // Use dynamic numRanges instead of fixed 4
        if (rpm >= modeRanges[i][0] && rpm < modeRanges[i][1]) {
            return i + 1; // Mode 1 to Mode N
        }
    }
    return 0; // Default case (out of range)
}

// ================================================================== WIFI FUNCTIONS ======================================================
void handleRanges() {
  if (server.method() == HTTP_POST) {
    String jsonData = server.arg("plain");
    DynamicJsonDocument doc(512);  // Increase size if needed
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
      
      // Create ranges by pairing consecutive boundaries.
      int newNumRanges = count - 1;
      if (newNumRanges > MAX_RANGES) {
        Serial.println("Too many ranges");
        server.send(400, "text/plain", "Too many ranges");
        return;
      }
      
      for (int i = 0; i < newNumRanges; i++) {
        modeRanges[i][0] = boundaries[i].as<int>();
        modeRanges[i][1] = boundaries[i + 1].as<int>();
      }
      generateServoPositions(numRanges);
      numRanges = newNumRanges;
      storeRanges();
      server.send(200, "text/plain", "Ranges updated successfully");
      Serial.println("Ranges updated successfully");
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
    Serial.println("Sending RPM ranges: " + jsonData);
    server.send(200, "application/json", jsonData);
  }
}

void sendCurrentMode() {
  StaticJsonDocument<200> jsonDoc; // Create a JSON document
  jsonDoc["current_position"] = mode; // Add the key-value pair

  String jsonString; // String to hold the serialized JSON
  serializeJson(jsonDoc, jsonString); // Serialize JSON to a string

  server.send(200, "application/json", jsonString); // Send the JSON response
}

// HTTP handler to send RPM data
void handleRPMData() {
  DynamicJsonDocument doc(256);

  // Send RPM value
  doc["rpm"] = rpm;

  String jsonData;
  serializeJson(doc, jsonData);
  Serial.print("Sending RPM data: " + jsonData);
  Serial.printf("    | Mode: %d\n", mode);

  server.send(200, "application/json", jsonData);
}

void handleTargetPosition() {
  // Allocate a buffer for incoming JSON (adjust size as needed)
  DynamicJsonDocument doc(256);

  // Parse the incoming JSON body from the client
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  // Check if "targetPosition" key exists
  if (!doc.containsKey("targetPosition")) {
    server.send(400, "application/json", "{\"error\":\"Missing 'targetPosition'\"}");
    return;
  }

  int targetMode = doc["targetPosition"];

  // Optional: clamp the target range
  targetMode = constrain(targetMode, 1, numRanges);  // Adjust based on your mode limits

  // JSON response
  DynamicJsonDocument response(256);
  JsonArray modePath = response.createNestedArray("mode_path");

  // extern int mode;  // Make sure this reflects current mode
  if (targetMode > mode) {
    for (int i = mode; i <= targetMode; i++) {
      modePath.add(i);
      st.WritePosEx(SERVO_ID, modeServoPositions[i - 1], 0, 20);
      delay(2000);
    }
  } else {
    for (int i = mode; i >= targetMode; i--) {
      modePath.add(i);
      st.WritePosEx(SERVO_ID, modeServoPositions[i - 1], 0, 20);
      delay(2000);
    }
  }

  // Update current mode
  mode = targetMode;

  // Send JSON response
  String jsonData;
  serializeJson(response, jsonData);
  Serial.println("Sending mode path: " + jsonData);
  server.send(200, "application/json", jsonData);
}

// HTTP handler to send test result (if test_check is true)
void handleTestResult() {
  DynamicJsonDocument doc(256);

  // Create the JSON array "mode_path"
  JsonArray modePath = doc.createNestedArray("mode_path");

  // Generate the pattern [1, 2, 3, 4, 3, 2, 1]
  for (int i = 1; i <= numRanges; i++) {
    modePath.add(i); // Add descending values: 3, 2, 1
    st.WritePosEx(SERVO_ID, modeServoPositions[i-1], 0, 20);  // Move to 360° (full rotation)
    delay(2000);
  }
  for (int i = numRanges - 1; i >= 1; i--) {
    
    modePath.add(i); // Add descending values: 3, 2, 1
    st.WritePosEx(SERVO_ID, modeServoPositions[i-1], 0, 20);
    delay(2000);
  }

  // Serialize the JSON document to a string
  String jsonData;
  serializeJson(doc, jsonData);

  // Print the JSON to the serial monitor for debugging
  Serial.println("Sending mode path: " + jsonData);

  // Send the JSON response to the client
  server.send(200, "application/json", jsonData);
}

// HTTP handler to get a boolean value called "test_check"
void handleTestCheck() {
  if (server.method() == HTTP_POST) {
    String jsonData = server.arg("plain");
    Serial.println("Received JSON data for test_check: " + jsonData);

    // Parse the JSON data
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, jsonData);

    if (error) {
      Serial.println("Failed to parse JSON");
      server.send(400, "text/plain", "Invalid JSON");
      return;
    }

    // Check for test_check and update the flag
    if (doc.containsKey("test_check")) {
      testCheck = doc["test_check"];
      Serial.print("Received test_check: ");
      Serial.println(testCheck);
      
    }

    server.send(200, "text/plain", "Test check updated successfully");
  }
}

void handleSync() {
  if (server.method() == HTTP_POST) {
    DynamicJsonDocument doc(256);

    // Send sync status as a boolean
    doc["sync"] = syncStatus;

    String jsonData;
    serializeJson(doc, jsonData);
    Serial.println("Sending sync status: " + jsonData);
    server.send(200, "application/json", jsonData);
  }
}

void startWiFi() {
   WiFi.softAPConfig(IPAddress(192, 168, 4, 6), IPAddress(192, 168, 4, 6), IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid, password, 1, false, 1);
  WiFi.setSleep(WIFI_PS_NONE); // Disable WiFi sleep
  IPAddress IP = WiFi.softAPIP();
  Serial.println("AP IP address: " + IP.toString());

    server.begin();

  Serial.print("Connecting to WiFi");
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  
}

void enableWiFi(){
  Serial.println("📶 Turning WiFi ON – allowing client connections");
  startWiFi();
}

void disableWiFi() {
  Serial.println("📴 Turning WiFi OFF – hiding SSID, no client access");
  WiFi.softAPdisconnect(true);     // Stop access point
  esp_wifi_stop(); 
}


void setup() { 
  Serial.begin(115200);

  
  // eraseNVS();
  attachInterrupt(digitalPinToInterrupt(interruptPin), onRise, RISING);
  pinMode(interruptPin, INPUT);

  // Set up Wi-Fi Access Point
  WiFi.softAPConfig(IPAddress(192, 168, 4, 6), IPAddress(192, 168, 4, 6), IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid, password, 1, false, 1);
  WiFi.setSleep(WIFI_PS_NONE); // Disable WiFi sleep
  IPAddress IP = WiFi.softAPIP();
  Serial.println("AP IP address: " + IP.toString());

  // Load saved RPM ranges from NVS
  loadRanges();
  
  // Ensure first and last values remain fixed
  modeRanges[0][0] = 0;
  modeRanges[numRanges - 1][1] = 14000;

  // Configure HTTP routes
  server.on("/data", HTTP_GET, handleRPMData);          // Send RPM data
  server.on("/test_result", HTTP_GET, handleTestResult);  // Send Test Results

  server.on("/current_position",HTTP_GET ,  sendCurrentMode);
  server.on("/save_test_check", HTTP_POST, handleTestCheck); // Receive test_check flag as boolean value
  server.on("/receive_target_position", HTTP_GET, handleTargetPosition);  // Send Test Results
  
  server.on("/save_ranges", HTTP_POST, handleRanges); // Receive ranges for modes
  server.on("/sync", HTTP_POST, handleSync); // Send sync status (boolean)
  server.on("/ranges", HTTP_GET, handleSendRanges); // Send RPM ranges
  server.begin();
  Serial.println("HTTP server started");

  randomSeed(analogRead(0));  // Seed random generator with noise from an analog pin
  pinMode(wifiButtonPin, INPUT_PULLUP);

  // button initialization
  pinMode(STATUS_BUTTON, INPUT_PULLUP);  
  pinMode(POWER_BUTTON, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(STATUS_BUTTON), handleStatusInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(POWER_BUTTON), handlePowerInterrupt, FALLING);


  // SERVO SETUP
  Serial1.begin(1000000, SERIAL_8N1, RX, TX);  // UART at 1Mbps
  st.pSerial = &Serial1;
  delay(1000);
  st.WritePosEx(SERVO_ID, 0, 0, 20);
  Serial.println("Position : 0");

  delay(5000);
  Serial.println("Servo is ready!");
  generateServoPositions(numRanges);

}




void loop() {
  
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

  delay(50);
}

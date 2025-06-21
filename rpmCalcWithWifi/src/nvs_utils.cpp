#include "../include/nvs_utils.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <Arduino.h>
#include "../include/servo.h"
#include "../include/pin_utils.h"
#include "../include/rpm.h"
#include "../include/state.h"

const int MAX_RANGES = 12;
int32_t numRanges = 0;
int32_t modeRanges[MAX_RANGES][2];
int32_t modeServoPositions[MAX_RANGES];

void setDefaultRanges() {
  numRanges = 4;
  modeRanges[0][0] = 0;       modeRanges[0][1] = 3000;
  modeRanges[1][0] = 3001;    modeRanges[1][1] = 5000;
  modeRanges[2][0] = 5001;    modeRanges[2][1] = 8000;
  modeRanges[3][0] = 8001;    modeRanges[3][1] = 14000;
}

void initNVS() {
  if (nvs_flash_init() != ESP_OK) {
    Serial.println("❌ NVS initialization failed.");
    return;
  }

  // === Ranges ===
  if (!loadRanges()) {
    Serial.println("⚠️ No stored ranges found. Using defaults...");
    setDefaultRanges();
    storeRanges();
  } else {
    Serial.println("✅ Loaded RPM ranges from NVS.");
  }

  // === Servo Positions ===
  if (!loadServoPositions()) {
    Serial.println("⚠️ No stored servo positions found. Using default 0° positions.");
    generateServoPositions(numRanges);
  } else {
    Serial.println("✅ Loaded servo positions from NVS.");
  }

  // === Mechanical Parameters ===
  if (!loadMechanicalParams()) {
    Serial.println("⚠️ No mechanical parameters found. Using defaults.");
    setPinionRadius(15.5);    // mm
    setRackLength(57.0);      // mm
    storeMechanicalParams();
  } else {
    Serial.println("✅ Loaded pinion & rack values from NVS.");
  }
  // === Pin Assignments ===
  loadPinAssignments();
}

void eraseNVS() {
  if (nvs_flash_erase() == ESP_OK) {
    Serial.println("🧹 NVS erased.");
    if (nvs_flash_init() == ESP_OK) {
      setDefaultRanges();
      storeRanges();
      Serial.println("🔁 Default ranges restored.");
    }
  }
}

void storeRanges() {
  nvs_handle_t handle;
  if (nvs_open("storage", NVS_READWRITE, &handle) != ESP_OK) return;

  nvs_set_i32(handle, "numRanges", numRanges);

  for (int i = 0; i < numRanges; i++) {
    char key0[16], key1[16];
    sprintf(key0, "range_%d_0", i);
    sprintf(key1, "range_%d_1", i);
    nvs_set_i32(handle, key0, modeRanges[i][0]);
    nvs_set_i32(handle, key1, modeRanges[i][1]);
  }

  nvs_commit(handle);
  nvs_close(handle);
}

bool loadRanges() {
  nvs_handle_t handle;
  if (nvs_open("storage", NVS_READONLY, &handle) != ESP_OK) return false;

  int32_t temp;
  if (nvs_get_i32(handle, "numRanges", &temp) != ESP_OK) {
    nvs_close(handle);
    return false;
  }

  numRanges = temp;

  for (int i = 0; i < numRanges; i++) {
    char key0[16], key1[16];
    sprintf(key0, "range_%d_0", i);
    sprintf(key1, "range_%d_1", i);
    if (nvs_get_i32(handle, key0, &modeRanges[i][0]) != ESP_OK ||
        nvs_get_i32(handle, key1, &modeRanges[i][1]) != ESP_OK) {
      nvs_close(handle);
      return false;
    }
  }

  nvs_close(handle);
  return true;
}

void listNVSContents() {
  nvs_handle_t handle;
  if (nvs_open("storage", NVS_READONLY, &handle) != ESP_OK) {
    Serial.println("❌ Failed to open NVS.");
    return;
  }

  Serial.println("📄 NVS Stored Values:\n");

  // === RANGES ===
  int32_t num;
  if (nvs_get_i32(handle, "numRanges", &num) == ESP_OK) {
    Serial.printf("🔢 Number of RPM Ranges: %d\n", num);
    for (int i = 0; i < num; ++i) {
      char k0[16], k1[16];
      sprintf(k0, "range_%d_0", i);
      sprintf(k1, "range_%d_1", i);
      int32_t r0, r1;
      if (nvs_get_i32(handle, k0, &r0) == ESP_OK && nvs_get_i32(handle, k1, &r1) == ESP_OK) {
        Serial.printf("  Range %d: %d – %d\n", i + 1, r0, r1);
      }
    }
  }

  // === SERVO POSITIONS ===
  Serial.println("\n🦾 Servo Positions per Mode:");
  for (int i = 0; i < MAX_RANGES; ++i) {
    char key[24];
    sprintf(key, "servo_pos_%d", i);
    int32_t pos;
    if (nvs_get_i32(handle, key, &pos) == ESP_OK) {
      Serial.printf("  Mode %d: Position %d\n", i + 1, pos);
    }
  }

  // === MECHANICAL PARAMETERS ===
  int32_t rack, pinion;

  if (nvs_get_i32(handle, "rack_len", &rack) == ESP_OK) {
    Serial.printf("\n📏 Rack Length: %.3f mm\n", rack / 1000.0f);
  }
  if (nvs_get_i32(handle, "pinion_rad", &pinion) == ESP_OK) {
    Serial.printf("\n⍉ Pinion Radius: %.3f mm\n", pinion / 1000.0f);
  }

  // === PIN DEFINITIONS ===
  int32_t rpmPin, buttonPin, movementPin, rxPin, txPin, markPin;

  if (nvs_get_i32(handle, "rpm_pin", &rpmPin) == ESP_OK)
    Serial.printf("\n📍 RPM Sensor Pin: GPIO %d\n", rpmPin);
  else
    Serial.println("\n📍 RPM Sensor Pin: ❌ Not stored in NVS");

  if (nvs_get_i32(handle, "mode_button_pin", &buttonPin) == ESP_OK)
    Serial.printf("🔘 Mode Button Pin: GPIO %d\n", buttonPin);
  else
    Serial.println("🔘 Mode Button Pin: ❌ Not stored in NVS");

  if (nvs_get_i32(handle, "movement_pin", &movementPin) == ESP_OK)
    Serial.printf("⚡ Movement Pin: GPIO %d\n", movementPin);
  else
    Serial.println("⚡ Movement Pin: ❌ Not stored in NVS");

  if (nvs_get_i32(handle, "mark_pin", &markPin) == ESP_OK)
    Serial.printf("⚡ Mark Pin: GPIO %d\n", markPin);
  else
    Serial.println("⚡ Mark Pin: ❌ Not stored in NVS");

  if (nvs_get_i32(handle, "servo_rx_pin", &rxPin) == ESP_OK &&
      nvs_get_i32(handle, "servo_tx_pin", &txPin) == ESP_OK)
    Serial.printf("🦾 Servo UART Pins → RX: %d, TX: %d\n", rxPin, txPin);
  else
    Serial.println("🦾 Servo UART Pins: ❌ RX/TX not stored in NVS");

  Serial.println("\n✅ Done reading NVS contents.\n");
  nvs_close(handle);
}

void updateAndStoreRanges(const std::vector<int>& thresholds) {
  if (thresholds.size() < 2 || thresholds.size() > MAX_RANGES + 1) {
    Serial.println("❌ Invalid number of thresholds.");
    return;
  }

  numRanges = thresholds.size() - 1;
  for (int i = 0; i < numRanges; i++) {
    modeRanges[i][0] = thresholds[i] + (i > 0 ? 1 : 0);  // Inclusive, avoid overlap
    modeRanges[i][1] = thresholds[i + 1];
  }

  storeRanges();
  Serial.println("✅ Ranges updated and stored.");
}

int determineMode(int rpm) {
  for (int i = 0; i < numRanges; ++i) {
    if (rpm >= modeRanges[i][0] && rpm <= modeRanges[i][1]) {
      return i + 1;  // 1-based
    }
  }
  return -1;  // Not found
}

int getServoPositionForMode(int mode) {
  if (mode < 1 || mode > numRanges) return -1;
  return modeServoPositions[mode - 1];
}

void storeServoPositions() {
  nvs_handle_t handle;
  if (nvs_open("storage", NVS_READWRITE, &handle) != ESP_OK) return;

  for (int i = 0; i < numRanges; ++i) {
    char key[20];
    sprintf(key, "servo_pos_%d", i);
    int32_t pos = getServoPositionForMode(i + 1);
    nvs_set_i32(handle, key, pos);
  }

  nvs_commit(handle);
  nvs_close(handle);
}

bool loadServoPositions() {
  nvs_handle_t handle;
  if (nvs_open("storage", NVS_READONLY, &handle) != ESP_OK) return false;

  for (int i = 0; i < numRanges; ++i) {
    char key[20];
    sprintf(key, "servo_pos_%d", i);
    int32_t pos;
    if (nvs_get_i32(handle, key, &pos) == ESP_OK) {
      modeServoPositions[i] = pos;
    }
  }

  nvs_close(handle);
  return true;
}

void storeMechanicalParams() {
  nvs_handle_t handle;
  if (nvs_open("storage", NVS_READWRITE, &handle) != ESP_OK) return;

  nvs_set_i32(handle, "rack_len", (int32_t)(getRackLength() * 1000));
  nvs_set_i32(handle, "pinion_rad", (int32_t)(getPinionRadius() * 1000));

  nvs_commit(handle);
  nvs_close(handle);
}

bool loadMechanicalParams() {
  nvs_handle_t handle;
  if (nvs_open("storage", NVS_READONLY, &handle) != ESP_OK) return false;

  int32_t rack, pinion;
  if (nvs_get_i32(handle, "rack_len", &rack) == ESP_OK) {
    setRackLength(rack / 1000.0f);
  }
  if (nvs_get_i32(handle, "pinion_rad", &pinion) == ESP_OK) {
    setPinionRadius(pinion / 1000.0f);
  }

  nvs_close(handle);
  return true;
}

void storePinAssignments() {
  nvs_handle_t handle;
  if (nvs_open("storage", NVS_READWRITE, &handle) != ESP_OK) return;

  nvs_set_i32(handle, "rpm_pin", getRpmPin());
  nvs_set_i32(handle, "mode_button_pin", getModeSwitchButtonPin());
  nvs_set_i32(handle, "servo_rx_pin", SERVO_RX);
  nvs_set_i32(handle, "servo_tx_pin", SERVO_TX);
  nvs_set_i32(handle, "movement_pin", getMovementPin());
  nvs_set_i32(handle, "mark_pin", getMarkPin());
  nvs_commit(handle);
  nvs_close(handle);
}


bool loadPinAssignments() {
  nvs_handle_t handle;
  if (nvs_open("storage", NVS_READWRITE, &handle) != ESP_OK) {
    Serial.println("❌ Failed to open NVS for pin assignments.");
    return false;
  }

  int32_t val, val1;
  bool updated = false;

  // === RPM PIN ===
  if (nvs_get_i32(handle, "rpm_pin", &val) == ESP_OK) {
    setRpmPin(val);
    Serial.printf("✅ Loaded RPM pin: %d\n", val);
  } else {
    // setRpmPin(DEFAULT_RPM_PIN);  // e.g. 18
    // nvs_set_i32(handle, "rpm_pin", DEFAULT_RPM_PIN);
    Serial.println("RPM pin failure");
    updated = true;
  }

  // === MODE BUTTON ===
  if (nvs_get_i32(handle, "mode_button_pin", &val) == ESP_OK) {
    setModeSwitchButtonPin(val);
    Serial.printf("✅ Loaded Mode button pin: %d\n", val);
  } else {
    // setModeSwitchButtonPin(DEFAULT_MODE_BUTTON);
    // nvs_set_i32(handle, "mode_button_pin", DEFAULT_MODE_BUTTON);
    Serial.println("mode button pin failure");
    updated = true;
  }

  // === MOVEMENT PIN ===
  if (nvs_get_i32(handle, "movement_pin", &val) == ESP_OK) {
    setMovementPin(val);
    Serial.printf("✅ Loaded Movement pin: %d\n", val);
  } else {
    // setMovementPin(DEFAULT_MOVEMENT_PIN);  // e.g. 10
    // nvs_set_i32(handle, "movement_pin", DEFAULT_MOVEMENT_PIN);
    Serial.println("movement pin failure");
    updated = true;
  }

  // === MARK PIN ===
  if (nvs_get_i32(handle, "mark_pin", &val) == ESP_OK) {
    setMarkPin(val);
    Serial.printf("✅ Loaded Mark pin: %d\n", val);
  } else {
    // setMovementPin(DEFAULT_MOVEMENT_PIN);  // e.g. 10
    // nvs_set_i32(handle, "movement_pin", DEFAULT_MOVEMENT_PIN);
    Serial.println("mark pin failure");
    updated = true;
  }

  // === SERVO UART RX ===
  if (nvs_get_i32(handle, "servo_rx_pin", &val) == ESP_OK && 
      nvs_get_i32(handle, "servo_tx_pin", &val1) == ESP_OK ) {
        configureServoPins(val, val1);
    Serial.printf("✅ Loaded Servo RX pin: %d\n", val);
  } else {
    // setServoRxPin(DEFAULT_SERVO_RX);
    // nvs_set_i32(handle, "servo_rx_pin", DEFAULT_SERVO_RX);
    Serial.println("rx tx pin failure");
    updated = true;
  }

  

  if (updated) {
    Serial.println("💾 Some pin values were missing — default values written to NVS.");
    nvs_commit(handle);
  }

  nvs_close(handle);
  return true;
}

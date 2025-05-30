#include "../include/nvs_utils.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <Arduino.h>

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
    Serial.println("NVS init failed.");
    return;
  }

  if (!loadRanges()) {
    Serial.println("⚠️ No NVS data found. Using defaults.");
    setDefaultRanges();
    storeRanges();
  } else {
    Serial.println("✅ Loaded ranges from NVS.");
  }
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
  Serial.println("📄 Current Servo RPM Ranges:");
  for (int i = 0; i < numRanges; ++i) {
    Serial.printf("  Range %d: %d - %d\n", i+1, modeRanges[i][0], modeRanges[i][1]);
  }
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
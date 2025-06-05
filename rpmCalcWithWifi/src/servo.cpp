// ===== Default pin values =====
#define DEFAULT_SERVO_RX 2
#define DEFAULT_SERVO_TX 3

#include <Arduino.h>
#include <SCServo.h>
#include "include/rpm_data.h"
#include "include/rpm.h"
#include "include/nvs_utils.h"
#include "include/pin_utils.h"
#include "include/servo.h"



// ===== Active RX/TX configuration =====
int SERVO_RX = DEFAULT_SERVO_RX;
int SERVO_TX = DEFAULT_SERVO_TX;

const int SERVO_ID = 1;

float rackLength = 57.0; // mm
float pinionRadius = 15.5; // mm

float maxServoDegrees = 0;

SMS_STS st;
HardwareSerial servoSerial(1);;

int lastServoPos = -1;
bool servoFollowingEnabled = false;

// ===== Initialization =====

void calculateMaxServoDegrees(){
    maxServoDegrees = min((rackLength / (2 * PI * pinionRadius)) * 360.0, 360.0);
}

void servo_init_uart() {
    servoSerial.end();  // In case it was previously running
    delay(50);
    servoSerial.begin(1000000, SERIAL_8N1, SERVO_RX, SERVO_TX);
    st.pSerial = &servoSerial;
}

void servo_initialize() {
    delay(300);
    st.WritePosEx(SERVO_ID, 0, 0, 20);  // Move to 0°
    initMovementPin();
    delay(500);
    calculateMaxServoDegrees();
}

// ===== Combined boot init function =====
void servo_defaultInit() {
    SERVO_RX = DEFAULT_SERVO_RX;
    SERVO_TX = DEFAULT_SERVO_TX;
    servo_init_uart();
    servo_initialize();
    generateServoPositions(numRanges);
}

// ===== Dynamic CLI function =====
bool configureServoPins(int rx, int tx) {
    // Sanity check for valid GPIOs
    if (rx < 0 || tx < 0 || rx == tx) {
        Serial.println("❌ Invalid RX/TX pins.");
        return false;
    }

    // Stop current UART only if active
    if (servoSerial) {
        servoSerial.flush();  // Finish any pending tx
        delay(50);
        servoSerial.end();
        delay(50);
    }

    // Apply new pins
    SERVO_RX = rx;
    SERVO_TX = tx;

    // Re-initialize UART
    servoSerial.begin(1000000, SERIAL_8N1, SERVO_RX, SERVO_TX);
    st.pSerial = &servoSerial;

    // Optional: Try a no-op read to ensure communication doesn’t crash the system
    short dummy = st.ReadPos(SERVO_ID);

    // Avoid calling generateServoPositions here to prevent overwriting custom values
    // You can do it manually via CLI if needed

    return true;
}

// Set servo to specific angle in degrees (0–360°)
void setServoAngle(int degrees) {
    degrees = constrain(degrees, 0, maxServoDegrees);  // <-- Clamp to safe angle
    if (maxServoDegrees <= 0.0) {
        Serial.println("⚠️ maxServoDegrees not initialized!");
    }
    int pos = int(4096 * (degrees / 360.0));

    // Signal movement ON if angle > 0, OFF otherwise
    if (degrees > 0) {
        setMovementHigh();
    } else {
        setMovementLow();
    }

    st.WritePosEx(SERVO_ID, pos, 0, 50);
    lastServoPos = pos;
    // Serial.printf("Set Servo Angle: %d° (pos: %d)\n", degrees, pos);
}

// Get current angle (converted from pos)
int getServoAngle() {
    short pos = st.ReadPos(SERVO_ID);
    return int((pos / 4096.0) * 360.0);
}

// Sweep from 'fromDeg' to 'toDeg' in 'step' steps with 'delayMs' delay
void sweepServo(int fromDeg, int toDeg, int step, int delayMs) {
    if (fromDeg > toDeg) step = -step;
    for (int i = fromDeg; i != toDeg + step; i += step) {
        setServoAngle(i);
        delay(delayMs);
    }
}

// Full sweep (0° -> 180° -> 0°)
void servoSweepForward() {
    sweepServo(0, (int)maxServoDegrees, 5, 20);
}

void servoSweepFullCycle() {
    sweepServo(0, (int)maxServoDegrees, 5, 20);
    sweepServo((int)maxServoDegrees, 0, -5, 20);
}
// Reset servo to 0°
void resetServo() {
    setServoAngle(0);
}

int degreesToPos(float degrees) {
    return int(4096 * (degrees / 360.0));
}

// Enable servo following live RPM values
void enableServoFollow() {
    servoFollowingEnabled = true;
}

// Disable servo tracking
void disableServoFollow() {
    servoFollowingEnabled = false;
}

// Check and update servo if tracking is active
void updateServoIfFollowing() {
    if (!servoFollowingEnabled) return;

    float currentRPM = getRPMUnified();
    int modeNow = determineMode(currentRPM);
    int targetPos = modeServoPositions[modeNow - 1];

    // Signal movement ON if angle > 0, OFF otherwise
    if (targetPos > 0) {
        setMovementHigh();
    } else {
        setMovementLow();
    }

    if (targetPos != lastServoPos) {
        st.WritePosEx(SERVO_ID, targetPos, 0, 50);
        lastServoPos = targetPos;
    }
}

void generateServoPositions(int steps) {
    if (steps < 2 || steps > MAX_RANGES) return;
  
    for (int i = 0; i < steps; ++i) {
        float degrees = (i * maxServoDegrees) / (steps - 1);
        modeServoPositions[i] = int(4096 * (degrees / 360.0));  // convert degrees to servo position
    }
}

void printModeRangeMapping() {
    Serial.println("🔧 Current Mode-to-Servo Mapping:");
    for (int i = 0; i < numRanges; i++) {
        Serial.printf("  Mode %2d → RPM [%5d–%5d] → Servo Pos: %4d\n",
                      i + 1,
                      modeRanges[i][0],
                      modeRanges[i][1],
                      modeServoPositions[i]);
    }
}

void servoRPM(){
    Serial.println("Servo RPM");
    float currentRPM = getRPMUnified();
    Serial.printf("RPM = %.1f\n", currentRPM);
    Serial.printf("%s\n", getRPMSourceName());
}

void servoStatus() {
    Serial.println(F("\n============ SERVO STATUS REPORT ============\n"));
  
    // 1. Connection Details
    Serial.print(F("📌 Servo RX Pin: ")); Serial.println(SERVO_RX);
    Serial.print(F("📌 Servo TX Pin: ")); Serial.println(SERVO_TX);
    Serial.print(F("🔗 Servo ID: ")); Serial.println(SERVO_ID);
  
    // 2. Attachment and Communication Status
    Serial.print(F("🔧 Servo UART active: "));
    Serial.println(st.pSerial != nullptr ? "✅ YES" : "❌ NO");
  
    // 3. RPM Follow Status
    Serial.print(F("🔁 Servo follows RPM? "));
    Serial.println(servoFollowingEnabled ? "✅ ENABLED" : "❌ DISABLED");
  
    // 4. Mode Ranges & Servo Position Mapping
    Serial.println(F("\n🎯 RPM Ranges and Servo Positions:"));
    for (int i = 0; i < numRanges; ++i) {
      Serial.printf("  Mode %d: RPM [%d – %d] → Position: %d (%.1f°)\n",
                    i + 1,
                    modeRanges[i][0], modeRanges[i][1],
                    modeServoPositions[i],
                    360.0 * modeServoPositions[i] / 4095.0);
    }

    

    // 5. Physical Constraints
    Serial.println("\nPhysical Properties");
    Serial.printf("Pinion Radius: %f\n", pinionRadius);
    Serial.printf("Rack Length: %f\n", rackLength);
    Serial.printf("Maximum angle of the mechanism: %f\n", maxServoDegrees);
    Serial.printf("Maximum travel of the mechanism: %f\n", maxServoDegrees*pinionRadius);
  
    // 6. Current Mode and Position
    Serial.print(F("\n📡 Current Mode: "));
    float currentRPM = getRPMUnified();
    int modeIndex = determineMode(currentRPM);
    if (modeIndex != -1)
      Serial.printf("%d (RPM = %.1f)\n", modeIndex, currentRPM);
    else
      Serial.printf("❌ Out of range (RPM = %.1f)\n", currentRPM);
  
    Serial.print(F("📍 Current Servo Position: "));
    int currentPos = st.ReadPos(SERVO_ID);
    Serial.printf("%d (%.1f°)\n", currentPos, 360.0 * currentPos / 4095.0);
  
    Serial.println(F("\n=============================================\n"));
  }



float getPinionRadius() {
    return pinionRadius;
}

float getRackLength() {
    return rackLength;
}

void setPinionRadius(float newRadius) {
    pinionRadius = newRadius;
    calculateMaxServoDegrees();
}

void setRackLength(float newLength) {
    rackLength = newLength;
    calculateMaxServoDegrees();
}
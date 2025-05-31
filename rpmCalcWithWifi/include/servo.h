#ifndef SERVO_H
#define SERVO_H

#include <Arduino.h>
#include <SMS_STS.h>  
#include <SCServo.h>

extern SMS_STS st;
extern const int SERVO_ID;

extern int SERVO_RX;
extern int SERVO_TX;

extern int lastServoPos;
extern bool servoFollowingEnabled;

// ===== Initialization Functions =====

// Initializes UART communication for the servo using SERVO_RX and SERVO_TX
void servo_init_uart();

// Initializes the servo by setting it to 0° and generating servo positions
void servo_initialize();

// Combined function to initialize UART and servo using default RX/TX pins
void servo_defaultInit();

// Dynamically configure servo RX and TX pins (used by CLI)
bool configureServoPins(int rx, int tx);


// ===== Servo Movement Functions =====

// Set servo to a specific angle (0–360°)
void setServoAngle(int degrees);

// Get the current servo angle (converted from raw position)
int getServoAngle();

// Sweep from 'fromDeg' to 'toDeg' in steps of 'step' with 'delayMs' delay
void sweepServo(int fromDeg, int toDeg, int step, int delayMs);

// Perform a full sweep 0° → 360°
void servoSweepForward();

// Perform a full sweep 0° → 360° → 0°
void servoSweepFullCycle();

// Reset the servo to 0°
void resetServo();

// Calculate servo position from given degrees
int degreesToPos(float degrees);


// ===== Servo-RPM Interaction =====

// Enable automatic servo movement based on live RPM
void enableServoFollow();

// Disable servo-RPM following
void disableServoFollow();

// Called regularly in loop() to update servo position if following is enabled
void updateServoIfFollowing();

void generateServoPositions(int steps);

void printModeRangeMapping();

// Servo status
void servoStatus();

void servoRPM();



#endif  // SERVO_H
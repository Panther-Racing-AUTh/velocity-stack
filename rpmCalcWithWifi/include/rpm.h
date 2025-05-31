#ifndef RPM_H
#define RPM_H

#include <Arduino.h>

// === RPM Data Source Options ===
enum RPMSource {
    SENSOR,
    MANUAL,
    SIMULATED
};

// ===============================
// RPM Module - Header File
// ===============================
// This file declares the functions and variables used
// to measure, simulate, and interpret RPM data.
// It should be included in the main .ino file and any
// module that interacts with engine speed or mode selection.

// === External Interface ===

// Sets the current RPM source (sensor, manual input, or simulated)
void setRPMSource(RPMSource source);

// Returns the current RPM source as a string (for debug or CLI display)
const char* getRPMSourceName();

// Returns the current RPM value based on the selected source
float getRPMUnified();

// Returns the raw sensor-based RPM (only valid when using SENSOR mode)
float getCurrentRPM();

// Sets the RPM value manually (only used when source is MANUAL)
void setRPM(int manual_rpm);

// Simulates an RPM value (cycles through Moto3 values)
int simulateMoto3RPM();

// Interrupt Service Routine for sensor pulse timing
void IRAM_ATTR rpmSensorISR();

void initRpmSensorInterrupt();

int getRpmPin();

void setRpmPin(int pin);

#endif  // RPM_H
#include <Arduino.h>
#include "include/rpm_data.h"
#include "include/nvs_utils.h"
#include "include/rpm.h"
#include <pins_arduino.h>

// === Internal Timing Variables ===
volatile unsigned long t1 = 0, t2 = 0;
volatile float period = 0.0;
volatile float rpm = 0.0;
volatile bool risingEdgeDetected = false;

// === RPM Source Management ===
RPMSource currentRPMSource = SENSOR;  // Default source

int RPM_SENSOR_PIN = 18;

// === Sensor Interrupt Service Routine ===
void IRAM_ATTR rpmSensorISR() {
    static unsigned long last_rise_time = 0;
    unsigned long current_time = micros();

    if (current_time - last_rise_time > period / 2) {
        last_rise_time = current_time;

        if (risingEdgeDetected) {
            t2 = current_time;
            if (t1 < t2) {
                period = t2 - t1;
            }
            risingEdgeDetected = false;
        } else {
            t1 = current_time;
            risingEdgeDetected = true;
        }

        if (period > 0) {
            float freq = 1000000.0 / period;
            rpm = (freq * 120.0) / 36.0;  // Assuming 36 pulses per revolution
        }
    }
}

// === Source Setter ===
void setRPMSource(RPMSource source) {
    currentRPMSource = source;
}

// === Source Name (as string) ===
const char* getRPMSourceName() {
    switch (currentRPMSource) {
        case SENSOR:    return "sensor";
        case MANUAL:    return "manual";
        case SIMULATED: return "simulated";
        default:        return "unknown";
    }
}

// === Manual RPM Setter ===
void setRPM(int manual_rpm) {
    rpm = manual_rpm;
}

// === Simulated RPM Generator ===
int simulateMoto3RPM() {
    static unsigned long lastUpdate = 0;
    static int index = 0;
    unsigned long now = millis();

    if (now - lastUpdate > 100) {
        lastUpdate = now;
        index++;
        if (index >= rpmLength) {
            index = 0;  // Wrap around
        }
    }

    return rpm1[index];
}

// === Raw Sensor-Based RPM Accessor ===
float getCurrentRPM() {
    noInterrupts();
    float freq = (period > 0) ? (1000000.0 / period) : 0.0;
    float r = (freq * 120.0) / 36.0;
    interrupts();
    return r;
}

// === Unified RPM Accessor Based on Input Source ===
float getRPMUnified() {
    if (currentRPMSource == SIMULATED) {
        rpm = simulateMoto3RPM();  // overwrite directly
    }
    return rpm;
}

void initRpmSensorInterrupt() {
    pinMode(RPM_SENSOR_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(RPM_SENSOR_PIN), rpmSensorISR, RISING);
}

int getRpmPin() {
    return RPM_SENSOR_PIN;
}

void setRpmPin(int pin) {
    detachInterrupt(digitalPinToInterrupt(RPM_SENSOR_PIN));
    RPM_SENSOR_PIN = pin;
    initRpmSensorInterrupt();
}
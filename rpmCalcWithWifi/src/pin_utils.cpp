#include "../include/pin_utils.h"

void clearPin(int pin) {
    detachInterrupt(digitalPinToInterrupt(pin));
    pinMode(pin, INPUT);  // set to neutral state
    Serial.printf("🧹 Cleared pin %d: interrupts detached, mode set to INPUT\n", pin);
}
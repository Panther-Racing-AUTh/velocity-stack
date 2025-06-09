#include "../include/pin_utils.h"
#include "../include/nvs_utils.h"

int movementPin;  // Current pin (initialized to invalid)

void clearInterruptPin(int pin) {
  detachInterrupt(digitalPinToInterrupt(pin));
  pinMode(pin, INPUT);  // set to neutral state
  Serial.printf("🧹 Cleared pin %d: interrupts detached, mode set to INPUT\n", pin);
}

void initMovementPin() {
  clearInterruptPin(movementPin);       // Clean any interrupt use (safe no-op if unused)
  pinMode(movementPin, OUTPUT);         // Set correctly from the start
  digitalWrite(movementPin, LOW);       // Safe state
}

void setMovementHigh() {
  digitalWrite(movementPin, HIGH);
}

void setMovementLow() {
  digitalWrite(movementPin, LOW); 
}

int getMovementPin() {
  return movementPin;
}


int getMovementPinState() {
  return digitalRead(movementPin);  // returns HIGH or LOW
}

void setMovementPin(int pin) {
  movementPin = pin;
}

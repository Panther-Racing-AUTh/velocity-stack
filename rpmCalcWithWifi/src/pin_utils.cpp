#include "../include/pin_utils.h"

static int movementPin = 10;  // Current pin (initialized to invalid)

void clearInterruptPin(int pin) {
    detachInterrupt(digitalPinToInterrupt(pin));
    pinMode(pin, INPUT);  // set to neutral state
    Serial.printf("🧹 Cleared pin %d: interrupts detached, mode set to INPUT\n", pin);
}

void initMovementPin() {
  pinMode(movementPin, OUTPUT);
  digitalWrite(movementPin, LOW);
}

void initMovementPin(int pin) {
  pinMode(movementPin, INPUT);     // Optional: disable old pin
  digitalWrite(movementPin, LOW);  // Safety
  movementPin = pin;
  pinMode(movementPin, OUTPUT);
  digitalWrite(movementPin, LOW);    // Start LOW
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

void changeMovementPin(int newPin) {
  digitalWrite(movementPin, LOW);
  pinMode(movementPin, INPUT);  // Disable old pin
  

  movementPin = newPin;
  initMovementPin(newPin);
}

int getMovementPinState() {
  return digitalRead(movementPin);  // returns HIGH or LOW
}
#include "../include/pin_utils.h"
#include "../include/nvs_utils.h"
#include <pins_arduino.h>

int movementPin;  // Current pin (initialized to invalid)
int markPin;       // Current pin

bool markButtonPressed = false;

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
  clearInterruptPin(movementPin);
  movementPin = pin;
}

// Mark Pin commands

void initMarkPin() {
  clearInterruptPin(markPin);       // Clean any interrupt use (safe no-op if unused)
  pinMode(markPin, INPUT_PULLUP);         // Set correctly from the start
  // digitalWrite(markPin, INPUT);       // Safe state
  attachInterrupt(digitalPinToInterrupt(markPin), markPinISR, FALLING);
}

int getMarkPin() {
  return markPin;
}

void setMarkPin(int pin) {
  clearInterruptPin(markPin);
  markPin = pin;
}

volatile unsigned long lastMarkTime = 0;

void IRAM_ATTR markPinISR() {

  unsigned long now = millis();
  if (now - lastMarkTime > 200) {
    int pinState = getMovementPinState();

    markButtonPressed = true;

    lastMarkTime = now;
  }
  
}

void toggleMovementPin() {
  if (markButtonPressed) {
    int pinState = getMovementPinState();

    // Toggle
    setMovementHigh();
    delay(20);
    setMovementLow();
    delay(10);
    setMovementHigh();
    delay(20);
    setMovementLow();
    delay(10);
    setMovementHigh();
    delay(20);
    setMovementLow();


    // Restore State
    if (pinState) setMovementHigh();
    markButtonPressed = false;
  }
}
#ifndef PIN_UTILS_H
#define PIN_UTILS_H

#include <Arduino.h>

void clearInterruptPin(int pin);

void initMovementPin();

void initMovementPin(int pin);

void setMovementHigh();

void setMovementLow();

int getMovementPin();

void changeMovementPin(int newPin);

int getMovementPinState();

#endif


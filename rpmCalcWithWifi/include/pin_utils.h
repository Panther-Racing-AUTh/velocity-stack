#ifndef PIN_UTILS_H
#define PIN_UTILS_H

#include <Arduino.h>

void clearInterruptPin(int pin);

void initMovementPin();

void setMovementHigh();

void setMovementLow();

int getMovementPin();

int getMovementPinState();

void setMovementPin(int pin);

void initMarkPin();

int getMarkPin();

void setMarkPin(int pin);

void IRAM_ATTR markPinISR();

void toggleMovementPin();

#endif


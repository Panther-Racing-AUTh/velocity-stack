#pragma once

#include <Arduino.h>

// ====== Define Available States ======
enum class SystemState {
    RACE,
    DIAGNOSTICS,
    CONFIG,
    OFF,
    UNKNOWN
};

// ====== Exposed State API ======
int getModeSwitchButtonPin();
void setModeSwitchButtonPin(int pin);
void initState();
void initModeButtonInterrupt();
SystemState getCurrentState();
const char* getCurrentStateName();
bool setState(SystemState newState);
bool setStateByName(const String& name);  // CLI/Wi-Fi friendly
void IRAM_ATTR modeSwitchISR();
void changeStatus();

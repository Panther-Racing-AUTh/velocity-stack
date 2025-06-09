#include "../include/state.h"
#include "../include/servo.h"
#include "../include/rpm.h"
#include "../include/wifi_utils.h"
#include "../include/nvs_utils.h"

// Internal current state variable
static SystemState currentState = SystemState::UNKNOWN;

// Variables for mode change from button press
volatile bool waitingForSecondPress = false;
volatile bool doublePressDetected = false;
volatile unsigned long lastPressTime = 0;
unsigned long singlePressTimer = 0;
bool singlePressPending = false;

int MODE_SWITCH_PIN = 1;

// Forward declarations
void raceMode();
void diagnosticsMode();
void offMode();
bool isSafeCommand(const String& input);

int getModeSwitchButtonPin() {
    return MODE_SWITCH_PIN;
}

void setModeSwitchButtonPin(int pin) {
    MODE_SWITCH_PIN = pin;
}

// Called once at boot
void initState() {
    currentState = SystemState::CONFIG;  // default mode
    Serial.println(F("🔧 System state initialized to CONFIG"));
}

void initModeButtonInterrupt() {
    pinMode(MODE_SWITCH_PIN, INPUT_PULLUP);  // Safe for buttons, assumes active-low
    detachInterrupt(digitalPinToInterrupt(MODE_SWITCH_PIN));  // Avoid double bindings
    attachInterrupt(digitalPinToInterrupt(MODE_SWITCH_PIN), modeSwitchISR, FALLING);
}

SystemState getCurrentState() {
    return currentState;
}

const char* getCurrentStateName() {
    switch (currentState) {
        case SystemState::RACE: return "race";
        case SystemState::DIAGNOSTICS: return "diagnostics";
        case SystemState::CONFIG: return "config";
        case SystemState::OFF: return "off";
        default: return "unknown";
    }
}

bool setState(SystemState newState) {
    if (newState == currentState) return false;

    // Optional: exit logic for old state
    Serial.printf("🚦 Switching state: %s → %s\n", getCurrentStateName(),
                  (newState == SystemState::RACE) ? "race" :
                  (newState == SystemState::DIAGNOSTICS) ? "diagnostics" :
                  (newState == SystemState::CONFIG) ? "config" : 
                  (newState == SystemState::OFF) ? "off" : "unknown");

    currentState = newState;
    if (newState == SystemState::RACE) {
        raceMode();
    } else if (newState == SystemState::DIAGNOSTICS) {
        diagnosticsMode();
    } else if (newState == SystemState::CONFIG) {
        offMode();
    } else if (newState == SystemState::OFF) {
        offMode();
    }

    // Optional: enter logic for new state
    // e.g., resetServo(), stopWiFi(), enableDiagnostics()

    return true;
}

bool setStateByName(const String& name) {
    if (name == "race") return setState(SystemState::RACE);
    if (name == "diagnostics") return setState(SystemState::DIAGNOSTICS);
    if (name == "config") return setState(SystemState::CONFIG);
    if (name == "off") return setState(SystemState::OFF);
    return false;
}

void IRAM_ATTR modeSwitchISR() {
    static unsigned long lastTrigger = 0;
    unsigned long now = millis();
  
    if (now - lastTrigger < 150) return;  // Debounce
    lastTrigger = now;
  
    if (waitingForSecondPress && (now - lastPressTime < 500)) {
      doublePressDetected = true;
      waitingForSecondPress = false;
    } else {
      waitingForSecondPress = true;
      lastPressTime = now;
      singlePressTimer = now;
      singlePressPending = true;
    }
}

void changeStatus(){
    if (doublePressDetected) {
        doublePressDetected = false;
        singlePressPending = false;
        waitingForSecondPress = false;

        if (currentState == SystemState::DIAGNOSTICS) {
            setState(SystemState::RACE);
            Serial.println("Switched to RACE mode");
        } else {
            setState(SystemState::DIAGNOSTICS);
            Serial.println("Switched to DIAGNOSTICS mode");
        }

        return;  // Done this loop
    }

    // Handle single press after timeout
    if (singlePressPending && millis() - singlePressTimer > 1000) {
        singlePressPending = false;
        waitingForSecondPress = false;

        if (currentState == SystemState::OFF) {
            setState(SystemState::DIAGNOSTICS);
            Serial.println("Turned ON: DIAGNOSTICS mode");
        } else {
            setState(SystemState::OFF);
            Serial.printf("Turned OFF\n");
        }
    }
}

void raceMode() {
    enableServoFollow();         // Servo follows live RPM
    setRPMSource(SENSOR);        // Real RPM input
    disableWiFi();               // No config access during race
    Serial.println("🏁 Race mode activated: live RPM, servo follow, Wi-Fi disabled");
}

void diagnosticsMode() {
    disableServoFollow();       // Manual servo control
    setRPMSource(MANUAL);       // Use test values
    enableWiFi();               // Allow access via Wi-Fi
    Serial.println("🧪 Diagnostics mode: manual RPM, Wi-Fi enabled, manual servo");
}

void offMode() {
    disableServoFollow();       // Turn off any movement
    setRPMSource(MANUAL);       // Safe fallback
    disableWiFi();              // Shut off comms
    Serial.println("🛑 OFF mode: all systems disabled");
}

#ifndef CLI_H
#define CLI_H

#include <Arduino.h>

// ===========================
// Command Line Interface (CLI)
// ===========================
// This header defines the interface for the serial-based CLI,
// allowing runtime interaction with the ESP32 for diagnostics,
// mode switching, testing, and debugging.

// === CLI Entry Point ===
// Call this function inside the main loop() to check for input and execute commands
void handleCLI();

// === Help Command ===
// Displays a list of available commands and usage instructions
void printMenu();

#endif  // CLI_H
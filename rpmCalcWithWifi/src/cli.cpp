#include <Arduino.h>
#include "../include/cli.h"
#include "../include/rpm_data.h"
#include "../include/nvs_utils.h"
#include "../include/rpm.h"
#include "../include/servo.h"
#include "../include/wifi_utils.h"
#include "../include/state.h"
#include "../include/pin_utils.h"
#include <WiFi.h>


String SYSTEM_VERSION = "1.1.1";

extern int STATUS_BUTTON;

void printMenu() {
    Serial.println(F("\n============= 📜 COMMAND MENU ============="));

    Serial.println(F("\n🔧 NVS COMMANDS"));
    Serial.println(F("  nvs store <thresholds>     – Store servo RPM thresholds"));
    Serial.println(F("  nvs list                   – List ALL stored values in storage"));
    Serial.println(F("  nvs reset                  – Reset and restore default ranges"));
    Serial.println(F("  nvs map                    – Show range → servo mapping"));

    Serial.println(F("\n📈 RPM COMMANDS"));
    Serial.println(F("  rpm read                   – Show current RPM and source"));
    Serial.println(F("  rpm set <value>            – Manually set RPM"));
    Serial.println(F("  rpm source <sensor|sim|manual> – Change RPM input source"));
    Serial.println(F("  rpm live                   – Live RPM updates (type 'exit' to leave)"));
    Serial.println(F("  rpm pin get                – Show RPM sensor pin"));
    Serial.println(F("  rpm pin set <pin>          – Set RPM sensor pin"));

    Serial.println(F("\n🦾 SERVO COMMANDS"));
    Serial.println(F("  servo set <angle>          – Set servo to angle (0–360°)"));
    Serial.println(F("  servo get                  – Get current servo angle"));
    Serial.println(F("  servo sweep <f> <t> <s> <d> – Sweep servo (from–to, step, delay)"));
    Serial.println(F("  servo full                 – 0→360° sweep (no return)"));
    Serial.println(F("  servo fullcircle           – 0→360°→0° sweep"));
    Serial.println(F("  servo reset                – Reset servo to 0°"));
    Serial.println(F("  servo follow               – Enable servo RPM following"));
    Serial.println(F("  servo unfollow             – Disable servo tracking"));
    Serial.println(F("  servo init default         – Init servo UART with default pins"));
    Serial.println(F("  servo init <rx> <tx>       – Init servo with custom UART pins"));
    Serial.println(F("  servo positions set        – Manually set servo positions per range"));
    Serial.println(F("  servo map                  – Show active RPM→position mapping"));
    Serial.println(F("  servo rpm                  – Print position for current RPM"));
    Serial.println(F("  servo status               – Print servo config and state"));

    Serial.println(F("\n⚙️ MECHANICAL CONFIGURATION"));
    Serial.println(F("  rack get                   – Show current rack length (mm)"));
    Serial.println(F("  rack set <length_mm>       – Set rack length in mm"));
    Serial.println(F("  pinion get                 – Show current pinion radius (mm)"));
    Serial.println(F("  pinion set <radius_mm>     – Set pinion gear radius in mm"));

    Serial.println(F("\n📶 WIFI COMMANDS"));
    Serial.println(F("  wifi enable                – Start Wi-Fi in AP mode"));
    Serial.println(F("  wifi disable               – Disable Wi-Fi"));
    Serial.println(F("  wifi ip                    – Show AP IP address"));
    Serial.println(F("  wifi txpower               – Show TX power"));
    Serial.println(F("  wifi txpower <value>       – Set TX power in dBm"));
    Serial.println(F("  wifi mac                   – Print MAC address"));
    Serial.println(F("  wifi clients               – Show number of connected clients"));
    Serial.println(F("  wifi status                – Show full Wi-Fi status"));

    Serial.println(F("\n🧭 STATE COMMANDS"));
    Serial.println(F("  state get                  – Show current system state"));
    Serial.println(F("  state set <race|diagnostics|config> – Change system state"));
    Serial.println(F("  state pin get              – Get button pin used for state switching"));
    Serial.println(F("  state pin set <pin>        – Set button pin and reattach interrupt"));
    Serial.println(F("  state list                 – Show all valid system states"));

    Serial.println(F("\n📌 PIN COMMANDS"));
    Serial.println(F("  movement pin get           – Show current movement pin"));
    Serial.println(F("  movement pin set <pin>     – Change movement pin and reinitialize"));
    Serial.println(F("  movement pin read          – Read logic level of movement pin"));
    Serial.println(F("  pin clear <pin>            – Detach interrupts and disable pin"));
    Serial.println(F("  pin status                 – Display all GPIO pin assignments"));

    Serial.println(F("\n🧰 MISC COMMANDS"));
    Serial.println(F("  status                     – Full system status report"));
    Serial.println(F("  version                    – Show system version"));
    Serial.println(F("  pinout                     – Show pinout of board"));
    Serial.println(F("  help                       – Show this command menu"));

    Serial.println(F("\n===========================================\n"));
}

void printPinout() {
    Serial.println("|---------------------------------------------|");
    Serial.println("|             ESP32-C3-WROOM-02               |");
    Serial.println("|                                             |");
    Serial.println("|  GND        |---------------|       GND     |");
    Serial.println("|  3V3        |               |       GPIO0   |");
    Serial.println("|  3V3        |               |       GPIO1   |");
    Serial.println("|  RST        |               |       GPIO2   |");
    Serial.println("|  GND        |               |       GPIO3   |");
    Serial.println("|  GPIO4      |               |       GND     |");
    Serial.println("|  GPIO5      |               |       GPIO10  |");
    Serial.println("|  GPIO6      |               |       GND     |");
    Serial.println("|  GPIO7      |               |       GPIO20  |");
    Serial.println("|  GND        |               |       GPIO21  |");
    Serial.println("|  GPIO8      |               |       GND     |");
    Serial.println("|  GPIO9      |               |       GPIO18  |");
    Serial.println("|  5V         |               |       GPIO19  |");
    Serial.println("|  5V         |               |       GND     |");
    Serial.println("|  GND        |---------------|       GND     |");
    Serial.println("|---------------------------------------------|");
}


bool isSafeCommand(const String& input) {
    return input.startsWith("status") || input.startsWith("state");
}

void handleCLI() {
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();  // Remove whitespace

        // Only allow full CLI in DIAGNOSTICS mode
        if (getCurrentState() != SystemState::CONFIG && !isSafeCommand(input)) {
            Serial.println("⚠️ CLI is restricted. Switch to CONFIG mode to access full commands.");
            return;
        }
        
        // ================ NVS COMMANDS ===================
        
        if (input.startsWith("nvs store")) {
            input.trim();
            
            // Prepare a safe mutable buffer
            char buf[128];
            input.toCharArray(buf, sizeof(buf));
            char *token = strtok(buf, " ");
        
            int tokenCount = 0;
            std::vector<int> thresholds;
        
            while (token != nullptr) {
                if (tokenCount >= 2) {
                    int val = atoi(token);
                    if (val < 0 || val > 14500) {
                        Serial.printf("❌ Invalid threshold '%d'. Must be between 0 and 14500.\n", val);
                        return;
                    }
                    thresholds.push_back(val);
                }
                token = strtok(nullptr, " ");
                tokenCount++;
            }
        
            if (thresholds.size() < 1) {
                Serial.println("⚠️ No thresholds provided. Usage: nvs store 3000 6000 9000");
                return;
            }
        
            // Sort and validate
            std::sort(thresholds.begin(), thresholds.end());
            for (size_t i = 1; i < thresholds.size(); ++i) {
                if (thresholds[i] <= thresholds[i - 1]) {
                    Serial.println("❌ Thresholds must be strictly increasing.");
                    return;
                }
            }
        
            // Ensure 0 and 14500 are included
            if (thresholds.front() != 0) thresholds.insert(thresholds.begin(), 0);
            if (thresholds.back() != 14500) thresholds.push_back(14500);
        
            if (thresholds.size() < 3) {
                Serial.println("❌ At least three thresholds required (including 0 and 14500).");
                return;
            }
        
            updateAndStoreRanges(thresholds);
            generateServoPositions(numRanges);
        }
        
        else if (input == "nvs list") {
            Serial.println("📄 Current active servo RPM ranges:");
            listNVSContents();  // Just prints modeRanges[]
        }
        
        else if (input == "nvs reset") {
            Serial.println("♻️ Resetting NVS and restoring defaults...");
            eraseNVS();  // Erases + reinitializes with setDefaultRanges()
        }

        else if (input == "nvs map") {
            printModeRangeMapping();
        }

        // ================= RPM COMMANDS =================
        else if (input.startsWith("rpm set ")) {
            String valStr = input.substring(8);
            valStr.trim();
            if (valStr.length() == 0 || !valStr[0] || !valStr[0] >= '0') {
                Serial.println("⚠️ Invalid RPM value. Use: rpm set <positive_integer>");
            } else {
                int value = valStr.toInt();
                if (value >= 0) {
                    setRPM(value);
                    setRPMSource(MANUAL);
                    Serial.printf("✅ Manual RPM set to %d\n", value);
                } else {
                    Serial.println("⚠️ RPM must be non-negative.");
                }
            }
        }
        
        else if (input == "rpm read") {
            Serial.printf("📈 Current RPM (%s): %.2f\n", getRPMSourceName(), getRPMUnified());
        }
        
        else if (input.startsWith("rpm source ")) {
            String source = input.substring(11);
            source.trim();
        
            if (source == "sensor") {
                setRPMSource(SENSOR);
                Serial.println("📡 RPM source set to SENSOR (real input)");
            } else if (source == "sim") {
                setRPMSource(SIMULATED);
                Serial.println("🧪 RPM source set to SIMULATED (test values)");
            } else if (source == "manual") {
                setRPMSource(MANUAL);
                Serial.println("✍️ RPM source set to MANUAL (user-defined)");
            } else {
                Serial.println("⚠️ Unknown RPM source. Use one of: sensor, sim, manual");
            }
        }

        else if (input == "rpm live") {
            Serial.println("🔁 Entering live RPM mode. Type 'exit' to stop.");
        
            while (true) {
                // Check for user input to exit
                if (Serial.available()) {
                    String exitCommand = Serial.readStringUntil('\n');
                    exitCommand.trim();
                    if (exitCommand == "exit") {
                        Serial.println("❎ Exiting live RPM mode.");
                        break;
                    }
                }
        
                // Print current RPM with source
                float currentRPM = getRPMUnified();
                Serial.printf("📈 RPM (%s): %.2f\n", getRPMSourceName(), currentRPM);
                updateServoIfFollowing();
                delay(200);  // Delay between updates to avoid flooding the output
            }
        }
       
        else if (input.startsWith("rpm pin")) {
            if (input == "rpm pin get") {
                Serial.printf("📍 RPM sensor pin: %d\n", getRpmPin());
            } else if (input.startsWith("rpm pin set")) {
                int pin = input.substring(12).toInt();  // skip "rpm pin set "
                setRpmPin(pin);
                storePinAssignments();
                Serial.printf("✅ RPM sensor pin set to %d\n", pin);
            } else {
                Serial.println("❌ Usage: 'rpm pin get' or 'rpm pin set <pin>'");
            }
        }

        // =============== SERVO COMMANDS ================
        else if (input.startsWith("servo set ")) {
            int angle = input.substring(10).toInt();
            if (angle < 0 || angle > 360) {
                Serial.println("❌ Invalid angle. Use 0–360°");
            } else {
                disableServoFollow();
                setServoAngle(angle);
                Serial.printf("🎯 Servo angle set to %d°\n", angle);
            }
        }
        
        else if (input == "servo get") {
            int angle = getServoAngle();
            Serial.printf("📏 Current servo angle: %d°\n", angle);
        }
        
        else if (input.startsWith("servo sweep ")) {
            int f, t, s, d;
            int matched = sscanf(input.c_str(), "servo sweep %d %d %d %d", &f, &t, &s, &d);
            if (matched == 4 && s != 0 && d >= 0 && f >= 0 && f <= 360 && t >= 0 && t <= 360) {
                disableServoFollow();
                sweepServo(f, t, s, d);
                Serial.println("🔄 Sweep complete.");
            } else {
                Serial.println("❌ Usage: servo sweep <from> <to> <step> <delay_ms> (0–360°)");
            }
        }
        
        else if (input == "servo fullcircle") {
            disableServoFollow();
            servoSweepFullCycle();
            Serial.println("🌀 Servo did full 0°→360°→0° sweep");
        }

        else if (input == "servo full") {
            disableServoFollow();
            servoSweepForward();
            Serial.println("🌀 Servo did a 0°→360° sweep (no return)");
        }
        
        else if (input == "servo reset") {
            disableServoFollow();
            resetServo();
            Serial.println("↩️ Servo reset to 0°");
        }
        
        else if (input == "servo follow") {
            enableServoFollow();
            Serial.println("📡 Servo will now follow RPM live.");
        }
        
        else if (input == "servo unfollow") {
            disableServoFollow();
            Serial.println("🛑 Servo tracking disabled. Manual control resumed.");
        }
        
        else if (input == "servo init default") {
            servo_defaultInit();
            Serial.println("🔧 Servo and UART initialized with default pins.");
        }
        
        else if (input.startsWith("servo init ")) {
            int rx, tx;
            int matched = sscanf(input.c_str(), "servo init %d %d", &rx, &tx);
            if (matched == 2 && rx >= 0 && tx >= 0) {
                configureServoPins(rx, tx);
                storePinAssignments();
                Serial.printf("🛠️ Servo initialized with RX=%d TX=%d\n", rx, tx);
            } else {
                Serial.println("❌ Usage: servo init <rx> <tx> (non-negative values only)");
            }
        }
        
        else if(input == "servo status"){
            servoStatus();
        }

        else if(input == "servo positions set"){
            Serial.println("🔧 Interactive Servo Position Setup");
            for (int i = 0; i < numRanges; ++i) {
                int pos = -1;
                while (true) {
                    Serial.printf("Enter servo position for RPM range [%d – %d]: ", 
                                modeRanges[i][0], modeRanges[i][1]);
                    while (!Serial.available()) delay(10);
                    
                    String input = Serial.readStringUntil('\n');
                    input.trim();
                    pos = input.toInt();
                    if (pos >= 0 && pos <= 4095) break;

                    Serial.println("❌ Invalid input. Must be between 0 and 4095.");
                }
                modeServoPositions[i] = pos;
            }
            storeServoPositions();
            Serial.println("✅ Custom servo positions saved:");
            for (int i = 0; i < numRanges; ++i) {
                Serial.printf("Range %d [%d – %d] => Servo Position: %d\n",
                            i + 1, modeRanges[i][0], modeRanges[i][1], modeServoPositions[i]);
            }
        }
        
        else if (input == "servo map") {
            printModeRangeMapping();
        }

        else if (input == "servo rpm"){
            servoRPM();
        }
        
        else if (input == "pinion get"){
            Serial.printf("Current pinion radius: %f mm\n", getPinionRadius());
        }

        else if (input == "rack get"){
            Serial.printf("Current rack length: %f mm\n", getRackLength());
        }

        else if (input.startsWith("rack set ")) {
            float len = input.substring(9).toFloat();  // "rack set " is 9 chars
            setRackLength(len);
            Serial.printf("✅ Rack length set to %.2f mm\n", len);
        }
        
        else if (input.startsWith("pinion set ")) {
            float rad = input.substring(11).toFloat();  // "pinion set " is 11 chars
            setPinionRadius(rad);
            Serial.printf("✅ Pinion radius set to %.2f mm\n", rad);
        }

        

        // ============= WIFI COMMANDS ===================
        else if (input == "wifi enable") {
            enableWiFi();
        }
          
        else if (input == "wifi disable") {
        disableWiFi();
        }
          
        else if (input == "wifi ip") {
        Serial.print("📡 Access Point IP: ");
        Serial.println(espAPIP);
        }

        else if (input.startsWith("wifi txpower")) {
            if (input == "wifi txpower") {
                showTxPower();
            } else {
                float value = input.substring(13).toFloat();  // skip "wifi txpower "
                setTxPower(value);
            }
        }

        else if (input == "wifi clients") {
            listConnectedClients();
        }

        else if (input == "wifi mac") {
            printWifiMac();
        }

        else if (input == "wifi status") {
            Serial.println(F("📶 Wi-Fi Status Report"));
        
            wifi_mode_t mode = WiFi.getMode();
            int clientCount = WiFi.softAPgetStationNum();
            float txPower = WiFi.getTxPower();
            String mac = WiFi.softAPmacAddress();
        
            Serial.printf("  MAC Address: %s\n", mac.c_str());
            showTxPower();
        
            if (mode == WIFI_MODE_AP) {
                Serial.println(F("  Mode: Access Point (AP)"));
                Serial.print(F("  SSID: ")); Serial.println(ssid);
                Serial.print(F("  Password: ")); Serial.println(password);
                Serial.print(F("  IP Address: ")); Serial.println(espAPIP);
                Serial.print(F("  Clients connected: ")); Serial.println(clientCount);
            } else if (mode == WIFI_MODE_STA) {
                Serial.println(F("  Mode: Station (STA)"));
                Serial.print(F("  Connected to: ")); Serial.println(WiFi.SSID());
                Serial.print(F("  IP Address: ")); Serial.println(WiFi.localIP());
                Serial.print(F("  RSSI: ")); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
            } else {
                Serial.println(F("  Status: ❌ Wi-Fi is OFF"));
            }
        
            // Optional: show time since enabled
            static unsigned long wifiStartMillis = millis();
            unsigned long uptimeSec = (millis() - wifiStartMillis) / 1000;
            Serial.printf("  Wi-Fi Uptime: %lu seconds\n", uptimeSec);
        }

        // ============= STATE COMMANDS ===================
        else if (input == "state get") {
            Serial.printf("📟 Current system state: %s\n", getCurrentStateName());
        }
        
        else if (input.startsWith("state set ")) {
            String stateName = input.substring(10);  // skip "state set "
            if (setStateByName(stateName)) {
                Serial.printf("✅ State switched to: %s\n", getCurrentStateName());
            } else {
                Serial.println("❌ Invalid state name. Use: race, diagnostics, or config.");
            }
        }
        
        else if (input == "state pin get") {
            Serial.printf("📍 Current button pin: GPIO %d\n", getModeSwitchButtonPin());
        }
        
        else if (input.startsWith("state pin set ")) {
            int pin = input.substring(14).toInt();  // skip "state pin set "
            if (pin <= 0 || pin >= 40) {
                Serial.println("❌ Invalid GPIO number.");
            } else {
                setModeSwitchButtonPin(pin);
                storePinAssignments();
                initModeButtonInterrupt();  // Reattach interrupt to the new pin
                Serial.printf("📍 Button pin set to GPIO %d and interrupt attached.\n", pin);
            }
        }

        else if (input == "state list") {
            Serial.println("🧭 Valid system modes:");
            Serial.println("  - race");
            Serial.println("  - diagnostics");
            Serial.println("  - config");
            Serial.println("  - off");
        }
       
        // ============== PIN COMMANDS ===================  

        else if (input == "movement pin get") {
            Serial.printf("📍 Current movement pin: GPIO %d\n", getMovementPin());
        }

        else if (input.startsWith("movement pin set ")) {
            int pin = input.substring(17).toInt();  // skip "state pin set "
            if (pin <= 0 || pin >= 40) {
                Serial.println("❌ Invalid GPIO number.");
            } else {
                setMovementPin(pin);
                storePinAssignments();
                initMovementPin();                
                Serial.printf("📍 Movement pin set to GPIO %d and initialized.\n", pin);
            }
        }

        else if (input == "movement pin read") {
            Serial.printf("Current button pin: GPIO %d\n", getMovementPinState());
        }

        else if (input.startsWith("pin clear ")) {
            int pin = input.substring(10).toInt();  // skip "pin clear "
            clearInterruptPin(pin);
        }

        else if (input == "pin status") {
            Serial.println(F("\n========= 📊 PIN STATUS =========\n"));

            // RPM interrupt pin
            Serial.print(F("Interrupt pin for RPM read: "));
            Serial.println(getRpmPin());
            
            // Button pin for changing states
            Serial.print(F("Button pin for changing states: "));
            Serial.println(getModeSwitchButtonPin());

            // Button pin for viewing system status
            Serial.printf("Button pin for viewing system status: %d\n", STATUS_BUTTON);
            
            // Rx/Tx pins used for servo communication
            Serial.printf("Rx/Tx pins used for servo communication: RX: %d, TX: %d\n", SERVO_RX, SERVO_TX);
            
            // Pin for servo position
            Serial.print(F("Pin for servo position: "));
            Serial.println(getMovementPin());

            // === COMPLETION ===
            Serial.println(F("\n===========================================\n"));
        }

        // ============== MISC COMMANDS ===================     
        
        else if (input == "status") {
            Serial.println(F("\n========= 📊 SYSTEM STATUS REPORT =========\n"));
        
            // === SYSTEM MODE ===
            Serial.print(F("🧭 Mode: "));
            Serial.println(getCurrentStateName());
        
            // === BUTTON ===
            Serial.print(F("🔘 Mode Switch Pin: GPIO "));
            Serial.println(getModeSwitchButtonPin());
        
            // === RPM ===
            Serial.println(F("\n📈 RPM Subsystem"));
            Serial.printf("  Source: %s\n", getRPMSourceName());
            Serial.printf("  Value: %.2f RPM\n", getRPMUnified());
            Serial.printf("  Sensor Pin: GPIO %d\n", getRpmPin());
        
            // === SERVO ===
            Serial.println(F("\n🦾 Servo Subsystem"));
            Serial.printf("  RX Pin: GPIO %d\n", SERVO_RX);
            Serial.printf("  TX Pin: GPIO %d\n", SERVO_TX);
            Serial.printf("  Following RPM: %s\n", servoFollowingEnabled ? "✅ YES" : "❌ NO");
            Serial.printf("  Current Angle: %d°\n", getServoAngle());
            Serial.println("  Range → Position Mapping:");
            for (int i = 0; i < numRanges; ++i) {
                Serial.printf("    [%5d – %5d] RPM → Pos: %4d\n",
                              modeRanges[i][0], modeRanges[i][1], modeServoPositions[i]);
            }
        
            // === WIFI ===
            Serial.println(F("\n📡 Wi-Fi Subsystem"));
        
            wifi_mode_t mode = WiFi.getMode();
            int clientCount = WiFi.softAPgetStationNum();
            float txPower = WiFi.getTxPower();
            String mac = WiFi.softAPmacAddress();
        
            Serial.printf("  MAC Address: %s\n", mac.c_str());
            showTxPower();
        
            if (mode == WIFI_MODE_AP) {
                Serial.println(F("  Mode: Access Point (AP)"));
                Serial.print(F("  SSID: ")); Serial.println(ssid);
                Serial.print(F("  Password: ")); Serial.println(password);
                Serial.print(F("  IP Address: ")); Serial.println(espAPIP);
                Serial.print(F("  Clients Connected: ")); Serial.println(clientCount);
            } else if (mode == WIFI_MODE_STA) {
                Serial.println(F("  Mode: Station (STA)"));
                Serial.print(F("  Connected to: ")); Serial.println(WiFi.SSID());
                Serial.print(F("  IP Address: ")); Serial.println(WiFi.localIP());
                Serial.print(F("  RSSI: ")); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
            } else {
                Serial.println(F("  Status: ❌ OFF"));
            }
        
            // === COMPLETION ===
            Serial.println(F("\n===========================================\n"));
        }

       else if (input == "version") {
        Serial.print("Current System Version: ");
        Serial.println(SYSTEM_VERSION);
       } 

        else if (input == "help") {
            printMenu();
        }

        else if (input == "pinout"){
            printPinout();
        }

        else {
            Serial.println("❌ Unknown command. Type 'help' to see available commands.");
        }
    }
}
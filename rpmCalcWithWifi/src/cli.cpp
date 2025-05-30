#include <Arduino.h>
#include "../include/cli.h"
#include "../include/rpm_data.h"
#include "../include/nvs_utils.h"
#include "../include/rpm.h"
#include "../include/servo.h"
#include "../include/wifi_utils.h"
#include <WiFi.h>


// variables from rpmCalcWithWifi.ino
extern String status;
extern bool servoFollowingRPM;


void printMenu() {
    Serial.println("\n========= 🛠️ Command Menu ===================");

    Serial.println("\n📦 SYSTEM COMMANDS");
    Serial.println("  mode set <value>         - Set system mode (rpm, wifi, diagnostics, off)");
    Serial.println("  mode list                - List all valid modes");

    Serial.println("\n🔁 RPM COMMANDS");
    Serial.println("  rpm set <value>          - Set a specific RPM value");
    Serial.println("  rpm read                 - Show the current RPM");
    Serial.println("  rpm source sensor        - Use RPM from live sensor input");
    Serial.println("  rpm source sim           - Use simulated RPM values");
    Serial.println("  rpm source manual        - Use manually defined RPM");
    Serial.println("  rpm live                 - Live monitor RPM (exit with 'exit')");

    Serial.println("\n⚙️ SERVO COMMANDS");
    Serial.println("  servo status           → Show servo status and configuration"); 
    Serial.println("  servo set <angle>        - Set the servo to a specific angle (0–360°)");
    Serial.println("  servo get                - Show the current servo angle");
    Serial.println("  servo sweep <f> <t> <s> <d> - Sweep from angle f to t in steps s with delay d (ms)");
    Serial.println("  servo fullcircle         - Full sweep 0° → 360° → 0°");
    Serial.println("  servo full                - One-way 0° → 360° sweep (no return)");
    Serial.println("  servo reset              - Move servo to 0°");
    Serial.println("  servo init default       - Initialize UART and servo with default pins");
    Serial.println("  servo init <rx> <tx>     - Dynamically assign UART pins to servo");
    Serial.println("  servo follow             - Make servo follow live RPM values");
    Serial.println("  servo unfollow           - Stop servo from following RPM");
    Serial.println("  servo map                - Show current RPM ranges and mapped servo positions");

    Serial.println("\n💾 NVS COMMANDS");
    Serial.println("  nvs store <val> <val>... - Set new RPM thresholds (0–14500)");
    Serial.println("                             Requires min 3 ascending values.");
    Serial.println("                             0 and 14500 will be added if missing.");
    Serial.println("  nvs list                 - Show current RPM ranges");
    Serial.println("  nvs reset                - Erase NVS and restore default ranges");
    Serial.println("  nvs map                  - Show mapping from RPM ranges to servo positions");

    Serial.println("\n📡 WIFI COMMANDS");
    Serial.println("  wifi enable              - Turn Wi-Fi access point ON");
    Serial.println("  wifi disable             - Turn Wi-Fi OFF");
    Serial.println("  wifi status              - Show current Wi-Fi mode and stats");
    Serial.println("  wifi ip                  - Show the ESP32 IP address");
    Serial.println("  wifi mac                 - Show the ESP32 Mac address");
    Serial.println("  wifi clients             - List connected devices with IP/MAC");
    Serial.println("  wifi txpower             - Show current TX power in dBm");
    Serial.println("  wifi txpower <val>       - Set Wi-Fi TX power (8.5–20.5 dBm)");

    Serial.println("\n🆘 OTHER");
    Serial.println("  help                     - Show this menu again");
    Serial.println("  status                   - Show current system status");

    Serial.println("=============================================\n");
}


void handleCLI() {
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();  // Remove whitespace

        // =============== SYSTEM COMMANDS ===============
        
            if (input.startsWith("mode set ")) {
                String mode = input.substring(9); // after "mode set "
                mode.toLowerCase();
            
                if (mode == "race" || mode == "wifi" || mode == "diagnostics" || mode == "off") {
                    status = mode;
                    Serial.print("✅ System mode set to: ");
                    Serial.println(status);
                } else {
                    Serial.print("❌ Invalid mode: ");
                    Serial.println(status);
                    Serial.println("Use 'mode list' to view valid modes.");
                }
            }
            
            else if (input == "mode list") {
                Serial.println("🧭 Valid system modes:");
                Serial.println("  - rpm");
                Serial.println("  - wifi");
                Serial.println("  - diagnostics");
                Serial.println("  - off");
            }
            
        
        // ================ NVS COMMANDS ===================
        
        else if (input.startsWith("nvs store")) {
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
                delay(200);  // Delay between updates to avoid flooding the output
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
        
        else if (input == "wifi status") {
            Serial.println("📶 Wi-Fi Status Report");
            
            if (WiFi.getMode() == WIFI_MODE_AP && WiFi.softAPgetStationNum() >= 0) {
                Serial.println("  Mode: Access Point (AP)");
                Serial.print("  SSID: ");
                Serial.println(ssid);
                Serial.print("  Password: ");
                Serial.println(password);
                Serial.print("  IP Address: ");
                Serial.println(espAPIP);
                Serial.print("  Clients connected: ");
                Serial.println(WiFi.softAPgetStationNum());
            } else {
                Serial.println("  Status: ❌ Wi-Fi is OFF");
            }
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

        // ============== MISC COMMANDS ===================     
        
        else if(input == "status"){
            Serial.print("Current mode: ");
            Serial.println(status);
        }

        else if (input == "help") {
            printMenu();
        }

        else {
            Serial.println("❌ Unknown command. Type 'help' to see available commands.");
        }
    }
}
/**
 * ESP32-C5 UART Test Code for Bruce Firmware
 *
 * Add this code to your Bruce firmware's interface.cpp to test
 * basic UART communication with Flipper Zero.
 */

// ============================================================
// TEST 1: Basic Serial Output Test
// ============================================================
// Add to _setup_gpio() function in interface.cpp

void _setup_gpio() {
    // ... existing GPIO setup code ...

    // Initialize Serial for testing
    Serial.begin(115200);
    delay(100);  // Wait for serial to stabilize

    Serial.println("\n========================================");
    Serial.println("ESP32-C5 UART TEST - Bruce Firmware");
    Serial.println("========================================");
    Serial.println("If you see this on Flipper, RX works!");
    Serial.println("Baud rate: 115200");
    Serial.println("Data bits: 8, Parity: None, Stop bits: 1");
    Serial.println("========================================\n");

    // Simple heartbeat for testing
    Serial.println("[TEST] Initialization complete");
}

// ============================================================
// TEST 2: Continuous Heartbeat Test
// ============================================================
// Add to loop() or create a FreeRTOS task

void uart_test_heartbeat() {
    static unsigned long lastHeartbeat = 0;
    static int counter = 0;

    if (millis() - lastHeartbeat >= 1000) {
        lastHeartbeat = millis();
        counter++;

        // Send plain text
        Serial.print("HEARTBEAT #");
        Serial.print(counter);
        Serial.print(" - Time: ");
        Serial.print(millis());
        Serial.println(" ms");

        // Send test pattern (binary)
        Serial.write(0xAA);  // Header
        Serial.write(0x55);  // Test byte
        Serial.write(counter & 0xFF);
        Serial.println();
    }
}

// ============================================================
// TEST 3: UART Loopback Test (Echo Mode)
// ============================================================
// Tests RX by echoing back everything received

void uart_test_loopback() {
    while (Serial.available() > 0) {
        char c = Serial.read();

        // Echo back with prefix
        Serial.print("[ECHO] Received: '");
        Serial.print(c);
        Serial.print("' (0x");
        Serial.print(c, HEX);
        Serial.println(")");

        // If Flipper sends button commands
        if (c == 'U') Serial.println("  -> UP button detected!");
        if (c == 'D') Serial.println("  -> DOWN button detected!");
        if (c == 'S') Serial.println("  -> SELECT button detected!");
        if (c == 'E') Serial.println("  -> ESCAPE button detected!");
        if (c == 'L') Serial.println("  -> LEFT button detected!");
        if (c == 'R') Serial.println("  -> RIGHT button detected!");
    }
}

// ============================================================
// TEST 4: Binary Pattern Test
// ============================================================
// Tests binary data transmission integrity

void uart_test_binary_pattern() {
    static unsigned long lastTest = 0;

    if (millis() - lastTest >= 2000) {
        lastTest = millis();

        Serial.println("[BINARY TEST] Sending test pattern...");

        // Send incrementing pattern
        uint8_t pattern[] = {
            0xAA,  // Header
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
            0xFF   // Tail
        };

        Serial.write(pattern, sizeof(pattern));
        Serial.println("\n[BINARY TEST] Pattern sent!");
    }
}

// ============================================================
// TEST 5: Complete Integration Test
// ============================================================
// Combines all tests in one function

void uart_complete_test() {
    static bool initialized = false;

    if (!initialized) {
        Serial.begin(115200);
        delay(100);
        Serial.println("\n=== ESP32-C5 UART COMPLETE TEST ===");
        Serial.println("Waiting for Flipper Zero...");
        initialized = true;
    }

    // Run all tests
    uart_test_heartbeat();      // Every 1 second
    uart_test_loopback();       // Continuous
    uart_test_binary_pattern(); // Every 2 seconds
}

// ============================================================
// USAGE INSTRUCTIONS
// ============================================================

/*
 * STEP 1: Add to Bruce firmware
 * -------------------------------
 * In boards/[your-board]/interface.cpp:
 *
 * void _setup_gpio() {
 *     // ... existing code ...
 *     Serial.begin(115200);
 *     Serial.println("UART Test Mode");
 * }
 *
 * void loop() {
 *     // ... existing code ...
 *     uart_complete_test();  // Add this line
 * }
 *
 *
 * STEP 2: Build and flash
 * ------------------------
 * pio run -e [your-board] -t upload
 * pio device monitor -b 115200
 *
 *
 * STEP 3: Connect to Flipper Zero
 * ---------------------------------
 * ESP32-C5          Flipper Zero
 * ---------         ------------
 * GPIO11 (TX)   ->  Pin 14 (RX/PB7)
 * GPIO12 (RX)   <-  Pin 13 (TX/PB6)
 * GND           --  GND
 *
 *
 * STEP 4: Check Flipper logs
 * ---------------------------
 * Launch Bruce Remote app and observe:
 * - RX counter should increase
 * - Flipper logs show received bytes
 * - Press buttons, check ESP32 echo
 *
 *
 * EXPECTED OUTPUT on ESP32 Monitor:
 * ----------------------------------
 * === ESP32-C5 UART COMPLETE TEST ===
 * Waiting for Flipper Zero...
 * HEARTBEAT #1 - Time: 1000 ms
 * HEARTBEAT #2 - Time: 2000 ms
 * [BINARY TEST] Sending test pattern...
 * [BINARY TEST] Pattern sent!
 * [ECHO] Received: 'U' (0x55)
 *   -> UP button detected!
 * HEARTBEAT #3 - Time: 3000 ms
 *
 *
 * EXPECTED on Flipper Screen:
 * ---------------------------
 * RX:15/0      <- Should increase, not stay 0/0
 *
 *
 * TROUBLESHOOTING:
 * ----------------
 * 1. If RX:0/0 on Flipper:
 *    - ESP32 not sending (check monitor output)
 *    - Wrong pin connection (TX/RX not crossed)
 *    - Baud rate mismatch (must be 115200)
 *    - Cable/breadboard issue
 *
 * 2. If ESP32 monitor shows nothing:
 *    - Wrong serial port selected
 *    - USB cable issue
 *    - Firmware not flashed correctly
 *
 * 3. If echo doesn't work:
 *    - Flipper TX -> ESP32 RX not connected
 *    - Check Flipper is sending (uart_worker_send_button)
 *
 * 4. If binary data corrupted:
 *    - Add Serial.flush() after Serial.write()
 *    - Reduce transmission speed
 *    - Check voltage levels (both 3.3V)
 */

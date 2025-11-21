/**
 * @file interface.cpp
 * @brief Modified interface for ESP32-C5-tft with Flipper Zero remote control
 *
 * This file is a complete replacement for boards/ESP32-C5-tft/interface.cpp
 * in the Bruce firmware repository.
 *
 * Modifications:
 * 1. Serial UART initialized at 115200 baud for Flipper Zero communication
 * 2. startAsyncSerial() called to enable real-time display streaming
 * 3. UART button input handler for remote control from Flipper Zero
 *
 * UART Pins (ESP32-C5):
 * - GPIO11: UART0 TX (to Flipper Pin 14 RX)
 * - GPIO12: UART0 RX (from Flipper Pin 13 TX)
 *
 * Installation:
 * 1. Copy this file to Bruce/boards/ESP32-C5-tft/interface.cpp
 * 2. Build: pio run -e esp32-c5-tft
 * 3. Flash: pio run -e esp32-c5-tft -t upload
 *
 * @date 2025-11-21
 */

// Include paths relative to boards/ESP32-C5-tft/
#include "../../src/core/globals.h"
#include "../../src/core/mykeyboard.h"
#include "../../src/core/display.h"

// Pin definitions for ESP32-C5-tft
#define TFT_CS    10
#define TFT_RST   6
#define TFT_DC    7
#define TFT_MOSI  23
#define TFT_MISO  19
#define TFT_SCLK  18
#define TFT_BL    38  // Backlight

// Button pins (adjust based on your hardware)
#define BTN_UP    0
#define BTN_DOWN  35
#define BTN_SEL   47
#define BTN_ESC   21

// UART for Flipper Zero (ESP32-C5 UART0)
// GPIO11 = TX, GPIO12 = RX (default UART0 pins)

/**
 * @brief Initialize GPIO pins and peripherals
 *
 * Called once during boot before TFT initialization
 */
void _setup_gpio() {
    // Initialize button pins with internal pull-up
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_SEL, INPUT_PULLUP);
    pinMode(BTN_ESC, INPUT_PULLUP);

    // Initialize TFT backlight
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);  // Turn on backlight

    // ★ MODIFICATION: Initialize Serial for Flipper Zero remote control
    Serial.begin(115200);
    Serial.println();
    Serial.println("===================================");
    Serial.println("  Bruce Firmware - ESP32-C5");
    Serial.println("  Flipper Zero Remote Mode");
    Serial.println("===================================");
    Serial.printf("UART0: TX=GPIO11, RX=GPIO12\n");
    Serial.printf("Baud Rate: 115200\n");
    Serial.println("Waiting for Flipper connection...");
}

/**
 * @brief Post-setup initialization
 *
 * Called after TFT is initialized, before main loop
 */
void _post_setup_gpio() {
    // Display welcome message on TFT
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("Bruce");
    tft.setTextSize(1);
    tft.setCursor(10, 40);
    tft.println("Flipper Remote Ready");
    tft.setCursor(10, 60);
    tft.println("UART: 115200 baud");
    delay(2000);

    // ★ MODIFICATION: Enable async serial display streaming to Flipper Zero
    Serial.println("\n[Bruce] Enabling remote display...");

    // Enable TFT logging
    tft.setLogging(true);

    // Start async serial streaming (sends display commands to Flipper)
    tft.startAsyncSerial();

    Serial.println("[tftLogger] Serial streaming started");
    Serial.println("[Bruce] Remote display enabled!");
    Serial.println("\nReady for Flipper Zero control:");
    Serial.println("  U = Up");
    Serial.println("  D = Down");
    Serial.println("  S = Select/OK");
    Serial.println("  E = Escape/Back");
    Serial.println("  L = Left");
    Serial.println("  R = Right");
    Serial.println();
}

/**
 * @brief Handle user input from buttons and UART
 *
 * Called continuously in main loop to check for input
 *
 * Global variables set by this function:
 * - PrevPress: true when Up/Left pressed
 * - NextPress: true when Down/Right pressed
 * - SelPress: true when Select/OK pressed
 * - EscPress: true when Escape/Back pressed
 * - AnyKeyPress: true when any key pressed
 * - LongPress: true for long press (held button)
 */
void InputHandler() {
    // Debounce timing
    static unsigned long tm = 0;
    if (millis() - tm < 200 && !LongPress) return;

    // ★ MODIFICATION: UART Remote Control Input (Flipper Zero → Bruce)
    // Process UART commands before physical buttons
    while (Serial.available()) {
        char c = Serial.read();

        // Debug: echo received character
        Serial.printf("[InputHandler] Received UART: '%c' (0x%02X)\n", c, c);

        // Single character commands
        switch(c) {
            case 'U':  // Up
                Serial.println("  → UP button");
                PrevPress = true;
                AnyKeyPress = true;
                tm = millis();
                return;

            case 'D':  // Down
                Serial.println("  → DOWN button");
                NextPress = true;
                AnyKeyPress = true;
                tm = millis();
                return;

            case 'S':  // Select/OK
                Serial.println("  → SELECT button");
                SelPress = true;
                AnyKeyPress = true;
                tm = millis();
                return;

            case 'E':  // Escape/Back
                Serial.println("  → ESCAPE button");
                EscPress = true;
                AnyKeyPress = true;
                tm = millis();
                return;

            case 'L':  // Left (treated as Up for menu navigation)
                Serial.println("  → LEFT button");
                PrevPress = true;
                AnyKeyPress = true;
                tm = millis();
                return;

            case 'R':  // Right (treated as Down for menu navigation)
                Serial.println("  → RIGHT button");
                NextPress = true;
                AnyKeyPress = true;
                tm = millis();
                return;

            default:
                // Ignore unknown characters
                Serial.printf("  → Unknown command\n");
                break;
        }
    }

    // Physical button handling (if buttons are present on hardware)
    // These are optional and can coexist with UART control

    // Reset button states
    PrevPress = false;
    NextPress = false;
    SelPress = false;
    EscPress = false;
    AnyKeyPress = false;

    // Check physical buttons
    if (digitalRead(BTN_UP) == LOW) {
        Serial.println("[Physical] UP pressed");
        PrevPress = true;
        AnyKeyPress = true;
        tm = millis();
        return;
    }

    if (digitalRead(BTN_DOWN) == LOW) {
        Serial.println("[Physical] DOWN pressed");
        NextPress = true;
        AnyKeyPress = true;
        tm = millis();
        return;
    }

    if (digitalRead(BTN_SEL) == LOW) {
        Serial.println("[Physical] SELECT pressed");
        SelPress = true;
        AnyKeyPress = true;
        tm = millis();
        return;
    }

    if (digitalRead(BTN_ESC) == LOW) {
        Serial.println("[Physical] ESCAPE pressed");
        EscPress = true;
        AnyKeyPress = true;
        tm = millis();
        return;
    }

    // Long press detection (hold button for 2 seconds)
    static bool buttonHeld = false;
    static unsigned long holdStartTime = 0;

    if (AnyKeyPress && !buttonHeld) {
        buttonHeld = true;
        holdStartTime = millis();
    } else if (AnyKeyPress && buttonHeld) {
        if (millis() - holdStartTime > 2000) {
            LongPress = true;
            Serial.println("[Physical] LONG PRESS detected");
        }
    } else {
        buttonHeld = false;
        LongPress = false;
    }
}

/**
 * @brief Power saving mode handler
 *
 * Called when device is idle to reduce power consumption
 */
void _power_save() {
    // Reduce backlight brightness
    analogWrite(TFT_BL, 50);  // Dim to 20%

    // You can add more power saving measures here:
    // - Reduce CPU frequency
    // - Disable WiFi if enabled
    // - Put peripherals to sleep

    Serial.println("[Power] Entering power save mode");
}

/**
 * @brief Wake from power save mode
 *
 * Called when user input is detected
 */
void _power_wake() {
    // Restore backlight brightness
    digitalWrite(TFT_BL, HIGH);  // Full brightness

    Serial.println("[Power] Waking from power save");
}

/**
 * @brief Get battery voltage (if applicable)
 *
 * @return Battery voltage in volts, or 0 if not supported
 */
float getBatteryVoltage() {
    // ESP32-C5-DevKitC-1 doesn't have built-in battery monitoring
    // If you add a voltage divider circuit, implement reading here

    // Example with ADC (if you connect battery to GPIO1 via voltage divider):
    // int rawValue = analogRead(1);
    // float voltage = (rawValue / 4095.0) * 3.3 * 2.0;  // Assuming 1:2 divider
    // return voltage;

    return 0.0;  // Not supported by default
}

/**
 * @brief Check if device is charging (if applicable)
 *
 * @return true if charging, false otherwise
 */
bool isCharging() {
    // ESP32-C5-DevKitC-1 is powered via USB
    // If you add battery and charging circuit, implement detection here

    return false;  // Not supported by default
}

/**
 * @brief Custom idle handler
 *
 * Called during main loop idle time for custom tasks
 */
void _custom_idle_handler() {
    // You can add periodic tasks here
    // Examples:
    // - Update WiFi status
    // - Check for OTA updates
    // - Send telemetry data

    // Keep this lightweight to avoid blocking main loop
}

/**
 * @brief LED indicator control (if applicable)
 *
 * @param state true to turn LED on, false to turn off
 */
void setLED(bool state) {
    // If your board has an LED, control it here
    // Example:
    // digitalWrite(LED_PIN, state ? HIGH : LOW);

    // ESP32-C5-DevKitC-1 has RGB LED on GPIO8 (WS2812)
    // You can implement WS2812 control if needed
}

/**
 * @brief Cleanup and shutdown
 *
 * Called before device shutdown or reset
 */
void _cleanup() {
    // Stop async serial streaming
    // Note: Bruce's tft_logger should have stopAsyncSerial() method
    // If available, call it here

    Serial.println("\n[Bruce] Shutting down...");
    Serial.println("[Bruce] Stopping remote display stream");

    // Turn off backlight
    digitalWrite(TFT_BL, LOW);

    // Clear display
    tft.fillScreen(TFT_BLACK);

    Serial.println("[Bruce] Cleanup complete");
}

// End of interface.cpp

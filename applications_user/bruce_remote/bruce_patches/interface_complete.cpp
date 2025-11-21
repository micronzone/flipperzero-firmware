/**
 * @file interface.cpp
 * @brief Safe minimal interface for ESP32-C5-tft with Flipper Zero remote control
 *
 * This file is a complete replacement for boards/ESP32-C5-tft/interface.cpp
 * in the Bruce firmware repository.
 *
 * Modifications from original:
 * 1. Serial UART initialized at 115200 baud for Flipper Zero communication
 * 2. TFT logging enabled (manual polling required for display streaming)
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

// Include paths - verified correct for Bruce firmware structure
#include "../../include/globals.h"
#include "../../src/core/display.h"
#include "../../src/core/mykeyboard.h"

/***************************************************************************************
** Function name: _setup_gpio()
** Location: main.cpp (called from setup())
** Description: Initial GPIO setup - TFT is NOT initialized yet at this point!
***************************************************************************************/
void _setup_gpio() {
    // ESP32-C5 GPIO initialization
    // NOTE: Do NOT use tft object here - it's not initialized yet!

    // Initialize Serial for Flipper Zero communication
    // Default UART0: GPIO11 (TX), GPIO12 (RX)
    Serial.begin(115200);
    delay(100);

    Serial.println();
    Serial.println("================================");
    Serial.println("  Bruce ESP32-C5");
    Serial.println("  Flipper Zero Remote Mode");
    Serial.println("================================");
    Serial.println("UART: GPIO11(TX) GPIO12(RX)");
    Serial.println("Baud: 115200");
    Serial.println();
}

/***************************************************************************************
** Function name: _post_setup_gpio()
** Location: main.cpp (called after tft.init() and storage init)
** Description: Post-initialization - TFT is ready here
***************************************************************************************/
void _post_setup_gpio() {
    // At this point, tft has been initialized and is safe to use

    Serial.println("[Bruce] Post-setup initialization");

    // Enable TFT command logging
    tft.setLogging(true);
    Serial.println("[tftLogger] Logging enabled");

    Serial.println();
    Serial.println("Ready for Flipper Zero remote control:");
    Serial.println("  Commands: U(up) D(down) S(select) E(esc)");
    Serial.println();
}

/***************************************************************************************
** Function name: InputHandler
** Location: Called continuously from main loop
** Description: Handle UART input from Flipper Zero and set global button variables
**
** Global variables set by this function:
** - PrevPress: true when Up pressed
** - NextPress: true when Down pressed
** - SelPress: true when Select pressed
** - EscPress: true when Escape pressed
** - AnyKeyPress: true when any key pressed
** - LongPress: true for long press (if applicable)
***************************************************************************************/
void InputHandler(void) {
    // Standard debounce pattern (200ms)
    static unsigned long tm = 0;
    if (millis() - tm < 200 && !LongPress) return;

    // Check UART input from Flipper Zero
    while (Serial.available()) {
        char c = Serial.read();
        tm = millis();

        // Process button commands
        switch(c) {
            case 'U':  // Up
                if (!wakeUpScreen()) {
                    PrevPress = true;
                    AnyKeyPress = true;
                }
                return;

            case 'D':  // Down
                if (!wakeUpScreen()) {
                    NextPress = true;
                    AnyKeyPress = true;
                }
                return;

            case 'S':  // Select/OK
                if (!wakeUpScreen()) {
                    SelPress = true;
                    AnyKeyPress = true;
                }
                return;

            case 'E':  // Escape/Back
                if (!wakeUpScreen()) {
                    EscPress = true;
                    AnyKeyPress = true;
                }
                return;

            case 'L':  // Left (same as Up)
                if (!wakeUpScreen()) {
                    PrevPress = true;
                    AnyKeyPress = true;
                }
                return;

            case 'R':  // Right (same as Down)
                if (!wakeUpScreen()) {
                    NextPress = true;
                    AnyKeyPress = true;
                }
                return;

            default:
                // Ignore unknown characters
                break;
        }
    }
}

/***************************************************************************************
** Function name: getBattery
** Location: display.cpp
** Description: Returns battery level 0-100
***************************************************************************************/
int getBattery() {
    // ESP32-C5-DevKitC-1 doesn't have built-in battery monitoring
    // Return 100 to indicate USB powered
    return 100;
}

/***************************************************************************************
** Function name: _setBrightness
** Location: settings.cpp
** Description: Set display brightness 0-100
***************************************************************************************/
void _setBrightness(uint8_t brightval) {
    // ESP32-C5-tft may have backlight control
    // Implement if your board has TFT_BL pin
    // Example:
    // int brightness = MINBRIGHT + ((255 - MINBRIGHT) * brightval / 100);
    // analogWrite(TFT_BL, brightness);

    // For now, do nothing (board-specific implementation needed)
}

/***************************************************************************************
** Function name: powerOff
** Location: mykeyboard.cpp
** Description: Power off the device
***************************************************************************************/
void powerOff() {
    Serial.println("[Bruce] Power off requested");

    // ESP32-C5 doesn't have hardware power control
    // Best we can do is deep sleep

    tft.fillScreen(TFT_BLACK);
    Serial.println("[Bruce] Entering deep sleep...");
    delay(1000);

    esp_deep_sleep_start();
}

/***************************************************************************************
** Function name: checkReboot
** Location: mykeyboard.cpp
** Description: Check if reboot is needed (button logic)
***************************************************************************************/
void checkReboot() {
    // No special reboot logic needed for ESP32-C5
    // This function can be empty
}

/***************************************************************************************
** Function name: isCharging
** Description: Returns true if device is charging
***************************************************************************************/
bool isCharging() {
    // ESP32-C5-DevKitC-1 is USB powered, always "charging"
    return true;
}

// End of interface.cpp

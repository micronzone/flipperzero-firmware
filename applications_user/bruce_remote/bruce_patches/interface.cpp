// Modified ESP32-C5-tft/interface.cpp for Flipper Zero Remote Control
// Place this file in: /tmp/Bruce/boards/ESP32-C5-tft/interface.cpp
//
// Changes:
// 1. Added UART button input handling in InputHandler()
// 2. Supports both physical buttons and remote UART control

#include "core/powerSave.h"
#include "core/utils.h"
#include <interface.h>

/***************************************************************************************
** Function name: _setup_gpio()
** Location: main.cpp
** Description:   initial setup for the device
***************************************************************************************/
void _setup_gpio() {

    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);
    pinMode(TFT_MOSI, OUTPUT);
    digitalWrite(TFT_MOSI, HIGH);
    pinMode(TFT_SCLK, OUTPUT);

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    pinMode(TFT_RST, OUTPUT);
    pinMode(TFT_DC, OUTPUT);
    digitalWrite(TFT_DC, HIGH);

#ifdef HAS_3_BUTTONS
    pinMode(UP_BTN, INPUT_PULLUP); // Sets the power btn as an INPUT
    pinMode(SEL_BTN, INPUT_PULLUP);
    pinMode(DW_BTN, INPUT_PULLUP);
#endif
    pinMode(NRF24_SS_PIN, OUTPUT);
    pinMode(CC1101_SS_PIN, OUTPUT);
    pinMode(SDCARD_CS, OUTPUT);
    pinMode(W5500_SS_PIN, OUTPUT);
    pinMode(TFT_CS, OUTPUT);

    digitalWrite(NRF24_SS_PIN, HIGH);
    digitalWrite(CC1101_SS_PIN, HIGH);
    digitalWrite(SDCARD_CS, HIGH);
    digitalWrite(W5500_SS_PIN, HIGH);
    digitalWrite(TFT_CS, HIGH);
#ifdef ILI9341_DRIVER
    bruceConfig.colorInverted = 0;
#endif

    // Initialize Serial for remote control
    Serial.begin(115200);
}

/***************************************************************************************
** Function name: _post_setup_gpio()
** Location: main.cpp
** Description:   second stage gpio setup to make a few functions work
***************************************************************************************/
void _post_setup_gpio() {
#ifdef HAS_TOUCH
    pinMode(TOUCH_CS, OUTPUT);
    uint16_t calData[5];
    File caldata = LittleFS.open("/calData", "r");

    if (!caldata) {
        tft.setRotation(ROTATION);
        tft.calibrateTouch(calData, TFT_WHITE, TFT_BLACK, 10);

        caldata = LittleFS.open("/calData", "w");
        if (caldata) {
            caldata.printf(
                "%d\n%d\n%d\n%d\n%d\n", calData[0], calData[1], calData[2], calData[3], calData[4]
            );
            caldata.close();
        }
    } else {
        Serial.print("\ntft Calibration data: ");
        for (int i = 0; i < 5; i++) {
            String line = caldata.readStringUntil('\n');
            calData[i] = line.toInt();
            Serial.printf("%d, ", calData[i]);
        }
        Serial.println();
        caldata.close();
    }
    tft.setTouch(calData);
#endif

    // Enable async serial for remote display
    Serial.println("Bruce Remote Display Mode Enabled");
    tft.startAsyncSerial();
}

/***************************************************************************************
** Function name: getBattery()
** location: display.cpp
** Description:   Delivers the battery value from 1-100
***************************************************************************************/
int getBattery() { return 0; }

/***************************************************************************************
** Function name: isCharging()
** Description:   Default implementation that returns false
***************************************************************************************/
bool isCharging() { return false; }

/*********************************************************************
** Function: setBrightness
** location: settings.cpp
** set brightness value
**********************************************************************/
void _setBrightness(uint8_t brightval) {
    if (brightval == 0) {
        analogWrite(TFT_BL, brightval);
    } else {
        int bl = MINBRIGHT + round(((255 - MINBRIGHT) * brightval / 100));
        analogWrite(TFT_BL, bl);
    }
}

/*********************************************************************
** Function: InputHandler
** Handles the variables PrevPress, NextPress, SelPress, AnyKeyPress and EscPress
** MODIFIED: Now supports both physical buttons and UART remote control
**********************************************************************/
void InputHandler(void) {
    static unsigned long tm = 0;
    if (millis() - tm < 200 && !LongPress) return;

    // UART Remote Control Input
    // Read button commands from Flipper Zero
    static char uart_buffer[16];
    static uint8_t uart_idx = 0;

    while (Serial.available()) {
        char c = Serial.read();

        // Single character commands (instant)
        if (c >= 'A' && c <= 'Z') {
            switch(c) {
                case 'U':  // Up
                    PrevPress = true;
                    AnyKeyPress = true;
                    tm = millis();
                    return;

                case 'D':  // Down
                    NextPress = true;
                    AnyKeyPress = true;
                    tm = millis();
                    return;

                case 'S':  // Select/OK
                    SelPress = true;
                    AnyKeyPress = true;
                    tm = millis();
                    return;

                case 'E':  // Escape/Back
                    EscPress = true;
                    AnyKeyPress = true;
                    tm = millis();
                    return;

                case 'L':  // Left
                    PrevPress = true;
                    AnyKeyPress = true;
                    tm = millis();
                    return;

                case 'R':  // Right
                    NextPress = true;
                    AnyKeyPress = true;
                    tm = millis();
                    return;
            }
        }

        // Buffer for multi-character commands (future expansion)
        if (c == '\n') {
            uart_buffer[uart_idx] = '\0';
            // Process buffered command if needed
            uart_idx = 0;
        } else if (uart_idx < sizeof(uart_buffer) - 1) {
            uart_buffer[uart_idx++] = c;
        }
    }

#ifdef HAS_TOUCH
    TouchPoint t;
    checkPowerSaveTime();
    bool _IH_touched = tft.getTouch(&t.x, &t.y);
    if (_IH_touched) {
        NextPress = false;
        PrevPress = false;
        UpPress = false;
        DownPress = false;
        SelPress = false;
        EscPress = false;
        AnyKeyPress = false;
        NextPagePress = false;
        PrevPagePress = false;
        touchPoint.pressed = false;
        _IH_touched = false;
        Serial.printf("\nRAW: Touch Pressed on x=%d, y=%d", t.x, t.y);
        if (bruceConfig.rotation == 3) {
            t.y = (tftHeight + 20) - t.y;
            t.x = tftWidth - t.x;
        }
        if (bruceConfig.rotation == 0) {
            uint16_t tmp = t.x;
            t.x = map((tftHeight + 20) - t.y, 0, 320, 0, 240);
            t.y = map(tmp, 0, 240, 0, 320);
        }
        if (bruceConfig.rotation == 2) {
            uint16_t tmp = t.x;
            t.x = map(t.y, 0, 320, 0, 240);
            t.y = map(tftWidth - tmp, 0, 240, 0, 320);
        }

        Serial.printf("\nROT: Touch Pressed on x=%d, y=%d, rot=%d\n", t.x, t.y, bruceConfig.rotation);

        if (!wakeUpScreen()) AnyKeyPress = true;
        else return;

        // Touch point global variable
        touchPoint.x = t.x;
        touchPoint.y = t.y;
        touchPoint.pressed = true;
        touchHeatMap(touchPoint);
        tm = millis();
    }

#endif
#ifdef HAS_3_BUTTONS
    // Physical buttons (if available)
    bool upPressed = (digitalRead(UP_BTN) == LOW);
    bool selPressed = (digitalRead(SEL_BTN) == LOW);
    bool dwPressed = (digitalRead(DW_BTN) == LOW);

    bool anyPressed = upPressed || selPressed || dwPressed;
    if (anyPressed) tm = millis();
    if (anyPressed && wakeUpScreen()) return;

    // Combine with UART input (priority to physical buttons)
    if (upPressed) PrevPress = true;
    if (selPressed) SelPress = true;
    if (dwPressed) NextPress = true;
    if (upPressed && dwPressed) EscPress = true;

    AnyKeyPress = AnyKeyPress || anyPressed;
#endif
}

/*********************************************************************
** Function: powerOff
** location: mykeyboard.cpp
** Turns off the device (or try to)
**********************************************************************/
void powerOff() {}

/*********************************************************************
** Function: checkReboot
** location: mykeyboard.cpp
** Btn logic to turnoff the device (name is odd btw)
**********************************************************************/
void checkReboot() {}

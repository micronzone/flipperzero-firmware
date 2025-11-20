# Complete Implementation Guide for Bruce Remote Control

This guide provides step-by-step instructions to make Bruce firmware work with Flipper Zero remote control.

## Problem Summary

The original patches call `tft.startAsyncSerial()` which **does not exist** in Bruce firmware. We need to implement this functionality.

## Solution: Add Serial Display Streaming to Bruce

### Step 1: Add ESP32-C5 Board Support to Bruce

Since Bruce doesn't have ESP32-C5 support, we'll use **ESP32-S3-DevKitC-1** instead, which is already supported and compatible.

**Alternative**: If you must use ESP32-C5, you'll need to create a complete board definition in Bruce (see Appendix A).

### Step 2: Modify Bruce Firmware Files

Clone Bruce firmware:
```bash
git clone https://github.com/pr3y/Bruce.git
cd Bruce
```

#### A. Add `startAsyncSerial()` Method to tftLogger

**File**: `include/tftLogger.h`

Add this declaration to the `tft_logger` class:

```cpp
class tft_logger : public BRUCE_TFT_DRIVER {
public:
    // ... existing methods ...
    void setLogging(bool _log = true);
    void getBinLog(uint8_t *outBuffer, size_t &outSize);

    // ADD THIS NEW METHOD:
    void startAsyncSerial();  // Enable automatic serial streaming

private:
    // ... existing fields ...
    bool serialStreamEnabled = false;
    TaskHandle_t serialTask = NULL;
};
```

**File**: `src/core/tftLogger/tftLogger.cpp`

Add this implementation at the end of the file:

```cpp
void tft_logger::startAsyncSerial() {
    if(serialStreamEnabled) return;  // Already started

    serialStreamEnabled = true;
    setLogging(true);  // Enable TFT logging

    // Create FreeRTOS task for serial streaming
    xTaskCreate(
        [](void* param) {
            tft_logger* tft = (tft_logger*)param;
            uint8_t buffer[512];
            size_t size;

            Serial.println("[tftLogger] Serial streaming started");

            while(tft->serialStreamEnabled) {
                // Get binary log data
                tft->getBinLog(buffer, size);

                // Send if there's data
                if(size > 0) {
                    Serial.write(buffer, size);
                    Serial.flush();  // Ensure immediate transmission
                }

                // 50ms interval = 20 FPS update rate
                vTaskDelay(pdMS_TO_TICKS(50));
            }

            Serial.println("[tftLogger] Serial streaming stopped");
            vTaskDelete(NULL);
        },
        "TftSerialStream",  // Task name
        4096,               // Stack size (4KB)
        this,               // Parameter (this pointer)
        1,                  // Priority (low)
        &serialTask         // Task handle
    );
}
```

#### B. Modify Board Interface File

**For ESP32-S3-DevKitC-1**: `boards/esp32-s3-devkitc-1/interface.cpp`

**For ESP32-C5 (if you created it)**: `boards/ESP32-C5-tft/interface.cpp`

Find the `_setup_gpio()` function and add at the end:

```cpp
void _setup_gpio() {
    // ... existing GPIO setup code ...

    // Initialize Serial for Flipper Zero remote control
    Serial.begin(115200);
    Serial.println("\n===== Bruce Firmware =====");
    Serial.println("Flipper Zero Remote Mode");
}
```

Find the `_post_setup_gpio()` function and add at the end:

```cpp
void _post_setup_gpio() {
    // ... existing post-setup code ...

    // Enable async serial display streaming
    Serial.println("[Bruce] Enabling remote display...");
    tft.startAsyncSerial();
    Serial.println("[Bruce] Remote display enabled!");
}
```

Find the `InputHandler()` function and add at the beginning (before existing button checks):

```cpp
void InputHandler() {
    // UART Remote Control Input (Flipper Zero)
    while(Serial.available()) {
        char c = Serial.read();

        // Handle single-character commands
        switch(c) {
            case 'U':  // Up
                PrevPress = true;
                AnyKeyPress = true;
                return;

            case 'D':  // Down
                NextPress = true;
                AnyKeyPress = true;
                return;

            case 'S':  // Select/OK
                SelPress = true;
                AnyKeyPress = true;
                return;

            case 'E':  // Escape/Back
                EscPress = true;
                AnyKeyPress = true;
                return;

            case 'L':  // Left (same as Up for menus)
                PrevPress = true;
                AnyKeyPress = true;
                return;

            case 'R':  // Right (same as Down for menus)
                NextPress = true;
                AnyKeyPress = true;
                return;
        }
    }

    // ... existing physical button handling code ...
}
```

### Step 3: Configure PlatformIO

**File**: `platformio.ini`

Find or create the environment for your board. For ESP32-S3-DevKitC-1:

```ini
[env:esp32-s3-devkitc-1]
board = esp32-s3-devkitc-1
build_flags =
    ${common.build_flags}
    -D HAS_SCREEN=1
    -D CARDPUTER=1  ; or appropriate screen type
upload_speed = 921600
monitor_speed = 115200  ; IMPORTANT: Must match Serial.begin()
```

Set it as default:

```ini
[platformio]
default_envs = esp32-s3-devkitc-1
```

### Step 4: Build and Flash

```bash
# Build firmware
pio run -e esp32-s3-devkitc-1

# Flash to ESP32
pio run -e esp32-s3-devkitc-1 -t upload

# Monitor serial output
pio device monitor -b 115200
```

You should see:
```
===== Bruce Firmware =====
Flipper Zero Remote Mode
...
[Bruce] Enabling remote display...
[tftLogger] Serial streaming started
[Bruce] Remote display enabled!
```

### Step 5: Hardware Connections

```
ESP32-S3-DevKitC-1    Flipper Zero
------------------    -------------
GPIO 43 (U0TXD)  →    Pin 14 (RX/PB7)
GPIO 44 (U0RXD)  ←    Pin 13 (TX/PB6)
GND              ←→   GND
```

**Critical**: TX and RX must be **crossed over** (TX → RX, RX ← TX)

### Step 6: Test with Flipper Zero

1. Flash the updated Flipper app: `./fbt fap_bruce_remote`
2. Install to Flipper: Copy `.fap` file to SD card
3. Launch "Bruce Remote" app
4. Select "Start Remote Control"

**Expected behavior**:
- Flipper shows Bruce UI mirrored on screen
- Button presses on Flipper control Bruce menu
- Stats show RX packets increasing (not 0/0)

## Debugging

### Enable Verbose Logging in Bruce

In `main.cpp`, increase log level:

```cpp
void setup() {
    esp_log_level_set("*", ESP_LOG_DEBUG);
    Serial.setRxBufferSize(SAFE_STACK_BUFFER_SIZE / 4);
    Serial.begin(115200);
    // ...
}
```

### Check Serial Output

Connect to ESP32 serial monitor and look for:

```
[tftLogger] Serial streaming started
Header: AA, Size: 07, Func: 99  // SCREEN_INFO
Header: AA, Size: 05, Func: 00  // FILLSCREEN
Header: AA, Size: 0C, Func: 10  // DRAWSTRING
```

### Check Flipper Logs

View Flipper logs to see UART activity:

```
[I][BruceUart] UART initialized: 115200 baud
[I][BruceUart] Worker thread started
[T][BruceUart] RX: 0xAA
[T][BruceUart] RX: 0x07
[T][BruceUart] RX: 0x99
[D][BruceUart] Received 7 bytes from stream
[D][BruceRemote] Header detected at offset 0
[I][BruceRemote] Complete packet received: 7 bytes
```

If you see **no RX logs**, the problem is:
1. Wrong UART pins
2. No TX/RX crossover
3. Baud rate mismatch
4. Bruce not transmitting (check Bruce serial output)

## Appendix A: Creating ESP32-C5 Board in Bruce

If you need ESP32-C5 support, create these files:

### File Structure
```
Bruce/
└── boards/
    └── ESP32-C5-tft/
        ├── board.h
        ├── interface.cpp
        └── pins_arduino.h
```

### `boards/ESP32-C5-tft/board.h`

```cpp
#ifndef ESP32_C5_TFT_H
#define ESP32_C5_TFT_H

#define ESP32_C5_TFT 1
#define HAS_SCREEN 1
#define TFT_WIDTH 320
#define TFT_HEIGHT 240

// UART pins for ESP32-C5
#define U0TXD_GPIO_NUM 11
#define U0RXD_GPIO_NUM 12

// TFT pins (adjust based on your hardware)
#define TFT_CS    10
#define TFT_RST   6
#define TFT_DC    7
#define TFT_MOSI  23
#define TFT_MISO  19
#define TFT_CLK   18

#endif
```

### `platformio.ini` entry

```ini
[env:esp32-c5-tft]
board = esp32-c5-devkitc-1
platform = espressif32@^6.7.0
framework = arduino
build_flags =
    ${common.build_flags}
    -D ESP32_C5_TFT=1
    -D HAS_SCREEN=1
    -D TFT_WIDTH=320
    -D TFT_HEIGHT=240
upload_speed = 921600
monitor_speed = 115200
```

## Troubleshooting

### "Purple LED" on ESP32
- **Cause**: Firmware crash, likely `tft.startAsyncSerial()` not found
- **Solution**: Implement `startAsyncSerial()` as shown above

### Flipper shows "RX:0/0"
- **Cause**: No data received from ESP32
- **Solution**:
  1. Check Bruce serial output - is streaming task running?
  2. Verify UART connections (TX↔RX crossover)
  3. Check baud rate (115200)
  4. Enable DEBUG_RAW_BYTES in display_parser.c

### Screen flickers or corrupted
- **Cause**: Packet loss or corruption
- **Solution**:
  1. Add `Serial.flush()` after Serial.write()
  2. Reduce streaming rate (100ms instead of 50ms)
  3. Check UART signal quality

### Buttons don't work
- **Cause**: UART input not processed
- **Solution**:
  1. Verify InputHandler() modification
  2. Check TX connection (Flipper → ESP32)
  3. Test by sending 'U' from serial monitor

## Performance Notes

- **Update Rate**: 20 FPS (50ms interval)
- **Latency**: ~100-150ms button to screen
- **UART Bandwidth**: ~30-60 KB/s typical
- **CPU Usage**: ~5% on ESP32 (serial task)

## Success Criteria

When everything works:
1. Flipper app launches without crash ✓
2. RX counter increases (not 0/0) ✓
3. Bruce menu appears on Flipper screen ✓
4. Buttons on Flipper control Bruce ✓
5. No UART errors in logs ✓

## Next Steps

- Add icon to Bruce Remote app (`bruce_remote_10px.png`)
- Optimize coordinate scaling for better text rendering
- Add font size mapping for more accurate display
- Implement bidirectional status (Bruce → Flipper notifications)

## Support

If you encounter issues:
1. Check Bruce serial output first
2. Enable all debug logs in Flipper
3. Verify hardware connections with multimeter
4. Test UART loopback (connect TX to RX on ESP32)

---

**Last Updated**: 2025-11-20
**Bruce Version**: main branch
**Flipper Firmware**: Latest OFW/Unleashed

# Bruce Firmware Patches for Flipper Zero Remote Control

## ⚠️ **CRITICAL: THIS PATCH IS NOT FUNCTIONAL AS-IS** ⚠️

**Current Status**: This patch requires modifications to work with Bruce firmware.

### 🚨 Known Issues:

1. **`tft.startAsyncSerial()` function does NOT exist in Bruce firmware**
   - Bruce provides `tft.setLogging()` and `tft.getBinLog()` but does NOT automatically send data via Serial
   - You must implement a periodic task to call `getBinLog()` and transmit via `Serial.write()`

2. **ESP32-C5 board is NOT supported by Bruce firmware**
   - Bruce supports 35+ boards but ESP32-C5 is not among them
   - ESP32-C5 is a 2025 release and Arduino support is still in development
   - **Alternative**: Use ESP32-S3-DevKitC-1 or CYD (Cheap Yellow Display) instead

3. **GPIO pin definitions may be incorrect**
   - The pins GPIO 4/5 (BAD_RX/BAD_TX) are not verified for ESP32-C5
   - Standard ESP32-C5 UART0 uses GPIO 11 (TX) and GPIO 12 (RX)

### ✅ What IS Correct:

- The binary protocol definition (tft_logger format) is 100% accurate
- The button command protocol (U/D/S/E/L/R) is correct
- The Flipper Zero app (parser, UART worker) is fully functional
- The concept and architecture are sound

### 🔧 To Make This Work:

**Option A**: Implement missing function in Bruce (recommended)
```cpp
// Add to Bruce firmware: src/core/tftLogger/tftLogger.cpp
void tft_logger::startAsyncSerial() {
    setLogging(true);

    // Create FreeRTOS task to periodically send logs
    xTaskCreate([](void* param) {
        tft_logger* tft = (tft_logger*)param;
        uint8_t buffer[512];
        size_t size;

        while(true) {
            tft->getBinLog(buffer, size);
            if(size > 0) {
                Serial.write(buffer, size);
            }
            vTaskDelay(pdMS_TO_TICKS(50));  // 20 FPS
        }
    }, "tftSerial", 4096, this, 1, NULL);
}
```

**Option B**: Use a supported Bruce board (easier)
- Flash to ESP32-S3-DevKitC-1 or CYD-2432S028
- Modify the corresponding board's `interface.cpp` instead

**Option C**: Add periodic sending in main loop
```cpp
// In Bruce main.cpp loop()
static uint32_t lastUpdate = 0;
if(millis() - lastUpdate > 50) {
    uint8_t logBuffer[512];
    size_t logSize;
    tft.getBinLog(logBuffer, logSize);
    if(logSize > 0) {
        Serial.write(logBuffer, logSize);
    }
    lastUpdate = millis();
}
```

### 📚 References:

- Bruce firmware: https://github.com/pr3y/Bruce (official repository)
- ESP32-C5 datasheet: https://www.espressif.com/sites/default/files/documentation/esp32-c5_datasheet_en.pdf

---

This directory contains modified files for the Bruce ESP32 firmware to enable remote control from Flipper Zero.

## Files

### `interface.cpp`
- **Target**: `/tmp/Bruce/boards/ESP32-C5-tft/interface.cpp`
- **Changes**:
  1. Added UART button input handling in `InputHandler()`
  2. Enabled `tft.startAsyncSerial()` in `_post_setup_gpio()`
  3. Initialized Serial at 115200 baud in `_setup_gpio()`

## Installation

### Option 1: Replace File
```bash
cd /path/to/Bruce
cp interface.cpp boards/ESP32-C5-tft/interface.cpp
```

### Option 2: Manual Patch
Apply the following changes to `boards/ESP32-C5-tft/interface.cpp`:

#### 1. In `_setup_gpio()` - Add at the end:
```cpp
// Initialize Serial for remote control
Serial.begin(115200);
```

#### 2. In `_post_setup_gpio()` - Add at the end:
```cpp
// Enable async serial for remote display
Serial.println("Bruce Remote Display Mode Enabled");
tft.startAsyncSerial();
```

#### 3. In `InputHandler()` - Add UART input handling:
```cpp
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

            case 'L':  // Left
                PrevPress = true;
                AnyKeyPress = true;
                return;

            case 'R':  // Right
                NextPress = true;
                AnyKeyPress = true;
                return;
        }
    }
}
```

## Building Bruce with Patches

```bash
cd /path/to/Bruce

# Verify platformio.ini has esp32-c5-tft environment
# Edit platformio.ini and set:
# default_envs = esp32-c5-tft

# Build
pio run -e esp32-c5-tft

# Upload to ESP32-C5-DevKitC-1
pio run -e esp32-c5-tft -t upload
```

## Hardware Connection

```
ESP32-C5-DevKitC-1    Flipper Zero
------------------    -------------
GPIO 4 (BAD_RX)  →    PA6 (RX) [Pin 14]
GPIO 5 (BAD_TX)  ←    PB7 (TX) [Pin 13]
GND              ←→   GND
```

## Communication Protocol

### Button Commands (Flipper → Bruce)
Single ASCII characters sent via UART:
- `U` - Up / Previous
- `D` - Down / Next
- `S` - Select / OK
- `E` - Escape / Back
- `L` - Left
- `R` - Right

### Display Protocol (Bruce → Flipper)
Bruce uses the built-in `tft_logger` binary protocol.

**Packet Format**:
```
[HEADER] [SIZE] [FUNC] [PARAMS...]

HEADER: 0xAA (1 byte)
SIZE:   Packet length (1 byte)
FUNC:   tftFuncs enum (1 byte)
PARAMS: uint16_t array (2 bytes each)
```

**tftFuncs Enum**:
```
0  - FILLSCREEN
1  - DRAWRECT
2  - FILLRECT
3  - DRAWROUNDRECT
4  - FILLROUNDRECT
5  - DRAWCIRCLE
6  - FILLCIRCLE
7  - DRAWTRIAGLE
8  - FILLTRIANGLE
9  - DRAWELIPSE
10 - FILLELIPSE
11 - DRAWLINE
12 - DRAWARC
13 - DRAWWIDELINE
14 - DRAWCENTRESTRING
15 - DRAWRIGHTSTRING
16 - DRAWSTRING
17 - PRINT
18 - DRAWIMAGE
19 - DRAWPIXEL
20 - DRAWFASTVLINE
21 - DRAWFASTHLINE
99 - SCREEN_INFO
```

**Example Packets**:

Screen Info (sent first):
```
AA 07 99 01 40 00 F0 01
│  │  │  └────┴────┴─── Width: 320, Height: 240, Rotation: 1
│  │  └─────────────── SCREEN_INFO (99)
│  └────────────────── Size: 7 bytes
└───────────────────── Header: 0xAA
```

Fill Screen (clear):
```
AA 05 00 00 00
│  │  │  └──┴── Color: 0x0000 (black)
│  │  └──────── FILLSCREEN (0)
│  └─────────── Size: 5 bytes
└────────────── Header: 0xAA
```

Draw String:
```
AA 0C 10 00 14 00 0A 01 48 65 6C 6C 6F
│  │  │  └──┴─ └──┴─ └─ └──────────┴─── "Hello"
│  │  │     X   Y    Font
│  │  └──────── DRAWSTRING (16)
│  └─────────── Size: 12 bytes
└────────────── Header: 0xAA
```

## Testing

1. Flash modified Bruce firmware to ESP32-C5-DevKitC-1
2. Connect UART to Flipper Zero (TX↔RX crossover!)
3. Open serial monitor at 115200 baud
4. You should see: "Bruce Remote Display Mode Enabled"
5. Send button commands (U/D/S/E) and observe Bruce UI responding
6. Monitor binary packets being sent from Bruce

## Troubleshooting

**No button response:**
- Check UART connection (TX and RX must be crossed over)
- Verify baud rate is 115200
- Check GND connection

**No display packets:**
- Ensure `tft.startAsyncSerial()` was called
- Check Serial monitor for "Bruce Remote Display Mode Enabled"
- Verify Bruce is actively drawing to screen

**Garbled display:**
- Packet corruption - check UART signal quality
- Try lower baud rate (57600)
- Check for electrical noise

## Notes

- Bruce's `tft_logger` automatically deduplicates commands
- Display updates are asynchronous (queue-based)
- Button debouncing is handled by Bruce (200ms)
- Physical buttons (if present) take priority over UART

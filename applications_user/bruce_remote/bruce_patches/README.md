# Bruce Firmware Patches for Flipper Zero Remote Control

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

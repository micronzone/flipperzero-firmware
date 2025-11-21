# Bruce Remote - ESP32-C5 UART Testing & Implementation

Complete documentation for testing and debugging UART communication between ESP32-C5 (Bruce firmware) and Flipper Zero (Bruce Remote app).

## Quick Start

**Current Problem**: Flipper shows "RX:0/0" - no data received from ESP32

**Solution**: Follow these guides in order:

1. **[TESTING_PROCEDURE.md](TESTING_PROCEDURE.md)** ← **START HERE**
   - Step-by-step testing from basic hardware to full protocol
   - 6 phases with clear success criteria
   - Troubleshooting decision tree

2. **[ESP32_C5_PINOUT.md](ESP32_C5_PINOUT.md)**
   - Official ESP32-C5 UART pin mapping
   - Flipper Zero GPIO pinout
   - Connection diagrams
   - Voltage verification

3. **[FLIPPER_DEBUGGING.md](FLIPPER_DEBUGGING.md)**
   - Flipper UART configuration details
   - How to read Flipper logs
   - Alternative UART channels (LPUART)
   - Common error messages explained

4. **[IMPLEMENTATION_GUIDE.md](IMPLEMENTATION_GUIDE.md)**
   - Complete Bruce firmware modification guide
   - Add `startAsyncSerial()` function
   - Modify `interface.cpp` for UART input
   - platformio.ini configuration

5. **[TEST_UART.cpp](TEST_UART.cpp)**
   - Ready-to-use ESP32 test code
   - Heartbeat, loopback, echo tests
   - Copy-paste into your Bruce project

## File Overview

```
bruce_patches/
├── README.md                    ← You are here
├── TESTING_PROCEDURE.md         ← Complete testing guide (Phase 1-6)
├── ESP32_C5_PINOUT.md          ← Hardware pinout & connections
├── FLIPPER_DEBUGGING.md        ← Flipper-side debugging
├── IMPLEMENTATION_GUIDE.md     ← Bruce firmware modifications
└── TEST_UART.cpp               ← ESP32 test code snippets
```

## What Each Guide Covers

### TESTING_PROCEDURE.md
**Use when**: You want to systematically test UART step-by-step

**Contents**:
- Phase 1: Hardware verification (power, ground, pins)
- Phase 2: ESP32 standalone tests (TX, loopback)
- Phase 3: Flipper standalone tests (UART init)
- Phase 4: Basic communication (one-way, two-way)
- Phase 5: Bruce protocol tests (display stream, buttons)
- Phase 6: Performance tests (stress, long-duration)

**Time**: 1-3 hours depending on issues found

---

### ESP32_C5_PINOUT.md
**Use when**: You need to verify pin numbers or connections

**Contents**:
- ESP32-C5-DevKitC-1 pinout diagram
- UART0 pins: GPIO11 (TX), GPIO12 (RX)
- Flipper Zero GPIO header pinout
- Pin 13 (TX/PB6), Pin 14 (RX/PB7)
- Voltage levels (3.3V)
- Cable recommendations

**Key Info**:
```
ESP32 GPIO11 (TX)  →  Flipper Pin 14 (RX)
ESP32 GPIO12 (RX)  ←  Flipper Pin 13 (TX)
ESP32 GND          ↔  Flipper GND
```

---

### FLIPPER_DEBUGGING.md
**Use when**: Flipper isn't receiving data or you need detailed logs

**Contents**:
- Current Flipper UART configuration
- How to enable trace-level logging
- What each log message means
- Alternative UART channels (USART vs LPUART)
- RX:0/0 diagnosis flowchart
- Error messages explained:
  - "Frame Error - baud rate mismatch?"
  - "Overrun Error - data loss!"
  - "Noise Error - check connections"

**Key Commands**:
```bash
# View Flipper logs
log set BruceUart trace
log set BruceRemote trace
```

---

### IMPLEMENTATION_GUIDE.md
**Use when**: Basic UART works, now implementing full Bruce integration

**Contents**:
- Add `startAsyncSerial()` to tftLogger
- Modify `interface.cpp` for:
  - Serial initialization
  - Display streaming
  - UART button input
- platformio.ini configuration
- Build and flash instructions
- Expected output at each step

**Key Code Additions**:
1. `tftLogger.h`: Add `void startAsyncSerial();`
2. `tftLogger.cpp`: Implement FreeRTOS streaming task
3. `interface.cpp`: Add Serial.begin(115200) and UART input handler

---

### TEST_UART.cpp
**Use when**: You need quick test code to verify UART

**Contents**:
- Test 1: Basic Serial output test
- Test 2: Continuous heartbeat test
- Test 3: UART loopback test (echo mode)
- Test 4: Binary pattern test
- Test 5: Complete integration test

**Usage**:
```cpp
// Copy functions into Bruce firmware interface.cpp
void _setup_gpio() {
    Serial.begin(115200);
    Serial.println("UART Test Mode");
}

void loop() {
    uart_complete_test();  // Run all tests
}
```

---

## Recommended Workflow

### For First-Time Setup

1. **Read** `TESTING_PROCEDURE.md` fully
2. **Prepare** hardware (ESP32, Flipper, cables, multimeter)
3. **Follow** Phase 1-4 of testing procedure
4. **If Phase 4 succeeds**, move to `IMPLEMENTATION_GUIDE.md`
5. **If Phase 4 fails**, use `FLIPPER_DEBUGGING.md`

### For Debugging "RX:0/0" Issue

1. **Check** `ESP32_C5_PINOUT.md` - verify connections
2. **Run** `TEST_UART.cpp` Test 2.1 on ESP32
3. **Enable** Flipper logs per `FLIPPER_DEBUGGING.md`
4. **Look for**:
   - ESP32 monitor shows "Heartbeat 1, 2, 3..."
   - Flipper logs show "[T][BruceUart] RX: 0x..."
5. **If no RX logs**, problem is hardware/connections
6. **If RX logs present**, problem is protocol parsing

### For Bruce Protocol Implementation

1. **Verify** basic UART works (Phase 4 of testing)
2. **Follow** `IMPLEMENTATION_GUIDE.md` step-by-step
3. **Test** after each modification:
   - After adding `startAsyncSerial()`, rebuild and flash
   - Check ESP32 logs for "[tftLogger] Serial streaming started"
   - Verify Flipper RX counter increases
4. **Debug** display rendering if needed

---

## Common Issues and Solutions

### Issue: RX:0/0 on Flipper

**Diagnosis**:
1. Check ESP32 monitor - is it sending? → If NO, see `IMPLEMENTATION_GUIDE.md`
2. Check Flipper logs - any `[T][BruceUart] RX:`? → If NO, hardware issue
3. Check connections per `ESP32_C5_PINOUT.md`

**Solutions**:
- `TESTING_PROCEDURE.md` Phase 1-2 (hardware)
- `ESP32_C5_PINOUT.md` (correct pins)
- `TEST_UART.cpp` Test 2.1 (verify ESP32 sends)

---

### Issue: Flipper shows "Frame Error"

**Cause**: Baud rate mismatch

**Solutions**:
- ESP32: Ensure `Serial.begin(115200);`
- Flipper: Check `bruce_remote_app.h` has `#define BRUCE_BAUD_RATE 115200`
- See `FLIPPER_DEBUGGING.md` Section 9

---

### Issue: ESP32 doesn't receive button presses

**Diagnosis**:
1. Check Flipper TX (Pin 13) to ESP32 RX (GPIO12) connection
2. Verify `InputHandler()` has UART input code

**Solutions**:
- `ESP32_C5_PINOUT.md` (verify RX pin)
- `IMPLEMENTATION_GUIDE.md` Step 2B (InputHandler modification)
- `TEST_UART.cpp` Test 4.2 (one-way Flipper→ESP32)

---

### Issue: Display shows but is garbled

**Cause**: Coordinate scaling or font size mismatch

**Solutions**:
- This is normal at first!
- Basic communication works ✓
- Optimization needed (out of scope for these guides)
- Adjust coordinate mapping in `display_parser.c`

---

## Hardware Requirements

### Minimum

- ESP32-C5-DevKitC-1 board (or ESP32-S3 alternative)
- Flipper Zero
- 3x jumper wires (TX, RX, GND)
- 2x USB cables (for power/programming)

### Recommended

- Multimeter (voltage + continuity checks)
- Breadboard (optional, but can be less reliable)
- Logic analyzer (for advanced debugging)

### Optional

- Oscilloscope (signal quality verification)
- Second computer (ESP32 monitor + Flipper logs simultaneously)

---

## Expected Timeline

| Task | Time | Success Rate |
|------|------|--------------|
| Read all documentation | 30 min | 100% |
| Phase 1-2: Hardware setup | 30 min | 95% |
| Phase 3-4: Basic UART | 30 min | 80% |
| Phase 5: Bruce protocol | 1 hour | 70% |
| Debugging (if issues) | 1-4 hours | Varies |
| **Total (smooth run)** | **2-3 hours** | - |
| **Total (with issues)** | **4-6 hours** | - |

---

## Success Criteria

You're done when:

1. ✓ Flipper shows "RX:142/0" (counter increasing, not 0/0)
2. ✓ Flipper screen shows Bruce menu (even if garbled)
3. ✓ Flipper buttons control Bruce navigation
4. ✓ ESP32 logs show "[tftLogger] Serial streaming started"
5. ✓ No UART errors in logs (Frame/Overrun/Noise)

---

## Next Steps After Success

### Optimization

1. Adjust coordinate scaling in `display_parser.c`:
   ```c
   // Map 320x240 TFT to 128x64 Flipper screen
   uint8_t flipper_x = (bruce_x * 128) / 320;
   uint8_t flipper_y = (bruce_y * 64) / 240;
   ```

2. Map font sizes:
   ```c
   // TFT fonts → Flipper fonts
   if(tft_font_size > 16) use FontPrimary;
   else use FontSecondary;
   ```

3. Add status indicators (battery, WiFi signal)

### Release Build

1. Remove debug logs (or make conditional)
2. Test on multiple ESP32 boards
3. Create icon for app (10x10px PNG)
4. Write user documentation

---

## Support and Troubleshooting

### Self-Help Checklist

Before asking for help:

- [ ] Read relevant guide completely
- [ ] Tried all tests in `TESTING_PROCEDURE.md` Phases 1-4
- [ ] Checked connections per `ESP32_C5_PINOUT.md`
- [ ] Verified baud rate is 115200 on both sides
- [ ] Confirmed TX-RX crossover (not TX-TX)
- [ ] Tested with `TEST_UART.cpp` code
- [ ] Enabled Flipper debug logs (`FLIPPER_DEBUGGING.md`)
- [ ] Checked ESP32 serial monitor output

### Logs to Collect

When reporting issues, include:

1. **ESP32 Serial Monitor Output**:
   ```
   === ESP32-C5 UART TEST ===
   Heartbeat 1
   Heartbeat 2
   ...
   ```

2. **Flipper Logs** (via qFlipper CLI):
   ```
   [I][BruceUart] UART initialized...
   [T][BruceUart] RX: 0x...
   ```

3. **Hardware Setup Photo** (showing connections)

4. **Voltage Measurements**:
   - ESP32 TX (GPIO11): ? V
   - Flipper RX (Pin 14): ? V
   - GND continuity: ? Ω

### Common Tools

- **PlatformIO**: ESP32 build/flash
- **qFlipper**: Flipper firmware management
- **Serial Monitor**: ESP32 debugging
- **Logic Analyzer**: Signal verification (optional)

---

## Version History

- **2025-11-21**: Created comprehensive testing documentation
  - Added TESTING_PROCEDURE.md
  - Added ESP32_C5_PINOUT.md
  - Added FLIPPER_DEBUGGING.md
  - Updated IMPLEMENTATION_GUIDE.md
  - Added TEST_UART.cpp

- **2025-11-20**: Initial IMPLEMENTATION_GUIDE.md
  - Bruce firmware modification instructions

---

## Credits

- **Bruce Firmware**: [https://github.com/pr3y/Bruce](https://github.com/pr3y/Bruce)
- **Flipper Zero**: [https://flipperzero.one/](https://flipperzero.one/)
- **ESP32-C5**: Espressif Systems

---

## License

These documentation files are provided as-is for educational purposes. Bruce firmware and Flipper firmware have their own respective licenses.

---

**Quick Links**:

| Need to... | See |
|------------|-----|
| Start from scratch | [TESTING_PROCEDURE.md](TESTING_PROCEDURE.md) |
| Verify pin connections | [ESP32_C5_PINOUT.md](ESP32_C5_PINOUT.md) |
| Debug Flipper side | [FLIPPER_DEBUGGING.md](FLIPPER_DEBUGGING.md) |
| Modify Bruce firmware | [IMPLEMENTATION_GUIDE.md](IMPLEMENTATION_GUIDE.md) |
| Get test code | [TEST_UART.cpp](TEST_UART.cpp) |

**TL;DR**: RX:0/0? → Run Phase 1-4 of [TESTING_PROCEDURE.md](TESTING_PROCEDURE.md)

---

## Legacy Information

This directory previously contained modified files for the Bruce ESP32 firmware to enable remote control from Flipper Zero.

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

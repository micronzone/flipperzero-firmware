# ESP32-C5 ↔ Flipper Zero UART Testing Procedure

Complete step-by-step testing guide to verify UART communication works **before** implementing the Bruce protocol.

## Phase 1: Hardware Verification (10 minutes)

### Test 1.1: Power and Ground

**Goal**: Verify both devices are properly powered

**Steps**:
1. Power ESP32-C5 via USB
2. Power Flipper Zero (battery or USB)
3. Use multimeter to check:
   - ESP32 3V3 pin → **3.0-3.6V**
   - Flipper Pin 1 (3V3) → **3.0-3.6V**
   - ESP32 GND to Flipper GND → **<1Ω resistance** (continuity)

**Success Criteria**:
- ✓ Both devices powered
- ✓ Common ground established
- ✓ No short circuits

**If Failed**:
- Check USB cables
- Charge Flipper battery
- Verify power switch on ESP32 board

---

### Test 1.2: Pin Identification

**Goal**: Physically locate and verify correct UART pins

**ESP32-C5-DevKitC-1**:
```
Find GPIO11 and GPIO12 on the board:

     [USB-C Port]
         ↓
    [ ESP32-C5 ]
         │
    ┌────┴────┐
    │ GPIO11  │ ← TX (UART0_TXD) [Find this pin]
    │ GPIO12  │ ← RX (UART0_RXD) [Find this pin]
    │  GND    │ ← Ground
    └─────────┘
```

**Flipper Zero**:
```
GPIO connector (back of device):

Pin 13 (PB6) = TX [6th pin from bottom on right side]
Pin 14 (PB7) = RX [7th pin from bottom on right side]
Pin 8/10/11  = GND [Multiple ground pins]
```

**Success Criteria**:
- ✓ Pins physically identified on both boards
- ✓ Pin numbers match datasheet
- ✓ Wires/cables available for connection

---

### Test 1.3: Initial Connection

**Goal**: Wire the two devices together correctly

**Connections**:
```
ESP32-C5              Cable Color       Flipper Zero
--------              -----------       ------------
GPIO11 (TX)  ────────  Red/Yellow  ───→  Pin 14 (RX)
GPIO12 (RX)  ────────  Blue/Green  ←───  Pin 13 (TX)
GND          ────────  Black       ───   Pin 8, 10, or 11
```

**CRITICAL**: TX crosses to RX! Do NOT connect TX to TX.

**Success Criteria**:
- ✓ Connections physically secure (no loose wires)
- ✓ TX-RX crossover verified
- ✓ Ground connected
- ✓ No accidental shorts (TX/RX to GND, etc.)

---

## Phase 2: ESP32 Standalone Test (15 minutes)

### Test 2.1: ESP32 Serial Monitor Test

**Goal**: Verify ESP32 can send data via UART0

**Code**: Add to Bruce firmware `interface.cpp` or create test sketch:

```cpp
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n=== ESP32-C5 UART TEST ===");
    Serial.println("If you see this, TX works!");
}

void loop() {
    static int counter = 0;
    Serial.print("Heartbeat ");
    Serial.println(counter++);
    delay(1000);
}
```

**Flash and Monitor**:
```bash
pio run -e [your-board] -t upload
pio device monitor -b 115200
```

**Expected Output**:
```
=== ESP32-C5 UART TEST ===
If you see this, TX works!
Heartbeat 0
Heartbeat 1
Heartbeat 2
...
```

**Success Criteria**:
- ✓ Text appears in serial monitor
- ✓ Updates every 1 second
- ✓ No garbled characters

**If Failed**:
- Wrong COM port selected
- Baud rate mismatch (ensure 115200)
- Code not flashed (check build errors)

---

### Test 2.2: ESP32 TX Pin Voltage Test

**Goal**: Verify UART TX pin is active

**Steps**:
1. Keep ESP32 running Test 2.1 code
2. Measure voltage on **GPIO11** (TX pin) with multimeter
3. Set multimeter to **DC voltage mode**

**Expected Results**:
- Idle (between transmissions): **~3.3V**
- During transmission: **Fluctuating** (~1.5-2.0V average on DC meter)
- Never: 0V continuously (means TX not working)

**Success Criteria**:
- ✓ Idle voltage is 3.3V
- ✓ Voltage drops during `Serial.println()`
- ✓ Returns to 3.3V after transmission

---

### Test 2.3: ESP32 Loopback Test

**Goal**: Verify ESP32 UART hardware works (both TX and RX)

**Physical Change**: Temporarily **connect GPIO11 to GPIO12** with a jumper wire (loopback)

**Code**:
```cpp
void setup() {
    Serial.begin(115200);
    delay(100);
}

void loop() {
    // Send
    Serial.println("LOOP");
    delay(100);

    // Receive
    if (Serial.available()) {
        String received = Serial.readStringUntil('\n');
        if (received == "LOOP") {
            Serial.println("✓ LOOPBACK OK!");
        } else {
            Serial.print("✗ Expected 'LOOP', got: ");
            Serial.println(received);
        }
    }
    delay(1000);
}
```

**Expected Output**:
```
LOOP
✓ LOOPBACK OK!
LOOP
✓ LOOPBACK OK!
```

**Success Criteria**:
- ✓ "LOOPBACK OK" appears
- ✓ No "Expected 'LOOP', got" errors

**After Success**: Remove loopback jumper, reconnect to Flipper!

---

## Phase 3: Flipper Standalone Test (15 minutes)

### Test 3.1: Flipper UART App Check

**Goal**: Verify Bruce Remote app is installed correctly

**Steps**:
1. On Flipper, navigate to **Apps → GPIO**
2. Look for **Bruce Remote** app
3. Launch it

**Expected**:
```
┌─────────────────┐
│ Bruce Remote    │
│                 │
│ Start Remote    │
│ Control         │
└─────────────────┘
```

**If App Missing**:
```bash
# Build and install
./fbt fap_bruce_remote
./fbt launch_app APPSRC=applications_user/bruce_remote
```

---

### Test 3.2: Flipper UART Initialization Check

**Goal**: Verify Flipper initializes UART correctly

**Steps**:
1. Launch Bruce Remote app
2. Select "Start Remote Control"
3. Connect via qFlipper CLI or serial to see logs

**Expected Logs**:
```
[I][BruceUart] UART initialized: 115200 baud, Pin 13(TX)/14(RX)
[I][BruceUart] Async RX started with error reporting
[I][BruceUart] Worker thread started
```

**Success Criteria**:
- ✓ All three log lines appear
- ✓ No "failed to acquire" errors
- ✓ Baud rate is 115200

---

### Test 3.3: Flipper RX Pin Voltage Test

**Goal**: Verify Flipper is ready to receive

**Steps**:
1. Keep Bruce Remote app running
2. Disconnect ESP32 TX from Flipper RX (Pin 14)
3. Measure voltage on Flipper **Pin 14** with multimeter

**Expected**:
- Pin 14 (RX) floating: **~0V to 0.3V** (no pull-up)
- ESP32 TX connected idle: **~3.3V**

**Success Criteria**:
- ✓ Flipper RX pin reads ~3.3V when ESP32 TX is connected

---

## Phase 4: Basic Communication Test (20 minutes)

### Test 4.1: One-Way ESP32 → Flipper

**Goal**: Verify Flipper can receive data from ESP32

**ESP32 Code** (in Bruce `interface.cpp` or test sketch):
```cpp
void setup() {
    Serial.begin(115200);
    delay(100);
}

void loop() {
    // Send simple pattern
    Serial.write(0xAA);  // Header
    Serial.write(0x55);  // Test
    Serial.flush();

    delay(1000);
}
```

**Flipper**: Launch Bruce Remote app, check logs

**Expected Flipper Logs**:
```
[T][BruceUart] RX: 0xAA
[T][BruceUart] RX: 0x55
[D][BruceUart] Received 2 bytes from stream
```

**Expected Flipper Screen**:
```
RX:2/0  (or RX:4/0, RX:6/0... incrementing!)
```

**Success Criteria**:
- ✓ RX counter increases (not stuck at 0/0)
- ✓ Logs show received bytes
- ✓ Bytes match what ESP32 sent

**If RX:0/0**:
1. Check ESP32 is running (monitor shows output)
2. Check cable (TX-RX crossover)
3. Check baud rate (both 115200)
4. Try different cable/wires

---

### Test 4.2: One-Way Flipper → ESP32

**Goal**: Verify ESP32 can receive data from Flipper

**ESP32 Code**:
```cpp
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("Waiting for Flipper...");
}

void loop() {
    if (Serial.available()) {
        uint8_t byte = Serial.read();
        Serial.print("Got: 0x");
        Serial.print(byte, HEX);
        Serial.print(" = '");
        Serial.print((char)byte);
        Serial.println("'");
    }
}
```

**Flipper**: Launch Bruce Remote, press buttons (Up/Down/etc.)

**Expected ESP32 Output**:
```
Waiting for Flipper...
Got: 0x55 = 'U'   <- Pressed Up
Got: 0x44 = 'D'   <- Pressed Down
Got: 0x53 = 'S'   <- Pressed Select
```

**Success Criteria**:
- ✓ Button presses appear on ESP32
- ✓ Correct characters received (U, D, S, E, L, R)
- ✓ No delays >100ms

**If No Output**:
1. Check Flipper TX (Pin 13) to ESP32 RX (GPIO12)
2. Verify button handler in bruce_remote_app.c calls `uart_worker_send_button()`
3. Check ESP32 RX pin in code

---

### Test 4.3: Two-Way Echo Test

**Goal**: Full-duplex communication works

**ESP32 Code**:
```cpp
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("Echo mode ready!");
}

void loop() {
    // Echo back anything received
    if (Serial.available()) {
        char c = Serial.read();
        Serial.print("Echo: ");
        Serial.println(c);
    }
}
```

**Flipper**: Press buttons, check screen shows "Echo: X"

**Expected**:
- Flipper sends 'U'
- ESP32 receives 'U', prints "Echo: U"
- Flipper receives "Echo: U" and displays it

**Success Criteria**:
- ✓ Bidirectional communication works
- ✓ No data corruption
- ✓ Both RX and TX counters increase

---

## Phase 5: Bruce Protocol Test (30 minutes)

### Test 5.1: Bruce Display Stream Test

**Goal**: Verify Bruce sends display data in correct format

**ESP32 Code**: Implement from `IMPLEMENTATION_GUIDE.md`:
- Add `startAsyncSerial()` to tftLogger
- Call `tft.startAsyncSerial()` in `_post_setup_gpio()`

**Expected ESP32 Logs**:
```
[tftLogger] Serial streaming started
Header: AA, Size: 07, Func: 99  <- SCREEN_INFO packet
Header: AA, Size: 05, Func: 00  <- FILLSCREEN packet
Header: AA, Size: 0C, Func: 10  <- DRAWSTRING packet
```

**Expected Flipper Logs**:
```
[D][BruceParser] Header detected at offset 0
[I][BruceParser] Complete packet received: 7 bytes
[I][BruceParser] Function: SCREEN_INFO (0x99)
```

**Success Criteria**:
- ✓ ESP32 sends packets continuously
- ✓ Flipper recognizes packet headers (0xAA)
- ✓ Packet sizes match protocol

---

### Test 5.2: Display Rendering Test

**Goal**: Flipper screen shows Bruce UI

**Expected Flipper Screen**:
```
┌────────────────┐
│ [Partial text] │ <- Bruce menu (might be garbled initially)
│ > WiFi         │
│   BLE          │
│                │
│ RX:45/0        │ <- Packets received
└────────────────┘
```

**Success Criteria**:
- ✓ Screen updates when Bruce menu changes
- ✓ Some text visible (exact rendering may be off)
- ✓ RX counter keeps increasing
- ✓ No crashes or freezes

**Known Issues at This Stage**:
- Text might be misaligned (coordinate scaling needs tuning)
- Font sizes may be wrong
- Colors might not match

---

### Test 5.3: Button Input Test

**Goal**: Flipper buttons control Bruce menu

**Steps**:
1. Launch Bruce on ESP32 (see menu on its screen)
2. Launch Bruce Remote on Flipper
3. Press **Down** on Flipper
4. Observe Bruce menu cursor moves down on ESP32 screen

**Expected ESP32 Logs**:
```
[INPUT] Received byte: 0x44 = 'D'
NextPress = true
```

**Expected Behavior**:
- ✓ Menu cursor moves with Flipper buttons
- ✓ Select button activates menu items
- ✓ Back button returns to previous screen
- ✓ Latency <200ms

---

## Phase 6: Performance and Stability Test (Optional)

### Test 6.1: Long-Duration Test

**Goal**: Verify no memory leaks or crashes

**Steps**:
1. Run both devices for 30 minutes
2. Navigate Bruce menus actively
3. Check for:
   - Increasing RX dropped counter
   - Flipper/ESP32 crashes
   - Memory warnings in logs

**Success Criteria**:
- ✓ RX dropped stays at 0 (or <1% of received)
- ✓ No crashes
- ✓ Latency remains constant

---

### Test 6.2: Stress Test

**Goal**: Push UART to limits

**ESP32 Code**:
```cpp
void loop() {
    // Spam data
    for(int i = 0; i < 100; i++) {
        Serial.write(0xAA);
        Serial.write(i);
    }
    Serial.flush();
    delay(10);  // 10KB/sec
}
```

**Expected**:
- Flipper handles burst without overrun errors
- RX dropped counter may increase slightly

**Success Criteria**:
- ✓ No "Overrun Error" logs
- ✓ Dropped packets <5%
- ✓ System remains responsive

---

## Troubleshooting Decision Tree

```
                  Start
                    |
            Are both devices powered?
                    |
            ┌───────┴───────┐
           NO              YES
            |                |
    Fix power issue   Is ground connected?
                            |
                    ┌───────┴───────┐
                   NO              YES
                    |                |
            Connect GND      ESP32 sending data?
                                    |
                            ┌───────┴───────┐
                           NO              YES
                            |                |
                Check code/flash    Flipper RX:0/0?
                                            |
                                    ┌───────┴───────┐
                                   YES              NO
                                    |                |
                            Check cables     SUCCESS!
                            Check baud       (move to
                            Check TX/RX      protocol
                             crossover)      testing)
```

## Quick Reference: Expected Values

| Test Point | Expected Value | Tolerance |
|-----------|----------------|-----------|
| ESP32 3V3 pin | 3.3V | ±0.3V |
| ESP32 TX idle | 3.3V | ±0.2V |
| Flipper 3V3 pin | 3.3V | ±0.2V |
| Flipper RX connected | 3.3V | ±0.2V |
| GND resistance | 0Ω | <1Ω |
| Baud rate | 115200 | Exact! |
| UART data bits | 8 | Exact! |
| Parity | None | Exact! |
| Stop bits | 1 | Exact! |

## Success Checkpoints

After completing all phases:

- [x] **Phase 1**: Hardware connected correctly
- [x] **Phase 2**: ESP32 can send data via UART
- [x] **Phase 3**: Flipper UART initialized
- [x] **Phase 4**: Basic ESP32 ↔ Flipper communication works
- [x] **Phase 5**: Bruce protocol packets recognized
- [x] **Phase 6**: Stable under load (optional)

## Files Reference

Created test/documentation files:
1. `/home/user/flipperzero-firmware/applications_user/bruce_remote/bruce_patches/TEST_UART.cpp`
   - ESP32 test code snippets
2. `/home/user/flipperzero-firmware/applications_user/bruce_remote/bruce_patches/ESP32_C5_PINOUT.md`
   - Complete pinout diagrams
3. `/home/user/flipperzero-firmware/applications_user/bruce_remote/bruce_patches/FLIPPER_DEBUGGING.md`
   - Flipper-side debugging
4. `/home/user/flipperzero-firmware/applications_user/bruce_remote/bruce_patches/IMPLEMENTATION_GUIDE.md`
   - Full Bruce firmware modification guide

---

**Timeline Estimate**:
- Quick test (Phases 1-4): **1 hour**
- Full test (all phases): **2-3 hours**
- If issues found: Add 1-4 hours debugging

**Required Tools**:
- Multimeter (voltage + continuity)
- USB cables for both devices
- Jumper wires (3-4 wires)
- Optional: Logic analyzer or oscilloscope

**Next Steps After Passing All Tests**:
1. Optimize display rendering (coordinate scaling)
2. Implement font size mapping
3. Add status indicators (battery, WiFi, etc.)
4. Package as release build

Good luck! 🚀

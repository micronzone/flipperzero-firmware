# Flipper Zero UART Debugging Guide for Bruce Remote

## Current Flipper Configuration Verification

### 1. UART Channel Configuration

**File**: `/home/user/flipperzero-firmware/applications_user/bruce_remote/bruce_remote_app.h`

```c
#define BRUCE_BAUD_RATE 115200
#define BRUCE_UART_CHANNEL FuriHalSerialIdUsart
```

**Analysis**:
- ✓ Uses **FuriHalSerialIdUsart** (USART on Pin 13/14)
- ✓ Baud rate: 115200 (matches ESP32)
- ✓ Pin 13 (PB6) = TX
- ✓ Pin 14 (PB7) = RX

### 2. UART Worker Implementation

**File**: `uart_worker.c`

**Current Features**:
- ✓ Async RX with interrupt callback
- ✓ Stream buffer (2048 bytes)
- ✓ Error reporting enabled (overrun, frame, noise errors)
- ✓ Debug logging on every byte (FURI_LOG_T)

**RX Callback Flow**:
```
Hardware RX IRQ
    ↓
uart_on_irq_cb()
    ↓
furi_hal_serial_async_rx()  <- Read byte
    ↓
furi_stream_buffer_send()   <- Buffer byte
    ↓
uart_worker_thread()        <- Process buffer
    ↓
callback(data, len)         <- Send to app
    ↓
display_parser_parse()      <- Parse Bruce protocol
```

### 3. Alternative UART Channels

The Flipper has **2 UART channels**:

| Channel | Pins | Speed | Notes |
|---------|------|-------|-------|
| **FuriHalSerialIdUsart** | Pin 13 (TX), Pin 14 (RX) | Up to 2 Mbps | Standard UART, used by most apps |
| **FuriHalSerialIdLpuart** | Pin 15 (TX), Pin 16 (RX) | Up to 9600 bps | Low-power UART, slower but more reliable |

**To try LPUART** (if USART has issues):

Edit `bruce_remote_app.h`:
```c
// Change from:
#define BRUCE_UART_CHANNEL FuriHalSerialIdUsart

// To:
#define BRUCE_UART_CHANNEL FuriHalSerialIdLpuart
```

**WARNING**: LPUART pins are different (15/16) and limited to 9600 baud!

### 4. Enable Maximum Debug Logging

#### A. Enable Trace-Level Logs in uart_worker.c

Current logging levels in `uart_worker.c`:

```c
// Line 18: FURI_LOG_T - Already enabled! (Trace level)
FURI_LOG_T("BruceUart", "RX: 0x%02X", byte);

// Line 23-30: Error logs - Already enabled!
FURI_LOG_E("BruceUart", "Overrun Error - data loss!");
FURI_LOG_E("BruceUart", "Frame Error - baud rate mismatch?");
FURI_LOG_W("BruceUart", "Noise Error - check connections");
```

**These logs are already active!** You can view them with:

```bash
# On Flipper (via qFlipper CLI or serial)
log set BruceUart trace
log set BruceRemote trace
```

#### B. Add Raw Byte Dump in display_parser.c

If you want to see **every byte received**, add to `display_parser.c`:

```c
// In display_parser_parse() function
void display_parser_parse(DisplayParser* parser, uint8_t* data, size_t len) {
    // ADD THIS AT THE START:
    #ifdef DEBUG_RAW_BYTES
    FURI_LOG_I("BruceParser", "Raw RX (%zu bytes):", len);
    for(size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
        if((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");
    #endif

    // ... existing parsing code ...
}
```

Then compile with:
```bash
./fbt fap_bruce_remote EXTRA_DEFINES=-DDEBUG_RAW_BYTES
```

### 5. Testing UART Reception

#### Test 1: Verify UART is Initialized

Check Flipper logs when app starts:

```
[I][BruceUart] UART initialized: 115200 baud, Pin 13(TX)/14(RX)
[I][BruceUart] Async RX started with error reporting
[I][BruceUart] Worker thread started
```

If you **don't see these**, the UART didn't initialize.

#### Test 2: Send Test Data from ESP32

On ESP32, add to loop():
```cpp
void loop() {
    static unsigned long lastSend = 0;
    if (millis() - lastSend > 1000) {
        lastSend = millis();
        Serial.write(0xAA);  // Header byte
        Serial.write(0x55);  // Test byte
        Serial.flush();
    }
}
```

Expected Flipper logs:
```
[T][BruceUart] RX: 0xAA
[T][BruceUart] RX: 0x55
[D][BruceUart] Received 2 bytes from stream
```

If you see **no [T][BruceUart] RX logs**:
- ❌ No data arriving at Flipper
- Check hardware connections
- Check ESP32 is actually sending (monitor serial output)

#### Test 3: Detect UART Errors

If you see these logs, there are signal issues:

```
[E][BruceUart] Frame Error - baud rate mismatch?
```
→ **Fix**: ESP32 and Flipper baud rates don't match. Both must be 115200.

```
[E][BruceUart] Overrun Error - data loss!
```
→ **Fix**: Data arriving too fast. Add `Serial.flush()` on ESP32 or reduce send rate.

```
[W][BruceUart] Noise Error - check connections
```
→ **Fix**: Electrical noise. Use shorter cables, twist TX/RX together, add ground.

### 6. RX Counter Stuck at 0/0 - Diagnosis

The RX counter is updated in `bruce_remote_app.c`:

```c
snprintf(stats, sizeof(stats), "RX:%lu/%lu",
    model->display_parser->display_buffer->packets_received,
    model->display_parser->display_buffer->packets_dropped);
```

**If RX:0/0 never changes**, the problem is **before** the display parser:

#### Diagnostic Steps:

**Step 1**: Check UART receives **any** data

Add to `uart_worker_thread()` in `uart_worker.c`:

```c
static int32_t uart_worker_thread(void* context) {
    UartWorker* worker = (UartWorker*)context;
    uint8_t data[256];
    FURI_LOG_I("BruceUart", "Worker thread started");

    while(worker->running) {
        size_t len = furi_stream_buffer_receive(worker->rx_stream, data, sizeof(data), 100);

        // ADD THIS LOG:
        if(len > 0) {
            FURI_LOG_I("BruceUart", "!!! RECEIVED %zu BYTES !!!", len);  // Should appear if UART works
        }

        if(len > 0) {
            FURI_LOG_D("BruceUart", "Received %zu bytes from stream", len);
            if(worker->callback) {
                worker->callback(data, len, worker->callback_context);
            }
        }
    }
    // ...
}
```

**Step 2**: Check callback is invoked

Add to `uart_rx_callback()` in `bruce_remote_app.c`:

```c
static void uart_rx_callback(uint8_t* data, size_t len, void* context) {
    BruceRemoteApp* app = (BruceRemoteApp*)context;

    // ADD THIS LOG:
    FURI_LOG_I("BruceRemote", "!!! CALLBACK INVOKED: %zu bytes !!!", len);

    // Parse display data
    display_parser_parse(app->display_parser, data, len);

    // ...
}
```

**Step 3**: Check parser receives data

Add to `display_parser_parse()` in `display_parser.c`:

```c
void display_parser_parse(DisplayParser* parser, uint8_t* data, size_t len) {
    // ADD THIS LOG:
    FURI_LOG_I("BruceParser", "!!! PARSER GOT %zu BYTES !!!", len);

    // ... existing code ...
}
```

**Interpretation**:

| Log Present | Problem Location |
|-------------|------------------|
| No `[T][BruceUart] RX:` | Hardware/ESP32 not sending |
| `RX:` logs but no `RECEIVED` | Stream buffer issue |
| `RECEIVED` but no `CALLBACK` | Callback not registered |
| `CALLBACK` but no `PARSER` | Parser crash/not called |
| `PARSER` but RX:0/0 | Packet parsing failed |

### 7. Button TX Test (Flipper → ESP32)

To verify Flipper can **send** data:

#### On ESP32 (Bruce), add to `InputHandler()`:

```cpp
void InputHandler() {
    // Debug: Show exactly what arrives
    if (Serial.available()) {
        char c = Serial.read();
        Serial.print("[INPUT] Received byte: 0x");
        Serial.print(c, HEX);
        Serial.print(" = '");
        Serial.print(c);
        Serial.println("'");

        // ... existing button handling ...
    }
}
```

#### On Flipper, press buttons and check ESP32 logs:

Expected ESP32 output:
```
[INPUT] Received byte: 0x55 = 'U'   <- Up button
[INPUT] Received byte: 0x44 = 'D'   <- Down button
[INPUT] Received byte: 0x53 = 'S'   <- Select button
```

If **no output**:
- Flipper TX (Pin 13) not connected to ESP32 RX
- ESP32 RX pin wrong
- Flipper not calling `uart_worker_send_button()`

### 8. Signal Integrity Verification

#### Oscilloscope / Logic Analyzer Check

If available, probe **ESP32 TX pin** (GPIO11) while sending:

Expected waveform for 'A' (0x41) at 115200 baud:
```
Idle (3.3V)
  │
  └─── Start bit (0V, 8.68μs)
  │
  ├─── Bit 0: 1 (3.3V, 8.68μs)  ┐
  ├─── Bit 1: 0 (0V, 8.68μs)    │
  ├─── Bit 2: 0 (0V, 8.68μs)    │ ASCII 'A'
  ├─── Bit 3: 0 (0V, 8.68μs)    │ = 0b01000001
  ├─── Bit 4: 0 (0V, 8.68μs)    │
  ├─── Bit 5: 0 (0V, 8.68μs)    │
  ├─── Bit 6: 1 (3.3V, 8.68μs)  │
  ├─── Bit 7: 0 (0V, 8.68μs)    ┘
  │
  └─── Stop bit (3.3V, 8.68μs)
  │
Idle (3.3V)
```

Bit time = 1 / 115200 = 8.68 microseconds

#### Multimeter Check (Simpler)

1. **Idle voltage**: TX pins should read **~3.3V** when not transmitting
2. **During transmission**: Voltage should fluctuate (multimeter may show ~1.6V average)
3. **Ground**: ESP32 GND to Flipper GND should be **0Ω** (continuity)

### 9. Common Issues and Fixes

#### Issue: "Frame Error - baud rate mismatch?"

**Cause**: ESP32 and Flipper using different baud rates

**Fix**:
```cpp
// ESP32: Ensure this is 115200
Serial.begin(115200);

// Flipper: Check bruce_remote_app.h
#define BRUCE_BAUD_RATE 115200
```

#### Issue: "Overrun Error - data loss!"

**Cause**: ESP32 sending data faster than Flipper can process

**Fix on ESP32**:
```cpp
Serial.write(data, len);
Serial.flush();  // Wait for TX complete
delay(1);        // Small delay between packets
```

**Fix on Flipper**: Increase buffer size in `uart_worker.c`:
```c
#define UART_RX_BUF_SIZE 4096  // Was 2048
```

#### Issue: RX shows some bytes but then stops

**Cause**: Parser crashes on malformed packet

**Fix**: Add error handling to `display_parser_parse()`:
```c
void display_parser_parse(DisplayParser* parser, uint8_t* data, size_t len) {
    // Validate before processing
    if(!parser || !data || len == 0) {
        FURI_LOG_E("BruceParser", "Invalid input!");
        return;
    }

    // ... rest of parsing with bounds checking ...
}
```

### 10. Complete Diagnostic Checklist

Run through this checklist in order:

- [ ] **ESP32 powered on** (LED/screen active)
- [ ] **Flipper powered on** (screen showing Bruce Remote app)
- [ ] **Connections verified**:
  - [ ] ESP32 TX (GPIO11) → Flipper RX (Pin 14)
  - [ ] ESP32 RX (GPIO12) ← Flipper TX (Pin 13)
  - [ ] ESP32 GND ↔ Flipper GND (Pin 8/10/11)
- [ ] **ESP32 code includes**:
  - [ ] `Serial.begin(115200);`
  - [ ] `Serial.println("TEST");` in loop
- [ ] **ESP32 serial monitor shows**:
  - [ ] "TEST" printed every loop
  - [ ] No compilation errors
  - [ ] Correct port selected
- [ ] **Flipper app compiled successfully**:
  - [ ] `./fbt fap_bruce_remote` → no errors
  - [ ] `.fap` file copied to SD card
- [ ] **Flipper logs show**:
  - [ ] "UART initialized: 115200 baud"
  - [ ] "Worker thread started"
  - [ ] "[T][BruceUart] RX: 0x..." (receiving bytes)

### 11. Emergency Recovery: Factory Reset UART

If UART is completely non-functional, try resetting to defaults:

**On Flipper**, add to `uart_worker_alloc()`:

```c
UartWorker* uart_worker_alloc(UartWorkerCallback callback, void* context) {
    // ... existing code ...

    // Force reset UART to known state
    furi_hal_serial_deinit(worker->serial_handle);
    furi_delay_ms(100);

    furi_hal_serial_init(worker->serial_handle, BRUCE_BAUD_RATE);
    furi_hal_serial_configure_framing(
        worker->serial_handle,
        FuriHalSerialDataBits8,
        FuriHalSerialParityNone,
        FuriHalSerialStopBits1
    );

    // ... rest of init ...
}
```

**On ESP32**, force reinit:

```cpp
void _setup_gpio() {
    Serial.end();  // Stop UART
    delay(100);
    Serial.begin(115200, SERIAL_8N1);  // Restart with explicit config
    delay(100);
}
```

### 12. Success Criteria

You know UART is working when you see:

**On ESP32 Serial Monitor**:
```
===== Bruce Firmware =====
Flipper Zero Remote Mode
[Bruce] Enabling remote display...
[tftLogger] Serial streaming started
[INPUT] Received byte: 0x55 = 'U'   <- Flipper button press!
```

**On Flipper Screen**:
```
┌─────────────────┐
│ Bruce Remote    │
│                 │
│ [Bruce Menu]    │ <- ESP32 display mirrored
│ > WiFi Tools    │
│   BLE Tools     │
│                 │
│ RX:142/0        │ <- Counter increasing!
└─────────────────┘
```

**On Flipper Logs** (via qFlipper CLI):
```
[T][BruceUart] RX: 0xAA
[T][BruceUart] RX: 0x07
[D][BruceUart] Received 7 bytes from stream
[I][BruceRemote] CALLBACK INVOKED: 7 bytes
[I][BruceParser] PARSER GOT 7 bytes
[I][BruceParser] Complete packet: func=0x99 (SCREEN_INFO)
```

---

**Next Steps After UART Works**:
1. Verify display rendering (might be garbled initially)
2. Optimize coordinate scaling
3. Map font sizes correctly
4. Test all button inputs
5. Measure and optimize latency

**Need More Help?**
- Check `/home/user/flipperzero-firmware/applications_user/bruce_remote/bruce_patches/TEST_UART.cpp` for test code
- Review `/home/user/flipperzero-firmware/applications_user/bruce_remote/bruce_patches/IMPLEMENTATION_GUIDE.md` for full setup
- Read `/home/user/flipperzero-firmware/applications_user/bruce_remote/bruce_patches/ESP32_C5_PINOUT.md` for pinout reference

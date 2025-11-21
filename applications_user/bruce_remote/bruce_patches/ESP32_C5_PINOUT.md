# ESP32-C5 DevKitC-1 UART Pinout and Configuration

## ESP32-C5 Official UART Pin Mapping

### Hardware Specifications

The ESP32-C5 has **2 UART controllers**:

| UART | Default TX Pin | Default RX Pin | Alternative Pins | Notes |
|------|---------------|----------------|------------------|-------|
| **UART0** | **GPIO11** | **GPIO12** | Can be remapped via GPIO Matrix | Primary UART, used for USB-Serial |
| **UART1** | GPIO5 | GPIO6 | Any GPIO via Matrix | Secondary UART |

### ESP32-C5-DevKitC-1 Board Physical Pinout

```
                    ESP32-C5-DevKitC-1

    3V3  [ ] [ ] GND
  GPIO0  [ ] [ ] GPIO1
  GPIO2  [ ] [ ] GPIO3
  GPIO4  [ ] [ ] GPIO5
  GPIO6  [ ] [ ] GPIO7
  GPIO8  [ ] [ ] GPIO9
 GPIO10  [ ] [ ] GPIO11  <- UART0 TX (U0TXD) *** USE THIS FOR FLIPPER ***
 GPIO12  [ ] [ ] GPIO13  <- UART0 RX (U0RXD) *** USE THIS FOR FLIPPER ***
 GPIO14  [ ] [ ] GPIO15
 GPIO18  [ ] [ ] GPIO19
 GPIO20  [ ] [ ] GPIO21
 GPIO22  [ ] [ ] GPIO23
    GND  [ ] [ ] 5V
```

### Recommended Connection for Flipper Zero

**Default Configuration (using UART0)**:

```
ESP32-C5-DevKitC-1              Flipper Zero
------------------              ------------
GPIO11 (U0TXD)  ───────────────→ Pin 14 (RX / PB7)  [USART RX]
GPIO12 (U0RXD)  ←─────────────── Pin 13 (TX / PB6)  [USART TX]
GND             ─────────────────GND (Pin 8 or 11)
3V3             (Not connected - both boards self-powered)
```

**CRITICAL**: TX and RX must be **crossed over** (TX → RX, RX ← TX)

### Flipper Zero GPIO Pinout Reference

```
Flipper Zero GPIO Header (Top View)
=====================================

Pin 1:  3V3 Out (max 1200mA)
Pin 2:  PA7 (GPIO)
Pin 3:  PA6 (GPIO)
Pin 4:  PA4 (GPIO)
Pin 5:  PB3 (GPIO)
Pin 6:  PB2 (GPIO)
Pin 7:  PC3 (GPIO)
Pin 8:  GND
Pin 9:  5V Out (max 1000mA)
Pin 10: GND
Pin 11: GND
Pin 12: PC1 (GPIO)
Pin 13: PB6 (USART TX) ← Connect to ESP32 RX
Pin 14: PB7 (USART RX) ← Connect to ESP32 TX
Pin 15: PC0 (GPIO)
Pin 16: PB8 (Speaker)
Pin 17: 1-Wire
Pin 18: NC
```

### UART Configuration

Both devices must use **identical** settings:

| Parameter | Value | Why |
|-----------|-------|-----|
| **Baud Rate** | 115200 | Standard rate, good balance of speed/reliability |
| **Data Bits** | 8 | Standard |
| **Parity** | None | No error checking (rely on protocol) |
| **Stop Bits** | 1 | Standard |
| **Voltage** | 3.3V | Both ESP32-C5 and Flipper use 3.3V logic |

### ESP32-C5 UART Initialization Code

#### Method 1: Using Arduino Serial (Recommended for Bruce)

```cpp
void _setup_gpio() {
    // Initialize UART0 at 115200 baud
    Serial.begin(115200);
    delay(100);  // Wait for UART to stabilize

    Serial.println("ESP32-C5 UART0 initialized");
    Serial.printf("TX Pin: GPIO%d\n", U0TXD_GPIO_NUM);
    Serial.printf("RX Pin: GPIO%d\n", U0RXD_GPIO_NUM);
}
```

#### Method 2: Explicit Pin Configuration (if pins need changing)

```cpp
#include <HardwareSerial.h>

void _setup_gpio() {
    // Option A: Use default pins (GPIO11 TX, GPIO12 RX)
    Serial.begin(115200, SERIAL_8N1);

    // Option B: Specify custom pins (ESP32-C5 GPIO matrix allows any GPIO)
    // Serial.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
    // Example: Serial.begin(115200, SERIAL_8N1, 12, 11);

    delay(100);
}
```

#### Method 3: Using IDF UART Driver (Advanced)

```cpp
#include "driver/uart.h"

void _setup_gpio() {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, 11, 12, -1, -1)); // TX, RX, RTS, CTS
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 2048, 2048, 0, NULL, 0));
}
```

### Flipper Zero UART Initialization (Already in bruce_remote)

```c
// From uart_worker.c
FuriHalSerialHandle* handle = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
furi_hal_serial_init(handle, 115200);
furi_hal_serial_async_rx_start(handle, uart_on_irq_cb, worker, true);
```

This uses:
- **FuriHalSerialIdUsart**: Hardware USART (Pin 13/14)
- **115200 baud**: Matches ESP32
- **Async RX**: Non-blocking receive with callback
- **Error reporting enabled**: Detects framing/overrun errors

### Alternative: Using LPUART on Flipper

If you encounter issues with USART, try **LPUART** (low-power UART):

```c
// In bruce_remote_app.h, change:
#define BRUCE_UART_CHANNEL FuriHalSerialIdLpuart  // Instead of FuriHalSerialIdUsart
```

LPUART uses different pins (check Flipper documentation), but has better noise immunity.

### Voltage Level Verification

Both ESP32-C5 and Flipper Zero use **3.3V logic levels**:

| Device | Logic High | Logic Low | Tolerance |
|--------|-----------|-----------|-----------|
| ESP32-C5 | 2.64-3.3V | 0-0.66V | 3.6V max |
| Flipper (STM32WB) | 2.4-3.3V | 0-0.9V | 3.6V max |

**Compatible**: No level shifter needed ✓

### Signal Quality Checks

Use multimeter to verify:

1. **Idle State (no data)**: TX pins should read ~3.3V
2. **Ground**: GND continuity between boards (< 1Ω)
3. **During transmission**: TX pins should pulse between 0V and 3.3V

### Cable Recommendations

| Cable Type | Max Length | Notes |
|-----------|------------|-------|
| Dupont jumper wires | 10-20cm | OK for testing, prone to noise |
| Twisted pair | Up to 1m | Better noise immunity |
| Shielded cable | Up to 3m | Best for long runs |

**Avoid**:
- Breadboards (unreliable contacts)
- Long untwisted wires (>30cm)
- Parallel runs with power cables

### Common Pin Mapping Mistakes

#### ❌ WRONG Connections

```
ESP32 TX  ->  Flipper TX   (both transmitting, no communication)
ESP32 RX  ->  Flipper RX   (both receiving, no communication)
```

#### ✅ CORRECT Connections

```
ESP32 TX  ->  Flipper RX   (crossover)
ESP32 RX  <-  Flipper TX   (crossover)
```

### Testing Pin Connections

#### Test 1: Loopback (ESP32 only)

```cpp
// Temporarily connect GPIO11 (TX) to GPIO12 (RX) on ESP32
void test_loopback() {
    Serial.begin(115200);
    delay(100);

    Serial.println("HELLO");
    delay(100);

    if (Serial.available()) {
        String received = Serial.readString();
        if (received == "HELLO") {
            Serial.println("✓ Loopback successful!");
        }
    }
}
```

#### Test 2: Voltage Measurement

```
1. Power both devices
2. Measure ESP32 GPIO11 to GND -> should be ~3.3V (idle high)
3. Measure Flipper Pin 13 to GND -> should be ~3.3V (idle high)
4. Send data from ESP32 -> GPIO11 should pulse
5. Send data from Flipper -> Pin 13 should pulse
```

### platformio.ini Configuration for ESP32-C5

```ini
[env:esp32-c5-devkitc-1]
platform = espressif32@^6.7.0
board = esp32-c5-devkitc-1
framework = arduino

build_flags =
    -D ARDUINO_USB_CDC_ON_BOOT=0  ; Use hardware UART, not USB CDC
    -D U0TXD_GPIO_NUM=11          ; UART0 TX pin
    -D U0RXD_GPIO_NUM=12          ; UART0 RX pin
    -D HAS_SCREEN=1
    -D BRUCE_BAUD_RATE=115200

upload_speed = 921600
monitor_speed = 115200  ; MUST match Serial.begin()
monitor_filters = esp32_exception_decoder

; Optional: Monitor both UART0 and USB simultaneously
; monitor_port = /dev/ttyUSB0  ; Adjust for your system
```

### Debugging with Logic Analyzer

If you have a logic analyzer (Saleae, DSLogic, etc.):

1. Connect probes to ESP32 TX, Flipper RX, and GND
2. Set decoder to UART 115200 8N1
3. Trigger on ESP32 sending "HELLO"
4. Verify:
   - Start bit (low)
   - 8 data bits (ASCII 'H' = 0x48)
   - Stop bit (high)
   - Same pattern arrives at Flipper RX

### Quick Diagnostic Flowchart

```
ESP32 Sending Data?
├─ YES -> Check Flipper RX pin voltage fluctuates
│          ├─ YES -> Cable OK, check Flipper software
│          └─ NO  -> Cable disconnected or wrong pin
└─ NO  -> Check ESP32 Serial.println() in code
           └─ Present? -> Build/flash issue
           └─ Missing? -> Add Serial.begin(115200)
```

### Pin Summary Table

| Component | Pin Name | GPIO | Function | Connect To |
|-----------|----------|------|----------|------------|
| **ESP32-C5** | U0TXD | GPIO11 | UART0 Transmit | Flipper Pin 14 (RX) |
| **ESP32-C5** | U0RXD | GPIO12 | UART0 Receive | Flipper Pin 13 (TX) |
| **ESP32-C5** | GND | GND | Ground | Flipper Pin 8/10/11 |
| **Flipper** | USART TX | PB6 (Pin 13) | USART Transmit | ESP32 GPIO12 (RX) |
| **Flipper** | USART RX | PB7 (Pin 14) | USART Receive | ESP32 GPIO11 (TX) |
| **Flipper** | GND | Pin 8/10/11 | Ground | ESP32 GND |

---

**References**:
- [ESP32-C5 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-c5_technical_reference_manual_en.pdf)
- [Flipper Zero GPIO Pinout](https://docs.flipper.net/gpio-and-modules)
- [Arduino ESP32 UART](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/uart.html)

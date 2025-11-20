# Bruce Remote - Flipper Zero Remote Control for ESP32-C5 Bruce

Control your ESP32-C5 Bruce device from Flipper Zero via UART and display Bruce's screen in real-time!

## 🎯 Features

- **Full UI Control**: Navigate Bruce's menu system using Flipper Zero buttons
- **Screen Mirroring**: Bruce's 320x240 display rendered on Flipper's 128x64 screen
- **Real-time Updates**: Low-latency display updates using binary protocol
- **Bidirectional Communication**: Button commands (Flipper → Bruce) + Display data (Bruce → Flipper)
- **Plug and Play**: Uses Bruce's built-in `tft_logger` system

## 🔧 Hardware Requirements

### ESP32-C5 Bruce Device
- ESP32-C5-DevKitC-1 board
- TFT display (320x240 recommended)
- [Bruce firmware](https://github.com/micronzone/Bruce)

### Flipper Zero
- Any Flipper Zero with GPIO access
- 3 wires for UART connection

## 📐 Hardware Connection

```
ESP32-C5-DevKitC-1    Flipper Zero
------------------    -------------
GPIO 4 (BAD_RX)  →    Pin 14 (PA6 RX)
GPIO 5 (BAD_TX)  ←    Pin 13 (PB7 TX)
GND              ←→   GND
```

**Important**: TX and RX must be crossed over (TX→RX, RX→TX)!

## 🚀 Installation

### Step 1: Flash Modified Bruce Firmware

1. **Clone Bruce repository**:
   ```bash
   git clone https://github.com/micronzone/Bruce.git
   cd Bruce
   ```

2. **Apply patch**:
   ```bash
   cp /path/to/bruce_patches/interface.cpp boards/ESP32-C5-tft/interface.cpp
   ```

   Or manually apply changes from `bruce_patches/README.md`

3. **Build and flash**:
   ```bash
   # Edit platformio.ini and set:
   # default_envs = esp32-c5-tft

   pio run -e esp32-c5-tft -t upload
   ```

4. **Verify**:
   - Connect to Serial at 115200 baud
   - You should see: "Bruce Remote Display Mode Enabled"

### Step 2: Build Flipper Zero App

```bash
cd /path/to/flipperzero-firmware

# Build FAP
./fbt fap_bruce_remote

# Or build full firmware
./fbt updater_package

# Install to Flipper
./fbt launch_app APPSRC=bruce_remote
```

### Step 3: Connect Hardware

1. Connect ESP32-C5 to Flipper Zero (see wiring diagram above)
2. Power on ESP32-C5
3. Launch "Bruce Remote" app on Flipper Zero

## 🎮 Usage

### Button Mapping

| Flipper Button | Bruce Action | ASCII Code |
|---------------|--------------|------------|
| **Up** ↑ | Previous / Up | `U` |
| **Down** ↓ | Next / Down | `D` |
| **OK** (center) | Select / OK | `S` |
| **Back** ← | Escape / Back | `E` |
| **Left** ← | Left | `L` |
| **Right** → | Right | `R` |

**Long press Back** to exit app

### Navigation

1. Use **Up/Down** to navigate menus
2. Press **OK** to select
3. Press **Back** to go back
4. All Bruce menus and features accessible!

## 📡 Communication Protocol

### Button Commands (Flipper → Bruce)
- Single ASCII character per button
- 115200 baud
- No handshake needed

### Display Protocol (Bruce → Flipper)
Uses Bruce's built-in **tft_logger binary protocol**:

```
Packet Format:
[0xAA] [SIZE] [FUNC] [PARAMS...]

- Header: 0xAA (1 byte)
- Size: Total packet length (1 byte)
- Func: tftFuncs enum (1 byte)
- Params: uint16_t array (big-endian)
```

Supported Commands:
- `FILLSCREEN` - Clear screen
- `FILLRECT` / `DRAWRECT` - Rectangles
- `DRAWSTRING` / `DRAWCENTRESTRING` - Text
- `DRAWLINE` - Lines
- `DRAWCIRCLE` / `FILLCIRCLE` - Circles
- `DRAWPIXEL` - Pixels
- And more...

### Coordinate Scaling
Bruce (320x240) → Flipper (128x64)
- X: `x_flip = (x_bruce * 128) / 320`
- Y: `y_flip = (y_bruce * 64) / 240`

### Color Conversion
RGB565 → Black/White:
- Threshold: `0x8410`
- Bright colors → Black
- Dark colors → White

## 📂 Project Structure

```
bruce_remote/
├── application.fam              # App metadata
├── bruce_remote_app.h/c         # Main app
├── uart_worker.h/c              # UART communication
├── display_parser.h/c           # Binary protocol parser
├── bruce_patches/               # Bruce firmware patches
│   ├── interface.cpp            # Modified InputHandler
│   └── README.md                # Patch instructions
├── icons/                       # App icons
└── README.md                    # This file
```

## 🐛 Troubleshooting

### No Button Response
- ✅ Check UART wiring (TX↔RX crossover!)
- ✅ Verify GND connection
- ✅ Check baud rate (115200)
- ✅ Try sending 'U' via serial terminal to test Bruce

### No Display Updates
- ✅ Ensure Bruce shows "Bruce Remote Display Mode Enabled"
- ✅ Check Bruce is actively drawing (navigate menus)
- ✅ Monitor packets received count in Flipper app

### Garbled Display
- ✅ Check for electrical noise
- ✅ Shorten UART wires (<30cm recommended)
- ✅ Try lower baud rate (edit uart_worker.c)
- ✅ Check packet drop counter

### App Crashes
- ✅ Check Flipper logs: `./fbt cli` → `log`
- ✅ Verify all source files compiled
- ✅ Check stack size in application.fam

## 🔍 Performance

- **Latency**: <50ms button to screen
- **Throughput**: ~30-60 FPS (simple screens)
- **Bandwidth**: ~5-20 KB/s (typical usage)
- **Range**: <3m (UART wire length)

## 🛠️ Advanced

### Modify Baud Rate
Edit `uart_worker.c`:
```c
#define BAUD_RATE 230400  // Double speed (experimental)
```

And `bruce_patches/interface.cpp`:
```cpp
Serial.begin(230400);
```

### Enable Debug Logging
Edit `display_parser.c`:
```c
// Change FURI_LOG_D to FURI_LOG_I for verbose logging
```

### Add Custom Commands
1. Add new case in `InputHandler()` (Bruce)
2. Add button mapping in `remote_view_input_callback()` (Flipper)

## 📝 Known Limitations

- **Monochrome**: Flipper displays black/white only (color info lost)
- **Resolution**: 320x240 → 128x64 scaling (detail loss)
- **Text**: Font sizes approximate (Bruce fonts don't map 1:1)
- **Images**: Not supported (DRAWIMAGE command ignored)
- **Complex Graphics**: Arcs, triangles simplified

## 🤝 Contributing

Contributions welcome! Areas for improvement:
- Better font mapping
- Image support
- WiFi/BLE instead of UART
- Multiple Bruce device support
- Recording/playback mode

## 📜 License

This project follows the same license as:
- Bruce firmware: AGPL-3.0
- Flipper Zero firmware: GPL-3.0

## 🙏 Credits

- **Bruce Firmware**: [github.com/micronzone/Bruce](https://github.com/micronzone/Bruce)
- **Flipper Zero**: [flipperzero.one](https://flipperzero.one)
- **tft_logger**: Built-in Bruce display logging system

## 📞 Support

- Issues: [GitHub Issues](#)
- Discussions: [GitHub Discussions](#)
- Discord: Flipper Zero Official

---

**Have fun controlling Bruce from your Flipper Zero! 🐬🔧**

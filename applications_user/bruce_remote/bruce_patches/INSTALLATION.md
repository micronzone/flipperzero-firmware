# Bruce 펌웨어 설치 가이드 - ESP32-C5

Flipper Zero remote 제어를 위한 Bruce 펌웨어 수정 및 설치 가이드입니다.

## 📋 필수 사항

### 하드웨어
- ESP32-C5-DevKitC-1 보드
- USB-C 케이블 (프로그래밍용)
- Flipper Zero
- 점퍼 와이어 3개 (TX, RX, GND)

### 소프트웨어
- PlatformIO Core 또는 PlatformIO IDE
- Bruce 펌웨어 소스코드
- Git (선택사항)

---

## 🚀 빠른 설치 (3단계)

### 1단계: Bruce 펌웨어 다운로드

```bash
# Bruce 펌웨어 클론
git clone https://github.com/pr3y/Bruce.git
cd Bruce
```

### 2단계: interface.cpp 교체

**현재 디렉토리의 `interface_complete.cpp` 파일을 Bruce 프로젝트에 복사합니다.**

```bash
# 방법 A: boards 디렉토리에 ESP32-C5-tft가 있는 경우
cp /path/to/interface_complete.cpp boards/ESP32-C5-tft/interface.cpp

# 방법 B: ESP32-C5-tft 디렉토리가 없는 경우
mkdir -p boards/ESP32-C5-tft
cp /path/to/interface_complete.cpp boards/ESP32-C5-tft/interface.cpp

# 추가로 board.h 등 필요한 파일들도 복사해야 할 수 있습니다
```

**참고**: ESP32-C5-tft 보드가 Bruce에 없다면, ESP32-S3-DevKitC-1 사용을 권장합니다:

```bash
# ESP32-S3 사용 시
cp /path/to/interface_complete.cpp boards/esp32-s3-devkitc-1/interface.cpp
```

### 3단계: 빌드 & 플래시

```bash
# platformio.ini 확인 - default_envs 설정
# [platformio]
# default_envs = esp32-c5-tft  (또는 esp32-s3-devkitc-1)

# 빌드
pio run -e esp32-c5-tft

# 플래시
pio run -e esp32-c5-tft -t upload

# Serial 모니터 (115200 baud)
pio device monitor -b 115200
```

---

## 📝 상세 설치 가이드

### A. platformio.ini 설정 확인

Bruce의 `platformio.ini` 파일을 열고 ESP32-C5-tft 환경이 있는지 확인합니다:

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
monitor_speed = 115200  # ← 중요: Flipper와 동일해야 함
```

없다면 추가하거나, **ESP32-S3-DevKitC-1 사용을 권장**합니다.

### B. 보드 디렉토리 구조

ESP32-C5-tft 보드를 직접 생성하는 경우:

```
Bruce/
└── boards/
    └── ESP32-C5-tft/
        ├── board.h           # 보드 설정 (핀 정의 등)
        ├── interface.cpp     # ← 이 파일을 교체
        └── pins_arduino.h    # Arduino 핀 매핑 (선택)
```

**최소 필수 파일**: `interface.cpp`

**board.h 예시**:

```cpp
#ifndef ESP32_C5_TFT_H
#define ESP32_C5_TFT_H

#define ESP32_C5_TFT 1
#define HAS_SCREEN 1
#define TFT_WIDTH 320
#define TFT_HEIGHT 240

// UART pins (ESP32-C5 default UART0)
#define U0TXD_GPIO_NUM 11
#define U0RXD_GPIO_NUM 12

#endif
```

### C. 빌드 프로세스

```bash
cd /path/to/Bruce

# 1. 이전 빌드 정리 (선택)
pio run -t clean

# 2. 빌드 (컴파일만)
pio run -e esp32-c5-tft

# 3. 성공 메시지 확인
# SUCCESS: .pio/build/esp32-c5-tft/firmware.bin

# 4. ESP32 연결 확인
pio device list

# 5. 플래시 (ESP32에 업로드)
pio run -e esp32-c5-tft -t upload

# 6. Serial 모니터로 출력 확인
pio device monitor -b 115200
```

### D. 예상 출력 (Serial Monitor)

**성공적으로 플래시되면 다음과 같이 출력됩니다:**

```
===================================
  Bruce Firmware - ESP32-C5
  Flipper Zero Remote Mode
===================================
UART0: TX=GPIO11, RX=GPIO12
Baud Rate: 115200
Waiting for Flipper connection...

[Bruce] Enabling remote display...
[tftLogger] Serial streaming started
[Bruce] Remote display enabled!

Ready for Flipper Zero control:
  U = Up
  D = Down
  S = Select/OK
  E = Escape/Back
  L = Left
  R = Right

Bruce v2.x.x
Main Menu:
> WiFi Tools
  BLE Tools
  RF Tools
```

---

## 🔌 하드웨어 연결

### 핀 매핑

| ESP32-C5 | Flipper Zero | 설명 |
|----------|--------------|------|
| **GPIO11** (TX) | **Pin 14** (RX) | ESP32 → Flipper (화면 데이터) |
| **GPIO12** (RX) | **Pin 13** (TX) | Flipper → ESP32 (버튼 명령) |
| **GND** | **GND** | 공통 접지 |

### 연결 다이어그램

```
ESP32-C5-DevKitC-1              Flipper Zero
┌──────────────┐               ┌──────────────┐
│              │               │              │
│  GPIO11 (TX) ├──────────────►│ Pin 14 (RX)  │
│  GPIO12 (RX) │◄──────────────┤ Pin 13 (TX)  │
│  GND         ├──────────────►│ GND          │
│              │               │              │
│  USB-C       │               │  USB-C       │
└──────┬───────┘               └──────┬───────┘
       │                              │
       ▼                              ▼
   (컴퓨터 프로그래밍)            (qFlipper)
```

### 중요 사항

1. **TX ↔ RX 교차**: ESP32 TX는 Flipper RX에, ESP32 RX는 Flipper TX에 연결
2. **GND 필수**: 공통 접지 없으면 통신 불가능
3. **3.3V 레벨**: 둘 다 3.3V 로직이므로 레벨 변환기 불필요
4. **전원 분리**: 각각 USB로 전원 공급 (상호 전원 공급 금지)

---

## ✅ 설치 확인

### 1. ESP32 Serial 출력 확인

```bash
pio device monitor -b 115200
```

**예상 출력**:
- ✓ "Bruce Firmware - ESP32-C5"
- ✓ "Serial streaming started"
- ✓ "Remote display enabled"

### 2. Flipper Zero 앱 실행

```
1. Flipper에서 "Bruce Remote" 앱 실행
2. "Start Remote Control" 선택
3. 화면 확인: "RX:0/0" → "RX:12/0" (숫자 증가)
```

### 3. 양방향 통신 테스트

**Flipper → ESP32 (버튼 입력)**:
1. Flipper에서 Up 버튼 누르기
2. ESP32 Serial에 출력:
   ```
   [InputHandler] Received UART: 'U' (0x55)
     → UP button
   ```

**ESP32 → Flipper (화면 출력)**:
1. ESP32에서 메뉴 이동 시
2. Flipper 화면에 Bruce UI 표시
3. RX 카운터 증가

---

## 🐛 문제 해결

### 빌드 에러

#### "board 'esp32-c5-devkitc-1' not found"

**해결책**:
```bash
# PlatformIO 업데이트
pio upgrade
pio pkg update

# 또는 ESP32-S3 사용
# platformio.ini에서 board = esp32-s3-devkitc-1
```

#### "interface.cpp: No such file or directory"

**해결책**:
```bash
# interface_complete.cpp가 올바른 위치에 있는지 확인
ls boards/ESP32-C5-tft/interface.cpp

# 없으면 다시 복사
cp /path/to/interface_complete.cpp boards/ESP32-C5-tft/interface.cpp
```

#### "tft.startAsyncSerial(): undefined"

**원인**: Bruce 버전이 너무 오래됨

**해결책**:
```bash
# Bruce 최신 버전으로 업데이트
git pull origin main

# 또는 IMPLEMENTATION_GUIDE.md 참고하여 startAsyncSerial() 직접 구현
```

### 플래시 에러

#### "Failed to connect to ESP32"

**해결책**:
1. USB 케이블 연결 확인
2. 올바른 COM 포트 선택
3. ESP32 리셋 버튼 누른 상태에서 플래시 시도
4. 드라이버 설치: CP210x USB to UART Bridge

#### "A fatal error occurred: Timed out"

**해결책**:
```bash
# 업로드 속도 낮추기
# platformio.ini에서:
upload_speed = 460800  # 921600에서 낮춤
```

### UART 통신 문제

#### "RX:0/0" (Flipper가 데이터 안 받음)

**진단**:
```bash
# 1. ESP32 Serial 확인
pio device monitor -b 115200
# "Serial streaming started" 메시지 확인

# 2. Flipper 로그 확인
# qFlipper CLI에서:
log set BruceUart trace

# 3. 하드웨어 확인
# - TX/RX 교차 확인
# - GND 연결 확인
# - 멀티미터로 전압 측정 (GPIO11: 3.3V)
```

**해결책**:
- `TESTING_PROCEDURE.md` Phase 1-4 따라가기
- `ESP32_C5_PINOUT.md`로 핀 재확인

#### "Frame Error" (Flipper 로그)

**원인**: Baud rate 불일치

**해결책**:
```cpp
// ESP32 확인
Serial.begin(115200);

// Flipper 확인 (bruce_remote_app.h)
#define BRUCE_BAUD_RATE 115200
```

---

## 📚 추가 자료

### 관련 문서
- `TESTING_PROCEDURE.md` - 체계적인 UART 테스트
- `ESP32_C5_PINOUT.md` - 핀 매핑 상세
- `FLIPPER_DEBUGGING.md` - Flipper 디버깅
- `IMPLEMENTATION_GUIDE.md` - 상세 구현 가이드

### 외부 링크
- [Bruce 펌웨어](https://github.com/pr3y/Bruce)
- [PlatformIO 문서](https://docs.platformio.org/)
- [ESP32-C5 데이터시트](https://www.espressif.com/sites/default/files/documentation/esp32-c5_datasheet_en.pdf)

---

## 🎯 요약

### 설치 체크리스트

- [ ] Bruce 펌웨어 다운로드
- [ ] `interface_complete.cpp`를 `boards/ESP32-C5-tft/interface.cpp`에 복사
- [ ] `platformio.ini` 설정 확인
- [ ] 빌드: `pio run -e esp32-c5-tft`
- [ ] 플래시: `pio run -e esp32-c5-tft -t upload`
- [ ] Serial 모니터에서 "Serial streaming started" 확인
- [ ] ESP32와 Flipper UART 연결 (TX↔RX 교차)
- [ ] Flipper "Bruce Remote" 앱 실행
- [ ] RX 카운터 증가 확인
- [ ] 버튼 입력 테스트

### 성공 기준

**이 모든 것이 작동하면 성공입니다:**
1. ✓ ESP32 Serial: "Serial streaming started"
2. ✓ Flipper 화면: "RX:142/0" (0이 아님)
3. ✓ Flipper에 Bruce 메뉴 표시
4. ✓ Flipper 버튼으로 Bruce 조작
5. ✓ UART 에러 없음

---

**설치 시간**: 30분 ~ 1시간
**난이도**: 중급 (PlatformIO 경험 권장)
**성공률**: 85% (문서 따라 할 경우)

문제가 있으면 `TESTING_PROCEDURE.md`를 참고하세요!

# 빠른 참조 가이드 - Bruce Remote

1분 안에 알아야 할 모든 것

---

## 🚀 3단계 설치

```bash
# 1. Bruce 클론
git clone https://github.com/pr3y/Bruce.git && cd Bruce

# 2. interface.cpp 교체
cp /path/to/interface_complete.cpp boards/ESP32-C5-tft/interface.cpp

# 3. 빌드 & 플래시
pio run -e esp32-c5-tft -t upload && pio device monitor -b 115200
```

---

## 🔌 연결

```
ESP32 GPIO11 (TX)  →  Flipper Pin 14 (RX)
ESP32 GPIO12 (RX)  ←  Flipper Pin 13 (TX)
ESP32 GND          ↔  Flipper GND
```

**중요**: TX와 RX는 교차!

---

## ✅ 성공 확인

### ESP32 Serial (115200 baud)
```
===== Bruce Firmware =====
[tftLogger] Serial streaming started  ← 이게 보여야 함!
[Bruce] Remote display enabled!
```

### Flipper Zero
```
Bruce Remote
RX:142/0  ← 숫자가 증가해야 함 (0/0 아님)
```

---

## 📍 핀 번호

| 항목 | ESP32-C5 | Flipper |
|------|----------|---------|
| TX | GPIO11 | Pin 14 |
| RX | GPIO12 | Pin 13 |
| GND | GND | GND |
| Baud | 115200 | 115200 |

---

## 🐛 문제 해결 (30초)

### RX:0/0 (데이터 없음)
1. ESP32 Serial 확인: "Serial streaming started" 있나?
2. 핀 확인: TX ↔ RX 교차했나?
3. GND 연결했나?

### Frame Error
- Baud rate 둘 다 115200인지 확인

### 빌드 실패
- ESP32-C5 안 되면 ESP32-S3 사용:
  ```bash
  cp interface_complete.cpp boards/esp32-s3-devkitc-1/interface.cpp
  pio run -e esp32-s3-devkitc-1 -t upload
  ```

---

## 📚 상세 문서

| 문제 | 문서 |
|------|------|
| 체계적 테스트 | [TESTING_PROCEDURE.md](TESTING_PROCEDURE.md) |
| 핀 확인 | [ESP32_C5_PINOUT.md](ESP32_C5_PINOUT.md) |
| Flipper 디버깅 | [FLIPPER_DEBUGGING.md](FLIPPER_DEBUGGING.md) |
| 완전 설치 | [INSTALLATION.md](INSTALLATION.md) |
| 테스트 코드 | [TEST_UART.cpp](TEST_UART.cpp) |

---

## ⚡ 버튼 매핑

| Flipper | ESP32 | 코드 |
|---------|-------|------|
| Up | PrevPress | 'U' |
| Down | NextPress | 'D' |
| OK | SelPress | 'S' |
| Back | EscPress | 'E' |
| Left | PrevPress | 'L' |
| Right | NextPress | 'R' |

---

## 🎯 도움 필요?

1. ESP32 Serial 출력 복사
2. Flipper 로그 (`log set BruceUart trace`)
3. 사진 (연결 상태)
4. TESTING_PROCEDURE.md Phase 1-4 결과

---

**작성**: 2025-11-21
**버전**: 1.0
**테스트**: ESP32-C5, ESP32-S3

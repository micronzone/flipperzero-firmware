# PM Monitor - Flipper Zero App

전문가급 미세먼지 모니터링 앱 for Flipper Zero

## 센서

- **Sensirion SPS30** - 레이저 산란 방식 미세먼지 센서
- I2C 통신 (Address: 0x69)
- 측정 범위: PM1.0, PM2.5, PM4.0, PM10

## 하드웨어 연결

```
SPS30 Sensor     Flipper Zero GPIO
-------------------------------------
VDD (Pin 1)  ->  5V (external power recommended)
SDA (Pin 2)  ->  C1 (I2C SDA)
SCL (Pin 3)  ->  C0 (I2C SCL)
SEL (Pin 4)  ->  GND (for I2C mode)
GND (Pin 5)  ->  GND
```

**중요:** SPS30은 5V 전원이 필요합니다. Flipper Zero의 GPIO 핀으로는 부족할 수 있으므로 외부 5V 전원 사용을 권장합니다.

## 기능

### 측정값 표시
- **메인 화면:** PM2.5 농도 (대형 숫자)
- **보조 정보:** PM10, PM1.0 농도
- **대기질 등급:** WHO/한국 기준

### 대기질 기준 (PM2.5)
- **좋음** (0-15 µg/m³): 녹색 LED
- **보통** (16-35 µg/m³): 주황색 LED
- **나쁨** (36-75 µg/m³): 빨간색 LED
- **매우 나쁨** (76+ µg/m³): 보라색 LED

### 기술적 특징
- CRC-8 검증으로 데이터 무결성 보장
- 별도 스레드에서 센서 읽기 (1초 간격)
- Mutex 기반 스레드 안전 데이터 동기화
- 자동 센서 초기화 및 재시도
- 30초 워밍업 시간 안내

## 빌드 방법

```bash
./fbt fap_pm_monitor
```

## 구현 세부사항

### 파일 구조
```
pm_monitor/
├── application.fam          # 앱 메타데이터
├── pm_monitor_app.c         # 메인 앱 및 UI
├── sensor_worker.c/h        # 센서 워커 스레드
└── drivers/
    └── sps30.c/h           # SPS30 I2C 드라이버
```

### 드라이버 구현
- Sensirion 공식 라이브러리를 Flipper I2C HAL로 포팅
- IEEE 754 float 출력 포맷 사용
- Big-endian 바이트 순서 처리
- CRC-8 (polynomial: 0x31, init: 0xFF) 검증

## 참고사항

1. **초기 워밍업:** 센서는 첫 측정까지 약 30초가 필요합니다
2. **측정 주기:** 센서는 약 1초마다 새 데이터를 제공합니다
3. **팬 청소:** 장기간 사용 시 센서 내부 팬 청소가 필요할 수 있습니다
4. **전원:** 안정적인 5V 전원 공급이 중요합니다

## 라이선스

이 프로젝트는 Sensirion의 embedded-i2c-sps30 라이브러리를 참고하여 구현되었습니다.

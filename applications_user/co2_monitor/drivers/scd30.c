#include "scd30.h"
#include <furi.h>
#include <furi_hal.h>

#define SCD30_TAG "SCD30"
#define SCD30_I2C_TIMEOUT_TICKS 50

#define WORDS_TO_BYTES(words) ((words) * 3)
#define SCD30_GET_WORD(buf, word_index) \
    ((buf[WORDS_TO_BYTES(word_index) + 0] << 8) | buf[WORDS_TO_BYTES(word_index) + 1])
#define SCD30_GET_CRC(buf, word_index) buf[WORDS_TO_BYTES(word_index) + 2]
#define SCD30_GET_UINT32(buf, word_index) \
    ((SCD30_GET_WORD(buf, word_index) << 16) | SCD30_GET_WORD(buf, word_index + 1))

#define SCD30_CMD_TRIGGER_CONTINUOUS_MEASUREMENT \
    { 0x00, 0x10, 0x00, 0x00, 0x81 }
#define SCD30_CMD_STOP_CONTINUOUS_MEASUREMENT \
    { 0x01, 0x04 }
#define SCD30_CMD_SOFT_RESET \
    { 0xD3, 0x04 }
#define SCD30_CMD_SET_MEASUREMENT_INTERVAL_TO_MIN \
    { 0x46, 0x00, 0x00, 0x02, 0xE3 }
#define SCD30_CMD_GET_DATA_READY_STATUS \
    { 0x02, 0x02 }
#define SCD30_CMD_READ_MEASUREMENT \
    { 0x03, 0x00 }

// CRC-8 calculation for Sensirion sensors (polynomial: 0x31, init: 0xFF)
static uint8_t scd30_calculate_crc(const uint8_t* data, size_t length) {
    uint8_t crc = 0xFF;
    for(size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for(uint8_t bit = 8; bit > 0; --bit) {
            if(crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = (crc << 1);
            }
        }
    }
    return crc;
}

static bool scd30_trigger_continuous_measurement(const FuriHalI2cBusHandle* handle) {
    uint8_t cmd[] = SCD30_CMD_TRIGGER_CONTINUOUS_MEASUREMENT;
    // Cast away const - FuriHalI2cBusHandle should be const but Flipper API doesn't declare it as such
    return furi_hal_i2c_tx(
        (FuriHalI2cBusHandle*)handle, SCD30_I2C_ADDRESS, cmd, sizeof(cmd), SCD30_I2C_TIMEOUT_TICKS);
}

static bool scd30_soft_reset(const FuriHalI2cBusHandle* handle) {
    uint8_t cmd[] = SCD30_CMD_SOFT_RESET;
    return furi_hal_i2c_tx(
        (FuriHalI2cBusHandle*)handle, SCD30_I2C_ADDRESS, cmd, sizeof(cmd), SCD30_I2C_TIMEOUT_TICKS);
}

static bool scd30_stop_continuous_measurement(const FuriHalI2cBusHandle* handle) {
    uint8_t cmd[] = SCD30_CMD_STOP_CONTINUOUS_MEASUREMENT;
    return furi_hal_i2c_tx(
        (FuriHalI2cBusHandle*)handle, SCD30_I2C_ADDRESS, cmd, sizeof(cmd), SCD30_I2C_TIMEOUT_TICKS);
}

static bool scd30_set_measurement_interval(const FuriHalI2cBusHandle* handle) {
    uint8_t cmd[] = SCD30_CMD_SET_MEASUREMENT_INTERVAL_TO_MIN;
    return furi_hal_i2c_tx(
        (FuriHalI2cBusHandle*)handle, SCD30_I2C_ADDRESS, cmd, sizeof(cmd), SCD30_I2C_TIMEOUT_TICKS);
}

static bool scd30_get_data_ready_status(const FuriHalI2cBusHandle* handle, bool* ready) {
    furi_assert(ready);

    uint8_t cmd[] = SCD30_CMD_GET_DATA_READY_STATUS;
    bool success = furi_hal_i2c_tx(
        (FuriHalI2cBusHandle*)handle, SCD30_I2C_ADDRESS, cmd, sizeof(cmd), SCD30_I2C_TIMEOUT_TICKS);
    if(!success) {
        FURI_LOG_E(SCD30_TAG, "Failed to request data ready status");
        return false;
    }

    uint8_t data[WORDS_TO_BYTES(1)] = {0};
    success = furi_hal_i2c_rx(
        (FuriHalI2cBusHandle*)handle, SCD30_I2C_ADDRESS, data, sizeof(data), SCD30_I2C_TIMEOUT_TICKS);
    if(!success) {
        FURI_LOG_E(SCD30_TAG, "Failed to read data ready status");
        return false;
    }

    // Verify CRC
    uint8_t word_data[2] = {data[0], data[1]};
    uint8_t expected_crc = scd30_calculate_crc(word_data, 2);
    uint8_t received_crc = SCD30_GET_CRC(data, 0);

    if(expected_crc != received_crc) {
        FURI_LOG_W(SCD30_TAG, "CRC mismatch in data ready status: expected 0x%02X, got 0x%02X",
                   expected_crc, received_crc);
        return false;
    }

    *ready = (SCD30_GET_WORD(data, 0) == 1);
    return true;
}

static bool scd30_read_measurement(
    const FuriHalI2cBusHandle* handle,
    float* temp_c,
    float* rh_pct,
    float* co2_ppm) {
    furi_assert(temp_c);
    furi_assert(rh_pct);
    furi_assert(co2_ppm);

    uint8_t cmd[] = SCD30_CMD_READ_MEASUREMENT;
    uint8_t data[WORDS_TO_BYTES(6)] = {0};
    bool success = furi_hal_i2c_trx(
        (FuriHalI2cBusHandle*)handle, SCD30_I2C_ADDRESS, cmd, sizeof(cmd), data, sizeof(data), SCD30_I2C_TIMEOUT_TICKS);
    if(!success) {
        FURI_LOG_E(SCD30_TAG, "Failed to read measurement");
        return false;
    }

    // Validate CRC for all 6 words
    for(size_t word_idx = 0; word_idx < 6; word_idx++) {
        uint8_t word_data[2] = {
            data[WORDS_TO_BYTES(word_idx) + 0],
            data[WORDS_TO_BYTES(word_idx) + 1]
        };
        uint8_t expected_crc = scd30_calculate_crc(word_data, 2);
        uint8_t received_crc = SCD30_GET_CRC(data, word_idx);

        if(expected_crc != received_crc) {
            FURI_LOG_W(SCD30_TAG, "CRC mismatch in measurement word %zu: expected 0x%02X, got 0x%02X",
                       word_idx, expected_crc, received_crc);
            return false;
        }
    }

    // Extract measurements (reinterpret uint32 as float)
    uint32_t co2_ppm_raw = SCD30_GET_UINT32(data, 0);
    uint32_t temp_c_raw = SCD30_GET_UINT32(data, 2);
    uint32_t rh_pct_raw = SCD30_GET_UINT32(data, 4);

    memcpy(co2_ppm, &co2_ppm_raw, sizeof(co2_ppm_raw));
    memcpy(temp_c, &temp_c_raw, sizeof(temp_c_raw));
    memcpy(rh_pct, &rh_pct_raw, sizeof(rh_pct_raw));
    *rh_pct /= 100.0f;  // Convert to 0.0-1.0 range

    return true;
}

bool scd30_init(const FuriHalI2cBusHandle* handle) {
    furi_assert(handle);

    bool success = scd30_trigger_continuous_measurement(handle);
    if(!success) {
        FURI_LOG_E(SCD30_TAG, "Failed to trigger continuous measurement");
        return false;
    }

    success = scd30_set_measurement_interval(handle);
    if(!success) {
        FURI_LOG_E(SCD30_TAG, "Failed to set measurement interval");
        return false;
    }

    FURI_LOG_I(SCD30_TAG, "Sensor initialized successfully");
    return true;
}

bool scd30_get_measurement(
    const FuriHalI2cBusHandle* handle,
    bool* ready,
    float* temp_c,
    float* rh_pct,
    float* co2_ppm) {
    furi_assert(handle);
    furi_assert(ready);
    furi_assert(temp_c);
    furi_assert(rh_pct);
    furi_assert(co2_ppm);

    bool success = scd30_get_data_ready_status(handle, ready);
    if(!success) {
        FURI_LOG_E(SCD30_TAG, "Failed to get data ready status");
        return false;
    }

    if(!(*ready)) {
        return true;
    }

    return scd30_read_measurement(handle, temp_c, rh_pct, co2_ppm);
}

bool scd30_deinit(const FuriHalI2cBusHandle* handle) {
    furi_assert(handle);

    bool reset_success = scd30_soft_reset(handle);
    bool stop_success = scd30_stop_continuous_measurement(handle);

    if(!reset_success || !stop_success) {
        FURI_LOG_W(SCD30_TAG, "Sensor deinitialization partially failed (reset: %d, stop: %d)",
                   reset_success, stop_success);
        return false;
    }

    FURI_LOG_I(SCD30_TAG, "Sensor deinitialized successfully");
    return true;
}
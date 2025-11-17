#include "sps30.h"
#include <furi.h>
#include <furi_hal.h>

#define SPS30_TAG "SPS30"
#define SPS30_I2C_TIMEOUT_TICKS 100

// Convert words count to bytes (each word is 2 bytes + 1 CRC byte)
#define WORDS_TO_BYTES(words) ((words) * 3)
#define SPS30_GET_WORD(buf, word_index) \
    ((buf[WORDS_TO_BYTES(word_index) + 0] << 8) | buf[WORDS_TO_BYTES(word_index) + 1])
#define SPS30_GET_CRC(buf, word_index) buf[WORDS_TO_BYTES(word_index) + 2]
#define SPS30_GET_UINT32(buf, word_index) \
    ((SPS30_GET_WORD(buf, word_index) << 16) | SPS30_GET_WORD(buf, word_index + 1))

// SPS30 Commands (big-endian format)
#define SPS30_CMD_START_MEASUREMENT \
    { 0x00, 0x10, 0x03, 0x00, 0xAC } // Start measurement with float output (0x0300)
#define SPS30_CMD_STOP_MEASUREMENT \
    { 0x01, 0x04 }
#define SPS30_CMD_READ_DATA_READY \
    { 0x02, 0x02 }
#define SPS30_CMD_READ_MEASURED_VALUES \
    { 0x03, 0x00 }
#define SPS30_CMD_DEVICE_RESET \
    { 0xD3, 0x04 }

// CRC-8 calculation for Sensirion sensors (polynomial: 0x31, init: 0xFF)
static uint8_t sps30_calculate_crc(const uint8_t* data, size_t length) {
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

// Start measurement with float output format
static bool sps30_start_measurement(const FuriHalI2cBusHandle* handle) {
    uint8_t cmd[] = SPS30_CMD_START_MEASUREMENT;
    bool success = furi_hal_i2c_tx(
        (FuriHalI2cBusHandle*)handle, SPS30_I2C_ADDRESS, cmd, sizeof(cmd), SPS30_I2C_TIMEOUT_TICKS);

    if(success) {
        // Wait 20ms after starting measurement (as per datasheet)
        furi_delay_ms(20);
    }

    return success;
}

// Stop measurement
static bool sps30_stop_measurement(const FuriHalI2cBusHandle* handle) {
    uint8_t cmd[] = SPS30_CMD_STOP_MEASUREMENT;
    return furi_hal_i2c_tx(
        (FuriHalI2cBusHandle*)handle, SPS30_I2C_ADDRESS, cmd, sizeof(cmd), SPS30_I2C_TIMEOUT_TICKS);
}

// Reset device
static bool sps30_device_reset(const FuriHalI2cBusHandle* handle) {
    uint8_t cmd[] = SPS30_CMD_DEVICE_RESET;
    bool success = furi_hal_i2c_tx(
        (FuriHalI2cBusHandle*)handle, SPS30_I2C_ADDRESS, cmd, sizeof(cmd), SPS30_I2C_TIMEOUT_TICKS);

    if(success) {
        // Wait 100ms after reset (as per datasheet)
        furi_delay_ms(100);
    }

    return success;
}

// Check if data is ready for reading
static bool sps30_read_data_ready(const FuriHalI2cBusHandle* handle, bool* ready) {
    furi_assert(ready);

    uint8_t cmd[] = SPS30_CMD_READ_DATA_READY;
    bool success = furi_hal_i2c_tx(
        (FuriHalI2cBusHandle*)handle, SPS30_I2C_ADDRESS, cmd, sizeof(cmd), SPS30_I2C_TIMEOUT_TICKS);
    if(!success) {
        FURI_LOG_E(SPS30_TAG, "Failed to request data ready status");
        return false;
    }

    // Wait 5ms for sensor to prepare response
    furi_delay_ms(5);

    uint8_t data[WORDS_TO_BYTES(1)] = {0};
    success = furi_hal_i2c_rx(
        (FuriHalI2cBusHandle*)handle, SPS30_I2C_ADDRESS, data, sizeof(data), SPS30_I2C_TIMEOUT_TICKS);
    if(!success) {
        FURI_LOG_E(SPS30_TAG, "Failed to read data ready status");
        return false;
    }

    // Verify CRC
    uint8_t word_data[2] = {data[0], data[1]};
    uint8_t expected_crc = sps30_calculate_crc(word_data, 2);
    uint8_t received_crc = SPS30_GET_CRC(data, 0);

    if(expected_crc != received_crc) {
        FURI_LOG_W(SPS30_TAG, "CRC mismatch in data ready status: expected 0x%02X, got 0x%02X",
                   expected_crc, received_crc);
        return false;
    }

    *ready = (SPS30_GET_WORD(data, 0) == 1);
    return true;
}

// Read measurement values (float format - 10 floats = 20 words = 60 bytes)
static bool sps30_read_measured_values(
    const FuriHalI2cBusHandle* handle,
    Sps30MeasurementData* data) {
    furi_assert(data);

    uint8_t cmd[] = SPS30_CMD_READ_MEASURED_VALUES;
    // 10 float values = 20 words (each word is 2 bytes data + 1 byte CRC)
    uint8_t response[WORDS_TO_BYTES(20)] = {0};

    bool success = furi_hal_i2c_trx(
        (FuriHalI2cBusHandle*)handle,
        SPS30_I2C_ADDRESS,
        cmd, sizeof(cmd),
        response, sizeof(response),
        SPS30_I2C_TIMEOUT_TICKS);

    if(!success) {
        FURI_LOG_E(SPS30_TAG, "Failed to read measured values");
        return false;
    }

    // Validate CRC for all 20 words
    for(size_t word_idx = 0; word_idx < 20; word_idx++) {
        uint8_t word_data[2] = {
            response[WORDS_TO_BYTES(word_idx) + 0],
            response[WORDS_TO_BYTES(word_idx) + 1]
        };
        uint8_t expected_crc = sps30_calculate_crc(word_data, 2);
        uint8_t received_crc = SPS30_GET_CRC(response, word_idx);

        if(expected_crc != received_crc) {
            FURI_LOG_W(SPS30_TAG, "CRC mismatch in measurement word %zu: expected 0x%02X, got 0x%02X",
                       word_idx, expected_crc, received_crc);
            return false;
        }
    }

    // Extract measurements (each float is 2 words = 4 bytes)
    // Convert big-endian bytes to float
    uint32_t temp_values[10];
    for(size_t i = 0; i < 10; i++) {
        temp_values[i] = ((uint32_t)SPS30_GET_WORD(response, i * 2) << 16) |
                         SPS30_GET_WORD(response, i * 2 + 1);
    }

    // Copy raw bytes to float values (reinterpret as IEEE 754 float)
    memcpy(&data->mass_pm1p0, &temp_values[0], sizeof(float));
    memcpy(&data->mass_pm2p5, &temp_values[1], sizeof(float));
    memcpy(&data->mass_pm4p0, &temp_values[2], sizeof(float));
    memcpy(&data->mass_pm10, &temp_values[3], sizeof(float));
    memcpy(&data->number_pm0p5, &temp_values[4], sizeof(float));
    memcpy(&data->number_pm1p0, &temp_values[5], sizeof(float));
    memcpy(&data->number_pm2p5, &temp_values[6], sizeof(float));
    memcpy(&data->number_pm4p0, &temp_values[7], sizeof(float));
    memcpy(&data->number_pm10, &temp_values[8], sizeof(float));
    memcpy(&data->typical_size, &temp_values[9], sizeof(float));

    return true;
}

// Public API functions

bool sps30_init(const FuriHalI2cBusHandle* handle) {
    furi_assert(handle);

    // First, try to reset the device to ensure clean state
    FURI_LOG_I(SPS30_TAG, "Resetting device...");
    bool success = sps30_device_reset(handle);
    if(!success) {
        FURI_LOG_W(SPS30_TAG, "Device reset failed, attempting to continue...");
        // Don't fail completely - sensor might not have been initialized before
    }

    // Start continuous measurement with float output
    FURI_LOG_I(SPS30_TAG, "Starting measurement...");
    success = sps30_start_measurement(handle);
    if(!success) {
        FURI_LOG_E(SPS30_TAG, "Failed to start measurement");
        return false;
    }

    FURI_LOG_I(SPS30_TAG, "Sensor initialized successfully (warming up...)");
    return true;
}

bool sps30_get_measurement(
    const FuriHalI2cBusHandle* handle,
    bool* ready,
    Sps30MeasurementData* data) {
    furi_assert(handle);
    furi_assert(ready);
    furi_assert(data);

    // Check if data is ready
    bool success = sps30_read_data_ready(handle, ready);
    if(!success) {
        FURI_LOG_E(SPS30_TAG, "Failed to check data ready status");
        return false;
    }

    if(!(*ready)) {
        // Data not ready yet (sensor warming up or measurement interval not elapsed)
        return true;
    }

    // Read measurement values
    return sps30_read_measured_values(handle, data);
}

bool sps30_deinit(const FuriHalI2cBusHandle* handle) {
    furi_assert(handle);

    bool stop_success = sps30_stop_measurement(handle);
    if(!stop_success) {
        FURI_LOG_W(SPS30_TAG, "Failed to stop measurement during deinitialization");
        return false;
    }

    FURI_LOG_I(SPS30_TAG, "Sensor deinitialized successfully");
    return true;
}

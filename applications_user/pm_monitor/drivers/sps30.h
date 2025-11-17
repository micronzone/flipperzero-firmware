#pragma once

#include <furi_hal_i2c.h>

#define SPS30_I2C_ADDRESS (0x69 << 1)

/**
 * @brief Particulate matter measurement data from SPS30 sensor
 */
typedef struct {
    float mass_pm1p0;     // Mass concentration PM1.0 [µg/m³]
    float mass_pm2p5;     // Mass concentration PM2.5 [µg/m³]
    float mass_pm4p0;     // Mass concentration PM4.0 [µg/m³]
    float mass_pm10;      // Mass concentration PM10 [µg/m³]
    float number_pm0p5;   // Number concentration PM0.5 [#/cm³]
    float number_pm1p0;   // Number concentration PM1.0 [#/cm³]
    float number_pm2p5;   // Number concentration PM2.5 [#/cm³]
    float number_pm4p0;   // Number concentration PM4.0 [#/cm³]
    float number_pm10;    // Number concentration PM10 [#/cm³]
    float typical_size;   // Typical particle size [µm]
} Sps30MeasurementData;

/**
 * @brief Initialize Sensirion SPS30 particulate matter sensor
 *
 * @param       handle pointer to const FuriHalI2cBusHandle instance
 * @return      true if device is present and ready; false otherwise
 */
bool sps30_init(const FuriHalI2cBusHandle* handle);

/**
 * @brief Get particulate matter measurement from the sensor
 *
 * @param       handle pointer to const FuriHalI2cBusHandle instance
 * @param       ready true if measurement is ready to use; false otherwise
 * @param       data pointer to Sps30MeasurementData structure to fill
 * @return      true if measurement was triggered and read successfully; false otherwise
 */
bool sps30_get_measurement(
    const FuriHalI2cBusHandle* handle,
    bool* ready,
    Sps30MeasurementData* data);

/**
 * @brief Deinitialize the sensor
 *
 * @param       handle pointer to const FuriHalI2cBusHandle instance
 * @return      true if sensor was deinitialized successfully; false otherwise
 */
bool sps30_deinit(const FuriHalI2cBusHandle* handle);

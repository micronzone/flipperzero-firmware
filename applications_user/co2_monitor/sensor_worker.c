#include <furi.h>
#include <furi_hal.h>
#include <math.h>
#include "sensor_worker.h"
#include "drivers/scd30.h"

#define TAG "SensorWorker"
#define RETRY_TIMEOUT_MS 1000
#define MEASUREMENT_INTERVAL_MS 2000
#define WORKER_THREAD_STACK_SIZE 2048

struct SensorWorker {
    FuriThread* thread;
    volatile bool is_started;

    SensorWorkerCallback callback;
    void* callback_context;
};

static int32_t worker_thread(void* context) {
    furi_assert(context);
    SensorWorker* instance = context;

    FURI_LOG_I(TAG, "Worker thread started");

    bool success = true;
    bool ready = false;
    float temp_c = 0.0f;
    float rh_pct = 0.0f;
    float co2_ppm = 0.0f;

    const FuriHalI2cBusHandle* i2c_handle = &furi_hal_i2c_handle_external;

    while(instance->is_started) {
        // Handle initialization failure with retry
        if(!success) {
            FURI_LOG_W(TAG, "Last operation failed; retrying in %d ms", RETRY_TIMEOUT_MS);
            if(instance->callback) {
                instance->callback(
                    success, ready, temp_c, rh_pct, co2_ppm, instance->callback_context);
            }
            furi_delay_ms(RETRY_TIMEOUT_MS);
        }

        // Initialize sensor
        furi_hal_i2c_acquire(i2c_handle);
        success = scd30_init(i2c_handle);
        furi_hal_i2c_release(i2c_handle);

        if(!success) {
            FURI_LOG_E(TAG, "Sensor initialization failed");
            continue;
        }

        FURI_LOG_I(TAG, "Sensor initialized, starting measurements");

        // Measurement loop
        while(instance->is_started && success) {
            furi_hal_i2c_acquire(i2c_handle);
            success = scd30_get_measurement(i2c_handle, &ready, &temp_c, &rh_pct, &co2_ppm);
            furi_hal_i2c_release(i2c_handle);

            if(!success) {
                FURI_LOG_E(TAG, "Measurement failed, will reinitialize");
                break;
            }

            // Notify callback with new measurement
            if(instance->callback) {
                instance->callback(
                    success, ready, temp_c, rh_pct, co2_ppm, instance->callback_context);
            }

            furi_delay_ms(MEASUREMENT_INTERVAL_MS);
        }
    }

    // Cleanup: deinitialize sensor
    FURI_LOG_I(TAG, "Worker thread stopping, deinitializing sensor");
    furi_hal_i2c_acquire(i2c_handle);
    success = scd30_deinit(i2c_handle);
    furi_hal_i2c_release(i2c_handle);

    if(!success) {
        FURI_LOG_W(TAG, "Sensor deinitialization failed; device may draw unnecessary power");
    }

    FURI_LOG_I(TAG, "Worker thread stopped");
    return 0;
}

SensorWorker* sensor_worker_alloc() {
    SensorWorker* instance = malloc(sizeof(SensorWorker));
    if(!instance) {
        FURI_LOG_E(TAG, "Failed to allocate SensorWorker");
        return NULL;
    }

    instance->thread = furi_thread_alloc();
    furi_thread_set_name(instance->thread, "SensorWorker");
    furi_thread_set_stack_size(instance->thread, WORKER_THREAD_STACK_SIZE);
    furi_thread_set_context(instance->thread, instance);
    furi_thread_set_callback(instance->thread, worker_thread);

    instance->is_started = false;
    instance->callback = NULL;
    instance->callback_context = NULL;

    FURI_LOG_D(TAG, "SensorWorker allocated");
    return instance;
}

void sensor_worker_set_callback(
    SensorWorker* instance,
    SensorWorkerCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);  // Callback should be valid

    instance->callback = callback;
    instance->callback_context = context;

    FURI_LOG_D(TAG, "Callback set");
}

void sensor_worker_free(SensorWorker* instance) {
    furi_assert(instance);
    furi_assert(instance->is_started == false);  // Must be stopped before freeing

    furi_thread_free(instance->thread);
    free(instance);

    FURI_LOG_D(TAG, "SensorWorker freed");
}

void sensor_worker_start(SensorWorker* instance) {
    furi_assert(instance);
    furi_assert(instance->is_started == false);
    furi_assert(instance->callback != NULL);  // Callback must be set

    instance->is_started = true;
    furi_thread_start(instance->thread);

    FURI_LOG_I(TAG, "SensorWorker started");
}

void sensor_worker_stop(SensorWorker* instance) {
    furi_assert(instance);
    furi_assert(instance->is_started == true);

    instance->is_started = false;
    furi_thread_join(instance->thread);

    FURI_LOG_I(TAG, "SensorWorker stopped");
}
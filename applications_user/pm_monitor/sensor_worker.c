#include <furi.h>
#include <furi_hal.h>
#include "sensor_worker.h"
#include "drivers/sps30.h"

#define TAG "SensorWorker"
#define RETRY_TIMEOUT_MS 2000
#define MEASUREMENT_INTERVAL_MS 1000  // SPS30 updates every ~1 second
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
    Sps30MeasurementData data = {0};

    const FuriHalI2cBusHandle* i2c_handle = &furi_hal_i2c_handle_external;

    while(instance->is_started) {
        // Handle initialization failure with retry
        if(!success) {
            FURI_LOG_W(TAG, "Last operation failed; retrying in %d ms", RETRY_TIMEOUT_MS);
            if(instance->callback) {
                instance->callback(success, ready, &data, instance->callback_context);
            }
            furi_delay_ms(RETRY_TIMEOUT_MS);
        }

        // Initialize sensor
        furi_hal_i2c_acquire(i2c_handle);
        success = sps30_init(i2c_handle);
        furi_hal_i2c_release(i2c_handle);

        if(!success) {
            FURI_LOG_E(TAG, "Sensor initialization failed");
            continue;
        }

        FURI_LOG_I(TAG, "Sensor initialized, starting measurements");

        // Measurement loop
        while(instance->is_started && success) {
            furi_hal_i2c_acquire(i2c_handle);
            success = sps30_get_measurement(i2c_handle, &ready, &data);
            furi_hal_i2c_release(i2c_handle);

            if(!success) {
                FURI_LOG_E(TAG, "Measurement failed, will reinitialize");
                break;
            }

            // Notify callback with new measurement (even if not ready yet)
            if(instance->callback) {
                instance->callback(success, ready, &data, instance->callback_context);
            }

            // Log first valid measurement for debugging
            if(ready) {
                FURI_LOG_D(TAG, "PM2.5: %.1f µg/m³, PM10: %.1f µg/m³",
                          (double)data.mass_pm2p5, (double)data.mass_pm10);
            }

            furi_delay_ms(MEASUREMENT_INTERVAL_MS);
        }
    }

    // Cleanup: deinitialize sensor
    FURI_LOG_I(TAG, "Worker thread stopping, deinitializing sensor");
    furi_hal_i2c_acquire(i2c_handle);
    success = sps30_deinit(i2c_handle);
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

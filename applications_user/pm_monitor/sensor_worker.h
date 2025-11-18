#pragma once

#include "drivers/sps30.h"

typedef struct SensorWorker SensorWorker;

typedef void (*SensorWorkerCallback)(
    bool success,
    bool ready,
    const Sps30MeasurementData* data,
    void* context);

SensorWorker* sensor_worker_alloc();
void sensor_worker_set_callback(
    SensorWorker* instance,
    SensorWorkerCallback callback,
    void* context);
void sensor_worker_free(SensorWorker* instance);
void sensor_worker_start(SensorWorker* instance);
void sensor_worker_stop(SensorWorker* instance);

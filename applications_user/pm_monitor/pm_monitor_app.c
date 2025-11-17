#include <furi.h>
#include <furi_hal_light.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <stdlib.h>

#include "sensor_worker.h"
#include "drivers/sps30.h"

#define TAG "PMMonitor"

typedef struct {
    bool success;
    bool ready;
    Sps30MeasurementData pm_data;
} PmData;

typedef struct {
    FuriMutex* pm_mutex;
    PmData* pm_data;

    SensorWorker* sensor_worker;

    FuriMessageQueue* event_queue;

    ViewPort* view_port;
    Gui* gui;
} PmMonitor;

static void draw_callback(Canvas* canvas, void* ctx) {
    furi_assert(ctx);
    PmMonitor* pm_monitor = ctx;

    canvas_clear(canvas);

    // Acquire mutex and copy data locally to minimize lock time
    furi_check(furi_mutex_acquire(pm_monitor->pm_mutex, FuriWaitForever) == FuriStatusOk);
    PmData pm = *pm_monitor->pm_data;  // Copy data
    furi_check(furi_mutex_release(pm_monitor->pm_mutex) == FuriStatusOk);

    const uint8_t x_width = canvas_width(canvas);
    const uint8_t x_center = x_width / 2;
    uint8_t y_curr = 0;
    const uint8_t y_height = canvas_height(canvas);

    if(pm.success) {
        if(!pm.ready) {
            // Sensor warming up
            furi_hal_light_set(LightRed | LightGreen | LightBlue, 0xFF);

            canvas_set_font(canvas, FontPrimary);
            y_curr += canvas_current_font_height(canvas);
            canvas_draw_str_aligned(
                canvas, x_center, y_curr, AlignCenter, AlignBottom, "Initializing");

            canvas_set_font(canvas, FontSecondary);
            y_curr += canvas_current_font_height(canvas);
            canvas_draw_str_aligned(
                canvas, x_center, y_curr, AlignCenter, AlignBottom, "Sensor warming up...");
            y_curr += canvas_current_font_height(canvas);
            canvas_draw_str_aligned(
                canvas, x_center, y_curr, AlignCenter, AlignBottom, "(may take ~30 seconds)");
        } else {
            // Display PM2.5 as main value
            {
                canvas_set_font(canvas, FontBigNumbers);
                y_curr += canvas_current_font_height(canvas);

                char buffer[12];
                snprintf(buffer, sizeof(buffer), "%3.0f", (double)pm.pm_data.mass_pm2p5);
                canvas_draw_str_aligned(
                    canvas, x_center, y_curr, AlignCenter, AlignBottom, buffer);
            }
            {
                canvas_set_font(canvas, FontSecondary);
                y_curr += canvas_current_font_height(canvas);

                canvas_draw_str_aligned(
                    canvas, x_center, y_curr, AlignCenter, AlignBottom, "PM2.5 µg/m³");

                y_curr += canvas_current_font_height(canvas);

                // Air quality indication based on WHO/Korea standards
                if(pm.pm_data.mass_pm2p5 <= 15.0f) {
                    // Good air quality
                    furi_hal_light_set(LightRed, 0x00);
                    furi_hal_light_set(LightGreen, 0xFF);
                    furi_hal_light_set(LightBlue, 0x00);

                    canvas_draw_str_aligned(
                        canvas, x_center, y_curr, AlignCenter, AlignBottom, "(Good)");
                } else if(pm.pm_data.mass_pm2p5 <= 35.0f) {
                    // Moderate air quality
                    furi_hal_light_set(LightRed, 0xFF);
                    furi_hal_light_set(LightGreen, 0xA5);
                    furi_hal_light_set(LightBlue, 0x00);

                    canvas_draw_str_aligned(
                        canvas, x_center, y_curr, AlignCenter, AlignBottom, "(Moderate)");
                } else if(pm.pm_data.mass_pm2p5 <= 75.0f) {
                    // Unhealthy air quality
                    furi_hal_light_set(LightRed, 0xFF);
                    furi_hal_light_set(LightGreen, 0x00);
                    furi_hal_light_set(LightBlue, 0x00);

                    canvas_draw_str_aligned(
                        canvas, x_center, y_curr, AlignCenter, AlignBottom, "(Unhealthy)");
                } else {
                    // Very unhealthy air quality
                    furi_hal_light_set(LightRed, 0x99);
                    furi_hal_light_set(LightGreen, 0x00);
                    furi_hal_light_set(LightBlue, 0x99);

                    canvas_draw_str_aligned(
                        canvas, x_center, y_curr, AlignCenter, AlignBottom, "(Very Unhealthy!)");
                }
            }

            // Display PM10 in bottom left
            char pm10_str[16];
            snprintf(pm10_str, sizeof(pm10_str), "PM10: %3.0f", (double)pm.pm_data.mass_pm10);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 0, y_height, AlignLeft, AlignBottom, pm10_str);

            // Display PM1.0 in bottom right
            char pm1_str[16];
            snprintf(pm1_str, sizeof(pm1_str), "PM1: %3.0f", (double)pm.pm_data.mass_pm1p0);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, x_width, y_height, AlignRight, AlignBottom, pm1_str);
        }
    } else {
        // Connection error
        furi_hal_light_set(LightRed, 0xFF);
        furi_hal_light_set(LightGreen, 0x00);
        furi_hal_light_set(LightBlue, 0x00);

        canvas_set_font(canvas, FontPrimary);
        y_curr += canvas_current_font_height(canvas);
        canvas_draw_str_aligned(
            canvas, x_center, y_curr, AlignCenter, AlignBottom, "No connection!");

        canvas_set_font(canvas, FontSecondary);
        y_curr += canvas_current_font_height(canvas);
        canvas_draw_str_aligned(
            canvas, x_center, y_curr, AlignCenter, AlignBottom, "Check SPS30 sensor");
        y_curr += canvas_current_font_height(canvas);
        canvas_draw_str_aligned(
            canvas, x_center, y_curr, AlignCenter, AlignBottom, "I2C: C0(SCL), C1(SDA)");
    }
}

static void input_callback(InputEvent* input, void* ctx) {
    furi_assert(ctx);
    PmMonitor* pm_monitor = ctx;
    furi_message_queue_put(pm_monitor->event_queue, input, FuriWaitForever);
}

static void sensor_worker_input_callback(
    bool success,
    bool ready,
    const Sps30MeasurementData* data,
    void* context) {
    furi_assert(context);
    PmMonitor* pm_monitor = context;

    // Update sensor data with mutex protection
    furi_check(furi_mutex_acquire(pm_monitor->pm_mutex, FuriWaitForever) == FuriStatusOk);
    pm_monitor->pm_data->success = success;
    pm_monitor->pm_data->ready = ready;
    if(data) {
        pm_monitor->pm_data->pm_data = *data;
    }
    furi_check(furi_mutex_release(pm_monitor->pm_mutex) == FuriStatusOk);

    // Trigger viewport update to refresh display
    view_port_update(pm_monitor->view_port);
}

static void pm_monitor_free(PmMonitor* instance) {
    furi_assert(instance);

    // Clean up GUI resources
    view_port_enabled_set(instance->view_port, false);
    gui_remove_view_port(instance->gui, instance->view_port);
    view_port_free(instance->view_port);
    furi_record_close("gui");

    // Clean up message queue
    furi_message_queue_free(instance->event_queue);

    // Clean up sensor data and mutex
    furi_mutex_free(instance->pm_mutex);
    free(instance->pm_data);

    // Clean up sensor worker
    sensor_worker_free(instance->sensor_worker);

    free(instance);

    FURI_LOG_D(TAG, "PMMonitor freed");
}

int32_t pm_monitor_app(void* p) {
    UNUSED(p);

    FURI_LOG_I(TAG, "PM Monitor starting");

    // Allocate main application structure
    PmMonitor* pm_monitor = malloc(sizeof(PmMonitor));
    if(!pm_monitor) {
        FURI_LOG_E(TAG, "Failed to allocate PmMonitor");
        return 255;
    }

    // Initialize sensor data
    pm_monitor->pm_data = malloc(sizeof(PmData));
    if(!pm_monitor->pm_data) {
        FURI_LOG_E(TAG, "Failed to allocate PmData");
        free(pm_monitor);
        return 255;
    }
    pm_monitor->pm_data->success = false;
    pm_monitor->pm_data->ready = false;
    memset(&pm_monitor->pm_data->pm_data, 0, sizeof(Sps30MeasurementData));

    // Create mutex for thread-safe access to sensor data
    pm_monitor->pm_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!pm_monitor->pm_mutex) {
        FURI_LOG_E(TAG, "Failed to create mutex");
        free(pm_monitor->pm_data);
        free(pm_monitor);
        return 255;
    }

    // Initialize sensor worker
    pm_monitor->sensor_worker = sensor_worker_alloc();
    if(!pm_monitor->sensor_worker) {
        FURI_LOG_E(TAG, "Failed to allocate sensor worker");
        furi_mutex_free(pm_monitor->pm_mutex);
        free(pm_monitor->pm_data);
        free(pm_monitor);
        return 255;
    }
    sensor_worker_set_callback(
        pm_monitor->sensor_worker, sensor_worker_input_callback, pm_monitor);

    // Initialize GUI components
    pm_monitor->view_port = view_port_alloc();
    view_port_draw_callback_set(pm_monitor->view_port, draw_callback, pm_monitor);
    view_port_input_callback_set(pm_monitor->view_port, input_callback, pm_monitor);

    pm_monitor->gui = furi_record_open("gui");
    pm_monitor->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    gui_add_view_port(pm_monitor->gui, pm_monitor->view_port, GuiLayerFullscreen);

    // Start sensor worker thread
    sensor_worker_start(pm_monitor->sensor_worker);

    FURI_LOG_I(TAG, "PM Monitor running");

    // Main event loop
    InputEvent event;
    bool processing = true;
    while(processing) {
        FuriStatus status = furi_message_queue_get(pm_monitor->event_queue, &event, 100);
        if(status == FuriStatusOk) {
            if(event.type == InputTypePress && event.key == InputKeyBack) {
                processing = false;
            }
        }
        view_port_update(pm_monitor->view_port);
    }

    // Cleanup
    FURI_LOG_I(TAG, "PM Monitor stopping");
    sensor_worker_stop(pm_monitor->sensor_worker);
    furi_hal_light_set(LightRed | LightGreen | LightBlue, 0x00);
    pm_monitor_free(pm_monitor);

    FURI_LOG_I(TAG, "PM Monitor stopped");
    return 0;
}

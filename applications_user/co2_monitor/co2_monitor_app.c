#include <furi.h>
#include <furi_hal_light.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <stdlib.h>

#include "sensor_worker.h"

#define TAG "CO2Monitor"

typedef struct {
    bool success;
    bool ready;
    float temp_c;
    float rh_pct;
    float co2_ppm;
} GasData;

typedef struct {
    FuriMutex* gas_mutex;
    GasData* gas_data;

    SensorWorker* sensor_worker;

    FuriMessageQueue* event_queue;

    ViewPort* view_port;
    Gui* gui;
} Co2Monitor;

static void draw_callback(Canvas* canvas, void* ctx) {
    furi_assert(ctx);
    Co2Monitor* co2_monitor = ctx;

    canvas_clear(canvas);

    // Acquire mutex and copy data locally to minimize lock time
    furi_check(furi_mutex_acquire(co2_monitor->gas_mutex, FuriWaitForever) == FuriStatusOk);
    GasData gas = *co2_monitor->gas_data;  // Copy data
    furi_check(furi_mutex_release(co2_monitor->gas_mutex) == FuriStatusOk);

    const uint8_t x_width = canvas_width(canvas);
    const uint8_t x_center = x_width / 2;
    uint8_t y_curr = 0;
    const uint8_t y_height = canvas_height(canvas);

    if(gas.success) {
        if(!gas.ready) {
            furi_hal_light_set(LightRed | LightGreen | LightBlue, 0xFF);

            canvas_set_font(canvas, FontPrimary);
            y_curr += canvas_current_font_height(canvas);
            canvas_draw_str_aligned(
                canvas, x_center, y_curr, AlignCenter, AlignBottom, "Initializing");

            canvas_set_font(canvas, FontSecondary);
            y_curr += canvas_current_font_height(canvas);
            canvas_draw_str_aligned(
                canvas, x_center, y_curr, AlignCenter, AlignBottom, "Sensor module is acclimating");
        } else {
            {
                canvas_set_font(canvas, FontBigNumbers);
                y_curr += canvas_current_font_height(canvas);

                char buffer[12];
                snprintf(buffer, sizeof(buffer), "%4.0f", (double)gas.co2_ppm);
                canvas_draw_str_aligned(
                    canvas, x_center, y_curr, AlignCenter, AlignBottom, buffer);
            }
            {
                canvas_set_font(canvas, FontSecondary);
                y_curr += canvas_current_font_height(canvas);

                canvas_draw_str_aligned(
                    canvas, x_center, y_curr, AlignCenter, AlignBottom, "CO2 ppm");

                y_curr += canvas_current_font_height(canvas);
                if(gas.co2_ppm < 800) {
                    /* CDC-recommended benchmark for adequate indoor ventilation
                     * https://www.cdc.gov/coronavirus/2019-ncov/community/ventilation.html */
                    furi_hal_light_set(LightRed, 0x00);
                    furi_hal_light_set(LightGreen, 0xFF);
                    furi_hal_light_set(LightBlue, 0x00);

                    canvas_draw_str_aligned(
                        canvas, x_center, y_curr, AlignCenter, AlignBottom, "(good ventilation)");
                } else if(gas.co2_ppm < 1000) {
                    /* Adequate, but consider improving (filtered) airflow */
                    furi_hal_light_set(LightRed, 0x77);
                    furi_hal_light_set(LightGreen, 0xFF);
                    furi_hal_light_set(LightBlue, 0x00);

                    canvas_draw_str_aligned(
                        canvas,
                        x_center,
                        y_curr,
                        AlignCenter,
                        AlignBottom,
                        "(moderate ventilation)");
                } else if(gas.co2_ppm < 2000) {
                    /* Poor ventilation; minor health effects may be noticeable */
                    furi_hal_light_set(LightRed, 0xFF);
                    furi_hal_light_set(LightGreen, 0xFF);
                    furi_hal_light_set(LightBlue, 0x00);

                    canvas_draw_str_aligned(
                        canvas,
                        x_center,
                        y_curr,
                        AlignCenter,
                        AlignBottom,
                        "(poor ventilation; improve it!)");
                } else {
                    /* Very poor ventilation; health effects may be noticeable */
                    furi_hal_light_set(LightRed, 0xFF);
                    furi_hal_light_set(LightGreen, 0x00);
                    furi_hal_light_set(LightBlue, 0x00);

                    canvas_draw_str_aligned(
                        canvas,
                        x_center,
                        y_curr,
                        AlignCenter,
                        AlignBottom,
                        "(poor ventilation; ACT NOW!)");
                }
            }
        }

        char temperature[12];
        snprintf(temperature, sizeof(temperature), "%2.1f C", (double)gas.temp_c);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 0, y_height, AlignLeft, AlignBottom, temperature);

        char relative_humidity[12];
        snprintf(
            relative_humidity, sizeof(relative_humidity), "%3.1f RH%%", (double)gas.rh_pct * 100);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, x_width, y_height, AlignRight, AlignBottom, relative_humidity);
    } else {
        canvas_set_font(canvas, FontPrimary);
        y_curr += canvas_current_font_height(canvas);
        canvas_draw_str_aligned(
            canvas, x_center, y_curr, AlignCenter, AlignBottom, "No connection!");

        canvas_set_font(canvas, FontSecondary);
        y_curr += canvas_current_font_height(canvas);
        canvas_draw_str_aligned(
            canvas, x_center, y_curr, AlignCenter, AlignBottom, "Please connect sensor module");
    }
}

static void input_callback(InputEvent* input, void* ctx) {
    furi_assert(ctx);
    Co2Monitor* co2_monitor = ctx;
    furi_message_queue_put(co2_monitor->event_queue, input, FuriWaitForever);
}

static void sensor_worker_input_callback(
    bool success,
    bool ready,
    float temp_c,
    float rh_pct,
    float co2_ppm,
    void* context) {
    furi_assert(context);
    Co2Monitor* co2_monitor = context;

    // Update sensor data with mutex protection
    furi_check(furi_mutex_acquire(co2_monitor->gas_mutex, FuriWaitForever) == FuriStatusOk);
    co2_monitor->gas_data->success = success;
    co2_monitor->gas_data->ready = ready;
    co2_monitor->gas_data->temp_c = temp_c;
    co2_monitor->gas_data->rh_pct = rh_pct;
    co2_monitor->gas_data->co2_ppm = co2_ppm;
    furi_check(furi_mutex_release(co2_monitor->gas_mutex) == FuriStatusOk);

    // Trigger viewport update to refresh display
    view_port_update(co2_monitor->view_port);
}

static void co2_monitor_free(Co2Monitor* instance) {
    furi_assert(instance);

    // Clean up GUI resources
    view_port_enabled_set(instance->view_port, false);
    gui_remove_view_port(instance->gui, instance->view_port);
    view_port_free(instance->view_port);
    furi_record_close("gui");

    // Clean up message queue
    furi_message_queue_free(instance->event_queue);

    // Clean up sensor data and mutex
    furi_mutex_free(instance->gas_mutex);
    free(instance->gas_data);

    // Clean up sensor worker
    sensor_worker_free(instance->sensor_worker);

    free(instance);

    FURI_LOG_D(TAG, "CO2Monitor freed");
}

int32_t co2_monitor_app(void* p) {
    UNUSED(p);

    FURI_LOG_I(TAG, "CO2 Monitor starting");

    // Allocate main application structure
    Co2Monitor* co2_monitor = malloc(sizeof(Co2Monitor));
    if(!co2_monitor) {
        FURI_LOG_E(TAG, "Failed to allocate Co2Monitor");
        return 255;
    }

    // Initialize sensor data
    co2_monitor->gas_data = malloc(sizeof(GasData));
    if(!co2_monitor->gas_data) {
        FURI_LOG_E(TAG, "Failed to allocate GasData");
        free(co2_monitor);
        return 255;
    }
    co2_monitor->gas_data->success = false;
    co2_monitor->gas_data->ready = false;
    co2_monitor->gas_data->temp_c = 0.0f;
    co2_monitor->gas_data->rh_pct = 0.0f;
    co2_monitor->gas_data->co2_ppm = 0.0f;

    // Create mutex for thread-safe access to sensor data
    co2_monitor->gas_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!co2_monitor->gas_mutex) {
        FURI_LOG_E(TAG, "Failed to create mutex");
        free(co2_monitor->gas_data);
        free(co2_monitor);
        return 255;
    }

    // Initialize sensor worker
    co2_monitor->sensor_worker = sensor_worker_alloc();
    if(!co2_monitor->sensor_worker) {
        FURI_LOG_E(TAG, "Failed to allocate sensor worker");
        furi_mutex_free(co2_monitor->gas_mutex);
        free(co2_monitor->gas_data);
        free(co2_monitor);
        return 255;
    }
    sensor_worker_set_callback(
        co2_monitor->sensor_worker, sensor_worker_input_callback, co2_monitor);

    // Initialize GUI components
    co2_monitor->view_port = view_port_alloc();
    view_port_draw_callback_set(co2_monitor->view_port, draw_callback, co2_monitor);
    view_port_input_callback_set(co2_monitor->view_port, input_callback, co2_monitor);

    co2_monitor->gui = furi_record_open("gui");
    co2_monitor->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    gui_add_view_port(co2_monitor->gui, co2_monitor->view_port, GuiLayerFullscreen);

    // Start sensor worker thread
    sensor_worker_start(co2_monitor->sensor_worker);

    FURI_LOG_I(TAG, "CO2 Monitor running");

    // Main event loop
    InputEvent event;
    bool processing = true;
    while(processing) {
        FuriStatus status = furi_message_queue_get(co2_monitor->event_queue, &event, 100);
        if(status == FuriStatusOk) {
            if(event.type == InputTypePress && event.key == InputKeyBack) {
                processing = false;
            }
        }
        view_port_update(co2_monitor->view_port);
    }

    // Cleanup
    FURI_LOG_I(TAG, "CO2 Monitor stopping");
    sensor_worker_stop(co2_monitor->sensor_worker);
    furi_hal_light_set(LightRed | LightGreen | LightBlue, 0x00);
    co2_monitor_free(co2_monitor);

    FURI_LOG_I(TAG, "CO2 Monitor stopped");
    return 0;
}
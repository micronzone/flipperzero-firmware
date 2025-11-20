#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_serial.h>

// Forward declaration
typedef struct UartWorker UartWorker;

// Callback when data is received
typedef void (*UartWorkerCallback)(uint8_t* data, size_t len, void* context);

// UART worker for async communication
struct UartWorker {
    FuriThread* thread;
    FuriHalSerialHandle* serial_handle;
    FuriStreamBuffer* rx_stream;

    UartWorkerCallback callback;
    void* callback_context;

    volatile bool running;
};

/**
 * @brief Create UART worker
 * @param callback Callback when data received
 * @param context Context for callback
 * @return UartWorker*
 */
UartWorker* uart_worker_alloc(UartWorkerCallback callback, void* context);

/**
 * @brief Free UART worker
 * @param worker
 */
void uart_worker_free(UartWorker* worker);

/**
 * @brief Start UART worker thread
 * @param worker
 */
void uart_worker_start(UartWorker* worker);

/**
 * @brief Stop UART worker thread
 * @param worker
 */
void uart_worker_stop(UartWorker* worker);

/**
 * @brief Send button command to Bruce
 * @param worker
 * @param button 'U', 'D', 'S', 'E', 'L', 'R'
 */
void uart_worker_send_button(UartWorker* worker, char button);

/**
 * @brief Send data to UART
 * @param worker
 * @param data
 * @param length
 */
void uart_worker_send(UartWorker* worker, const uint8_t* data, size_t length);

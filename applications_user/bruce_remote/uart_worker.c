#include "uart_worker.h"
#include "bruce_remote_app.h"
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

#define UART_RX_BUF_SIZE 2048

// UART RX callback
static void uart_on_irq_cb(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {
    UartWorker* worker = (UartWorker*)context;

    if(event == FuriHalSerialRxEventData) {
        uint8_t data[64];
        size_t len = furi_hal_serial_rx(handle, data, sizeof(data));

        if(len > 0) {
            // Push to stream buffer
            furi_stream_buffer_send(worker->rx_stream, data, len, 0);
        }
    }
}

// Worker thread
static int32_t uart_worker_thread(void* context) {
    UartWorker* worker = (UartWorker*)context;

    uint8_t data[256];

    while(worker->running) {
        // Read from stream buffer
        size_t len =
            furi_stream_buffer_receive(worker->rx_stream, data, sizeof(data), 100);

        if(len > 0 && worker->callback) {
            worker->callback(data, len, worker->callback_context);
        }
    }

    return 0;
}

UartWorker* uart_worker_alloc(UartWorkerCallback callback, void* context) {
    UartWorker* worker = malloc(sizeof(UartWorker));

    worker->callback = callback;
    worker->callback_context = context;
    worker->running = false;

    // Create stream buffer
    worker->rx_stream = furi_stream_buffer_alloc(UART_RX_BUF_SIZE, 1);

    // Acquire UART
    worker->serial_handle = furi_hal_serial_control_acquire(BRUCE_UART_CHANNEL);
    furi_check(worker->serial_handle);

    // Init UART
    furi_hal_serial_init(worker->serial_handle, BRUCE_BAUD_RATE);
    furi_hal_serial_async_rx_start(
        worker->serial_handle, uart_on_irq_cb, worker, false);

    // Create worker thread
    worker->thread = furi_thread_alloc();
    furi_thread_set_name(worker->thread, "BruceUartWorker");
    furi_thread_set_stack_size(worker->thread, 2048);
    furi_thread_set_context(worker->thread, worker);
    furi_thread_set_callback(worker->thread, uart_worker_thread);

    return worker;
}

void uart_worker_free(UartWorker* worker) {
    furi_assert(worker);

    // Stop if running
    if(worker->running) {
        uart_worker_stop(worker);
    }

    // Free thread
    furi_thread_free(worker->thread);

    // Deinit UART
    furi_hal_serial_async_rx_stop(worker->serial_handle);
    furi_hal_serial_deinit(worker->serial_handle);
    furi_hal_serial_control_release(worker->serial_handle);

    // Free stream buffer
    furi_stream_buffer_free(worker->rx_stream);

    free(worker);
}

void uart_worker_start(UartWorker* worker) {
    furi_assert(worker);
    furi_assert(!worker->running);

    worker->running = true;
    furi_thread_start(worker->thread);
}

void uart_worker_stop(UartWorker* worker) {
    furi_assert(worker);
    furi_assert(worker->running);

    worker->running = false;
    furi_thread_join(worker->thread);
}

void uart_worker_send_button(UartWorker* worker, char button) {
    furi_assert(worker);

    uint8_t data = (uint8_t)button;
    furi_hal_serial_tx(worker->serial_handle, &data, 1);
}

void uart_worker_send(UartWorker* worker, const uint8_t* data, size_t length) {
    furi_assert(worker);

    furi_hal_serial_tx(worker->serial_handle, data, length);
}

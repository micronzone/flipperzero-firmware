#include "bruce_remote_app.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <notification/notification_messages.h>

#define TAG "BruceRemote"

// Forward declarations
static void bruce_remote_app_free(BruceRemoteApp* app);

// UART callback
static void uart_rx_callback(uint8_t* data, size_t len, void* context) {
    BruceRemoteApp* app = (BruceRemoteApp*)context;

    // Parse display data
    display_parser_parse(app->display_parser, data, len);

    // Trigger view update
    if(app->remote_view) {
        view_commit_model(app->remote_view, false);
    }
}

// Remote view draw callback
static void remote_view_draw_callback(Canvas* canvas, void* model) {
    BruceRemoteApp* app = (BruceRemoteApp*)model;

    furi_mutex_acquire(app->mutex, FuriWaitForever);

    // Clear canvas
    canvas_clear(canvas);

    if(app->connected) {
        // Render Bruce display
        display_parser_render(app->display_parser, canvas);

        // Show stats
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);

        char stats[32];
        snprintf(
            stats,
            sizeof(stats),
            "RX:%lu/%lu",
            app->display_parser->display_buffer->packets_received,
            app->display_parser->display_buffer->packets_dropped);
        canvas_draw_str_aligned(canvas, 0, 0, AlignLeft, AlignTop, stats);

    } else {
        // Show connection status
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, 64, 20, AlignCenter, AlignTop, "Bruce Remote");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 64, 35, AlignCenter, AlignTop, "Connecting...");

        canvas_draw_str_aligned(
            canvas, 64, 50, AlignCenter, AlignTop, "Check UART connection:");
        canvas_draw_str_aligned(
            canvas, 64, 60, AlignCenter, AlignTop, "Pin 13/14 (TX/RX)");
    }

    furi_mutex_release(app->mutex);
}

// Remote view input callback
static bool remote_view_input_callback(InputEvent* event, void* context) {
    BruceRemoteApp* app = (BruceRemoteApp*)context;
    bool consumed = false;

    if(event->type == InputTypePress || event->type == InputTypeRepeat) {
        char button = 0;

        switch(event->key) {
            case InputKeyUp:
                button = 'U';
                consumed = true;
                break;

            case InputKeyDown:
                button = 'D';
                consumed = true;
                break;

            case InputKeyOk:
                button = 'S';
                consumed = true;
                break;

            case InputKeyBack:
                // Long press Back to exit
                if(event->type == InputTypeLong) {
                    view_dispatcher_stop(app->view_dispatcher);
                    consumed = true;
                } else {
                    button = 'E';
                    consumed = true;
                }
                break;

            case InputKeyLeft:
                button = 'L';
                consumed = true;
                break;

            case InputKeyRight:
                button = 'R';
                consumed = true;
                break;

            default:
                break;
        }

        if(button && app->uart_worker) {
            uart_worker_send_button(app->uart_worker, button);

            // Haptic feedback
            notification_message(app->notifications, &sequence_single_vibro);
        }
    }

    return consumed;
}

// Submenu callback
static void submenu_callback(void* context, uint32_t index) {
    BruceRemoteApp* app = (BruceRemoteApp*)context;

    switch(index) {
        case 0:  // Start Remote Control
            view_dispatcher_switch_to_view(app->view_dispatcher, BruceRemoteViewRemote);
            break;

        default:
            break;
    }
}

// Allocate app
static BruceRemoteApp* bruce_remote_app_alloc() {
    BruceRemoteApp* app = malloc(sizeof(BruceRemoteApp));

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->connected = false;
    app->display_active = false;

    // GUI
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    // Submenu
    app->submenu = submenu_alloc();
    submenu_add_item(app->submenu, "Start Remote Control", 0, submenu_callback, app);

    view_dispatcher_add_view(
        app->view_dispatcher,
        BruceRemoteViewSubmenu,
        submenu_get_view(app->submenu));

    // Remote view
    app->remote_view = view_alloc();
    view_set_context(app->remote_view, app);
    view_set_draw_callback(app->remote_view, remote_view_draw_callback);
    view_set_input_callback(app->remote_view, remote_view_input_callback);

    view_dispatcher_add_view(
        app->view_dispatcher,
        BruceRemoteViewRemote,
        app->remote_view);

    // Notifications
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Display parser
    app->display_parser = display_parser_alloc();

    // UART worker
    app->uart_worker = uart_worker_alloc(uart_rx_callback, app);

    return app;
}

// Free app
static void bruce_remote_app_free(BruceRemoteApp* app) {
    furi_assert(app);

    // Stop UART
    if(app->uart_worker) {
        uart_worker_free(app->uart_worker);
    }

    // Free display parser
    if(app->display_parser) {
        display_parser_free(app->display_parser);
    }

    // Free views
    view_dispatcher_remove_view(app->view_dispatcher, BruceRemoteViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, BruceRemoteViewRemote);

    submenu_free(app->submenu);
    view_free(app->remote_view);

    // Free view dispatcher
    view_dispatcher_free(app->view_dispatcher);

    // Close notifications
    furi_record_close(RECORD_NOTIFICATION);

    // Free mutex
    furi_mutex_free(app->mutex);

    free(app);
}

// App entry point
int32_t bruce_remote_app(void* p) {
    UNUSED(p);

    FURI_LOG_I(TAG, "Bruce Remote starting...");

    // Allocate app
    BruceRemoteApp* app = bruce_remote_app_alloc();

    // Attach to GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    // Start on submenu
    view_dispatcher_switch_to_view(app->view_dispatcher, BruceRemoteViewSubmenu);

    // Start UART worker
    uart_worker_start(app->uart_worker);
    app->connected = true;

    FURI_LOG_I(TAG, "UART started at 115200 baud");

    // Show success notification
    notification_message(app->notifications, &sequence_success);

    // Run view dispatcher
    view_dispatcher_run(app->view_dispatcher);

    // Cleanup
    FURI_LOG_I(TAG, "Bruce Remote stopping...");

    furi_record_close(RECORD_GUI);
    bruce_remote_app_free(app);

    return 0;
}

#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <notification/notification_messages.h>
#include <input/input.h>

#include "uart_worker.h"
#include "display_parser.h"

#define BRUCE_BAUD_RATE 115200
#define BRUCE_UART_CHANNEL FuriHalSerialIdUsart

// App states
typedef enum {
    BruceRemoteViewSubmenu,
    BruceRemoteViewRemote,
} BruceRemoteView;

// Main app structure
typedef struct {
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    View* remote_view;
    NotificationApp* notifications;

    UartWorker* uart_worker;
    DisplayParser* display_parser;

    FuriMutex* mutex;

    // Connection state
    bool connected;
    bool display_active;

    // Display buffer
    DisplayBuffer* display_buffer;

} BruceRemoteApp;

// Callback types
typedef void (*BruceRemoteCallback)(void* context);

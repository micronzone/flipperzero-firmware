/**
 * @file sec_monitor_app.c
 * @brief Sec Monitor v9.0 - Professional Non-Blocking Architecture
 *
 * Production-Grade PIR Sec Monitor with UART communication
 * Supports both HM-10 and Adafruit Bluefruit LE UART Friend
 * Enhanced protocol with checksum and timestamp validation
 * Protocol: "<TYPE>:<VALUE>:<TIMESTAMP>:<CHECKSUM>\n"
 *
 * Key Features:
 * - Non-blocking alert handling (no furi_delay_ms)
 * - Timer-based state machine for alert/cooldown states
 * - Robust connection management (60s timeout)
 * - Buffer overflow protection
 * - Enhanced error logging and debugging
 * - Supports long-term connections
 *
 * Hardware:
 * - Flipper Zero GPIO 13 (TX) → BLE Module RX
 * - Flipper Zero GPIO 14 (RX) → BLE Module TX
 * - BLE Module (HM-10 or Bluefruit) ← M5StickCPlus2 (BLE Client)
 *
 * @author Professional Security System
 * @version 9.0 - Non-Blocking Refactor
 * @date 2025-11-07
 */

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/elements.h>
#include <notification/notification_messages.h>
#include <input/input.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <furi_hal_serial_types.h>

#define TAG "SecMonitor"

// UART Configuration
#define UART_BAUD_RATE 9600
#define UART_RX_BUFFER_SIZE 256

// Connection timeout (60 seconds - increased for long connections)
#define CONNECTION_TIMEOUT_MS 60000

// Alert state timing
#define ALERT_DURATION_MS 3000
#define COOLDOWN_DURATION_MS 2000

// BLE Device name (for reference)
#define BLE_DEVICE_NAME "SecMonitor-PIR"

// System states
typedef enum {
    SystemStateIdle = 0,
    SystemStateAlert = 1,
    SystemStateCooldown = 2
} SystemState;

// Connection states
typedef enum {
    ConnectionStateDisconnected,
    ConnectionStateConnecting,
    ConnectionStateConnected
} ConnectionState;

// View model
typedef struct {
    ConnectionState connection_state;
    uint32_t alert_count;
    uint8_t battery_level;
    int16_t rssi_level;  // Signal strength in dBm (-999 = unknown)
    SystemState system_status;
    uint32_t last_alert_tick;
    uint32_t last_heartbeat_tick;
    uint32_t last_message_timestamp;
    bool blink_state;
    uint8_t animation_frame;
    uint32_t total_messages_received;
    uint32_t checksum_errors;
    uint32_t alert_state_start_tick;  // For non-blocking alert timing
    bool alert_notification_sent;     // Track if notifications were sent
} SecMonitorModel;

// App structure
typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    View* view_main;
    NotificationApp* notifications;
    FuriTimer* update_timer;
    FuriMutex* mutex;
    FuriHalSerialHandle* serial_handle;
    FuriStreamBuffer* rx_stream;
    FuriThread* uart_worker_thread;
    bool running;
} SecMonitorApp;

// Forward declarations
static void sec_monitor_draw_callback(Canvas* canvas, void* context);
static bool sec_monitor_input_callback(InputEvent* event, void* context);
static void sec_monitor_enter_callback(void* context);
static void sec_monitor_exit_callback(void* context);
static void sec_monitor_timer_callback(void* context);
static int32_t uart_worker(void* context);
static void uart_on_irq_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context);
static void process_uart_data(SecMonitorApp* app, const char* data);
static uint8_t calculate_checksum(const char* data, size_t length);
static bool parse_protocol_message(const char* message, char* type, char* value, uint32_t* timestamp, uint8_t* checksum);

/**
 * @brief Clean UI - Connected State
 */
static void draw_connected_view(Canvas* canvas, SecMonitorModel* model) {
    // Header
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);

    // Display RSSI signal strength (left side) - replacing "SEC"
    if(model->rssi_level != -999) {
        // Draw signal bars based on RSSI
        // RSSI thresholds: Excellent >= -50, Good >= -60, Fair >= -70, Poor < -70
        int bars = 0;
        if(model->rssi_level >= -50) {
            bars = 4;
        } else if(model->rssi_level >= -60) {
            bars = 3;
        } else if(model->rssi_level >= -70) {
            bars = 2;
        } else {
            bars = 1;
        }

        // Draw 4 signal bars (left side)
        for(int i = 0; i < 4; i++) {
            int bar_height = 2 + (i * 1);
            int bar_x = 2 + (i * 2);
            int bar_y = 8 - bar_height;

            if(i < bars) {
                // Filled bar
                canvas_draw_box(canvas, bar_x, bar_y, 1, bar_height);
            } else {
                // Empty bar (outline only)
                canvas_draw_frame(canvas, bar_x, bar_y, 1, bar_height);
            }
        }

        // Display RSSI value in dBm
        char rssi_str[12];
        snprintf(rssi_str, sizeof(rssi_str), "%ddB", model->rssi_level);
        canvas_draw_str_aligned(canvas, 12, 2, AlignLeft, AlignTop, rssi_str);
    } else {
        // Fallback: display "SEC" if RSSI not available
        canvas_draw_str_aligned(canvas, 2, 2, AlignLeft, AlignTop, "SEC");
    }

    char bat_str[12];
    snprintf(bat_str, sizeof(bat_str), "Bat:%u%%", model->battery_level);
    canvas_draw_str_aligned(canvas, 126, 2, AlignRight, AlignTop, bat_str);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 0, 10, 128, 10);

    // Hero section - Status and Time
    canvas_set_font(canvas, FontPrimary);
    const char* status_str;
    switch(model->system_status) {
        case SystemStateIdle:
            status_str = "All Clear";
            break;
        case SystemStateAlert:
            status_str = "ALERT!";
            break;
        case SystemStateCooldown:
            status_str = "Cooldown";
            break;
        default:
            status_str = "Unknown";
    }
    canvas_draw_str(canvas, 2, 30, status_str);

    char time_str[16];
    if(model->last_alert_tick > 0) {
        uint32_t seconds_ago = (furi_get_tick() - model->last_alert_tick) / 1000;
        if(seconds_ago < 60) {
            snprintf(time_str, sizeof(time_str), "%lus", seconds_ago);
        } else if(seconds_ago < 3600) {
            snprintf(time_str, sizeof(time_str), "%lum", seconds_ago / 60);
        } else {
            snprintf(time_str, sizeof(time_str), "%luh", seconds_ago / 3600);
        }
    } else {
        snprintf(time_str, sizeof(time_str), "Ready");
    }
    canvas_draw_str(canvas, 2, 42, time_str);

    // Alert count
    canvas_set_font(canvas, FontBigNumbers);
    char count_str[16];
    snprintf(count_str, sizeof(count_str), "%lu", model->alert_count);
    canvas_draw_str_aligned(canvas, 124, 32, AlignRight, AlignCenter, count_str);

    canvas_draw_line(canvas, 0, 54, 128, 54);

    // Footer
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 2, 56, AlignLeft, AlignTop, "Connected");
    canvas_draw_str_aligned(canvas, 126, 56, AlignRight, AlignTop, "M5StickC");

    // Alert animation
    if(model->system_status == SystemStateAlert && model->blink_state) {
        canvas_draw_frame(canvas, 0, 11, 128, 43);
    }
}

/**
 * @brief Clean UI - Disconnected State
 */
static void draw_disconnected_view(Canvas* canvas, SecMonitorModel* model) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Sec Monitor");
    canvas_draw_line(canvas, 0, 14, 128, 14);

    canvas_set_font(canvas, FontPrimary);
    const char* status;
    if(model->connection_state == ConnectionStateConnecting) {
        status = "Connecting";
        char status_with_dots[16];
        int dot_count = (model->animation_frame / 3) % 4;
        snprintf(status_with_dots, sizeof(status_with_dots), "%s%.*s", status, dot_count, "...");
        canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignTop, status_with_dots);
    } else {
        status = "Not Connected";
        canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignTop, status);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignTop, "Waiting for UART");

    canvas_draw_line(canvas, 0, 46, 128, 46);

    // canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignTop, "Bluefruit LE");
    canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignTop, BLE_DEVICE_NAME);
}

/**
 * @brief Main draw callback
 */
static void sec_monitor_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    SecMonitorModel* model = (SecMonitorModel*)context;

    canvas_clear(canvas);

    if(model->connection_state == ConnectionStateConnected) {
        draw_connected_view(canvas, model);
    } else {
        draw_disconnected_view(canvas, model);
    }
}

/**
 * @brief Simulate alert (for testing) - Non-blocking version
 */
static void simulate_alert(View* view, NotificationApp* notifications) {
    FURI_LOG_W(TAG, "🧪 Test alert triggered (non-blocking)!");

    with_view_model(
        view,
        SecMonitorModel * model,
        {
            model->alert_count++;
            model->system_status = SystemStateAlert;
            model->last_alert_tick = furi_get_tick();
            model->alert_state_start_tick = furi_get_tick();
            model->alert_notification_sent = false;
        },
        true);

    // Send notifications immediately (non-blocking)
    notification_message(notifications, &sequence_display_backlight_on);
    notification_message(notifications, &sequence_single_vibro);
    notification_message(notifications, &sequence_blink_red_100);

    // State transitions now handled by timer callback (non-blocking)
}

/**
 * @brief Input callback
 */
static bool sec_monitor_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    SecMonitorApp* app = (SecMonitorApp*)context;

    if(event->type != InputTypeShort) {
        return false;
    }

    if(event->key == InputKeyBack) {
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }

    if(event->key == InputKeyOk) {
        bool is_connected = false;

        with_view_model(
            app->view_main,
            SecMonitorModel * model,
            { is_connected = (model->connection_state == ConnectionStateConnected); },
            false);

        if(is_connected) {
            FURI_LOG_I(TAG, "Test alert");
            simulate_alert(app->view_main, app->notifications);
        }
        return true;
    }

    return false;
}

/**
 * @brief Enter callback
 */
static void sec_monitor_enter_callback(void* context) {
    furi_assert(context);
    SecMonitorApp* app = (SecMonitorApp*)context;

    FURI_LOG_I(TAG, "🛡️ Sec Monitor v9.0 started (non-blocking mode)");

    furi_timer_start(app->update_timer, furi_ms_to_ticks(200));
}

/**
 * @brief Exit callback
 */
static void sec_monitor_exit_callback(void* context) {
    furi_assert(context);
    SecMonitorApp* app = (SecMonitorApp*)context;

    FURI_LOG_I(TAG, "Sec Monitor exiting");
    furi_timer_stop(app->update_timer);
}

/**
 * @brief Timer callback - Non-blocking state machine
 */
static void sec_monitor_timer_callback(void* context) {
    furi_assert(context);
    SecMonitorApp* app = (SecMonitorApp*)context;

    with_view_model(
        app->view_main,
        SecMonitorModel * model,
        {
            model->animation_frame++;
            if(model->animation_frame > 100) model->animation_frame = 0;

            if(model->animation_frame % 2 == 0) {
                model->blink_state = !model->blink_state;
            }

            // Check connection timeout (60 seconds since last message)
            if(model->connection_state == ConnectionStateConnected) {
                uint32_t time_since_last_msg = furi_get_tick() - model->last_heartbeat_tick;
                if(time_since_last_msg > CONNECTION_TIMEOUT_MS) {
                    FURI_LOG_W(TAG, "⚠ Connection timeout! No messages for %lu ms", time_since_last_msg);
                    model->connection_state = ConnectionStateDisconnected;
                }
            }

            // Non-blocking alert state management
            if(model->system_status == SystemStateAlert) {
                uint32_t time_in_alert = furi_get_tick() - model->alert_state_start_tick;

                // Send notification once at the start of alert
                if(!model->alert_notification_sent) {
                    model->alert_notification_sent = true;
                    // Notifications will be sent outside with_view_model
                }

                // Transition to cooldown after ALERT_DURATION_MS
                if(time_in_alert >= ALERT_DURATION_MS) {
                    model->system_status = SystemStateCooldown;
                    model->alert_state_start_tick = furi_get_tick();
                    FURI_LOG_D(TAG, "→ Cooldown state");
                }
            } else if(model->system_status == SystemStateCooldown) {
                uint32_t time_in_cooldown = furi_get_tick() - model->alert_state_start_tick;

                // Transition to idle after COOLDOWN_DURATION_MS
                if(time_in_cooldown >= COOLDOWN_DURATION_MS) {
                    model->system_status = SystemStateIdle;
                    FURI_LOG_D(TAG, "→ Idle state");
                }
            }
        },
        true);
}

/**
 * @brief UART IRQ callback - Enhanced logging for debugging
 */
static void uart_on_irq_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    SecMonitorApp* app = context;

    if(event & FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        // Reduced logging - only log printable characters or control chars
        if(data == '\n' || data == '\r') {
            FURI_LOG_T(TAG, "UART: <NL>");
        } else if(data >= 32 && data < 127) {
            FURI_LOG_T(TAG, "UART: '%c' (0x%02X)", data, data);
        } else {
            FURI_LOG_T(TAG, "UART: 0x%02X", data);
        }
        furi_stream_buffer_send(app->rx_stream, &data, 1, 0);
    }
}

/**
 * @brief Calculate XOR checksum
 */
static uint8_t calculate_checksum(const char* data, size_t length) {
    uint8_t checksum = 0;
    for(size_t i = 0; i < length; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

/**
 * @brief Parse enhanced protocol message
 * Format: <TYPE>:<VALUE>:<TIMESTAMP>:<CHECKSUM>\n
 * @return true if successfully parsed, false otherwise
 */
static bool parse_protocol_message(const char* message, char* type, char* value, uint32_t* timestamp, uint8_t* checksum) {
    // Find delimiters
    const char* first_colon = strchr(message, ':');
    if(!first_colon) return false;

    const char* second_colon = strchr(first_colon + 1, ':');
    if(!second_colon) return false;

    const char* third_colon = strchr(second_colon + 1, ':');
    if(!third_colon) return false;

    // Extract TYPE
    size_t type_len = first_colon - message;
    if(type_len >= 32) return false;
    strncpy(type, message, type_len);
    type[type_len] = '\0';

    // Extract VALUE
    size_t value_len = second_colon - (first_colon + 1);
    if(value_len >= 32) return false;
    strncpy(value, first_colon + 1, value_len);
    value[value_len] = '\0';

    // Extract TIMESTAMP
    *timestamp = strtoul(second_colon + 1, NULL, 10);

    // Extract CHECKSUM (hex)
    *checksum = strtoul(third_colon + 1, NULL, 16);

    return true;
}

/**
 * @brief Process UART data with enhanced protocol
 */
static void process_uart_data(SecMonitorApp* app, const char* data) {
    FURI_LOG_I(TAG, "UART RX: %s", data);

    char type[32];
    char value[32];
    uint32_t timestamp;
    uint8_t received_checksum;

    // Parse message
    if(!parse_protocol_message(data, type, value, &timestamp, &received_checksum)) {
        FURI_LOG_E(TAG, "Failed to parse message: %s", data);
        return;
    }

    // Calculate expected checksum (TYPE:VALUE:TIMESTAMP)
    char checksum_base[128];
    snprintf(checksum_base, sizeof(checksum_base), "%s:%s:%lu", type, value, timestamp);
    uint8_t calculated_checksum = calculate_checksum(checksum_base, strlen(checksum_base));

    // Verify checksum
    if(calculated_checksum != received_checksum) {
        FURI_LOG_E(
            TAG,
            "❌ Checksum mismatch! Calc:%02X != Recv:%02X | Type:%s Val:%s",
            calculated_checksum,
            received_checksum,
            type,
            value);
        with_view_model(
            app->view_main,
            SecMonitorModel * model,
            {
                model->checksum_errors++;
                FURI_LOG_W(TAG, "Total checksum errors: %lu", model->checksum_errors);
            },
            false);
        return;
    }

    // Update message counters and connection timestamp
    with_view_model(
        app->view_main,
        SecMonitorModel * model,
        {
            model->total_messages_received++;
            model->last_message_timestamp = timestamp;
            model->last_heartbeat_tick = furi_get_tick();  // Update on every valid message
        },
        false);

    FURI_LOG_I(TAG, "✓ Valid message: %s = %s (ts=%lu)", type, value, timestamp);

    // Process message by type
    if(strcmp(type, "ALERT") == 0) {
        int alert_value = atoi(value);
        if(alert_value == 1) {
            FURI_LOG_W(TAG, "🚨 PIR ALERT RECEIVED (non-blocking)");

            bool should_notify = false;

            with_view_model(
                app->view_main,
                SecMonitorModel * model,
                {
                    model->alert_count++;
                    model->system_status = SystemStateAlert;
                    model->last_alert_tick = furi_get_tick();
                    model->alert_state_start_tick = furi_get_tick();
                    model->alert_notification_sent = false;
                    should_notify = true;
                },
                true);

            // Send notifications immediately (non-blocking)
            if(should_notify) {
                FURI_LOG_I(TAG, "→ Sending notifications (backlight, vibrate, LED)");
                notification_message(app->notifications, &sequence_display_backlight_on);
                notification_message(app->notifications, &sequence_single_vibro);
                notification_message(app->notifications, &sequence_blink_red_100);
            }

            // State transitions now handled by timer callback (non-blocking)
        }
    } else if(strcmp(type, "COUNT") == 0) {
        uint32_t count = strtoul(value, NULL, 10);
        with_view_model(
            app->view_main,
            SecMonitorModel * model,
            { model->alert_count = count; },
            true);
    } else if(strcmp(type, "BATTERY") == 0) {
        uint8_t battery = atoi(value);
        with_view_model(
            app->view_main,
            SecMonitorModel * model,
            { model->battery_level = battery; },
            true);

        // Mark as connected on first battery message
        with_view_model(
            app->view_main,
            SecMonitorModel * model,
            {
                if(model->connection_state != ConnectionStateConnected) {
                    model->connection_state = ConnectionStateConnected;
                    FURI_LOG_I(TAG, "✓ Connection established!");
                    notification_message(app->notifications, &sequence_success);
                }
            },
            true);
    } else if(strcmp(type, "STATUS") == 0) {
        // DEPRECATED: Ignore STATUS messages from M5StickC
        // Flipper Zero manages its own state machine based on ALERT messages
        // Processing STATUS would cause race conditions and state conflicts
        FURI_LOG_D(TAG, "⚠ STATUS message ignored (deprecated): %s", value);
    } else if(strcmp(type, "HEARTBEAT") == 0) {
        FURI_LOG_D(TAG, "♥ Heartbeat received");
        // last_heartbeat_tick is already updated above
    } else if(strcmp(type, "RSSI") == 0) {
        int16_t rssi = (int16_t)atoi(value);
        with_view_model(
            app->view_main,
            SecMonitorModel * model,
            { model->rssi_level = rssi; },
            true);
        FURI_LOG_D(TAG, "📶 RSSI updated: %d dBm", rssi);
    } else {
        FURI_LOG_W(TAG, "Unknown message type: %s", type);
    }
}

/**
 * @brief Process potentially concatenated messages in a line
 * BLE modules like Bluefruit can buffer multiple messages and send them together
 * This function splits them by looking for the protocol pattern
 */
static void process_concatenated_line(SecMonitorApp* app, const char* line) {
    // Protocol pattern: TYPE:VALUE:TIMESTAMP:CHECKSUM
    // Look for patterns like "HEARTBEAT:OK:..." within the line

    const char* current = line;
    char msg_buffer[128];
    size_t msg_pos = 0;
    int field_count = 0;

    FURI_LOG_D(TAG, "📦 Processing line (%zu bytes): %s", strlen(line), line);

    while(*current) {
        // Count colons to detect message boundaries
        if(*current == ':') {
            field_count++;
        }

        msg_buffer[msg_pos++] = *current;

        // Check if we have a complete message (4 fields: TYPE:VALUE:TIMESTAMP:CHECKSUM)
        if(field_count >= 3 && msg_pos < sizeof(msg_buffer) - 1) {
            // Look ahead to see if next char starts a new message pattern
            const char* next = current + 1;

            // Check if next part looks like a new message type
            // Common types: HEARTBEAT, BATTERY, ALERT, COUNT, STATUS
            bool looks_like_new_message = false;
            if(*next != '\0') {
                if(strncmp(next, "HEARTBEAT:", 10) == 0 ||
                   strncmp(next, "BATTERY:", 8) == 0 ||
                   strncmp(next, "ALERT:", 6) == 0 ||
                   strncmp(next, "COUNT:", 6) == 0 ||
                   strncmp(next, "STATUS:", 7) == 0) {
                    looks_like_new_message = true;
                }
            }

            if(looks_like_new_message || *next == '\0') {
                // Complete message found
                msg_buffer[msg_pos] = '\0';
                FURI_LOG_D(TAG, "✂️ Extracted message: %s", msg_buffer);
                process_uart_data(app, msg_buffer);

                // Reset for next message
                msg_pos = 0;
                field_count = 0;
            }
        }

        // Buffer overflow protection
        if(msg_pos >= sizeof(msg_buffer) - 1) {
            FURI_LOG_E(TAG, "⚠ Message buffer overflow! Resetting...");
            msg_pos = 0;
            field_count = 0;
        }

        current++;
    }

    // Process any remaining data
    if(msg_pos > 0) {
        msg_buffer[msg_pos] = '\0';
        FURI_LOG_D(TAG, "✂️ Final message: %s", msg_buffer);
        process_uart_data(app, msg_buffer);
    }
}

/**
 * @brief UART worker thread - Enhanced with buffer overflow protection and concatenation handling
 */
static int32_t uart_worker(void* context) {
    SecMonitorApp* app = context;
    char line_buffer[256];  // Increased to handle concatenated messages
    size_t line_pos = 0;
    uint32_t line_overflow_count = 0;

    FURI_LOG_I(TAG, "📡 UART worker started (enhanced mode with concat handling)");

    while(app->running) {
        uint8_t data;
        size_t received = furi_stream_buffer_receive(app->rx_stream, &data, 1, 100);

        if(received > 0) {
            if(data == '\n' || data == '\r') {
                if(line_pos > 0) {
                    line_buffer[line_pos] = '\0';

                    // Check if line contains multiple messages (concatenated)
                    // Count colons - if more than 4, likely concatenated
                    int colon_count = 0;
                    for(size_t i = 0; i < line_pos; i++) {
                        if(line_buffer[i] == ':') colon_count++;
                    }

                    if(colon_count > 4) {
                        FURI_LOG_W(TAG, "⚠ Concatenated messages detected (%d colons)", colon_count);
                        process_concatenated_line(app, line_buffer);
                    } else {
                        FURI_LOG_D(TAG, "📩 RX line (%zu bytes): %s", line_pos, line_buffer);
                        process_uart_data(app, line_buffer);
                    }

                    line_pos = 0;
                }
            } else if(line_pos < sizeof(line_buffer) - 1) {
                line_buffer[line_pos++] = data;
            } else {
                // Buffer overflow protection
                if(line_overflow_count == 0) {
                    FURI_LOG_E(TAG, "⚠ Line buffer overflow! Discarding data...");
                }
                line_overflow_count++;
                // Discard data until newline
                if(data == '\n' || data == '\r') {
                    FURI_LOG_W(TAG, "Buffer overflow: discarded %lu bytes", line_overflow_count);
                    line_overflow_count = 0;
                    line_pos = 0;
                }
            }
        }
    }

    FURI_LOG_I(TAG, "📡 UART worker stopped");
    return 0;
}

/**
 * @brief Allocate view
 */
static View* sec_monitor_view_alloc(SecMonitorApp* app) {
    View* view = view_alloc();

    view_allocate_model(view, ViewModelTypeLockFree, sizeof(SecMonitorModel));

    with_view_model(
        view,
        SecMonitorModel * model,
        {
            model->connection_state = ConnectionStateDisconnected;
            model->alert_count = 0;
            model->battery_level = 0;
            model->rssi_level = -999;  // Unknown RSSI initially
            model->system_status = SystemStateIdle;
            model->last_alert_tick = 0;
            model->last_heartbeat_tick = 0;
            model->last_message_timestamp = 0;
            model->blink_state = false;
            model->animation_frame = 0;
            model->total_messages_received = 0;
            model->checksum_errors = 0;
            model->alert_state_start_tick = 0;
            model->alert_notification_sent = false;
        },
        false);

    view_set_context(view, app);
    view_set_draw_callback(view, sec_monitor_draw_callback);
    view_set_input_callback(view, sec_monitor_input_callback);
    view_set_enter_callback(view, sec_monitor_enter_callback);
    view_set_exit_callback(view, sec_monitor_exit_callback);

    return view;
}

/**
 * @brief Allocate app
 */
static SecMonitorApp* sec_monitor_app_alloc(void) {
    SecMonitorApp* app = malloc(sizeof(SecMonitorApp));
    memset(app, 0, sizeof(SecMonitorApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->view_main = sec_monitor_view_alloc(app);
    view_dispatcher_add_view(app->view_dispatcher, 0, app->view_main);

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    app->update_timer =
        furi_timer_alloc(sec_monitor_timer_callback, FuriTimerTypePeriodic, app);

    // Initialize UART
    app->serial_handle = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    furi_check(app->serial_handle);

    furi_hal_serial_init(app->serial_handle, UART_BAUD_RATE);

    // Create RX stream buffer
    app->rx_stream = furi_stream_buffer_alloc(UART_RX_BUFFER_SIZE, 1);

    // Start async RX
    furi_hal_serial_async_rx_start(app->serial_handle, uart_on_irq_cb, app, false);

    // Start UART worker thread
    app->running = true;
    app->uart_worker_thread = furi_thread_alloc();
    furi_thread_set_name(app->uart_worker_thread, "UartWorker");
    furi_thread_set_stack_size(app->uart_worker_thread, 2048);
    furi_thread_set_context(app->uart_worker_thread, app);
    furi_thread_set_callback(app->uart_worker_thread, uart_worker);
    furi_thread_start(app->uart_worker_thread);

    FURI_LOG_I(TAG, "UART initialized at %d baud", UART_BAUD_RATE);

    return app;
}

/**
 * @brief Free app
 */
static void sec_monitor_app_free(SecMonitorApp* app) {
    furi_assert(app);

    // Stop UART worker
    app->running = false;
    furi_thread_join(app->uart_worker_thread);
    furi_thread_free(app->uart_worker_thread);

    // Stop UART
    furi_hal_serial_async_rx_stop(app->serial_handle);
    furi_hal_serial_deinit(app->serial_handle);
    furi_hal_serial_control_release(app->serial_handle);

    // Free stream buffer
    furi_stream_buffer_free(app->rx_stream);

    furi_timer_stop(app->update_timer);
    furi_timer_free(app->update_timer);

    furi_mutex_free(app->mutex);

    view_dispatcher_remove_view(app->view_dispatcher, 0);
    view_free(app->view_main);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

/**
 * @brief Main entry point
 */
int32_t sec_monitor_app(void* p) {
    UNUSED(p);

    FURI_LOG_I(TAG, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    FURI_LOG_I(TAG, " 🛡️  Sec Monitor v9.0 Professional");
    FURI_LOG_I(TAG, " ⚡ Non-Blocking Architecture");
    FURI_LOG_I(TAG, " 📡 Supports HM-10 & Bluefruit LE");
    FURI_LOG_I(TAG, " ✓  Enhanced Protocol + Validation");
    FURI_LOG_I(TAG, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");

    SecMonitorApp* app = sec_monitor_app_alloc();

    view_dispatcher_switch_to_view(app->view_dispatcher, 0);

    notification_message(app->notifications, &sequence_display_backlight_on);

    view_dispatcher_run(app->view_dispatcher);

    sec_monitor_app_free(app);

    FURI_LOG_I(TAG, "Sec Monitor stopped");

    return 0;
}

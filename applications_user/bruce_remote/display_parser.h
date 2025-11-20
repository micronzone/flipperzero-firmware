#pragma once

#include <furi.h>
#include <gui/canvas.h>

// Bruce tft_logger protocol
#define TFT_LOG_HEADER 0xAA

// tftFuncs enum from Bruce
typedef enum {
    TFT_FILLSCREEN = 0,
    TFT_DRAWRECT = 1,
    TFT_FILLRECT = 2,
    TFT_DRAWROUNDRECT = 3,
    TFT_FILLROUNDRECT = 4,
    TFT_DRAWCIRCLE = 5,
    TFT_FILLCIRCLE = 6,
    TFT_DRAWTRIAGLE = 7,
    TFT_FILLTRIANGLE = 8,
    TFT_DRAWELIPSE = 9,
    TFT_FILLELIPSE = 10,
    TFT_DRAWLINE = 11,
    TFT_DRAWARC = 12,
    TFT_DRAWWIDELINE = 13,
    TFT_DRAWCENTRESTRING = 14,
    TFT_DRAWRIGHTSTRING = 15,
    TFT_DRAWSTRING = 16,
    TFT_PRINT = 17,
    TFT_DRAWIMAGE = 18,
    TFT_DRAWPIXEL = 19,
    TFT_DRAWFASTVLINE = 20,
    TFT_DRAWFASTHLINE = 21,
    TFT_SCREEN_INFO = 99,
} TftFunc;

// Display buffer for rendering
typedef struct {
    // Screen dimensions from Bruce
    uint16_t width;   // Bruce screen width (e.g., 320)
    uint16_t height;  // Bruce screen height (e.g., 240)
    uint8_t rotation;

    // Display commands queue
    FuriMessageQueue* command_queue;

    // Stats
    uint32_t packets_received;
    uint32_t packets_dropped;

} DisplayBuffer;

// Display command
typedef struct {
    TftFunc func;
    uint16_t params[16];  // Max 16 uint16_t parameters
    uint8_t param_count;
    char* text;  // For string commands
} DisplayCommand;

// Parser state machine
typedef struct {
    uint8_t buffer[256];
    size_t buffer_pos;

    bool in_packet;
    uint8_t packet_size;
    uint8_t bytes_received;

    DisplayBuffer* display_buffer;
} DisplayParser;

/**
 * @brief Create display parser
 * @return DisplayParser*
 */
DisplayParser* display_parser_alloc(void);

/**
 * @brief Free display parser
 * @param parser
 */
void display_parser_free(DisplayParser* parser);

/**
 * @brief Parse incoming UART data
 * @param parser
 * @param data
 * @param length
 */
void display_parser_parse(DisplayParser* parser, const uint8_t* data, size_t length);

/**
 * @brief Get next display command
 * @param parser
 * @param command Output command
 * @return bool True if command available
 */
bool display_parser_get_command(DisplayParser* parser, DisplayCommand* command);

/**
 * @brief Render display buffer to Canvas
 * @param parser
 * @param canvas
 */
void display_parser_render(DisplayParser* parser, Canvas* canvas);

/**
 * @brief Clear display buffer
 * @param parser
 */
void display_parser_clear(DisplayParser* parser);

/**
 * @brief Scale coordinate from Bruce to Flipper
 * @param value Bruce coordinate
 * @param bruce_max Bruce screen dimension
 * @param flipper_max Flipper screen dimension
 * @return Scaled coordinate
 */
static inline int scale_coord(int value, int bruce_max, int flipper_max) {
    return (value * flipper_max) / bruce_max;
}

/**
 * @brief Convert RGB565 color to Flipper Black/White
 * @param rgb565 16-bit color
 * @return Color (ColorBlack or ColorWhite)
 */
static inline Color rgb565_to_bw(uint16_t rgb565) {
    // Simple threshold: if color is bright, use white, otherwise black
    return (rgb565 > 0x8410) ? ColorBlack : ColorWhite;
}

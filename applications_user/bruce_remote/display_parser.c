#include "display_parser.h"
#include <furi.h>
#include <gui/canvas.h>

#define MAX_COMMAND_QUEUE 32

// Debug logging configuration
#define DEBUG_PARSER 1
#define DEBUG_PACKET_DETAILS 1
#define DEBUG_RAW_BYTES 0  // Set to 1 for very verbose byte-by-byte logging

// Helper function to log hex data
static void log_hex_data(const char* prefix, const uint8_t* data, size_t length) {
    if(length == 0) return;

    // Log in chunks of 16 bytes for readability
    for(size_t i = 0; i < length; i += 16) {
        char hex_str[64];
        size_t hex_pos = 0;
        size_t chunk_len = (length - i) > 16 ? 16 : (length - i);

        for(size_t j = 0; j < chunk_len && hex_pos < sizeof(hex_str) - 3; j++) {
            hex_pos += snprintf(hex_str + hex_pos, sizeof(hex_str) - hex_pos,
                               "%02X ", data[i + j]);
        }

        FURI_LOG_D("BruceRemote", "%s[%zu]: %s", prefix, i, hex_str);
    }
}

DisplayParser* display_parser_alloc(void) {
    DisplayParser* parser = malloc(sizeof(DisplayParser));

    parser->buffer_pos = 0;
    parser->in_packet = false;
    parser->packet_size = 0;
    parser->bytes_received = 0;

    // Allocate display buffer
    parser->display_buffer = malloc(sizeof(DisplayBuffer));
    parser->display_buffer->width = 320;   // Default Bruce dimensions
    parser->display_buffer->height = 240;
    parser->display_buffer->rotation = 1;
    parser->display_buffer->packets_received = 0;
    parser->display_buffer->packets_dropped = 0;

    // Create command queue
    parser->display_buffer->command_queue =
        furi_message_queue_alloc(MAX_COMMAND_QUEUE, sizeof(DisplayCommand));

    return parser;
}

void display_parser_free(DisplayParser* parser) {
    furi_assert(parser);

    if(parser->display_buffer) {
        if(parser->display_buffer->command_queue) {
            furi_message_queue_free(parser->display_buffer->command_queue);
        }
        free(parser->display_buffer);
    }

    free(parser);
}

// Parse uint16_t from buffer (big-endian)
static uint16_t parse_uint16(const uint8_t* data, size_t offset) {
    return (data[offset] << 8) | data[offset + 1];
}

// Parse single packet
static void parse_packet(DisplayParser* parser, const uint8_t* packet, uint8_t size) {
    if(size < 3) {
        FURI_LOG_E("BruceRemote", "Parse: Packet too small (%d bytes)", size);
        return;  // Minimum: header + size + func
    }

    uint8_t func = packet[2];

#if DEBUG_PACKET_DETAILS
    FURI_LOG_I("BruceRemote", "Parse: size=%d func=0x%02X (%d)", size, func, func);
    log_hex_data("Parse packet", packet, size);
#endif

    DisplayCommand cmd = {0};
    cmd.func = func;
    cmd.text = NULL;

    // Parse parameters based on function
    switch(func) {
        case TFT_SCREEN_INFO:
            // AA 07 99 WW WW HH HH RR
            if(size >= 8) {
                parser->display_buffer->width = parse_uint16(packet, 3);
                parser->display_buffer->height = parse_uint16(packet, 5);
                parser->display_buffer->rotation = packet[7];
                FURI_LOG_I("BruceRemote", "SCREEN_INFO: %dx%d rot=%d",
                    parser->display_buffer->width,
                    parser->display_buffer->height,
                    parser->display_buffer->rotation);
            } else {
                FURI_LOG_E("BruceRemote", "SCREEN_INFO: Invalid size %d (need 8)", size);
            }
            return;  // Don't queue this command

        case TFT_FILLSCREEN:
            // AA 05 00 CC CC
            if(size >= 5) {
                cmd.params[0] = parse_uint16(packet, 3);
                cmd.param_count = 1;
#if DEBUG_PACKET_DETAILS
                FURI_LOG_D("BruceRemote", "FILLSCREEN: color=0x%04X", cmd.params[0]);
#endif
            } else {
                FURI_LOG_E("BruceRemote", "FILLSCREEN: Invalid size %d (need 5)", size);
                return;
            }
            break;

        case TFT_DRAWRECT:
        case TFT_FILLRECT:
            // AA 0D 01/02 XX XX YY YY WW WW HH HH CC CC
            if(size >= 13) {
                cmd.params[0] = parse_uint16(packet, 3);   // x
                cmd.params[1] = parse_uint16(packet, 5);   // y
                cmd.params[2] = parse_uint16(packet, 7);   // w
                cmd.params[3] = parse_uint16(packet, 9);   // h
                cmd.params[4] = parse_uint16(packet, 11);  // color
                cmd.param_count = 5;
#if DEBUG_PACKET_DETAILS
                FURI_LOG_D("BruceRemote", "%s: x=%d y=%d w=%d h=%d color=0x%04X",
                    func == TFT_DRAWRECT ? "DRAWRECT" : "FILLRECT",
                    cmd.params[0], cmd.params[1], cmd.params[2], cmd.params[3], cmd.params[4]);
#endif
            } else {
                FURI_LOG_E("BruceRemote", "%s: Invalid size %d (need 13)",
                    func == TFT_DRAWRECT ? "DRAWRECT" : "FILLRECT", size);
                return;
            }
            break;

        case TFT_DRAWROUNDRECT:
        case TFT_FILLROUNDRECT:
            // AA 0F 03/04 XX XX YY YY WW WW HH HH RR RR CC CC
            if(size >= 15) {
                cmd.params[0] = parse_uint16(packet, 3);   // x
                cmd.params[1] = parse_uint16(packet, 5);   // y
                cmd.params[2] = parse_uint16(packet, 7);   // w
                cmd.params[3] = parse_uint16(packet, 9);   // h
                cmd.params[4] = parse_uint16(packet, 11);  // r (radius)
                cmd.params[5] = parse_uint16(packet, 13);  // color
                cmd.param_count = 6;
#if DEBUG_PACKET_DETAILS
                FURI_LOG_D("BruceRemote", "%s: x=%d y=%d w=%d h=%d r=%d color=0x%04X",
                    func == TFT_DRAWROUNDRECT ? "DRAWROUNDRECT" : "FILLROUNDRECT",
                    cmd.params[0], cmd.params[1], cmd.params[2], cmd.params[3],
                    cmd.params[4], cmd.params[5]);
#endif
            } else {
                FURI_LOG_E("BruceRemote", "%s: Invalid size %d (need 15)",
                    func == TFT_DRAWROUNDRECT ? "DRAWROUNDRECT" : "FILLROUNDRECT", size);
                return;
            }
            break;

        case TFT_DRAWCIRCLE:
        case TFT_FILLCIRCLE:
            // AA 0B 05/06 XX XX YY YY RR RR CC CC
            if(size >= 11) {
                cmd.params[0] = parse_uint16(packet, 3);  // x
                cmd.params[1] = parse_uint16(packet, 5);  // y
                cmd.params[2] = parse_uint16(packet, 7);  // r
                cmd.params[3] = parse_uint16(packet, 9);  // color
                cmd.param_count = 4;
#if DEBUG_PACKET_DETAILS
                FURI_LOG_D("BruceRemote", "%s: x=%d y=%d r=%d color=0x%04X",
                    func == TFT_DRAWCIRCLE ? "DRAWCIRCLE" : "FILLCIRCLE",
                    cmd.params[0], cmd.params[1], cmd.params[2], cmd.params[3]);
#endif
            } else {
                FURI_LOG_E("BruceRemote", "%s: Invalid size %d (need 11)",
                    func == TFT_DRAWCIRCLE ? "DRAWCIRCLE" : "FILLCIRCLE", size);
                return;
            }
            break;

        case TFT_DRAWLINE:
            // AA 0D 0B XX XX YY YY X1 X1 Y1 Y1 CC CC
            if(size >= 13) {
                cmd.params[0] = parse_uint16(packet, 3);   // x0
                cmd.params[1] = parse_uint16(packet, 5);   // y0
                cmd.params[2] = parse_uint16(packet, 7);   // x1
                cmd.params[3] = parse_uint16(packet, 9);   // y1
                cmd.params[4] = parse_uint16(packet, 11);  // color
                cmd.param_count = 5;
#if DEBUG_PACKET_DETAILS
                FURI_LOG_D("BruceRemote", "DRAWLINE: (%d,%d)->(%d,%d) color=0x%04X",
                    cmd.params[0], cmd.params[1], cmd.params[2], cmd.params[3], cmd.params[4]);
#endif
            } else {
                FURI_LOG_E("BruceRemote", "DRAWLINE: Invalid size %d (need 13)", size);
                return;
            }
            break;

        case TFT_DRAWSTRING:
        case TFT_DRAWCENTRESTRING:
        case TFT_DRAWRIGHTSTRING:
            // AA SS 16/14/15 XX XX YY YY FF "text..."
            if(size >= 8) {
                cmd.params[0] = parse_uint16(packet, 3);  // x
                cmd.params[1] = parse_uint16(packet, 5);  // y
                cmd.params[2] = packet[7];                // font
                cmd.param_count = 3;

                // Extract text
                size_t text_len = size - 8;
                if(text_len > 0) {
                    cmd.text = malloc(text_len + 1);
                    memcpy(cmd.text, &packet[8], text_len);
                    cmd.text[text_len] = '\0';
#if DEBUG_PACKET_DETAILS
                    const char* func_name = (func == TFT_DRAWSTRING) ? "DRAWSTRING" :
                                           (func == TFT_DRAWCENTRESTRING) ? "DRAWCENTRESTRING" :
                                           "DRAWRIGHTSTRING";
                    FURI_LOG_D("BruceRemote", "%s: x=%d y=%d font=%d text='%s'",
                        func_name, cmd.params[0], cmd.params[1], cmd.params[2], cmd.text);
#endif
                }
            } else {
                FURI_LOG_E("BruceRemote", "DRAWSTRING: Invalid size %d (need >=8)", size);
                return;
            }
            break;

        case TFT_PRINT:
            // AA SS 17 "text..."
            if(size > 3) {
                size_t text_len = size - 3;
                cmd.text = malloc(text_len + 1);
                memcpy(cmd.text, &packet[3], text_len);
                cmd.text[text_len] = '\0';
                cmd.param_count = 0;
#if DEBUG_PACKET_DETAILS
                FURI_LOG_D("BruceRemote", "PRINT: text='%s'", cmd.text);
#endif
            } else {
                FURI_LOG_E("BruceRemote", "PRINT: Invalid size %d (need >3)", size);
                return;
            }
            break;

        case TFT_DRAWPIXEL:
            // AA 09 19 XX XX YY YY CC CC
            if(size >= 9) {
                cmd.params[0] = parse_uint16(packet, 3);  // x
                cmd.params[1] = parse_uint16(packet, 5);  // y
                cmd.params[2] = parse_uint16(packet, 7);  // color
                cmd.param_count = 3;
#if DEBUG_PACKET_DETAILS
                FURI_LOG_D("BruceRemote", "DRAWPIXEL: x=%d y=%d color=0x%04X",
                    cmd.params[0], cmd.params[1], cmd.params[2]);
#endif
            } else {
                FURI_LOG_E("BruceRemote", "DRAWPIXEL: Invalid size %d (need 9)", size);
                return;
            }
            break;

        case TFT_DRAWFASTVLINE:
        case TFT_DRAWFASTHLINE:
            // AA 0B 20/21 XX XX YY YY LL LL CC CC
            if(size >= 11) {
                cmd.params[0] = parse_uint16(packet, 3);  // x
                cmd.params[1] = parse_uint16(packet, 5);  // y
                cmd.params[2] = parse_uint16(packet, 7);  // length
                cmd.params[3] = parse_uint16(packet, 9);  // color
                cmd.param_count = 4;
#if DEBUG_PACKET_DETAILS
                FURI_LOG_D("BruceRemote", "%s: x=%d y=%d len=%d color=0x%04X",
                    func == TFT_DRAWFASTVLINE ? "DRAWFASTVLINE" : "DRAWFASTHLINE",
                    cmd.params[0], cmd.params[1], cmd.params[2], cmd.params[3]);
#endif
            } else {
                FURI_LOG_E("BruceRemote", "%s: Invalid size %d (need 11)",
                    func == TFT_DRAWFASTVLINE ? "DRAWFASTVLINE" : "DRAWFASTHLINE", size);
                return;
            }
            break;

        default:
            // Unsupported function - skip
            FURI_LOG_W("BruceRemote", "Unsupported func: 0x%02X (%d)", func, func);
            log_hex_data("Unsupported packet", packet, size);
            return;
    }

    // Queue command
    if(furi_message_queue_put(parser->display_buffer->command_queue, &cmd, 0) != FuriStatusOk) {
        parser->display_buffer->packets_dropped++;
        FURI_LOG_W("BruceRemote", "Queue full! Dropped func=%d (dropped=%lu)",
            func, parser->display_buffer->packets_dropped);
        if(cmd.text) free(cmd.text);
    } else {
        parser->display_buffer->packets_received++;
#if DEBUG_PARSER
        FURI_LOG_D("BruceRemote", "Queued command func=%d (rcvd=%lu)",
            func, parser->display_buffer->packets_received);
#endif
    }
}

void display_parser_parse(DisplayParser* parser, const uint8_t* data, size_t length) {
    furi_assert(parser);

#if DEBUG_RAW_BYTES
    FURI_LOG_D("BruceRemote", "Parsing %zu bytes", length);
    log_hex_data("Raw input", data, length);
#endif

    for(size_t i = 0; i < length; i++) {
        uint8_t byte = data[i];

#if DEBUG_RAW_BYTES
        FURI_LOG_T("BruceRemote", "[%zu] byte=0x%02X in_packet=%d pos=%zu",
            i, byte, parser->in_packet, parser->buffer_pos);
#endif

        if(!parser->in_packet) {
            // Looking for packet header
            if(byte == TFT_LOG_HEADER) {
#if DEBUG_PARSER
                FURI_LOG_D("BruceRemote", "Header detected at offset %zu", i);
#endif
                parser->in_packet = true;
                parser->buffer[0] = byte;
                parser->buffer_pos = 1;
                parser->bytes_received = 1;
                parser->packet_size = 0;
            }
#if DEBUG_RAW_BYTES
            else {
                FURI_LOG_T("BruceRemote", "Skipping non-header byte 0x%02X", byte);
            }
#endif
        } else {
            // Receiving packet
            parser->buffer[parser->buffer_pos++] = byte;
            parser->bytes_received++;

            // Second byte is packet size
            if(parser->bytes_received == 2) {
                parser->packet_size = byte;

#if DEBUG_PARSER
                FURI_LOG_D("BruceRemote", "Packet size: %d bytes", parser->packet_size);
#endif

                // Sanity check
                if(parser->packet_size > (uint8_t)sizeof(parser->buffer)) {
                    FURI_LOG_E("BruceRemote", "Packet too large: %d (max %zu)",
                        parser->packet_size, sizeof(parser->buffer));
                    log_hex_data("Invalid packet", parser->buffer, parser->buffer_pos);
                    parser->in_packet = false;
                    parser->buffer_pos = 0;
                    continue;
                }

                if(parser->packet_size < 3) {
                    FURI_LOG_E("BruceRemote", "Packet too small: %d (min 3)", parser->packet_size);
                    parser->in_packet = false;
                    parser->buffer_pos = 0;
                    continue;
                }
            }

            // Complete packet?
            if(parser->packet_size > 0 && parser->bytes_received >= parser->packet_size) {
#if DEBUG_PARSER
                FURI_LOG_I("BruceRemote", "Complete packet received: %d bytes", parser->packet_size);
#endif
                // Parse complete packet
                parse_packet(parser, parser->buffer, parser->packet_size);

                // Reset for next packet
                parser->in_packet = false;
                parser->buffer_pos = 0;
                parser->bytes_received = 0;
#if DEBUG_PARSER
                FURI_LOG_D("BruceRemote", "Parser reset, looking for next header");
#endif
            }

            // Buffer overflow protection
            if(parser->buffer_pos >= sizeof(parser->buffer)) {
                FURI_LOG_E("BruceRemote", "Buffer overflow! pos=%zu max=%zu",
                    parser->buffer_pos, sizeof(parser->buffer));
                log_hex_data("Overflow buffer", parser->buffer, sizeof(parser->buffer));
                parser->in_packet = false;
                parser->buffer_pos = 0;
            }
        }
    }

#if DEBUG_PARSER
    if(parser->in_packet) {
        FURI_LOG_D("BruceRemote", "Partial packet: %d/%d bytes",
            parser->bytes_received, parser->packet_size);
    }
#endif
}

bool display_parser_get_command(DisplayParser* parser, DisplayCommand* command) {
    furi_assert(parser);
    furi_assert(command);

    return furi_message_queue_get(parser->display_buffer->command_queue, command, 0) ==
           FuriStatusOk;
}

void display_parser_render(DisplayParser* parser, Canvas* canvas) {
    furi_assert(parser);
    furi_assert(canvas);

    int bruce_w = parser->display_buffer->width;
    int bruce_h = parser->display_buffer->height;
    int flip_w = canvas_width(canvas);
    int flip_h = canvas_height(canvas);

    // Process all pending commands
    DisplayCommand cmd;
    uint32_t rendered_count = 0;

    while(display_parser_get_command(parser, &cmd)) {
        rendered_count++;

        // Scale coordinates
        int x = scale_coord(cmd.params[0], bruce_w, flip_w);
        int y = scale_coord(cmd.params[1], bruce_h, flip_h);

#if DEBUG_PACKET_DETAILS
        FURI_LOG_T("BruceRemote", "Render func=%d at (%d,%d)", cmd.func, x, y);
#endif

        switch(cmd.func) {
            case TFT_FILLSCREEN:
                canvas_set_color(canvas, rgb565_to_bw(cmd.params[0]));
                canvas_draw_box(canvas, 0, 0, flip_w, flip_h);
                break;

            case TFT_FILLRECT: {
                int w = scale_coord(cmd.params[2], bruce_w, flip_w);
                int h = scale_coord(cmd.params[3], bruce_h, flip_h);
                canvas_set_color(canvas, rgb565_to_bw(cmd.params[4]));
                canvas_draw_box(canvas, x, y, w, h);
                break;
            }

            case TFT_DRAWRECT: {
                int w = scale_coord(cmd.params[2], bruce_w, flip_w);
                int h = scale_coord(cmd.params[3], bruce_h, flip_h);
                canvas_set_color(canvas, rgb565_to_bw(cmd.params[4]));
                canvas_draw_frame(canvas, x, y, w, h);
                break;
            }

            case TFT_DRAWROUNDRECT: {
                // Flipper doesn't have native rounded rect, draw regular rect
                int w = scale_coord(cmd.params[2], bruce_w, flip_w);
                int h = scale_coord(cmd.params[3], bruce_h, flip_h);
                canvas_set_color(canvas, rgb565_to_bw(cmd.params[5]));
                canvas_draw_frame(canvas, x, y, w, h);
                break;
            }

            case TFT_FILLROUNDRECT: {
                // Flipper doesn't have native rounded rect, draw filled rect
                int w = scale_coord(cmd.params[2], bruce_w, flip_w);
                int h = scale_coord(cmd.params[3], bruce_h, flip_h);
                canvas_set_color(canvas, rgb565_to_bw(cmd.params[5]));
                canvas_draw_box(canvas, x, y, w, h);
                break;
            }

            case TFT_DRAWLINE: {
                int x1 = scale_coord(cmd.params[2], bruce_w, flip_w);
                int y1 = scale_coord(cmd.params[3], bruce_h, flip_h);
                canvas_set_color(canvas, rgb565_to_bw(cmd.params[4]));
                canvas_draw_line(canvas, x, y, x1, y1);
                break;
            }

            case TFT_DRAWCIRCLE: {
                int r = scale_coord(cmd.params[2], bruce_w, flip_w);
                canvas_set_color(canvas, rgb565_to_bw(cmd.params[3]));
                canvas_draw_circle(canvas, x, y, r);
                break;
            }

            case TFT_FILLCIRCLE: {
                int r = scale_coord(cmd.params[2], bruce_w, flip_w);
                canvas_set_color(canvas, rgb565_to_bw(cmd.params[3]));
                canvas_draw_disc(canvas, x, y, r);
                break;
            }

            case TFT_DRAWSTRING:
            case TFT_DRAWCENTRESTRING:
            case TFT_DRAWRIGHTSTRING:
                if(cmd.text) {
                    canvas_set_color(canvas, ColorBlack);

                    // Font scaling (approximate)
                    if(cmd.params[2] <= 2) {
                        canvas_set_font(canvas, FontSecondary);
                    } else {
                        canvas_set_font(canvas, FontPrimary);
                    }

                    // Alignment
                    if(cmd.func == TFT_DRAWCENTRESTRING) {
                        canvas_draw_str_aligned(canvas, x, y, AlignCenter, AlignTop, cmd.text);
                    } else if(cmd.func == TFT_DRAWRIGHTSTRING) {
                        canvas_draw_str_aligned(canvas, x, y, AlignRight, AlignTop, cmd.text);
                    } else {
                        canvas_draw_str(canvas, x, y, cmd.text);
                    }
                }
                break;

            case TFT_PRINT:
                // TFT_PRINT would need cursor position tracking
                // For now, just log it
                if(cmd.text) {
                    FURI_LOG_D("BruceRemote", "PRINT (not rendered): %s", cmd.text);
                }
                break;

            case TFT_DRAWPIXEL:
                canvas_set_color(canvas, rgb565_to_bw(cmd.params[2]));
                canvas_draw_dot(canvas, x, y);
                break;

            case TFT_DRAWFASTVLINE: {
                int len = scale_coord(cmd.params[2], bruce_h, flip_h);
                canvas_set_color(canvas, rgb565_to_bw(cmd.params[3]));
                canvas_draw_line(canvas, x, y, x, y + len);
                break;
            }

            case TFT_DRAWFASTHLINE: {
                int len = scale_coord(cmd.params[2], bruce_w, flip_w);
                canvas_set_color(canvas, rgb565_to_bw(cmd.params[3]));
                canvas_draw_line(canvas, x, y, x + len, y);
                break;
            }

            default:
                // Unsupported - already logged during parsing
#if DEBUG_PACKET_DETAILS
                FURI_LOG_W("BruceRemote", "Render: Unhandled func=%d", cmd.func);
#endif
                break;
        }

        // Free text if allocated
        if(cmd.text) {
            free(cmd.text);
        }
    }

#if DEBUG_PARSER
    if(rendered_count > 0) {
        FURI_LOG_D("BruceRemote", "Rendered %lu commands", rendered_count);
    }
#endif
}

void display_parser_clear(DisplayParser* parser) {
    furi_assert(parser);

    // Clear command queue
    DisplayCommand cmd;
    while(furi_message_queue_get(parser->display_buffer->command_queue, &cmd, 0) ==
          FuriStatusOk) {
        if(cmd.text) free(cmd.text);
    }

    parser->display_buffer->packets_received = 0;
    parser->display_buffer->packets_dropped = 0;
}

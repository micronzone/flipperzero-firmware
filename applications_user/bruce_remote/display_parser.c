#include "display_parser.h"
#include <furi.h>
#include <gui/canvas.h>

#define MAX_COMMAND_QUEUE 32

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
    if(size < 3) return;  // Minimum: header + size + func

    uint8_t func = packet[2];
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
                FURI_LOG_I("BruceRemote", "Screen: %dx%d rot=%d",
                    parser->display_buffer->width,
                    parser->display_buffer->height,
                    parser->display_buffer->rotation);
            }
            return;  // Don't queue this command

        case TFT_FILLSCREEN:
            // AA 05 00 CC CC
            if(size >= 5) {
                cmd.params[0] = parse_uint16(packet, 3);
                cmd.param_count = 1;
            }
            break;

        case TFT_DRAWRECT:
        case TFT_FILLRECT:
            // AA 0B 01/02 XX XX YY YY WW WW HH HH CC CC
            if(size >= 11) {
                cmd.params[0] = parse_uint16(packet, 3);   // x
                cmd.params[1] = parse_uint16(packet, 5);   // y
                cmd.params[2] = parse_uint16(packet, 7);   // w
                cmd.params[3] = parse_uint16(packet, 9);   // h
                cmd.params[4] = parse_uint16(packet, 11);  // color
                cmd.param_count = 5;
            }
            break;

        case TFT_DRAWROUNDRECT:
        case TFT_FILLROUNDRECT:
            // AA 0D 03/04 XX XX YY YY WW WW HH HH RR RR CC CC
            if(size >= 13) {
                cmd.params[0] = parse_uint16(packet, 3);   // x
                cmd.params[1] = parse_uint16(packet, 5);   // y
                cmd.params[2] = parse_uint16(packet, 7);   // w
                cmd.params[3] = parse_uint16(packet, 9);   // h
                cmd.params[4] = parse_uint16(packet, 11);  // r (radius)
                cmd.params[5] = parse_uint16(packet, 13);  // color
                cmd.param_count = 6;
            }
            break;

        case TFT_DRAWCIRCLE:
        case TFT_FILLCIRCLE:
            // AA 09 05/06 XX XX YY YY RR RR CC CC
            if(size >= 9) {
                cmd.params[0] = parse_uint16(packet, 3);  // x
                cmd.params[1] = parse_uint16(packet, 5);  // y
                cmd.params[2] = parse_uint16(packet, 7);  // r
                cmd.params[3] = parse_uint16(packet, 9);  // color
                cmd.param_count = 4;
            }
            break;

        case TFT_DRAWLINE:
            // AA 0B 0B XX XX YY YY X1 X1 Y1 Y1 CC CC
            if(size >= 11) {
                cmd.params[0] = parse_uint16(packet, 3);   // x0
                cmd.params[1] = parse_uint16(packet, 5);   // y0
                cmd.params[2] = parse_uint16(packet, 7);   // x1
                cmd.params[3] = parse_uint16(packet, 9);   // y1
                cmd.params[4] = parse_uint16(packet, 11);  // color
                cmd.param_count = 5;
            }
            break;

        case TFT_DRAWSTRING:
        case TFT_DRAWCENTRESTRING:
        case TFT_DRAWRIGHTSTRING:
            // AA SS 10/14/15 XX XX YY YY FF "text..."
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
                }
            }
            break;

        case TFT_PRINT:
            // AA SS 11 "text..."
            if(size > 3) {
                size_t text_len = size - 3;
                cmd.text = malloc(text_len + 1);
                memcpy(cmd.text, &packet[3], text_len);
                cmd.text[text_len] = '\0';
                cmd.param_count = 0;
            }
            break;

        case TFT_DRAWPIXEL:
            // AA 07 13 XX XX YY YY CC CC
            if(size >= 7) {
                cmd.params[0] = parse_uint16(packet, 3);  // x
                cmd.params[1] = parse_uint16(packet, 5);  // y
                cmd.params[2] = parse_uint16(packet, 7);  // color
                cmd.param_count = 3;
            }
            break;

        case TFT_DRAWFASTVLINE:
        case TFT_DRAWFASTHLINE:
            // AA 09 14/15 XX XX YY YY LL LL CC CC
            if(size >= 9) {
                cmd.params[0] = parse_uint16(packet, 3);  // x
                cmd.params[1] = parse_uint16(packet, 5);  // y
                cmd.params[2] = parse_uint16(packet, 7);  // length
                cmd.params[3] = parse_uint16(packet, 9);  // color
                cmd.param_count = 4;
            }
            break;

        default:
            // Unsupported function - skip
            FURI_LOG_D("BruceRemote", "Unsupported func: %d", func);
            return;
    }

    // Queue command
    if(furi_message_queue_put(parser->display_buffer->command_queue, &cmd, 0) != FuriStatusOk) {
        parser->display_buffer->packets_dropped++;
        if(cmd.text) free(cmd.text);
    } else {
        parser->display_buffer->packets_received++;
    }
}

void display_parser_parse(DisplayParser* parser, const uint8_t* data, size_t length) {
    furi_assert(parser);

    for(size_t i = 0; i < length; i++) {
        uint8_t byte = data[i];

        if(!parser->in_packet) {
            // Looking for packet header
            if(byte == TFT_LOG_HEADER) {
                parser->in_packet = true;
                parser->buffer[0] = byte;
                parser->buffer_pos = 1;
                parser->bytes_received = 1;
                parser->packet_size = 0;
            }
        } else {
            // Receiving packet
            parser->buffer[parser->buffer_pos++] = byte;
            parser->bytes_received++;

            // Second byte is packet size
            if(parser->bytes_received == 2) {
                parser->packet_size = byte;

                // Sanity check
                if(parser->packet_size > (uint8_t)sizeof(parser->buffer)) {
                    FURI_LOG_W("BruceRemote", "Packet too large: %d", parser->packet_size);
                    parser->in_packet = false;
                    parser->buffer_pos = 0;
                    continue;
                }
            }

            // Complete packet?
            if(parser->packet_size > 0 && parser->bytes_received >= parser->packet_size) {
                // Parse complete packet
                parse_packet(parser, parser->buffer, parser->packet_size);

                // Reset for next packet
                parser->in_packet = false;
                parser->buffer_pos = 0;
                parser->bytes_received = 0;
            }

            // Buffer overflow protection
            if(parser->buffer_pos >= sizeof(parser->buffer)) {
                FURI_LOG_E("BruceRemote", "Buffer overflow!");
                parser->in_packet = false;
                parser->buffer_pos = 0;
            }
        }
    }
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

    // Process all pending commands
    DisplayCommand cmd;
    while(display_parser_get_command(parser, &cmd)) {
        int bruce_w = parser->display_buffer->width;
        int bruce_h = parser->display_buffer->height;
        int flip_w = canvas_width(canvas);
        int flip_h = canvas_height(canvas);

        // Scale coordinates
        int x = scale_coord(cmd.params[0], bruce_w, flip_w);
        int y = scale_coord(cmd.params[1], bruce_h, flip_h);

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

            case TFT_DRAWPIXEL:
                canvas_set_color(canvas, rgb565_to_bw(cmd.params[2]));
                canvas_draw_dot(canvas, x, y);
                break;

            default:
                // Unsupported - already logged
                break;
        }

        // Free text if allocated
        if(cmd.text) {
            free(cmd.text);
        }
    }
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

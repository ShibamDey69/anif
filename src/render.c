#define _POSIX_C_SOURCE 200809L
#include "anif.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *g_render_buffer = NULL;
static size_t g_buffer_capacity = 0;
static char g_ascii_char[32] = DEFAULT_CHAR;
static bool g_use_luminance = false;
static bool g_use_half_block = false;

/* Standard ASCII density ramp ordered by increasing luminance */
static const char *LUMINANCE_RAMP = " .:-=+*#%@";
static const size_t LUMINANCE_RAMP_LEN = 10;

static inline char *append_uint8(char *p, uint8_t v) {
    if (v >= 100) {
        *p++ = (char)('0' + (v / 100));
        v %= 100;
        *p++ = (char)('0' + (v / 10));
        *p++ = (char)('0' + (v % 10));
    } else if (v >= 10) {
        *p++ = (char)('0' + (v / 10));
        *p++ = (char)('0' + (v % 10));
    } else {
        *p++ = (char)('0' + v);
    }
    return p;
}

int render_init(int render_w, int render_h, const char *ascii_char, bool use_luminance, bool use_half_block) {
    if (ascii_char && ascii_char[0] != '\0') {
        snprintf(g_ascii_char, sizeof(g_ascii_char), "%s", ascii_char);
    } else {
        snprintf(g_ascii_char, sizeof(g_ascii_char), DEFAULT_CHAR);
    }
    g_use_luminance = use_luminance;
    g_use_half_block = use_half_block;

    size_t required_cap = (size_t)render_w * (size_t)render_h * 48 + 16384;
    if (required_cap > g_buffer_capacity) {
        char *new_buf = (char *)realloc(g_render_buffer, required_cap);
        if (!new_buf) {
            return -1;
        }
        g_render_buffer = new_buf;
        g_buffer_capacity = required_cap;
    }

    return 0;
}

void render_free(void) {
    if (g_render_buffer) {
        free(g_render_buffer);
        g_render_buffer = NULL;
        g_buffer_capacity = 0;
    }
}

void render_frame(const uint8_t *rgb_data, int render_w, int render_h) {
    if (!g_render_buffer || !rgb_data) return;

    char *p = g_render_buffer;

    /* Cursor home: \x1b[H */
    *p++ = '\x1b';
    *p++ = '[';
    *p++ = 'H';

    if (g_use_half_block) {
        /* Half-block TrueColor mode: 2 vertical pixels per terminal cell */
        int last_fg_r = -1, last_fg_g = -1, last_fg_b = -1;
        int last_bg_r = -1, last_bg_g = -1, last_bg_b = -1;

        size_t row_stride = (size_t)render_w * 3;

        for (int y = 0; y < render_h; y += 2) {
            const uint8_t *top_row = rgb_data + (size_t)y * row_stride;
            const uint8_t *bot_row = (y + 1 < render_h) ? (top_row + row_stride) : NULL;

            for (int x = 0; x < render_w; x++) {
                uint8_t fg_r = *top_row++;
                uint8_t fg_g = *top_row++;
                uint8_t fg_b = *top_row++;

                uint8_t bg_r = bot_row ? *bot_row++ : 0;
                uint8_t bg_g = bot_row ? *bot_row++ : 0;
                uint8_t bg_b = bot_row ? *bot_row++ : 0;

                /* Set Foreground color (top half) */
                if (fg_r != last_fg_r || fg_g != last_fg_g || fg_b != last_fg_b) {
                    *p++ = '\x1b'; *p++ = '['; *p++ = '3'; *p++ = '8'; *p++ = ';'; *p++ = '2'; *p++ = ';';
                    p = append_uint8(p, fg_r); *p++ = ';';
                    p = append_uint8(p, fg_g); *p++ = ';';
                    p = append_uint8(p, fg_b); *p++ = 'm';
                    last_fg_r = fg_r; last_fg_g = fg_g; last_fg_b = fg_b;
                }

                /* Set Background color (bottom half) */
                if (bg_r != last_bg_r || bg_g != last_bg_g || bg_b != last_bg_b) {
                    *p++ = '\x1b'; *p++ = '['; *p++ = '4'; *p++ = '8'; *p++ = ';'; *p++ = '2'; *p++ = ';';
                    p = append_uint8(p, bg_r); *p++ = ';';
                    p = append_uint8(p, bg_g); *p++ = ';';
                    p = append_uint8(p, bg_b); *p++ = 'm';
                    last_bg_r = bg_r; last_bg_g = bg_g; last_bg_b = bg_b;
                }

                /* UTF-8 Upper Half Block: U+2580 (\xe2\x96\x80) */
                *p++ = '\xe2';
                *p++ = '\x96';
                *p++ = '\x80';
            }

            /* End of line: reset color and add carriage return + newline if not final line */
            *p++ = '\x1b'; *p++ = '['; *p++ = '0'; *p++ = 'm';
            if (y + 2 < render_h) {
                *p++ = '\r';
                *p++ = '\n';
            }
            last_fg_r = last_fg_g = last_fg_b = -1;
            last_bg_r = last_bg_g = last_bg_b = -1;
        }
    } else {
        /* Standard Character or Luminance Ramp mode */
        int last_r = -1, last_g = -1, last_b = -1;
        size_t ascii_char_len = strlen(g_ascii_char);
        const uint8_t *src = rgb_data;

        for (int y = 0; y < render_h; y++) {
            for (int x = 0; x < render_w; x++) {
                uint8_t r = *src++;
                uint8_t g = *src++;
                uint8_t b = *src++;

                if (r != last_r || g != last_g || b != last_b) {
                    *p++ = '\x1b'; *p++ = '['; *p++ = '3'; *p++ = '8'; *p++ = ';'; *p++ = '2'; *p++ = ';';
                    p = append_uint8(p, r); *p++ = ';';
                    p = append_uint8(p, g); *p++ = ';';
                    p = append_uint8(p, b); *p++ = 'm';
                    last_r = r; last_g = g; last_b = b;
                }

                if (g_use_luminance) {
                    uint32_t lum = (77U * r + 150U * g + 29U * b) >> 8;
                    size_t char_idx = (lum * (LUMINANCE_RAMP_LEN - 1)) / 255;
                    *p++ = LUMINANCE_RAMP[char_idx];
                } else {
                    for (size_t i = 0; i < ascii_char_len; i++) {
                        *p++ = g_ascii_char[i];
                    }
                }
            }

            /* End of line: reset color and add carriage return + newline if not final line */
            *p++ = '\x1b'; *p++ = '['; *p++ = '0'; *p++ = 'm';
            if (y + 1 < render_h) {
                *p++ = '\r';
                *p++ = '\n';
            }
            last_r = last_g = last_b = -1;
        }
    }

    size_t total_len = (size_t)(p - g_render_buffer);
    ssize_t written = 0;
    while ((size_t)written < total_len) {
        ssize_t n = write(STDOUT_FILENO, g_render_buffer + written, total_len - written);
        if (n <= 0) break;
        written += n;
    }
}

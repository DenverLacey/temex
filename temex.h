#ifndef _TEMEX_H_
#define _TEMEX_H_

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

// +==============================================================================================+
// | Type Declarations                                                                            |
// +==============================================================================================+

/// 3-Dimensional Vector. Z-Component used for layering
typedef struct TxVector {
    float x, y, z;
} TxVector;

/// Rectangle defined by position and size
typedef struct TxRectangle {
    TxVector pos, size;
} TxRectangle;

/// Log levels that can be used to configure logging of Temex
typedef enum TxLogLevel {
    TxLogLevel_ALL,
    TxLogLevel_DEBUG,
    TxLogLevel_INFO,
    TxLogLevel_ERROR,
    TxLogLevel_NONE,
    TxLogLevel_COUNT = TxLogLevel_NONE,
} TxLogLevel;

/// Key code enum
typedef enum TxKeyCode {
    TxKeyCode_ESC,
    TxKeyCode_COUNT,
} TxKeyCode;

typedef uint8_t TxKeyState;
#define TxKeyState_PRESSED  0x01
#define TxKeyState_HELD     0x02
#define TxKeyState_RELEASED 0x04

// +==============================================================================================+
// | Functions Declarations                                                                       |
// +==============================================================================================+

/// Prepare terminal to act like a graphical window
void tx_prepare_terminal(void);

/// Restore terminal to default state
void tx_restore_terminal(void);

/// Get the cached width of the terminal
uint16_t tx_get_screen_width(void);

/// Get the cached height of the terminal
uint16_t tx_get_screen_height(void);

/// Poll events/inputs
void tx_poll_events(void);

/// Get the current state of a particular key
TxKeyState tx_get_key_state(TxKeyCode key);

/// Test if a given key has been pressed
bool tx_is_key_pressed(TxKeyCode key);

/// Test if a given key is being held down
bool tx_is_key_held(TxKeyCode key);

/// Test if a given key has been released
bool tx_is_key_released(TxKeyCode key);

/// Renders screen to the terminal
void tx_render_to_terminal(void);

/// Clear the terminal screen
void tx_clear_screen(void);

/// Draw the outline of a rectangle
void tx_draw_rec(TxRectangle rec);

/// Draw a filled in rectangle
void tx_fill_rec(TxRectangle rec);

/// Set the minimum log level to log
void tx_set_log_level(TxLogLevel lv);

void tx_log(TxLogLevel lv, const char *restrict fmt, ...);
#define tx_dbg(...)   tx_log(TxLogLevel_DEBUG, __VA_ARGS__);
#define tx_info(...)  tx_log(TxLogLevel_INFO, __VA_ARGS__);
#define tx_error(...) tx_log(TxLogLevel_ERROR, __VA_ARGS__);

/// Encodes a character to UTF-8 into a buffer
bool tx_to_utf8(uint32_t c, char buf[static 5]);

TxVector TxVector_add(TxVector a, TxVector b);

#endif // _TEMEX_H_

// +==============================================================================================+
// | Temex Implementation                                                                         |
// +==============================================================================================+

#ifdef TX_IMPLEMENTATION
#ifndef _TEMEX_H_IMPLEMENTATION_
#define _TEMEX_H_IMPLEMENTATION_

#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

// +==============================================================================================+
// | Forward Declarations                                                                         |
// +==============================================================================================+

static bool     tx_get_screen_size_(uint16_t *x, uint16_t *y);
static TxVector tx_round_pos_(TxVector p);
static int      tx_pos_to_idx_(TxVector p);
static void     tx_set_cell_(uint32_t c, TxVector p);
static int      tx_codepoint_length_(uint32_t c);

struct TxState_ {
    TxLogLevel log_level;
    uint16_t   screen_width, screen_height;
    uint32_t * screen;
    float *    depth_buffer;
    TxKeyState keys[TxKeyCode_COUNT];
} tx_state_;

void tx_prepare_terminal(void) {
    if (!tx_get_screen_size_(&tx_state_.screen_width, &tx_state_.screen_height)) {
        return;
    }

    tx_state_.screen = calloc(tx_state_.screen_width * tx_state_.screen_height, sizeof(*tx_state_.screen));
    if (!tx_state_.screen) {
        tx_error("Failed to allocate screen");
        return;
    }

    tx_state_.depth_buffer = calloc(tx_state_.screen_width * tx_state_.screen_height, sizeof(*tx_state_.depth_buffer));
    if (!tx_state_.depth_buffer) {
        tx_error("Failed to allocate depth buffer");
        return;
    }

    // TODO: Enable raw mode for terminal and shift to alt-screen
}

void tx_restore_terminal(void) {
    free(tx_state_.screen);
    free(tx_state_.depth_buffer);
}

uint16_t tx_get_screen_width(void) {
    return tx_state_.screen_width;
}

uint16_t tx_get_screen_height(void) {
    return tx_state_.screen_height;
}

void tx_poll_events(void) {
}

bool tx_is_key_pressed(TxKeyCode key) {
    return tx_state_.keys[key] & TxKeyState_PRESSED;
}

bool tx_is_key_held(TxKeyCode key) {
    return tx_state_.keys[key] & TxKeyState_HELD;
}

bool tx_is_key_released(TxKeyCode key) {
    return tx_state_.keys[key] & TxKeyState_RELEASED;
}

void tx_render_to_terminal(void) {
    char character_buffer[5] = {0};
    for (int y = 0; y < tx_state_.screen_height; y++) {
        for (int x = 0; x < tx_state_.screen_width; x++) {
            uint32_t c = tx_state_.screen[x + y * tx_state_.screen_width];
            if (c == 0) {
                printf(" ");
                continue;
            }

            if (!tx_to_utf8(c, character_buffer)) {
                tx_error("Failed to encode character to UTF-8: 0x%X", c);
                tx_clear_screen();
                return;
            }

            printf("%s", character_buffer);
        }

        printf("\r\n");
    }
}

void tx_clear_screen(void) {
    memset(tx_state_.screen, 0, (tx_state_.screen_width * tx_state_.screen_height) * sizeof(*tx_state_.screen));
}

void tx_draw_rec(TxRectangle rec) {
    static const uint32_t palette[] = {
        0x2500, // ─ - Horizontal
        0x2502, // │ - Vertical
        0x250C, // ┌ - Top Left
        0x2510, // ┐ - Top Right
        0x2514, // └ - Bottom Left
        0x2518  // ┘ - Bottom Right
    };

    TxVector min = tx_round_pos_(rec.pos);
    TxVector max = tx_round_pos_(TxVector_add(rec.pos, (TxVector){.x=rec.size.x, .y=rec.size.y}));

    // corners
    tx_set_cell_(palette[2], min);
    tx_set_cell_(palette[3], (TxVector){.x=max.x, .y=min.y, .z=min.z});
    tx_set_cell_(palette[4], (TxVector){.x=min.x, .y=max.y, .z=min.z});
    tx_set_cell_(palette[5], max);

    // left edge
    for (float y = min.y + 1.f; y < max.y; y += 1.f) {
        tx_set_cell_(palette[1], (TxVector){.x=min.x, .y=y, .z=min.z});
    }

    // top edge
    for (float x = min.x + 1.f; x < max.x; x += 1.f) {
        tx_set_cell_(palette[0], (TxVector){.x=x, .y=min.y, .z=min.z});
    }

    // right edge
    for (float y = min.y + 1.f; y < max.y; y += 1.f) {
        tx_set_cell_(palette[1], (TxVector){.x=max.x, .y=y, .z=min.z});
    }

    // bottom edge
    for (float x = min.x + 1.f; x < max.x; x += 1.f) {
        tx_set_cell_(palette[0], (TxVector){.x=x, .y=max.y, .z=min.z});
    }
}

void tx_fill_rec(TxRectangle rec) {
    static const uint32_t palette[] = {
        0x2584, // ▄ - Top Horizontal
        0x2580, // ▀ - Bottom Horizontal
        0x258C, // ▌ - Right Vertical
        0x2590, // ▐ - Left Vertical
        0x2588, // █ - Fill
        0x2597, // ▗ - Top Left Corner
        0x2596, // ▖ - Top Right Corner
        0x259D, // ▝ - Bottom Left Corner
        0x2598, // ▘ - Bottom Right Corner
    };

    TxVector min = tx_round_pos_(rec.pos);
    TxVector max = tx_round_pos_(TxVector_add(rec.pos, (TxVector){.x=rec.size.x, .y=rec.size.y}));

    // corners
    tx_set_cell_(palette[5], min);
    tx_set_cell_(palette[6], (TxVector){.x=max.x, .y=min.y, .z=min.z});
    tx_set_cell_(palette[7], (TxVector){.x=min.x, .y=max.y, .z=min.z});
    tx_set_cell_(palette[8], max);

    // left edge
    for (float y = min.y + 1.f; y < max.y; y += 1.f) {
        tx_set_cell_(palette[3], (TxVector){.x=min.x, .y=y, .z=min.z});
    }

    // top edge
    for (float x = min.x + 1.f; x < max.x; x += 1.f) {
        tx_set_cell_(palette[0], (TxVector){.x=x, .y=min.y, .z=min.z});
    }

    // right edge
    for (float y = min.y + 1.f; y < max.y; y += 1.f) {
        tx_set_cell_(palette[2], (TxVector){.x=max.x, .y=y, .z=min.z});
    }


    // bottom edge
    for (float x = min.x + 1.f; x < max.x; x += 1.f) {
        tx_set_cell_(palette[1], (TxVector){.x=x, .y=max.y, .z=min.z});
    }

    // fill
    for (float y = min.y + 1.f; y <= max.y - 1.f; y += 1.f) {
        for (float x = min.x + 1.f; x <= max.x - 1.f; x += 1.f) {
            tx_set_cell_(palette[4], (TxVector){.x=x, .y=y, .z=min.z});
        }
    }
}

void tx_set_log_level(TxLogLevel lv) {
    tx_state_.log_level = lv;
}

void tx_log(TxLogLevel lv, const char *restrict fmt, ...) {
    static const char *level_labels[TxLogLevel_COUNT] = {
        [TxLogLevel_ALL]   = "LOG",
        [TxLogLevel_DEBUG] = "DEBUG",
        [TxLogLevel_INFO]  = "INFO",
        [TxLogLevel_ERROR] = "ERROR",
    };

    if (lv >= TxLogLevel_NONE) return;

    va_list args;
    va_start(args, fmt);
    {
        printf("%s: ", level_labels[lv]);
        vprintf(fmt, args);
        printf("\n");
    }
    va_end(args);
}

bool tx_to_utf8(uint32_t c, char buf[static 5]) {
    memset(buf, 0, 5);

    int clen = tx_codepoint_length_(c);
    if (clen == 0) {
        return false;
    }

    switch (clen) {
        case 1:
            buf[0] = c & 0x7F;
            break;
        case 2:
            buf[0] = 0xC0 | ((c >> 6) & 0x1F);
            buf[1] = 0x80 | (c & 0x3F);
            break;
        case 3:
            buf[0] = 0xE0 | ((c >> 12) & 0x0F);
            buf[1] = 0x80 | ((c >> 6) & 0x3F);
            buf[2] = 0x80 | (c & 0x3F);
            break;
        case 4:
            buf[0] = 0xF0 | ((c >> 18) & 0x07);
            buf[1] = 0x80 | ((c >> 12) & 0x3F);
            buf[2] = 0x80 | ((c >> 6) & 0x3F);
            buf[3] = 0x80 | (c & 0x3F);
            break;
    }

    return true;
}

TxVector TxVector_add(TxVector a, TxVector b) {
    return (TxVector) {
        .x = a.x + b.x,
        .y = a.y + b.y,
    };
}

static bool tx_get_screen_size_(uint16_t *w, uint16_t *h) {
    int fd = open("/dev/tty", O_RDWR);
    if (fd < 0) {
        tx_error("Failed to open terminal file");
        return false;
    }

    struct winsize ws;
    int r = ioctl(fd, TIOCGWINSZ, &ws);
    if (r < 0) {
        tx_error("Failed to read size of terminal");
        close(fd);
        return false;
    }

    *w = ws.ws_col;
    *h = ws.ws_row;

    close(fd);
    return true;
}

static TxVector tx_round_pos_(TxVector p) {
    return (TxVector) {
        roundf(p.x),
        roundf(p.y),
        p.z,
    };
}

static int tx_pos_to_idx_(TxVector p) {
    int x = (int)roundf(p.x);
    int y = (int)roundf(p.y);
    return x + y * tx_state_.screen_width;
}

static void tx_set_cell_(uint32_t c, TxVector p) {
    int cell = tx_pos_to_idx_(p);
    if (tx_state_.depth_buffer[cell] > p.z) return;
    tx_state_.screen[cell] = c;
}

static int tx_codepoint_length_(uint32_t c) {
    if (c <= 0x7F) {
        return 1;
    } else if (c <= 0x7FF) {
        return 2;
    } else if (c <= 0xFFFF) {
        return 3;
    } else if (c <= 0x10FFFF) {
        return 4;
    } else {
        return 0;
    }
}

#endif // _TEMEX_H_IMPLEMENTATION_
#endif // TX_IMPLEMENTATION


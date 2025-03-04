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
    TxKeyCode_BACKSPACE = 8,
    TxKeyCode_ESC       = 27,
    TxKeyCode_DELETE    = 127,
    TxKeyCode_COUNT     = 256,
} TxKeyCode;

typedef uint8_t TxKeyState;
#define TxKeyState_PRESSED  0x01
#define TxKeyState_HELD     0x02
#define TxKeyState_RELEASED 0x04

// +==============================================================================================+
// | Functions Declarations                                                                       |
// +==============================================================================================+

/// Prepare terminal to act like a graphical window
bool tx_prepare_terminal(void);

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

/// Iterate over all keys pressed this frame
bool tx_pressed_keys(uint32_t *c);

/// Renders screen to the terminal
void tx_render_to_terminal(void);

/// Clear the terminal screen
void tx_clear_screen(void);

/// Draw the outline of a rectangle
void tx_draw_rec(TxRectangle rec);

/// Draw a filled in rectangle
void tx_fill_rec(TxRectangle rec);

/// Draw some text at a position
void tx_draw_text(const char *text, TxVector pos);

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
#include <termios.h>
#include <unistd.h>

// +==============================================================================================+
// | Forward Declarations                                                                         |
// +==============================================================================================+

static bool     tx_enable_raw_mode_(void);
static void     tx_disable_raw_mode_(void);
static void     tx_enter_alt_screen_(void);
static void     tx_exit_alt_screen_(void);
static void     tx_hide_cursor_(void);
static void     tx_show_cursor_(void);
static bool     tx_get_screen_size_(uint16_t *x, uint16_t *y);
static TxVector tx_round_pos_(TxVector p);
static int      tx_pos_to_idx_(TxVector p);
static void     tx_set_cell_(uint32_t c, TxVector p);
static void     tx_set_cell_by_idx_(uint32_t c, int idx);
static int      tx_codepoint_length_(uint32_t c);
static void     tx_move_cursor_to_origin_(void);

struct TxState_ {
    TxLogLevel log_level;
    uint16_t   screen_width, screen_height;
    uint32_t * screen;
    float *    depth_buffer;
    struct termios default_termios;
    TxKeyState keys[TxKeyCode_COUNT];
} TX_;

bool tx_prepare_terminal(void) {
    // Get screen information
    if (!tx_get_screen_size_(&TX_.screen_width, &TX_.screen_height)) {
        return false;
    }

    // Allocate buffers
    TX_.screen = calloc(TX_.screen_width * TX_.screen_height, sizeof(*TX_.screen));
    if (!TX_.screen) {
        tx_error("Failed to allocate screen");
        return false;
    }

    TX_.depth_buffer = calloc(TX_.screen_width * TX_.screen_height, sizeof(*TX_.depth_buffer));
    if (!TX_.depth_buffer) {
        tx_error("Failed to allocate depth buffer");
        return false;
    }

    if (!tx_enable_raw_mode_()) {
        return false;
    }

    tx_hide_cursor_();

    return true;
}

void tx_restore_terminal(void) {
    free(TX_.screen);
    free(TX_.depth_buffer);
}

uint16_t tx_get_screen_width(void) {
    return TX_.screen_width;
}

uint16_t tx_get_screen_height(void) {
    return TX_.screen_height;
}

void tx_poll_events(void) {
    bool keys[256] = {0};

    char c = 0;
    while (read(STDIN_FILENO, &c, 1) == 1) {
        keys[(int)c] = true;
    }

    for (int i = 0; i < 256; i++) {
        if (keys[i] == false) {
            if (TX_.keys[i] & TxKeyState_HELD) TX_.keys[i] = TxKeyState_RELEASED;
            else                               TX_.keys[i] = 0;
        } else {
            if (TX_.keys[i] & TxKeyState_PRESSED)   TX_.keys[i] = TxKeyState_HELD;
            else if (TX_.keys[i] & TxKeyState_HELD) TX_.keys[i] = TxKeyState_HELD;
            else                                    TX_.keys[i] = TxKeyState_PRESSED;
        }
    }
}

bool tx_is_key_pressed(TxKeyCode key) {
    return TX_.keys[key] & TxKeyState_PRESSED;
}

bool tx_is_key_held(TxKeyCode key) {
    return TX_.keys[key] & TxKeyState_HELD;
}

bool tx_is_key_released(TxKeyCode key) {
    return TX_.keys[key] & TxKeyState_RELEASED;
}

bool tx_pressed_keys(uint32_t *c) {
    (*c)++;
    for (; *c < 256; (*c)++) {
        if (TX_.keys[*c] & TxKeyState_PRESSED)
            return true;
    }
    return false;
}

void tx_render_to_terminal(void) {
    tx_move_cursor_to_origin_();

    char cbuf[5] = {0};
    for (int y = 0; y < TX_.screen_height; y++) {
        for (int x = 0; x < TX_.screen_width; x++) {
            uint32_t c = TX_.screen[x + y * TX_.screen_width];
            if (c == 0) {
                printf(" ");
                continue;
            }

            if (!tx_to_utf8(c, cbuf)) {
                tx_error("Failed to encode character to UTF-8: 0x%X", c);
                tx_clear_screen();
                return;
            }

            printf("%s", cbuf);
        }

        printf("\r\n");
    }
}

void tx_clear_screen(void) {
    memset(TX_.screen, 0, (TX_.screen_width * TX_.screen_height) * sizeof(*TX_.screen));
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

void tx_draw_text(const char *text, TxVector pos) {
    int text_len = strlen(text);
    int idx = tx_pos_to_idx_(pos);
    for (int i = 0; i < text_len; i++) {
        if (idx >= TX_.screen_width * TX_.screen_height) {
            break;
        }
        tx_set_cell_by_idx_(text[i], idx);
        idx++;
    }
}

void tx_set_log_level(TxLogLevel lv) {
    TX_.log_level = lv;
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
        fprintf(stderr, "%s: ", level_labels[lv]);
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
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

static bool tx_enable_raw_mode_(void) {
    if (tcgetattr(STDIN_FILENO, &TX_.default_termios) == -1) {
        tx_error("Failed to save default state of terminal");
        return false;
    }

    atexit(tx_disable_raw_mode_);

    struct termios raw = TX_.default_termios;
    raw.c_iflag     &= ~(BRKINT | ICRNL | INPCK | IXON);
    raw.c_oflag     &= ~(OPOST);
    raw.c_cflag     |= (CS8);
    raw.c_lflag     &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        tx_error("Failed to enable raw mode");
        return false;
    }

    tx_enter_alt_screen_();

    return true;
}

static void tx_disable_raw_mode_(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &TX_.default_termios);
    tx_exit_alt_screen_();
    tx_show_cursor_();
}

static void tx_enter_alt_screen_(void) {
    printf("\x1b[?1049h");
}

static void tx_exit_alt_screen_(void) {
    printf("\x1b[?1049l");
}

static void tx_hide_cursor_(void) {
    printf("\033[?25l");
}

static void tx_show_cursor_(void) {
    printf("\033[?25h");
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
    *h = ws.ws_row - 1;

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
    return x + y * TX_.screen_width;
}

static void tx_set_cell_(uint32_t c, TxVector p) {
    int idx = tx_pos_to_idx_(p);
    if (TX_.depth_buffer[idx] > p.z) return;
    tx_set_cell_by_idx_(c, idx);
}

static void tx_set_cell_by_idx_(uint32_t c, int idx) {
    TX_.screen[idx] = c;
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

static void tx_move_cursor_to_origin_(void) {
    printf("\x1b[H");
}

#endif // _TEMEX_H_IMPLEMENTATION_
#endif // TX_IMPLEMENTATION


#include <ctype.h>
#define TX_IMPLEMENTATION
#include <temex.h>

int main(void) {
    tx_prepare_terminal();

    #define TEXT_CAP 101
    char text[TEXT_CAP] = {0};
    int carrot = -1;

    TxRectangle text_box = {
        .pos = {.x = 10, .y = (float)(tx_get_screen_height() / 2 - 2)},
        .size = {.x = TEXT_CAP + 1, .y = 2},
    };

    for (;;) {
        tx_poll_events();
        if (tx_is_key_pressed(TxKeyCode_ESC)) {
            break;
        }

        if (tx_is_key_pressed(TxKeyCode_BACKSPACE) || tx_is_key_pressed(TxKeyCode_DELETE)) {
            if (carrot >= 0)
                text[carrot--] = 0;
        }

        uint32_t c = 0;
        while (tx_pressed_keys(&c)) {
            if (!iscntrl(c)) {
                if (carrot < TEXT_CAP - 2) {
                    text[++carrot] = (char)c;
                }
            }
        }

        tx_clear_screen();
        tx_draw_rec(text_box);
        tx_draw_text(text, TxVector_add(text_box.pos, (TxVector){.x=1, .y=1}));

        tx_render_to_terminal();
    }

    tx_restore_terminal();
    return 0;
}


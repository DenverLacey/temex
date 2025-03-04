#include <stdbool.h>

#define TX_IMPLEMENTATION
#include <temex.h>

int main(void) {
    #ifdef DEBUG
        tx_set_log_level(TxLogLevel_ALL);
    #else
        tx_set_log_level(TxLogLevel_ERROR);
    #endif

    tx_prepare_terminal();

    for (;;) {
        tx_poll_events();
        char buf[5] = {0};
        read(STDIN_FILENO, buf, 5);
        if (strcmp(buf, "q") == 0 || tx_is_key_pressed(TxKeyCode_ESC)) {
            break;
        }

        tx_clear_screen();
        tx_draw_rec((TxRectangle){.pos=(TxVector){.x=1, .y=1}, .size=(TxVector){.x=10, .y=5}});
        tx_fill_rec((TxRectangle){.pos=(TxVector){.x=13, .y=1}, .size=(TxVector){.x=10, .y=5}});
        tx_fill_rec((TxRectangle){.pos=(TxVector){.x=1, .y=7}, .size=(TxVector){.x=10, .y=5}});
        tx_draw_rec((TxRectangle){.pos=(TxVector){.x=13, .y=7}, .size=(TxVector){.x=10, .y=5}});

        tx_render_to_terminal();
    }

    tx_restore_terminal();
    return 0;
}


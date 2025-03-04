#define TX_IMPLEMENTATION
#include <temex.h>

uint32_t get_player_char_for_dir(TxVector dir) {
    static uint32_t arrows[] = {
        0x25B3, // △ - Up Arrow
        0x25B7, // ▷ - Right Arrow
        0x25BD, // ▽ - Down Arrow
        0x25C1, // ◁ - Left Arrow
    };

    if (dir.x ==  0.f && dir.y == -1.f) return arrows[0];
    if (dir.x ==  1.f && dir.y ==  0.f) return arrows[1];
    if (dir.x ==  0.f && dir.y ==  1.f) return arrows[2];
    if (dir.x == -1.f && dir.y ==  0.f) return arrows[3];

    return 0;
}

int main(void) {
    tx_prepare_terminal();

    TxVector pos = {tx_get_screen_width() / 2, tx_get_screen_height() / 2, 0};
    TxVector dir = {0, -1, 0};
    uint32_t p_char = get_player_char_for_dir(dir);

    for (;;) {
        tx_poll_events();
        if (tx_is_key_pressed(TxKeyCode_ESC)) {
            break;
        }

        dir = (TxVector){0};
        if (tx_is_key_pressed('w')) dir.y -= 1.f;
        if (tx_is_key_pressed('a')) dir.x -= 1.f;
        if (tx_is_key_pressed('s')) dir.y += 1.f;
        if (tx_is_key_pressed('d')) dir.x += 1.f;

        uint32_t pc = get_player_char_for_dir(dir);
        if (pc != 0) p_char = pc;

        TxVector np = TxVector_add(pos, TxVector_mul(dir, (TxVector){2.f, 1.f, 1.f}));
        if (np.x >= 0 && np.x <= tx_get_screen_width() &&
            np.y >= 0 && np.y <= tx_get_screen_height())
        {
            pos = np;
        }

        tx_clear_screen();

        tx_draw_char(p_char, pos);

        tx_render_to_terminal();

        usleep(1000);
    }

    tx_restore_terminal();
    return 0;
}


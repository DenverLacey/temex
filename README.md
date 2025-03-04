# Temex (Terminal Graphics)

Temex is a header-only (stb style) library that lets you make interactive "graphical" applications in the terminal.

The library uses unicode characters to create various shapes using only characters. So everything is ~~ASCII~~ Unicode art.

# Getting Started

To get started, all you have to do is add the file `temex.h` to your project and include it.  
**NOTE**: You must define `TX_IMPLEMENTATION` exactly **once** in your whole project. Typically in your main file is good.

```c
#define TX_IMPLEMENTATION
#include <temex.h>

int main(void) {
    tx_prepare_terminal();

    for (;;) {
        tx_poll_events();
        if (tx_is_key_pressed(TxKeyCode_ESC)) {
            break;
        }
    }

    tx_restore_terminal();
    return 0;
}
```

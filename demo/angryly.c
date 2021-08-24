#include <stdio.h>
#include <termbox.h>

#define IN_TXTSIZE 10

typedef struct tb_event event_t;

// test code, delete later
static void draw_input(int x, int y) {
        tb_change_cell(x -1, y - 1, 0x250c, TB_CYAN, TB_DEFAULT);
        tb_change_cell(x -1, y, '|', TB_CYAN, TB_DEFAULT);
        tb_change_cell(x -1, y + 1, 0x2514, TB_CYAN, TB_DEFAULT);
    for(int i = 0; i < IN_TXTSIZE; i++) {
        tb_change_cell(x + i, y - 1, 0x2500, TB_CYAN, TB_DEFAULT);
        tb_change_cell(x + i, y + 1, 0x2500, TB_CYAN, TB_DEFAULT);
        tb_change_cell(x + i, y, 'o', TB_DEFAULT, TB_BLUE);
    }
        tb_change_cell(x + IN_TXTSIZE, y - 1, 0x2510, TB_CYAN, TB_DEFAULT);
        tb_change_cell(x + IN_TXTSIZE, y, '|', TB_CYAN, TB_DEFAULT);
        tb_change_cell(x + IN_TXTSIZE, y + 1, 0x2518, TB_CYAN, TB_DEFAULT);
    tb_present();
}

static int angryly(void) {
    int quit = 0;
    event_t ev;
    int term_width, term_height;

    tb_init();

    term_width = tb_width();
    term_height = tb_height();

    draw_input(term_width/2, term_height/2);
    while(!quit) {
        tb_poll_event(&ev);

        if(ev.key == TB_KEY_CTRL_C)
            quit = 1;
    }

    tb_shutdown();

    return 0;
}
#include <finsh.h>
MSH_CMD_EXPORT(angryly, angryly)

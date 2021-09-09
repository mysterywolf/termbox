#include <string>
#include <stdint.h>
#include <sys/time.h>
#include "../termbox.h"

struct tbclock_t {
    // parameters
    uint16_t bg;
    uint16_t fg;
};

static const int font_width = 3;
static const int font_height = 5;

static const rt_bool_t font[][15] = {
    {1,1,1,
     1,0,1,
     1,0,1,
     1,0,1,
     1,1,1}, // 0
    {0,0,1,
     0,0,1,
     0,0,1,
     0,0,1,
     0,0,1}, // 1
    {1,1,1,
     0,0,1,
     1,1,1,
     1,0,0,
     1,1,1}, // 2
    {1,1,1,
     0,0,1,
     1,1,1,
     0,0,1,
     1,1,1}, // 3
    {1,0,1,
     1,0,1,
     1,1,1,
     0,0,1,
     0,0,1}, // 4
    {1,1,1,
     1,0,0,
     1,1,1,
     0,0,1,
     1,1,1}, // 5
    {1,1,1,
     1,0,0,
     1,1,1,
     1,0,1,
     1,1,1}, // 6
    {1,1,1,
     0,0,1,
     0,0,1,
     0,0,1,
     0,0,1}, // 7
    {1,1,1,
     1,0,1,
     1,1,1,
     1,0,1,
     1,1,1}, // 8
    {1,1,1,
     1,0,1,
     1,1,1,
     0,0,1,
     1,1,1}, // 9
    {0,0,0,
     0,1,0,
     0,0,0,
     0,1,0,
     0,0,0}, // ':'
};


struct pixelfield_t {
    int width;
    int height;
    int pixel_width;
    int pixel_height;

    pixelfield_t(int w, int h, int pw, int ph):
        width(0), height(0), pixel_width(pw), pixel_height(ph)
    {
        if (!pixel_width)
            pixel_width = 1;
        if (!pixel_height)
            pixel_height = 1;
        width = w / pixel_width;
        height = h / pixel_height;
    }

    void plot_pixel(int x, int y, uint16_t color)
    {
        for (int ox = 0; ox < pixel_width; ++ox) {
            for (int oy = 0; oy < pixel_height; ++oy) {
                int rx = x * pixel_width + ox;
                int ry = y * pixel_height + oy;
                tb_change_cell(rx, ry, ' ', TB_BLACK, color);
            }
        }
    }
};

static void draw_digit(pixelfield_t &pf, int x, int y,
               uint16_t color, int digit)
{
    for (int ox = 0; ox < font_width; ++ox) {
        for (int oy = 0; oy < font_height; ++oy) {
            if (font[digit][oy * font_width + ox]) {
                pf.plot_pixel(ox+x, oy+y, color);
            }
        }
    }
}

static void get_clock_dimensions(int &width, int &height, bool seconds=true)
{
    int w = 1 + font_width + 1 + 3 * font_width + 1 + font_width;
    if (seconds)
        w += 2 * font_width + 1 + font_width + 1;
    else
        w += 1;
    width = w;
    height = font_height + 2;
}

static void draw_clock(pixelfield_t &pf, bool seconds=true)
{
    int cw, ch;
    get_clock_dimensions(cw, ch, seconds);
    int x = (pf.width - cw) / 2;
    int y = (pf.height - ch) / 2 + 1;

    int hour[2];
    int minute[2];
    int second[2];
    time_t ti = time(NULL);
    struct tm* tt = localtime(&ti);

    hour[0] = tt->tm_hour / 10;
    hour[1] = tt->tm_hour % 10;
    minute[0] = tt->tm_min / 10;
    minute[1] = tt->tm_min % 10;
    second[0] = tt->tm_sec / 10;
    second[1] = tt->tm_sec % 10;

    draw_digit(pf, x+1, y, TB_GREEN, hour[0]);
    draw_digit(pf, x+5, y, TB_GREEN, hour[1]);
    draw_digit(pf, x+8, y, TB_GREEN, 10);
    draw_digit(pf, x+11, y, TB_GREEN, minute[0]);
    draw_digit(pf, x+15, y, TB_GREEN, minute[1]);
    if (seconds) {
        draw_digit(pf, x+18, y, TB_GREEN, 10);
        draw_digit(pf, x+21, y, TB_GREEN, second[0]);
        draw_digit(pf, x+25, y, TB_GREEN, second[1]);
    }
}

static int tb_clock (int argc, char **argv)
{
    tb_init();
    tb_clear();
    int w, h;
    get_clock_dimensions(w, h);
    unsigned int tbw = tb_width();
    unsigned int tbh = tb_height();
    pixelfield_t pf(tbw, tbh, tbw / w, (tbw / w) / 2);
    draw_clock(pf);
    tb_present();

    struct tb_event ev;
    while (1) {
        int result = tb_peek_event(&ev, 200); /* fresh the terminal per 200ms */
        if (result) {
            switch (ev.type) {
            case TB_EVENT_KEY:
                if (ev.key == TB_KEY_ESC || ev.key == TB_KEY_CTRL_C) {
                    tb_shutdown();
                    return 0;
                }
            }
        } else {
            tb_clear();
            draw_clock(pf);
            tb_present();
        }
    }
}
#include <finsh.h>
MSH_CMD_EXPORT(tb_clock, termbox clock demo)

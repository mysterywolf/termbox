#include <stdio.h>
#include "../termbox.h"

static uint32_t fg_color = TB_WHITE;
static uint32_t selected_fg_color = TB_MAGENTA;
static uint32_t bg_color = TB_DEFAULT;

static char * items[30] = {
  "Option 1",
  "Option 2",
  "Option 3",
  "Option 4",
  "Option 5",
  "Option 6",
  "Option 7",
  "Option 8",
  "Option 9",
  "Option 10",
  "Option 1",
  "Option 2",
  "Option 3",
  "Option 4",
  "Option 5",
  "Option 6",
  "Option 7",
  "Option 8",
  "Option 9",
  "Option 10",
  "Option 1",
  "Option 2",
  "Option 3",
  "Option 4",
  "Option 5",
  "Option 6",
  "Option 7",
  "Option 8",
  "Option 9",
  "Option 10",
};

static int w = 0, h = 0;
static int selected = -1;
static int offset = 0;
static int num_items = 30;

static int margin_left = 1;
static int margin_top = 2;
static int margin_bottom = 2;

/* drawing functions */
static int draw_options(void) {
  int i, line;

  for (i = 0; i < (h - (margin_top + margin_bottom)); i++) {
    tb_empty(margin_left, i + margin_top, TB_DEFAULT, w - margin_left);
  }

  for (i = 0; i < (h - (margin_top + margin_bottom)); i++) {
    line = i + offset;
    if (line >= num_items) break;
    tb_stringf(margin_left, i + margin_top, line == selected ? selected_fg_color : fg_color, bg_color, "%s", items[line]);
  }
  return 0;
}

static void draw_title(void) {
  tb_string(margin_left, 0, TB_RED, bg_color, "A music player.");
}

static void draw_status(void) {
  tb_empty(0, h-1, TB_CYAN, w - margin_left);
  tb_stringf(margin_left, h-1, TB_BLACK, TB_CYAN, "Playing song: %s", selected >= 0 ? items[selected] : "None");
}

static void draw_window(void) {
  draw_title();
  draw_options();
  draw_status();
}

/* movement, select */
static int move_up(int lines) {
  if (lines <= 0) return 0;
  selected -= lines;
  if (selected <= 0) selected = 0;
  if (selected < offset) offset -= lines;
  if (offset < 0) offset = 0;
  return 0;
}

static int move_down(int lines) {
  if (lines <= 0) return 0;
  int menu_h = (h - (margin_top + margin_bottom));

  selected += lines;
  if (selected >= num_items) selected = num_items-1;

  // TODO: simplify this
  if ((selected + lines) >= menu_h) {
    // if (lines > 0 && num_items > 0 && (result > (num_items - menu_h + lines))) {
    if ((offset + lines) > (num_items - menu_h + lines))
      return 0;

    offset += lines;
  }

  return 0;
}

static int set_selected(int number) {
  selected = number;
  return 0;
}

static int play_song(int number) {
  tb_stringf(w - 12, 0, fg_color, bg_color, "Playing song: %d", number);
  return 0;
}

void tb_player(void) {

  tb_init();

  tb_select_input_mode(TB_INPUT_ESC | TB_INPUT_MOUSE);

  // get the screen resolution
  w = tb_width();
  h = tb_height();

  draw_window();
  tb_present();

  // now, wait for keyboard or input
  struct tb_event ev;
  while (tb_poll_event(&ev) != -1) {
    switch (ev.type) {

    case TB_EVENT_KEY:
      if (ev.key == TB_KEY_ESC || ev.key == TB_KEY_CTRL_C)
        goto done;

      if (ev.key == TB_KEY_ENTER)
        play_song(selected);

      if (ev.key == TB_KEY_ARROW_UP)
        move_up(1);

      if (ev.key == TB_KEY_ARROW_DOWN)
        move_down(1);

      if (ev.key == TB_KEY_MOUSE_WHEEL_UP)
        move_up(h/2);

      if (ev.key == TB_KEY_MOUSE_WHEEL_DOWN)
        move_down(h/2);

      break;
    case TB_EVENT_MOUSE:
      if (ev.key == TB_KEY_MOUSE_LEFT) {
        set_selected(offset + (ev.y - margin_top));
        if (ev.h == 2) play_song(selected);
      }

      if (ev.key == TB_KEY_MOUSE_WHEEL_UP)
        move_up(5);

      if (ev.key == TB_KEY_MOUSE_WHEEL_DOWN)
        move_down(5);

      break;
    }

    draw_window();
    tb_present();
  }

  done:
  // make sure to shutdown
  tb_shutdown();
}
#include <finsh.h>
MSH_CMD_EXPORT(tb_player, termbox player demo)

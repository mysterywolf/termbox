/*
 * Copyright (c) 2021, Meco Jianting Man <jiantingman@foxmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-08-23     Meco Man     port to RT-Thread
 */

#include <stddef.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <posix/stdlib.h>
#include <posix/wchar.h>
#include <rtthread.h>
#include "termbox.h"

#define DBG_TAG    "termbox"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>

/*---------------------memstream---------------------------*/
struct memstream
{
    size_t  pos;
    size_t capa;
    int file;
    unsigned char* data;
};

static void memstream_init(struct memstream* s, int fd, void* buffer, size_t len)
{
    s->file = fd;
    s->data = buffer;
    s->pos = 0;
    s->capa = len;
}

static void memstream_flush(struct memstream* s)
{
    write(s->file, s->data, s->pos);
    s->pos = 0;
}

static void memstream_write(struct memstream* s, void* source, size_t len)
{
    unsigned char* data = source;

    if (s->pos + len > s->capa)
    {
        memstream_flush(s);
    }

    rt_memcpy(s->data + s->pos, data, len);
    s->pos += len;
}

static void memstream_puts(struct memstream* s, const char* str)
{
    memstream_write(s, (void*) str, strlen(str));
}

/*---------------------ringbuffer---------------------------*/
#define ERINGBUFFER_ALLOC_FAIL -1

struct ringbuffer
{
    char* buf;
    size_t size;

    char* begin;
    char* end;
};

static void clear_ringbuffer(struct ringbuffer* r)
{
    r->begin = 0;
    r->end = 0;
}

static int init_ringbuffer(struct ringbuffer* r, size_t size)
{
    r->buf = (char*)rt_malloc(size);
    if (r->buf == RT_NULL)
    {
        LOG_E("init_ringbuffer malloc error!");
        return ERINGBUFFER_ALLOC_FAIL;
    }

    r->size = size;
    clear_ringbuffer(r);

    return 0;
}

static void free_ringbuffer(struct ringbuffer* r)
{
    rt_free(r->buf);
}

static size_t ringbuffer_free_space(struct ringbuffer* r)
{
    if (r->begin == 0 && r->end == 0)
    {
        return r->size;
    }

    if (r->begin < r->end)
    {
        return r->size - (r->end - r->begin) - 1;
    }
    else
    {
        return r->begin - r->end - 1;
    }
}

static size_t ringbuffer_data_size(struct ringbuffer* r)
{
    if (r->begin == 0 && r->end == 0)
    {
        return 0;
    }

    if (r->begin <= r->end)
    {
        return r->end - r->begin + 1;
    }
    else
    {
        return r->size - (r->begin - r->end) + 1;
    }
}


static void ringbuffer_push(struct ringbuffer* r, const void* data, size_t size)
{
    if (ringbuffer_free_space(r) < size)
    {
        return;
    }

    if (r->begin == 0 && r->end == 0)
    {
        rt_memcpy(r->buf, data, size);
        r->begin = r->buf;
        r->end = r->buf + size - 1;
        return;
    }

    r->end++;

    if (r->begin < r->end)
    {
        if ((size_t)(r->buf + (ptrdiff_t)r->size - r->begin) >= size)
        {
            // we can fit without cut
            rt_memcpy(r->end, data, size);
            r->end += size - 1;
        }
        else
        {
            // make a cut
            size_t s = r->buf + r->size - r->end;
            rt_memcpy(r->end, data, s);
            size -= s;
            rt_memcpy(r->buf, (char*)data + s, size);
            r->end = r->buf + size - 1;
        }
    }
    else
    {
        rt_memcpy(r->end, data, size);
        r->end += size - 1;
    }
}

static void ringbuffer_pop(struct ringbuffer* r, void* data, size_t size)
{
    if (ringbuffer_data_size(r) < size)
    {
        return;
    }

    int need_clear = 0;

    if (ringbuffer_data_size(r) == size)
    {
        need_clear = 1;
    }

    if (r->begin < r->end)
    {
        if (data)
        {
            rt_memcpy(data, r->begin, size);
        }

        r->begin += size;
    }
    else
    {
        if ((size_t)(r->buf + (ptrdiff_t)r->size - r->begin) >= size)
        {
            if (data)
            {
                rt_memcpy(data, r->begin, size);
            }

            r->begin += size;
        }
        else
        {
            size_t s = r->buf + r->size - r->begin;

            if (data)
            {
                rt_memcpy(data, r->begin, s);
            }

            size -= s;

            if (data)
            {
                rt_memcpy((char*)data + s, r->buf, size);
            }

            r->begin = r->buf + size;
        }
    }

    if (need_clear)
    {
        clear_ringbuffer(r);
    }
}

static void ringbuffer_read(struct ringbuffer* r, void* data, size_t size)
{
    if (ringbuffer_data_size(r) < size)
    {
        return;
    }

    if (r->begin < r->end)
    {
        rt_memcpy(data, r->begin, size);
    }
    else
    {
        if ((size_t)(r->buf + (ptrdiff_t)r->size - r->begin) >= size)
        {
            rt_memcpy(data, r->begin, size);
        }
        else
        {
            size_t s = r->buf + r->size - r->begin;
            rt_memcpy(data, r->begin, s);
            size -= s;
            rt_memcpy((char*)data + s, r->buf, size);
        }
    }
}

/*---------------------utf8---------------------------*/
static const unsigned char utf8_mask[6] =
{
    0x7F,
    0x1F,
    0x0F,
    0x07,
    0x03,
    0x01
};

#if 0
static const char utf8len_tab[256] = {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /*bogus*/
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /*bogus*/
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,1,1,
};

int utf8_char_length(char c)
{
    return utf8len_tab[(unsigned char)c];
}
#else
int utf8_char_length(char c)
{
  if ((signed char)c >= 0) {  // 1-byte char (0x00 ~ 0x7F)
    return 1;
  } else if ((c & 0xE0) == 0xC0) {  // 2-byte char
    return 2;
  } else if ((c & 0xF0) == 0xE0) {  // 3-byte char
    return 3;
  } else if ((c & 0xF8) == 0xF0) {  // 4-byte char
    return 4;
  }
  // invalid char
  return 0;
}
#endif

int utf8_char_to_unicode(uint32_t* out, const char* c)
{
    if (*c == 0)
    {
        return TB_EOF;
    }

    int i;
    unsigned char len = utf8_char_length(*c);
    unsigned char mask = utf8_mask[len - 1];
    uint32_t result = c[0] & mask;

    for (i = 1; i < len; ++i)
    {
        result <<= 6;
        result |= c[i] & 0x3f;
    }

    *out = result;
    return (int)len;
}

int utf8_unicode_to_char(char* out, uint32_t c)
{
    int len = 0;
    int first;
    int i;

    if (c < 0x80)
    {
        first = 0;
        len = 1;
    }
    else if (c < 0x800)
    {
        first = 0xc0;
        len = 2;
    }
    else if (c < 0x10000)
    {
        first = 0xe0;
        len = 3;
    }
    else if (c < 0x200000)
    {
        first = 0xf0;
        len = 4;
    }
    else if (c < 0x4000000)
    {
        first = 0xf8;
        len = 5;
    }
    else
    {
        first = 0xfc;
        len = 6;
    }

    for (i = len - 1; i > 0; --i)
    {
        out[i] = (c & 0x3f) | 0x80;
        c >>= 6;
    }

    out[0] = c | first;

    return len;
}

/*---------------------term---------------------------*/
enum
{
    T_ENTER_CA,
    T_EXIT_CA,
    T_SHOW_CURSOR,
    T_HIDE_CURSOR,
    T_CLEAR_SCREEN,
    T_SGR0,
    T_UNDERLINE,
    T_BOLD,
    T_BLINK,
    T_REVERSE,
    T_ENTER_KEYPAD,
    T_EXIT_KEYPAD,
    T_ENTER_MOUSE,
    T_EXIT_MOUSE,
    T_FUNCS_NUM,
};

#define ENTER_MOUSE_SEQ "\x1b[?1000h\x1b[?1002h\x1b[?1015h\x1b[?1006h"
#define EXIT_MOUSE_SEQ "\x1b[?1006l\x1b[?1015l\x1b[?1002l\x1b[?1000l"

// rxvt-256color
// static const char* rxvt_256color_keys[] =
// {
//  "\033[11~", "\033[12~", "\033[13~", "\033[14~", "\033[15~", "\033[17~",
//  "\033[18~", "\033[19~", "\033[20~", "\033[21~", "\033[23~", "\033[24~",
//  "\033[2~", "\033[3~", "\033[7~", "\033[8~", "\033[5~", "\033[6~",
//  "\033[A", "\033[B", "\033[D", "\033[C", NULL
// };
// static const char* rxvt_256color_funcs[] =
// {
//  "\0337\033[?47h", "\033[2J\033[?47l\0338", "\033[?25h", "\033[?25l",
//  "\033[H\033[2J", "\033[m", "\033[4m", "\033[1m", "\033[5m", "\033[7m",
//  "\033=", "\033>", ENTER_MOUSE_SEQ, EXIT_MOUSE_SEQ,
// };

// Eterm
// static const char* eterm_keys[] =
// {
//  "\033[11~", "\033[12~", "\033[13~", "\033[14~", "\033[15~", "\033[17~",
//  "\033[18~", "\033[19~", "\033[20~", "\033[21~", "\033[23~", "\033[24~",
//  "\033[2~", "\033[3~", "\033[7~", "\033[8~", "\033[5~", "\033[6~",
//  "\033[A", "\033[B", "\033[D", "\033[C", NULL
// };
// static const char* eterm_funcs[] =
// {
//  "\0337\033[?47h", "\033[2J\033[?47l\0338", "\033[?25h", "\033[?25l",
//  "\033[H\033[2J", "\033[m", "\033[4m", "\033[1m", "\033[5m", "\033[7m",
//  "", "", "", "",
// };

// screen
// static const char* screen_keys[] =
// {
//  "\033OP", "\033OQ", "\033OR", "\033OS", "\033[15~", "\033[17~",
//  "\033[18~", "\033[19~", "\033[20~", "\033[21~", "\033[23~", "\033[24~",
//  "\033[2~", "\033[3~", "\033[1~", "\033[4~", "\033[5~", "\033[6~",
//  "\033OA", "\033OB", "\033OD", "\033OC", NULL
// };
// static const char* screen_funcs[] =
// {
//  "\033[?1049h", "\033[?1049l", "\033[34h\033[?25h", "\033[?25l",
//  "\033[H\033[J", "\033[m", "\033[4m", "\033[1m", "\033[5m", "\033[7m",
//  "\033[?1h\033=", "\033[?1l\033>", ENTER_MOUSE_SEQ, EXIT_MOUSE_SEQ,
// };

// rxvt-unicode
// static const char* rxvt_unicode_keys[] =
// {
//  "\033[11~", "\033[12~", "\033[13~", "\033[14~", "\033[15~", "\033[17~",
//  "\033[18~", "\033[19~", "\033[20~", "\033[21~", "\033[23~", "\033[24~",
//  "\033[2~", "\033[3~", "\033[7~", "\033[8~", "\033[5~", "\033[6~",
//  "\033[A", "\033[B", "\033[D", "\033[C", NULL
// };
// static const char* rxvt_unicode_funcs[] =
// {
//  "\033[?1049h", "\033[r\033[?1049l", "\033[?25h", "\033[?25l",
//  "\033[H\033[2J", "\033[m\033(B", "\033[4m", "\033[1m", "\033[5m",
//  "\033[7m", "\033=", "\033>", ENTER_MOUSE_SEQ, EXIT_MOUSE_SEQ,
// };

// linux
// static const char* linux_keys[] =
// {
//     "\033[[A", "\033[[B", "\033[[C", "\033[[D", "\033[[E", "\033[17~",
//     "\033[18~", "\033[19~", "\033[20~", "\033[21~", "\033[23~", "\033[24~",
//     "\033[2~", "\033[3~", "\033[1~", "\033[4~", "\033[5~", "\033[6~",
//     "\033[A", "\033[B", "\033[D", "\033[C", NULL
// };
// static const char* linux_funcs[] =
// {
//     "", "", "\033[?25h\033[?0c", "\033[?25l\033[?1c", "\033[H\033[J",
//     "\033[0;10m", "\033[4m", "\033[1m", "\033[5m", "\033[7m", "", "", "", "",
// };

// xterm
static const char* xterm_keys[] =
{
    "\033OP", "\033OQ", "\033OR", "\033OS", "\033[15~", "\033[17~", "\033[18~",
    "\033[19~", "\033[20~", "\033[21~", "\033[23~", "\033[24~", "\033[2~",
    "\033[3~", "\033OH", "\033OF", "\033[5~", "\033[6~", "\033OA", "\033OB",
    "\033OD", "\033OC", NULL
};
static const char* xterm_funcs[] =
{
    "\033[?1049h", "\033[?1049l", "\033[?12l\033[?25h", "\033[?25l",
    "\033[H\033[2J", "\033(B\033[m", "\033[4m", "\033[1m", "\033[5m", "\033[7m",
    "\033[?1h\033=", "\033[?1l\033>", ENTER_MOUSE_SEQ, EXIT_MOUSE_SEQ,
};

static const char** keys;
static const char** funcs;

static int init_term(void)
{
    /* PuTTY supports sterm by default, which can let you to use mouse */
    keys = xterm_keys;
    funcs = xterm_funcs;
    return 0;
}

/*---------------------input---------------------------*/
#define BUFFER_SIZE_MAX 16

// if s1 starts with s2 returns 1, else 0
static int starts_with(const char* s1, const char* s2)
{
    while (*s2)
    {
        if (*s1++ != *s2++)
        {
            return 0;
        }
    }

    return 1;
}

static int parse_mouse_event(struct tb_event* event, const char* buf, int len)
{
    if ((len >= 6) && starts_with(buf, "\033[M"))
    {
        // X10 mouse encoding, the simplest one
        // \033 [ M Cb Cx Cy
        int b = buf[3] - 32;

        switch (b & 3)
        {
            case 0:
                if ((b & 64) != 0)
                {
                    event->key = TB_KEY_MOUSE_WHEEL_UP;
                }
                else
                {
                    event->key = TB_KEY_MOUSE_LEFT;
                }

                break;

            case 1:
                if ((b & 64) != 0)
                {
                    event->key = TB_KEY_MOUSE_WHEEL_DOWN;
                }
                else
                {
                    event->key = TB_KEY_MOUSE_MIDDLE;
                }

                break;

            case 2:
                event->key = TB_KEY_MOUSE_RIGHT;
                break;

            case 3:
                event->key = TB_KEY_MOUSE_RELEASE;
                break;

            default:
                return -6;
        }

        event->type = TB_EVENT_MOUSE; // TB_EVENT_KEY by default

        if ((b & 32) != 0)
        {
            event->mod |= TB_MOD_MOTION;
        }

        // the coord is 1,1 for upper left
        event->x = (uint8_t)buf[4] - 1 - 32;
        event->y = (uint8_t)buf[5] - 1 - 32;
        return 6;
    }
    else if (starts_with(buf, "\033[<") || starts_with(buf, "\033["))
    {
        // xterm 1006 extended mode or urxvt 1015 extended mode
        // xterm: \033 [ < Cb ; Cx ; Cy (M or m)
        // urxvt: \033 [ Cb ; Cx ; Cy M
        int i, mi = -1, starti = -1;
        int isM, isU, s1 = -1, s2 = -1;
        int n1 = 0, n2 = 0, n3 = 0;

        for (i = 0; i < len; i++)
        {
            // We search the first (s1) and the last (s2) ';'
            if (buf[i] == ';')
            {
                if (s1 == -1)
                {
                    s1 = i;
                }

                s2 = i;
            }

            // We search for the first 'm' or 'M'
            if ((buf[i] == 'm' || buf[i] == 'M') && mi == -1)
            {
                mi = i;
                break;
            }
        }

        if (mi == -1)
        {
            return 0;
        }

        // whether it's a capital M or not
        isM = (buf[mi] == 'M');

        if (buf[2] == '<')
        {
            isU = 0;
            starti = 3;
        }
        else
        {
            isU = 1;
            starti = 2;
        }

        if (s1 == -1 || s2 == -1 || s1 == s2)
        {
            return 0;
        }

        n1 = strtoul(&buf[starti], NULL, 10);
        n2 = strtoul(&buf[s1 + 1], NULL, 10);
        n3 = strtoul(&buf[s2 + 1], NULL, 10);

        if (isU)
        {
            n1 -= 32;
        }

        switch (n1 & 3)
        {
            case 0:
                if ((n1 & 64) != 0)
                {
                    event->key = TB_KEY_MOUSE_WHEEL_UP;
                }
                else
                {
                    event->key = TB_KEY_MOUSE_LEFT;
                }

                break;

            case 1:
                if ((n1 & 64) != 0)
                {
                    event->key = TB_KEY_MOUSE_WHEEL_DOWN;
                }
                else
                {
                    event->key = TB_KEY_MOUSE_MIDDLE;
                }

                break;

            case 2:
                event->key = TB_KEY_MOUSE_RIGHT;
                break;

            case 3:
                event->key = TB_KEY_MOUSE_RELEASE;
                break;

            default:
                return mi + 1;
        }

        if (!isM)
        {
            // on xterm mouse release is signaled by lowercase m
            event->key = TB_KEY_MOUSE_RELEASE;
        }

        event->type = TB_EVENT_MOUSE; // TB_EVENT_KEY by default

        if ((n1 & 32) != 0)
        {
            event->mod |= TB_MOD_MOTION;
        }

        event->x = (uint8_t)n2 - 1;
        event->y = (uint8_t)n3 - 1;
        return mi + 1;
    }

    return 0;
}

// convert escape sequence to event, and return consumed bytes on success (failure == 0)
static int parse_escape_seq(struct tb_event* event, const char* buf, int len)
{
    int mouse_parsed = parse_mouse_event(event, buf, len);

    if (mouse_parsed != 0)
    {
        return mouse_parsed;
    }

    // it's pretty simple here, find 'starts_with' match and return success, else return failure
    int i;

    for (i = 0; keys[i]; i++)
    {
        if (starts_with(buf, keys[i]))
        {
            event->ch = 0;
            event->key = 0xFFFF - i;
            return strlen(keys[i]);
        }
    }

    return 0;
}

static rt_bool_t extract_event(struct tb_event* event, struct ringbuffer* inbuf,
    int inputmode)
{
    char buf[BUFFER_SIZE_MAX + 1];
    int nbytes = ringbuffer_data_size(inbuf);

    if (nbytes > BUFFER_SIZE_MAX)
    {
        nbytes = BUFFER_SIZE_MAX;
    }

    if (nbytes == 0)
    {
        return RT_FALSE;
    }

    ringbuffer_read(inbuf, buf, nbytes);
    buf[nbytes] = '\0';

    if (buf[0] == '\033')
    {
        int n = parse_escape_seq(event, buf, nbytes);

        if (n != 0)
        {
            rt_bool_t success = RT_TRUE;

            if (n < 0)
            {
                success = RT_FALSE;
                n = -n;
            }

            ringbuffer_pop(inbuf, 0, n);
            return success;
        }
        else
        {
            // it's not escape sequence, then it's ALT or ESC, check inputmode
            if (inputmode & TB_INPUT_ESC)
            {
                // if we're in escape mode, fill ESC event, pop buffer, return success
                event->ch = 0;
                event->key = TB_KEY_ESC;
                event->mod = 0;
                ringbuffer_pop(inbuf, 0, 1);
                return RT_TRUE;
            }
            else if (inputmode & TB_INPUT_ALT)
            {
                // if we're in alt mode, set ALT modifier to event and redo parsing
                event->mod = TB_MOD_ALT;
                ringbuffer_pop(inbuf, 0, 1);
                return extract_event(event, inbuf, inputmode);
            }
        }
    }

    //  if we're here, this is not an escape sequence and not an alt sequence
    //  so, it's a FUNCTIONAL KEY or a UNICODE character

    // first of all check if it's a functional key*/
    if ((unsigned char)buf[0] <= TB_KEY_SPACE ||
        (unsigned char)buf[0] == TB_KEY_BACKSPACE2)
    {
        // fill event, pop buffer, return success
        event->ch = 0;
        event->key = (uint16_t)buf[0];
        ringbuffer_pop(inbuf, 0, 1);
        return RT_TRUE;
    }

    // feh... we got utf8 here

    // check if there is all bytes
    if (nbytes >= utf8_char_length(buf[0]))
    {
        // everything ok, fill event, pop buffer, return success
        utf8_char_to_unicode(&event->ch, buf);
        event->key = 0;
        ringbuffer_pop(inbuf, 0, utf8_char_length(buf[0]));
        return RT_TRUE;
    }

    return RT_FALSE;
}


/*---------------------termbox---------------------------*/
#define TERMBOX_WAIT_FOREVER    RT_TICK_MAX/2 - 1

#ifndef TB_NO_MEMDEV
struct cellbuf
{
    int width;
    int height;
    struct tb_cell* cells;
};

#define CELL(buf, x, y) (buf)->cells[(y) * (buf)->width + (x)]
#endif

#define IS_CURSOR_HIDDEN(cx, cy) (cx == -1 || cy == -1)
#define LAST_COORD_INIT -1

#ifndef TB_INPUT_BUFFER_SIZE
#define TB_INPUT_BUFFER_SIZE RT_SERIAL_RB_BUFSZ
#endif

#ifndef TB_OUTPUT_BUFFER_SIZE
#define TB_OUTPUT_BUFFER_SIZE 512
#endif

#ifndef TB_NO_MEMDEV
static struct cellbuf back_buffer;
static struct cellbuf front_buffer;
#endif
static unsigned char write_buffer_data[TB_OUTPUT_BUFFER_SIZE];
static struct memstream write_buffer;

static int termw = -1;
static int termh = -1;

static int inputmode = TB_INPUT_ESC;
static int outputmode = TB_OUTPUT_NORMAL;

static struct ringbuffer inbuf;

static int lastx = LAST_COORD_INIT;
static int lasty = LAST_COORD_INIT;
static int cursor_x = -1;
static int cursor_y = -1;

static uint32_t background = TB_DEFAULT;
static uint32_t foreground = TB_DEFAULT;

static void write_cursor(int x, int y);
static void write_sgr(uint32_t fg, uint32_t bg);

#ifndef TB_NO_MEMDEV
static void cellbuf_init(struct cellbuf* buf, int width, int height);
static void cellbuf_resize(struct cellbuf* buf, int width, int height);
static void cellbuf_clear(struct cellbuf* buf);
static void cellbuf_free(struct cellbuf* buf);
#endif

static void update_size(void);
static void update_term_size(void);
static void send_attr(uint32_t fg, uint32_t bg);
static void send_char(int x, int y, uint32_t c);
static void send_clear(void);
static int wait_fill_event(struct tb_event* event, int timeout);

// may happen in a different thread
static volatile int buffer_size_change_request;

int tb_init(void)
{
    init_term();

    memstream_init(&write_buffer, STDOUT_FILENO, write_buffer_data,
        sizeof(write_buffer_data));
    memstream_puts(&write_buffer, funcs[T_ENTER_CA]);
    memstream_puts(&write_buffer, funcs[T_ENTER_KEYPAD]);
    memstream_puts(&write_buffer, funcs[T_HIDE_CURSOR]);
    send_clear();

    update_term_size();

#ifndef TB_NO_MEMDEV
    cellbuf_init(&back_buffer, termw, termh);
    cellbuf_init(&front_buffer, termw, termh);
    cellbuf_clear(&back_buffer);
    cellbuf_clear(&front_buffer);
#endif

    init_ringbuffer(&inbuf, TB_INPUT_BUFFER_SIZE);

    return 0;
}

void tb_shutdown(void)
{
    if (termw == -1)
    {
        return;
    }

    memstream_puts(&write_buffer, funcs[T_SHOW_CURSOR]);
    memstream_puts(&write_buffer, funcs[T_SGR0]);
    memstream_puts(&write_buffer, funcs[T_CLEAR_SCREEN]);
    memstream_puts(&write_buffer, funcs[T_EXIT_CA]);
    memstream_puts(&write_buffer, funcs[T_EXIT_KEYPAD]);
    memstream_puts(&write_buffer, funcs[T_EXIT_MOUSE]);
    memstream_flush(&write_buffer);

#ifndef TB_NO_MEMDEV
    cellbuf_free(&back_buffer);
    cellbuf_free(&front_buffer);
#endif
    free_ringbuffer(&inbuf);
    termw = termh = -1;
}

void tb_present(void)
{
#ifndef TB_NO_MEMDEV
    int x, y, w, i;
    struct tb_cell* back, *front;

    // invalidate cursor position
    lastx = LAST_COORD_INIT;
    lasty = LAST_COORD_INIT;

    if (buffer_size_change_request)
    {
        update_size();
        buffer_size_change_request = 0;
    }

    for (y = 0; y < front_buffer.height; ++y)
    {
        for (x = 0; x < front_buffer.width;)
        {
            back = &CELL(&back_buffer, x, y);
            front = &CELL(&front_buffer, x, y);
            w = wcwidth(back->ch);
            if (w < 1)
            {
                w = 1;
            }

            if (memcmp(back, front, sizeof(struct tb_cell)) == 0)
            {
                x += w;
                continue;
            }

            rt_memcpy(front, back, sizeof(struct tb_cell));
            send_attr(back->fg, back->bg);

            if (w > 1 && x >= front_buffer.width - (w - 1))
            {
                // Not enough room for wide ch, so send spaces
                for (i = x; i < front_buffer.width; ++i)
                {
                    send_char(i, y, ' ');
                }
            }
            else
            {
                send_char(x, y, back->ch);

                for (i = 1; i < w; ++i)
                {
                    front = &CELL(&front_buffer, x + i, y);
                    front->ch = 0;
                    front->fg = back->fg;
                    front->bg = back->bg;
                }
            }

            x += w;
        }
    }

    if (!IS_CURSOR_HIDDEN(cursor_x, cursor_y))
    {
        write_cursor(cursor_x, cursor_y);
    }
#endif /* TB_NO_MEMDEV */
    memstream_flush(&write_buffer);
}

void tb_set_cursor(int cx, int cy)
{
    if (IS_CURSOR_HIDDEN(cursor_x, cursor_y) && !IS_CURSOR_HIDDEN(cx, cy))
    {
        memstream_puts(&write_buffer, funcs[T_SHOW_CURSOR]);
    }

    if (!IS_CURSOR_HIDDEN(cursor_x, cursor_y) && IS_CURSOR_HIDDEN(cx, cy))
    {
        memstream_puts(&write_buffer, funcs[T_HIDE_CURSOR]);
    }

    cursor_x = cx;
    cursor_y = cy;

    if (!IS_CURSOR_HIDDEN(cursor_x, cursor_y))
    {
        write_cursor(cursor_x, cursor_y);
    }
}

void tb_put_cell(int x, int y, const struct tb_cell* cell)
{
#ifndef TB_NO_MEMDEV
    if ((unsigned)x >= (unsigned)back_buffer.width)
    {
        return;
    }

    if ((unsigned)y >= (unsigned)back_buffer.height)
    {
        return;
    }

    CELL(&back_buffer, x, y) = *cell;
#else
    send_attr(cell->fg, cell->bg);
    send_char(x, y, cell->ch);
#endif
}

void tb_change_cell(int x, int y, uint32_t ch, uint32_t fg, uint32_t bg)
{
    struct tb_cell c = {ch, fg, bg};
    tb_put_cell(x, y, &c);
}

#ifndef TB_NO_MEMDEV
void tb_blit(int x, int y, int w, int h, const struct tb_cell* cells)
{
    if (x + w < 0 || x >= back_buffer.width)
    {
        return;
    }

    if (y + h < 0 || y >= back_buffer.height)
    {
        return;
    }

    int xo = 0, yo = 0, ww = w, hh = h;

    if (x < 0)
    {
        xo = -x;
        ww -= xo;
        x = 0;
    }

    if (y < 0)
    {
        yo = -y;
        hh -= yo;
        y = 0;
    }

    if (ww > back_buffer.width - x)
    {
        ww = back_buffer.width - x;
    }

    if (hh > back_buffer.height - y)
    {
        hh = back_buffer.height - y;
    }

    int sy;
    struct tb_cell* dst = &CELL(&back_buffer, x, y);
    const struct tb_cell* src = cells + yo * w + xo;
    size_t size = sizeof(struct tb_cell) * ww;

    for (sy = 0; sy < hh; ++sy)
    {
        rt_memcpy(dst, src, size);
        dst += back_buffer.width;
        src += w;
    }
}

struct tb_cell* tb_cell_buffer(void)
{
    return back_buffer.cells;
}
#endif /* TB_NO_MEMDEV */

int tb_poll_event(struct tb_event* event)
{
    return wait_fill_event(event, TERMBOX_WAIT_FOREVER);
}

int tb_peek_event(struct tb_event* event, int timeout)
{
    return wait_fill_event(event, timeout);
}

int tb_width(void)
{
    return termw;
}

int tb_height(void)
{
    return termh;
}

void tb_clear(void)
{
    if (buffer_size_change_request)
    {
        update_size();
        buffer_size_change_request = 0;
    }

#ifndef TB_NO_MEMDEV
    cellbuf_clear(&back_buffer);
#endif
}

int tb_select_input_mode(int mode)
{
    if (mode)
    {
        if ((mode & (TB_INPUT_ESC | TB_INPUT_ALT)) == 0)
        {
            mode |= TB_INPUT_ESC;
        }

        // technically termbox can handle that, but let's be nice
        // and show here what mode is actually used
        if ((mode & (TB_INPUT_ESC | TB_INPUT_ALT)) == (TB_INPUT_ESC | TB_INPUT_ALT))
        {
            mode &= ~TB_INPUT_ALT;
        }

        inputmode = mode;

        if (mode & TB_INPUT_MOUSE)
        {
            memstream_puts(&write_buffer, funcs[T_ENTER_MOUSE]);
            memstream_flush(&write_buffer);
        }
        else
        {
            memstream_puts(&write_buffer, funcs[T_EXIT_MOUSE]);
            memstream_flush(&write_buffer);
        }
    }

    return inputmode;
}

int tb_select_output_mode(int mode)
{
    if (mode)
    {
        outputmode = mode;
    }

    return outputmode;
}

void tb_set_clear_attributes(uint32_t fg, uint32_t bg)
{
    foreground = fg;
    background = bg;
}

static unsigned convertnum(uint32_t num, char* buf)
{
    unsigned i, l = 0;
    int ch;

    do
    {
        buf[l++] = '0' + (num % 10);
        num /= 10;
    }
    while (num);

    for (i = 0; i < l / 2; i++)
    {
        ch = buf[i];
        buf[i] = buf[l - 1 - i];
        buf[l - 1 - i] = ch;
    }

    return l;
}

#define WRITE_LITERAL(X) memstream_write(&write_buffer, (X), sizeof(X) -1)
#define WRITE_INT(X) memstream_write(&write_buffer, buf, convertnum((X), buf))

static void write_cursor(int x, int y)
{
    char buf[32];
    WRITE_LITERAL("\033[");
    WRITE_INT(y + 1);
    WRITE_LITERAL(";");
    WRITE_INT(x + 1);
    WRITE_LITERAL("H");
}

static void write_sgr(uint32_t fg, uint32_t bg)
{
    char buf[32];

    if (outputmode != TB_OUTPUT_TRUECOLOR && fg == TB_DEFAULT && bg == TB_DEFAULT)
    {
        return;
    }

    switch (outputmode)
    {
        case TB_OUTPUT_TRUECOLOR:
            WRITE_LITERAL("\033[38;2;");
            WRITE_INT(fg >> 16 & 0xFF);
            WRITE_LITERAL(";");
            WRITE_INT(fg >> 8 & 0xFF);
            WRITE_LITERAL(";");
            WRITE_INT(fg & 0xFF);
            WRITE_LITERAL(";48;2;");
            WRITE_INT(bg >> 16 & 0xFF);
            WRITE_LITERAL(";");
            WRITE_INT(bg >> 8 & 0xFF);
            WRITE_LITERAL(";");
            WRITE_INT(bg & 0xFF);
            WRITE_LITERAL("m");
            break;

        case TB_OUTPUT_256:
        case TB_OUTPUT_216:
        case TB_OUTPUT_GRAYSCALE:
            WRITE_LITERAL("\033[");

            if (fg != TB_DEFAULT)
            {
                WRITE_LITERAL("38;5;");
                WRITE_INT(fg);

                if (bg != TB_DEFAULT)
                {
                    WRITE_LITERAL(";");
                }
            }

            if (bg != TB_DEFAULT)
            {
                WRITE_LITERAL("48;5;");
                WRITE_INT(bg);
            }

            WRITE_LITERAL("m");
            break;

        case TB_OUTPUT_NORMAL:
        default:
            WRITE_LITERAL("\033[");

            if (fg != TB_DEFAULT)
            {
                WRITE_LITERAL("3");
                WRITE_INT(fg - 1);

                if (bg != TB_DEFAULT)
                {
                    WRITE_LITERAL(";");
                }
            }

            if (bg != TB_DEFAULT)
            {
                WRITE_LITERAL("4");
                WRITE_INT(bg - 1);
            }

            WRITE_LITERAL("m");
            break;
    }
}

#ifndef TB_NO_MEMDEV
static void cellbuf_init(struct cellbuf* buf, int width, int height)
{
    buf->cells = (struct tb_cell*)rt_malloc(sizeof(struct tb_cell) * width * height);
    if(buf->cells == RT_NULL)
    {
        LOG_E("cellbuf_init malloc error!");
    }

    buf->width = width;
    buf->height = height;
}

static void cellbuf_resize(struct cellbuf* buf, int width, int height)
{
    if (buf->width == width && buf->height == height)
    {
        return;
    }

    if(buf->cells == RT_NULL)
    {
        return;
    }

    int oldw = buf->width;
    int oldh = buf->height;
    struct tb_cell* oldcells = buf->cells;

    cellbuf_init(buf, width, height);
    cellbuf_clear(buf);

    int minw = (width < oldw) ? width : oldw;
    int minh = (height < oldh) ? height : oldh;
    int i;

    for (i = 0; i < minh; ++i)
    {
        struct tb_cell* csrc = oldcells + (i * oldw);
        struct tb_cell* cdst = buf->cells + (i * width);
        rt_memcpy(cdst, csrc, sizeof(struct tb_cell) * minw);
    }

    rt_free(oldcells);
}

static void cellbuf_clear(struct cellbuf* buf)
{
    int i;
    int ncells = buf->width * buf->height;

    if(buf->cells == RT_NULL)
    {
        return;
    }

    for (i = 0; i < ncells; ++i)
    {
        buf->cells[i].ch = ' ';
        buf->cells[i].fg = foreground;
        buf->cells[i].bg = background;
    }
}

static void cellbuf_free(struct cellbuf* buf)
{
    rt_free(buf->cells);
}
#endif /* TB_NO_MEMDEV */

static void update_term_size(void)
{
    struct winsize sz;
    rt_memset(&sz, 0, sizeof(sz));

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &sz) < 0)
    {
        /* use default value if we cannot get the window size */
        termw = 80;
        termh = 24;
    }

    termw = sz.ws_col > 0 ? sz.ws_col : 80;
    termh = sz.ws_row > 0 ? sz.ws_row : 24;
}

static void send_attr(uint32_t fg, uint32_t bg)
{
#define LAST_ATTR_INIT 0xFFFFFFFF
    static uint32_t lastfg = LAST_ATTR_INIT, lastbg = LAST_ATTR_INIT;

    if (fg != lastfg || bg != lastbg)
    {
        memstream_puts(&write_buffer, funcs[T_SGR0]);
        uint32_t fgcol;
        uint32_t bgcol;

        switch (outputmode)
        {
            case TB_OUTPUT_TRUECOLOR:
                fgcol = fg;
                bgcol = bg;
                break;

            case TB_OUTPUT_256:
                fgcol = fg & 0xFF;
                bgcol = bg & 0xFF;
                break;

            case TB_OUTPUT_216:
                fgcol = fg & 0xFF;

                if (fgcol > 215)
                {
                    fgcol = 7;
                }

                bgcol = bg & 0xFF;

                if (bgcol > 215)
                {
                    bgcol = 0;
                }

                fgcol += 0x10;
                bgcol += 0x10;
                break;

            case TB_OUTPUT_GRAYSCALE:
                fgcol = fg & 0xFF;

                if (fgcol > 23)
                {
                    fgcol = 23;
                }

                bgcol = bg & 0xFF;

                if (bgcol > 23)
                {
                    bgcol = 0;
                }

                fgcol += 0xe8;
                bgcol += 0xe8;
                break;

            case TB_OUTPUT_NORMAL:
            default:
                fgcol = fg & 0x0F;
                bgcol = bg & 0x0F;
        }

        if (fg & TB_BOLD)
        {
            memstream_puts(&write_buffer, funcs[T_BOLD]);
        }

        if (bg & TB_BOLD)
        {
            memstream_puts(&write_buffer, funcs[T_BLINK]);
        }

        if (fg & TB_UNDERLINE)
        {
            memstream_puts(&write_buffer, funcs[T_UNDERLINE]);
        }

        if ((fg & TB_REVERSE) || (bg & TB_REVERSE))
        {
            memstream_puts(&write_buffer, funcs[T_REVERSE]);
        }

        write_sgr(fgcol, bgcol);

        lastfg = fg;
        lastbg = bg;
    }
#undef LAST_ATTR_INIT
}

static void send_char(int x, int y, uint32_t c)
{
    char buf[7];
    int bw = utf8_unicode_to_char(buf, c);
    buf[bw] = '\0';

    if (x - 1 != lastx || y != lasty)
    {
        write_cursor(x, y);
    }

    lastx = x;
    lasty = y;

    if (!c)
    {
        buf[0] = ' '; // replace 0 with whitespace
    }

    memstream_puts(&write_buffer, buf);
}

static void send_clear(void)
{
    send_attr(foreground, background);
    memstream_puts(&write_buffer, funcs[T_CLEAR_SCREEN]);

    if (!IS_CURSOR_HIDDEN(cursor_x, cursor_y))
    {
        write_cursor(cursor_x, cursor_y);
    }

    memstream_flush(&write_buffer);

    // we need to invalidate cursor position too and these two vars are
    // used only for simple cursor positioning optimization, cursor
    // actually may be in the correct place, but we simply discard
    // optimization once and it gives us simple solution for the case when
    // cursor moved
    lastx = LAST_COORD_INIT;
    lasty = LAST_COORD_INIT;
}

static void update_size(void)
{
    update_term_size();
#ifndef TB_NO_MEMDEV
    cellbuf_resize(&back_buffer, termw, termh);
    cellbuf_resize(&front_buffer, termw, termh);
    cellbuf_clear(&front_buffer);
#endif
    send_clear();
}

static int wait_fill_event(struct tb_event* event, int timeout)
{
    char ch_buf[BUFFER_SIZE_MAX];
    struct pollfd poll_fd;
    int ret;

    poll_fd.fd = STDIN_FILENO;
    poll_fd.events = POLLIN;

    rt_memset(event, 0, sizeof(struct tb_event));

    // try to extract event from input buffer, return on success
    event->type = TB_EVENT_KEY;
    if (extract_event(event, &inbuf, inputmode) == RT_TRUE)
    {
        return event->type;
    }

    while (1)
    {
        ret = poll(&poll_fd, 1, timeout);
        if(ret < 0)
        {
            return 0; /* poll error */
        }
        else if(ret == 0)
        {
            if(timeout == TERMBOX_WAIT_FOREVER)
            {
                continue; /* continue waitting forever */
            }
            else
            {
                return 0; /* timeout */
            }
        }

        if(poll_fd.revents & POLLIN)
        {
            ret = read(STDIN_FILENO, ch_buf, BUFFER_SIZE_MAX);
            if(ret > 0)
            {
                ringbuffer_push(&inbuf, ch_buf, ret);
            }
        }

        /* get the rest of sequence */
        while(1)
        {
            /* timeout:5. if the next character come in 5 ms, it will be
                consider as a part of this sequence*/
            ret = poll(&poll_fd, 1, 5);
            if(ret < 0)
            {
                break; /* poll error */
            }
            else if(ret == 0)
            {
                break; /* timeout */
            }

            if(poll_fd.revents & POLLIN)
            {
                ret = read(STDIN_FILENO, ch_buf, BUFFER_SIZE_MAX);
                if(ret <= 0)
                {
                    break;
                }
                ringbuffer_push(&inbuf, ch_buf, ret);
            }
        }

        /* got a complete sequence and begin to analyze this sequence */
        event->type = TB_EVENT_KEY;
        if (extract_event(event, &inbuf, inputmode) == RT_TRUE)
        {
            return event->type;
        }
    }
}

/*-------------------termbox2--------------------------*/
#define MAX_LIMIT RT_CONSOLEBUF_SIZE
static char _print_buf[MAX_LIMIT];

void tb_char(int x, int y, uint32_t fg, uint32_t bg, uint32_t ch)
{
    struct tb_cell c = {ch, fg, bg};
    tb_put_cell(x, y, &c);
}

int tb_string_with_limit(int x, int y, uint32_t fg, uint32_t bg, const char *str, int limit)
{
    uint32_t uni;
    int w, l = 0;

    while (*str && l < limit)
    {
        str += utf8_char_to_unicode(&uni, str);
        tb_char(x, y, fg, bg, uni);
        w = wcwidth(uni);
        x = x + w;
        l = l + w;
    }

    return l;
}

int tb_string(int x, int y, uint32_t fg, uint32_t bg, const char *str)
{
    return tb_string_with_limit(x, y, fg, bg, str, MAX_LIMIT);
}

int tb_stringf(int x, int y, uint32_t fg, uint32_t bg, const char *fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    rt_vsnprintf(_print_buf, sizeof(_print_buf), fmt, vl);
    va_end(vl);
    return tb_string(x, y, fg, bg, _print_buf);
}

void tb_empty(int x, int y, uint32_t bg, int width)
{
    rt_sprintf(_print_buf, "%*s", width, "");
    tb_string_with_limit(x, y, TB_DEFAULT, bg, _print_buf, width);
}

static const unsigned short int steps[6] = {47, 115, 155, 195, 235, 256}; // in between of each level

static uint8_t _get_256_color(uint32_t color)
{
  /* extract rgb values from number (e.g. 0xffcc00 -> 16763904) */
  uint8_t rgb[3] = { 0, 0, 0 }; // r, g, b
  rgb[0] = (color >> 16) & 0xFF; // e.g. 255
  rgb[1] = (color >> 8)  & 0xFF; // e.g. 204
  rgb[2] = (color >> 0)  & 0xFF; // e.g. 0

  /* now, translate rgb to their most similar value between the 0-5 range */
  int nums[3] = { 0, 0, 0 };
  for (int c = 0; c < 3; c++) {
    for (int i = 0; i < 6; i++) {
      if (steps[i] > rgb[c]) {
        nums[c] = i;
        break;
      }
    }
  }

  /* 16 + (r * 36) + (g * 6) + b */
  return 16 + (nums[0] * 36) + (nums[1] * 6) + nums[2];
}

static const uint8_t base_colors[8][3] =
{
 { 1, 1, 1 }, // black
 { 1, 0, 0 }, // red
 { 0, 1, 0 }, // green
 { 1, 1, 0 }, // yellow
 { 0, 0, 1 }, // blue
 { 1, 0, 1 }, // magenta
 { 0, 1, 1 }, // cyan
 { 1, 1, 1 }  // white
};

static uint8_t _get_base_color(uint32_t color)
{
  /* extract rgb values and round them to either 0 or 1 */
  uint8_t rgb[3] = { 0, 0, 0 }; // r, g, b
  rgb[0] = ((color >> 16) & 0xFF) > 128 ? 1 : 0;
  rgb[1] = ((color >> 8)  & 0xFF) > 128 ? 1 : 0;
  rgb[2] = ((color >> 0)  & 0xFF) > 128 ? 1 : 0;

  for (uint8_t i = 0; i < 8; i++) {
    if (base_colors[i][0] == rgb[0] && base_colors[i][1] == rgb[1] && base_colors[i][2] == rgb[2]) {
      return i;
    }
  }

  return 0; // default
}

uint8_t tb_rgb(uint32_t in)
{
  if (outputmode == TB_OUTPUT_256) {
    return _get_256_color(in);
  } else {
    return _get_base_color(in);
  }
}

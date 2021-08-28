#include "../termbox.h"

static int tb_language(void)
{
    struct tb_event ev;

    tb_init();

    /* 中文 */
    /* 注意中文是两个字符宽度,因此是0,2,4,6,8 */
    tb_change_cell(0, 0, 0x4f60, TB_CYAN, TB_DEFAULT); /* 你 */
    tb_change_cell(2, 0, 0x597d, TB_CYAN, TB_DEFAULT); /* 好 */
    tb_change_cell(4, 0, 0x4e16, TB_CYAN, TB_DEFAULT); /* 世 */
    tb_change_cell(6, 0, 0x754c, TB_CYAN, TB_DEFAULT); /* 界 */
    tb_change_cell(8, 0, 0x2665, TB_CYAN, TB_DEFAULT); /* ♥ */

    /* 日文 */
    /* 注意日文是两个字符宽度,因此是0,2,4,6,8 */
    tb_change_cell(0, 1, 0x3053, TB_CYAN, TB_DEFAULT); /* こ */
    tb_change_cell(2, 1, 0x3093, TB_CYAN, TB_DEFAULT); /* ん */
    tb_change_cell(4, 1, 0x306b, TB_CYAN, TB_DEFAULT); /* に */
    tb_change_cell(6, 1, 0x3061, TB_CYAN, TB_DEFAULT); /* ち */
    tb_change_cell(8, 1, 0x306f, TB_CYAN, TB_DEFAULT); /* は */

    /* 俄文 */
    /* 俄文虽然不是扩展ascii的一部分，但是其只占了一个字符宽度 */
    tb_change_cell(0, 2, 0x0442, TB_CYAN, TB_DEFAULT); /* т */
    tb_change_cell(1, 2, 0x043e, TB_CYAN, TB_DEFAULT); /* о */
    tb_change_cell(2, 2, 0x0432, TB_CYAN, TB_DEFAULT); /* в */
    tb_change_cell(3, 2, 0x0430, TB_CYAN, TB_DEFAULT); /* а */
    tb_change_cell(4, 2, 0x0440, TB_CYAN, TB_DEFAULT); /* р */
    tb_change_cell(5, 2, 0x0438, TB_CYAN, TB_DEFAULT); /* и */
    tb_change_cell(6, 2, 0x0449, TB_CYAN, TB_DEFAULT); /* щ */
    tb_change_cell(7, 2, 0x0438, TB_CYAN, TB_DEFAULT); /* и */


    /*以上展示的是使用原生termbox的API进行字符串输出，可以看到输出一个字符串非常的费劲*/
    /*为此，提供了termbox拓展API，即termbox2*/
    tb_string(0, 3, TB_CYAN, TB_DEFAULT, "你好中国");

    tb_present();

    while(1)
    {
        tb_poll_event(&ev);
        if(ev.key == TB_KEY_CTRL_C)
        {
            break;
        }
    }

    tb_shutdown();
}

#include <finsh.h>
MSH_CMD_EXPORT(tb_language, Termbox language demo)

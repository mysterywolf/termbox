# Termbox for RT-Thread 中文说明文档

## 目前文档不全，只挑重点说，详见'termbox.h'的注释，欢迎提交PR进行补充

Termbox 是在一个Linux下的一个轻量级TUI界面库，使用非常广泛，被封装为Go以及Python等其他语言的库。

现在Termbox也可以在RT-Thread上运行了！Termbox的绘制功能非常简单，大道至简，只提供12个绘制相关的API。但是却有非常强大的输入分析能力，可以分析键盘单键、组合键以及鼠标的点击和滚轮功能，目前这些功能都已经全部可以在RT-Thread上运行。

Termbox的输出字符是采用Unicode编码，即可以支持多语种的绘制，且自动判断字符的宽度。



## Termbox APIs

### 基本

#### int tb_init(void)

初始化Termbox。

#### void tb_shutdown(void)

结束Termbox。

#### int tb_width(void)

获取当前终端窗口的宽

#### int tb_height(void)

获取当前终端窗口的高



### 属性

#### tb_select_input_mode()

`tb_select_input_mode()`是用来设置输入触发模式的，而termbox默认是`TB_INPUT_ESC`模式。如果想要支持鼠标：

```c
tb_select_input_mode(TB_INPUT_ESC | TB_INPUT_MOUSE);
```

#### void tb_set_clear_attributes(uint32_t fg, uint32_t bg)

设定或清除字符的前景、背景色。其中`TB_DEFAULT`表示终端默认颜色。



### 绘制

Termbox的原生内置绘制方案非常的少，只有3个，仅支持最基本的在某个点绘制字符，而且3个里边只有1个是常用的。连画线，打字符串都不支持，这些都需要使用者在应用层自行构造。在Termbox里，一个字符显示位置，称之为cell。简单的理解就是一个cell就是一个字符。

#### void tb_change_cell(int x, int y, uint32_t ch, uint32_t fg, uint32_t bg)

这个函数是termbox三个绘制函数当中的唯一常用函数。这个函数用于显示一个字符在指定的(x,y)屏幕坐标上，ch表示的是字符的内容，要求使用unicode编码，当然ascii的字符直接填里也是可以的；最后两个参数是前景色和背景色。背景色如果采用默认色即`TB_DEFAULT`，表示和终端打开时的背景色一样，例如PuTTY打开的时候就是黑色，那么背景色就保持黑色。

#### void tb_put_cell(int x, int y, const struct tb_cell* cell)

这个函数和`tb_change_cell`函数行为一致，唯一的区别是形参的不同，`tb_cell`结构体实际上就表示了`tb_change_cell`函数中的`ch`、`fg`、`bg`三个参数。

#### void tb_blit(int x, int y, int w, int h, const struct tb_cell* cells)

此函数用的不多，该函数用于填充一个矩形区域，需要创建一个tb_cell结构体的二位数组。一般都是通过for循环直接用`tb_change_cell`函数绘制了。



### 缓冲区

#### void tb_present(void)

在调用上述三个绘制API时，Termbox并不直接将内容绘制到终端界面上，而是先存到缓冲区里，因此需要调用`tb_present()`函数将内容输出到终端上。

#### void tb_clear(void)

该函数用于清除缓冲区的内容。



### 事件等待

#### int tb_poll_event(struct tb_event* event)

永久阻塞等待键盘输入事件、鼠标事件或其他事件。

#### int tb_peek_event(struct tb_event* event, int timeout)

具有超时功能地等待键盘输入事件、鼠标事件或其他事件，单位是ms。



### unicode编码

由于Termbox只支持unicode编码（不是UTF-8、不是ascii），因此需要将我们熟悉的字符转为unicode编码，Termbox提供了两个函数来进行Unicode编码和UTF-8编码的相互转换。其实这两个函数不怎么方便的，因为如果字符的宽度大于1，有些编译器会开始报错（例如在keil编译器下，`char *p = “你” `会直接报错），认为char变量类型装不下。**莫不如到网上找一个unicode转换工具，直接生成unicode编码来的方便**。Termbox就是这个编码的问题不太友好。

#### int utf8_char_to_unicode(uint32_t* out, const char* c)

将一个以UTF-8编码形式的字符（ascii字符或者是ascii拓展字符），注意是一个，转化为unicode编码，从out里以指针的形式返回来。函数返回out数组的长度。因此需要实现准备一个缓冲区给out。这个函数只能处理一个字符，不能处理字符串！

#### int utf8_unicode_to_char(char* out, uint32_t c)

上面的函数的行为反过来。



## Termbox典型的绘制流程流程

```c
tb_init(); // 初始化termbox
tb_clear(); //将屏幕清屏

//绘制图案...
//0x250C 是字符的unicode码，可以填写中文的unicode码来在终端显示中文
tb_change_cell(0, 0, 0x250C, TB_WHITE, TB_DEFAULT); 
tb_change_cell(79, 0, 0x2510, TB_WHITE, TB_DEFAULT);
tb_change_cell(0, 23, 0x2514, TB_WHITE, TB_DEFAULT);
tb_change_cell(79, 23, 0x2518, TB_WHITE, TB_DEFAULT);

tb_present(); // 使用tb_change_cell仅仅是将绘制内容存到了缓冲区，利用该函数才能将绘制的内容显示出来

tb_shutdown(); //结束termbox，会将屏幕绘制的内容清空
```


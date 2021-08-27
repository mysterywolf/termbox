# Termbox for RT-Thread 中文说明文档

## 欢迎提交PR进行补充



## Termbox APIs

### 属性

#### tb_select_input_mode()

`tb_select_input_mode()`是用来设置输入触发模式的，如果使用PuTTY终端，这个函数没什么用，因为PuTTY不支持接收鼠标的信号，而termbox默认就是`TB_INPUT_ESC`模式，所以这个函数可以不用调用。



### 绘制

Termbox的原生内置绘制方案非常的少，只有3个，仅支持最基本的在某个点绘制字符，而且3个里边只有1个是常用的。连画线，打字符串都不支持，这些都需要使用者在应用层自行构造。在Termbox里，一个字符显示位置，称之为cell。简单的理解就是一个cell就是一个字符。

#### void tb_change_cell(int x, int y, uint32_t ch, uint32_t fg, uint32_t bg)

这个函数是termbox三个绘制函数当中的唯一常用函数。这个函数用于显示一个字符在指定的(x,y)屏幕坐标上，ch表示的是字符的内容，要求使用unicode编码，当然ascii的字符直接填里也是可以的；最后两个参数是前景色和背景色。背景色如果采用默认色即`TB_DEFAULT`，表示和终端打开时的背景色一样，例如PuTTY打开的时候就是黑色，那么背景色就保持黑色。

#### void tb_put_cell(int x, int y, const struct tb_cell* cell)

这个函数和`tb_change_cell`函数行为一致，唯一的区别是形参的不同，`tb_cell`结构体实际上就表示了`tb_change_cell`函数中的`ch`、`fg`、`bg`三个参数。

#### void tb_blit(int x, int y, int w, int h, const struct tb_cell* cells)

此函数用的不多，该函数用于填充一个矩形区域，需要创建一个tb_cell结构体的二位数组。一般都是通过for循环直接用`tb_change_cell`函数绘制了。



### 事件等待

#### int tb_poll_event(struct tb_event* event)

永久阻塞等待键盘输入事件、鼠标事件或其他事件。

#### int tb_peek_event(struct tb_event* event, int timeout)

具有超时功能地等待键盘输入事件、鼠标事件或其他事件，单位是ms。



## Termbox典型的绘制流程流程

```c
tb_init(); // 初始化termbox
tb_clear(); //将屏幕清屏

//绘制图案...
//0x250C 是字符的unicode码，可以填写中文的unicode码来在中断显示中文
tb_change_cell(0, 0, 0x250C, TB_WHITE, TB_DEFAULT); 
tb_change_cell(79, 0, 0x2510, TB_WHITE, TB_DEFAULT);
tb_change_cell(0, 23, 0x2514, TB_WHITE, TB_DEFAULT);
tb_change_cell(79, 23, 0x2518, TB_WHITE, TB_DEFAULT);

tb_present(); // 使用tb_change_cell仅仅是将绘制内容存到了缓冲区，利用该函数才能将绘制的内容显示出来

tb_shutdown(); //结束termbox，会将屏幕绘制的内容清空
```


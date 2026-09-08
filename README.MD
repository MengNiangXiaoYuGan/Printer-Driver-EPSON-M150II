# 基于zephyr驱动库的EPSON M150II 针式点阵打印机的驱动库
## 简述
 - 本仓库内容为基于zephyr的一种针式打印机的驱动文件，该文件挂载于DISPLAY API下，基础使用方式与DISPLAY子系统的用法一致，claer api被用来作为空行移进，使用的时候需要注意
 - 该驱动文件有以下特点
   - 1. 基于[zephyr](https://docs.zephyrproject.org/latest/index.html)框架，可以使用设备树初始化硬件，menuconfig进行程序裁剪以及一些相关设置
   - 2. 该驱动只使用到GPIO组以及GPIO中断，可以兼容一些功能比较贫瘠的控制器
   - 3. 该驱动具有一个基于周期的分频算法，当用户设计的打点触发电路输出的波形不是很美观，但又想测试电路的时候，可以使用该功能进行测试，但在实际中，请尽量使用数据手册上标注的触发方式进行打点作业
   - 4. 该驱动上层为Display子系统，用户可以像使用一款无限长度的屏幕一样使用该驱动
   - 5. 该驱动不需要内部缓冲区，通过坐标运算进行外部缓冲区的映射对齐
## 添加驱动到你的设备
 ### 了解zephyr
 - zephyr是一个开源的嵌入式操作系统，但同时具有设备树配置以及十分强大的驱动层-用户层分离，内核支持多种架构，用户函数极易移植，并且十分方便协同编程。
 - 具体请看 [zephyr introduction](https://docs.zephyrproject.org/latest/introduction/index.html)
 - 本仓库需要使用的api以及其他拓展内容请看 [zephyr display subsystem](https://docs.zephyrproject.org/latest/hardware/peripherals/display/index.html)
 - zephyr是一个十分具有前景的嵌入式操作系统，独特的设备树以及绑定文件实现驱动程序以及用户函数分离，用户可以使用特定的api操作任何抽象化的设备，而这些设备可以由另外的人进行声明或者编写驱动函数，这意味着用户代码端可以完全不在乎驱动程序是如何运行的，抽象的api机制也让大部分的用户函数极易移植到另外的设备上（只需要重新编写一份`conf`以及设备树文件`overlay`），如果用户此前有过嵌入式Linux编程经验，zephyr会十分容易入门，但如果用户只拥有mcu裸机编程能力或者仅仅只接触过freeRTOS,则该操作系统的学习曲线会稍微有些陡峭，但相信我，这值得，zephyr将是之后物联网嵌入式设备的新选择甚至可以是最优选择。
 ### 拉取仓库
 - 找一个风水宝地，把仓库拉下来，使用`git clone https://github.com/MengNiangXiaoYuGan/Printer-Driver-EPSON-M150II.git`
 ### CmakeLists
 - 要添加驱动，首先需要在项目`CmakeLists`添加路径,在你的`CmakeLists.txt`里，`find_package`前，添加`set(ZEPHYR_EXTRA_MODULES "your_path/Printer-Driver-EPSOM-M150II")`,或者用户可能更喜欢更换个其他的仓库名，也一样的
 ### DeviceTree
 - 添加驱动目录到`CmakeLists`后，驱动就可以编译了，但是用户仍然需要在设备树文件中声明打印机所使用的引脚才可以正常运行，用户可能需要阅览一下设备树的[绑定文件](./dts/bindings/printer/epson,m150ii.yaml)。
   ~~~~~
    以下是一个简单的设备树实例
    / {
        m150ii: m150ii {
            compatible = "epson,m150ii";
            reset-gpios = <&gpio0 10 (GPIO_PULL_UP | GPIO_ACTIVE_LOW)        timing-gpios = <&gpio0 9 (GPIO_PULL_DOWN | GPIO_ACTIVE_HIGH)>;
            motor-gpios = <&gpio0 21 (GPIO_PULL_DOWN | GPIO_ACTIVE_HIGH)>;
            print-d-gpios = <&gpio0 14 (GPIO_PULL_DOWN | GPIO_ACTIVE_HIGH)>;
            print-c-gpios = <&gpio0 13 (GPIO_PULL_DOWN | GPIO_ACTIVE_HIGH)>;
            print-b-gpios = <&gpio0 12 (GPIO_PULL_DOWN | GPIO_ACTIVE_HIGH)>;
            print-a-gpios = <&gpio0 11 (GPIO_PULL_DOWN | GPIO_ACTIVE_HIGH)>;
            status = "okay";
        };
    };
    请注意，设备引脚的上下拉以及高低电平有效都是有意义的，上下拉取决于用户的驱动电路板所需要的上下拉状态，建议在上下拉的时候确认该方式不会一设置引脚后就会启动某个部件
        特别是打印头，该型号的打印针运行的时候，单根打印针就能吃掉1.25A的电流，所以在上下拉的时候一定要注意，不要再一上电就把打印针弹出来
    至于有效电平，有效电平取决于用户的驱动板电路以及数据手册的时序图，假设时序图中该针脚为高电平启动（触发）而用户的电路做了反向处理（比如使用了pmos），则该引脚需要设置成
        GPIO_ACTIVE_LOW，以确保驱动程序输入1的时候，该引脚会被启动（触发），输入引脚也同理
   ~~~~~
 ### Menuconfig
   - 在设备树添加设备以及驱动程序后，就可以尝试编译了，虽然第一次编译估计会报错，这是因为给驱动程序依赖`Display`子系统，用户需要在项目目录下的`prj.conf`文件中添加以下内容
   ~~~~~
     CONFIG_DISPLAY
     CONFIG_EPSON_M150II
   ~~~~~
   - 第一个设置用于启动Display子系统，
   - 第二个设置用于包含M150的驱动程序
 ### 使用
 - 现在，用户的项目里就已经包含了最基础的驱动程序，用户可以选择`west build -t menuconfig`然后在`modules/m150ii/`中了解以及启动一些进阶功能，也可以直接使用
 - 使用设备的方式很简单，设备挂载在`Display`子系统下，所以首先要做的是包含子系统文件
   ~~~~~
    #include <zephyr/drivers/display.h>
   ~~~~~
 - 然后获取设备句柄
   ~~~~~
    const static struct device * dev = DEVICE_DT_GET(your_node_id);
   ~~~~~
 - 按照zephyr官方推荐的程序逻辑，接下来我们需要验证该设备是否可用
   ~~~~~
    if (!device_is_ready(dev))
    {
        /err_code/
    }
   ~~~~~
- 终于我们来到激动人心的绘画环节，由于该驱动当前没有y轴留白（之后会加的咕咕），所以y起点值是无用的数值，具体的有效值如下方所示
   ~~~~~
    struct display_buffer_descriptor desc = {
        .buf_size = ,      //有效，该数值应当是你buff的大小（用于计算实际需要写入的区域大小）
        .frame_incomplete = false,  //无效，该数值无论设置为多少，对驱动不会有影响
        .width = 96,        //有效，该数值为写入区域的宽度
        .pitch = 96,        //有效，该数值应当是一行buff的大小，像素值，假设width为95，意味着一行有95个点需要写入，但是buff是八位对齐的，所以pitch应当为96（12个字节一行）
        .height = 148,      //有效，该数值为写入区域的行数加上留白行数
    };
    display_write(dev,
                    0,  //X值，该值于width字段共同构成横向限制，剩余的空位会自动填充
                    0,  //y值，该值如果大于0，则会在0到y之间留白
                    &desc,  //
                    your_buff_ptr); //用户缓冲区，需要进行显示的文件
    PS: 留白这个词需要注意一下，留白并不是什么都不做，而是逻辑上的0，假设用户启动了反色（PIXEL_FORMAT_MONO_10），则留白的部分全部会被打点，点的颜色与色带颜色一致
   ~~~~~
 - 一个简单的图例可能可以更好理解上述意思
   ~~~~~
        首先需要明确的是，针式打印机与屏幕不一样，最大的缺点就是不支持局刷，每一次调用写入函数，
        打印头行头都成为一个新的x0和y0，于是写入函数中的x和y字段就用来进行留白，这样可以很方便地定义上下边距以及侧边距

                            0,0             x(buff zero)          x+width             95
                                ---------------------------------------------------------
                                ---------------------------------------------------------
                                ---------------------------------------------------------
                 y(buff zero)   ------------++++++++++++++++++++++++++-------------------
                                ------------++++++++++++++++++++++++++-------------------
                                ------------++++++++++++++++++++++++++-------------------
                                ------------++++++++++++++++++++++++++-------------------
                                ------------++++++++++++++++++++++++++-------------------
            buf_size/(pitch/8)  ------------++++++++++++++++++++++++++-------------------
                                ---------------------------------------------------------
                                ---------------------------------------------------------
                     y+height   ---------------------------------------------------------

        加号 （+） 所在区域 为传入的buff，驱动程序内部会将这个buff按照上述方式进行映射，映射的规范是点对齐，所以不会对x以及y有对齐限制了，但是buff大小必定为8位对齐
        所以需要使用pitch字段声明一行有几个buff字节（使用像素值）
        而y值大于零的情况下，小于y的部分则是留白，底部的留白则由buffsize和height字段共同限制，height字段限制画幅大小，而真正需要写入的部分则由buff大小确认
   ~~~~~
 - 需要注意的是，设备的buff为大端序，从打印机正面看，纸的左上角（打印机出口处，或者说第一根针落下的位置）为x0，y0，x向右递增，y向下递增，先行后列，并且一位为一个像素，在这种格式下，该驱动仅支持`PIXEL_FROMAT_MONO01`以及`PIXEL_FORMAT_MONO10`,前者像素位为`1`的时候有颜色（打点），后者像素位为`0`的时候打点（反色），用户可以通过`display_set_pixel_format(dev, PIXEL_FORMAT_MONO10)`或者`display_set_pixel_format(dev, PIXEL_FORMAT_MONO01)`在程序中动态设置反色，该设置会从下一次调用`display_write`生效。
 - 假设有一组数据为`0b1000 1100 0011 0001`,则该数据打印出的效果如下
   ~~~~~
    0b 1000 1100 0011 0001
    0x8b 0x31
    PIXEL_FROMAT_MONO01   bwww bbww wwbb wwwb
    PIXEL_FORMAT_MONO10   wbbb wwbb bbww bbbw
   ~~~~~
 - 当然，用户使用了蓝色的色带，打点出来的颜色自然也会是蓝色而不是黑色，选择是多种多样的
 - 如果用户启动了设置`PRINTER_FRAME_FORMAT_REVERSED`，在menuconfig里它的名称是`printer frame format reversed`，则会让buff的大端序变成小端序，关于这点，下方的示例应该可以帮助用户理解
   ~~~~~
    buff元素      [1]     [2]     [3]     [4]
                 0x01    0x02    0x04    0x08
                0b0001  0b0010  0b0100  0b1000
    大端序       wwwb    wwbw     wbww    bwww
    小端序       bwww    wbww     wwbw    wwwb

   打印点（大端   wwwbwwbwwbwwbwww
   打印点（小端   bwwwwbwwwwbwwwwb
   ~~~~~
 - 请根据自己所使用的缓冲buff格式选择合适的帧格式端序
  
 ## 更新内容
  ###  2026年9月8日更新
  1. 修改写入算法，抛弃内部buff，使用直接点对点映射 
  2. 添加大小端序支持
  3. 添加信号超时响应，现在设备不会光吐纸不打印了，如果一秒钟内没能获取触发信号（res或者tri），设备就会返回timeout错误
  4. 添加了EPSON M150II 的[数据手册](./doc/M150-II.pdf)

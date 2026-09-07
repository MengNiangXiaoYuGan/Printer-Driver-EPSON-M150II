# 基于zephyr驱动库的EPSON M150II 针式点阵打印机的驱动库
## 简述
 - 本仓库内容为基于zephyr的一种针式打印机的驱动文件，该文件挂载于DISPLAY API下，基础使用方式于DISPLAY子系统的用法一致，claer api被用来作为空行移进，使用的时候需要注意
 - 该驱动文件有以下特点
   - 1. 基于zephyr框架，可以使用设备树初始化硬件，menuconfig进行程序裁剪以及一些相关设置
   - 2. 该驱动只使用到GPIO组以及GPIO中断，可以兼容一些功能比较贫瘠的控制器
   - 3. 该驱动具有一个基于周期的分频算法，当用户设计的打点触发电路输出的波形不是很美观，但又想测试电路的时候，可以使用该功能进行测试，但在实际中，请尽量使用数据手册上标注的触发方式进行打点作业
   - 4. 该驱动上层为Display子系统，用户可以像使用一款无限长度的屏幕一样使用该驱动
   - 5. 该驱动内部具有一个额外的缓冲区，用以搭载之后会加入的不对齐显示方式（当前输入的buff x向起始点及长度必须八位对齐，之后会修改这一部分）
   - 6. 之后会考虑取消内部的缓冲区，以兼容更加低端的控制器
## 添加驱动到你的设备
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
   ~~~~~~
    #include <zephyr/drivers/display.h>
   ~~~~~~
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
        .buf_size = ,      //有效，该数值应当是你buff的大小（虽然并不会进行校验）
        .frame_incomplete = false,  //无效，该数值无论设置为多少，对驱动不会有影响
        .width = 96,        //有效，该数值应当为0或者8的倍数
        .pitch = 96,        //有效，该数值应当与width字段一致
        .height = 148,      //有效，该数值为行数
    };
    display_write(dev,
                    0,  //X值，该值于width字段共同构成横向限制，剩余的空位会由驱动内部的缓冲区填充
                    0,  //y值，当前无意义，可以任意输入数值
                    &desc,  //
                    your_buff_ptr); //用户缓冲区，需要进行显示的文件
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


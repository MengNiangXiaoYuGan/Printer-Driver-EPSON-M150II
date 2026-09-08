/*
 *  Custom DIYer
 *
 * SPX-License-Identifer: Apache-2.0
*/

#define DT_DRV_COMPAT epson_m150ii

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

#include "m150ii.h"

/***************************************************************************************************************************************************************** */
/*define and regiest*/
/*enable logging at CONFIG_EPSON_M150II_LOG_LEVEL*/
LOG_MODULE_REGISTER(M150II, CONFIG_EPSON_M150II_LOG_LEVEL);

/*define an private function*/
struct m150ii_gpio_config{
    struct gpio_dt_spec res_d,motor_c,ps_d,ps_c,ps_b,ps_a,tim_d;
    const int start_delay,zero_tri_delay;
};
struct m150ii_gpio_data{
    /*use for privare and chansmit data*/
    enum display_pixel_format pixel_format;
    /*set for timing trigger*/
    const struct device *dev;
    struct gpio_callback res_d_callback,tim_d_callback;
    /*use for bool*/
    bool res_tri;
    /*use for thread*/
#ifdef CONFIG_PRINTER_PREDICTION_TIMING
    k_timeout_t printer_delay;
    uint16_t last_time;
#else
    bool timing_tri;
#endif
};

/******************************************************************************************************************************************************************* */
/*callback fuction*/

/*
    @brief reset protector gpio call back
*/
static void m150ii_printer_res_d_callback(const struct device *dev,
                            struct gpio_callback *cb,
                            uint32_t pins)
{
    struct m150ii_gpio_data *data = 
        CONTAINER_OF(cb, struct m150ii_gpio_data, res_d_callback);
    ARG_UNUSED(pins);
    ARG_UNUSED(dev);
    data->res_tri = true;
}

/*
    @brief timing protector gpio call back
*/
static void m150ii_printer_timing_d_callback(const struct device *dev,
                            struct gpio_callback *cb,
                            uint32_t pins)
{
    struct m150ii_gpio_data *data = 
        CONTAINER_OF(cb, struct m150ii_gpio_data, tim_d_callback);
    ARG_UNUSED(pins);
    ARG_UNUSED(dev);
#ifdef CONFIG_PRINTER_PREDICTION_TIMING
    /*test 3 use edeg to define 1/2 timing*/
    uint16_t that_time = sys_clock_tick_get();
    int64_t time_delta = 0;
    if (that_time > data->last_time)
    {
        time_delta = that_time - data->last_time;
    }
    else
    {
        time_delta = (UINT64_MAX - data->last_time) + that_time;
    }
    time_delta /= 2;
    data->printer_delay = K_TICKS(time_delta);
    data->last_time = that_time;
#else
    data->timing_tri = true;
#endif
}

/*********************************************************************************************************************************************************************************** */
/*private function*/
static int m150ii_printer_init(const struct device *dev)
{
    const struct m150ii_gpio_config *cfg = (const struct m150ii_gpio_config *)dev->config;
    if (NULL == cfg)
    {
        LOG_ERR("config filel is not exit");
        return -ENODEV;
    }
    struct m150ii_gpio_data *data = (struct m150ii_gpio_data *)dev->data;
    if (NULL == data)
    {
        LOG_ERR("config filel is not exit");
        return -ENODEV;
    }
    data->dev = dev;
    LOG_DBG("system now will init gpio");
    /*check init all gpio pin*/
    if (!gpio_is_ready_dt(&cfg->motor_c))
    {
        LOG_ERR("motor contrl pin not ready");
        return -ENODEV;
    }
    if (!gpio_is_ready_dt(&cfg->ps_a))
    {
        LOG_ERR("printer solenid a pin not ready");
        return -ENODEV;
    }
    if (!gpio_is_ready_dt(&cfg->ps_b))
    {
        LOG_ERR("printer solenid b pin not ready");
        return -ENODEV;
    }
    if (!gpio_is_ready_dt(&cfg->ps_c))
    {
        LOG_ERR("printer solenid c pin not ready");
        return -ENODEV;
    }
    if (!gpio_is_ready_dt(&cfg->ps_d))
    {
        LOG_ERR("printer solenid d pin not ready");
        return -ENODEV;
    }
    if (!gpio_is_ready_dt(&cfg->res_d))
    {
        LOG_ERR("reset detector pin not ready");
        return -ENODEV;
    }
    if (!gpio_is_ready_dt(&cfg->tim_d))
    {
        LOG_ERR("timing detector pin not ready");
        return -ENODEV;
    }
    LOG_DBG("gpiopin is now rady");
    /*config all gpio pin*/
    int ret = gpio_pin_configure_dt(&cfg->motor_c,GPIO_OUTPUT_INACTIVE);
    if (ret)
    {
        LOG_ERR("can not configura motor contrl pin, error code %d",ret);
        return -ENODEV;
    }
    ret = gpio_pin_configure_dt(&cfg->ps_a,GPIO_OUTPUT_INACTIVE);
    if (ret)
    {
        LOG_ERR("can not configura printer solenoid a pin, error code %d",ret);
        return -ENODEV;
    }
    ret = gpio_pin_configure_dt(&cfg->ps_b,GPIO_OUTPUT_INACTIVE);
    if (ret)
    {
        LOG_ERR("can not configura printer solenoid b pin, error code %d",ret);
        return -ENODEV;
    }
    ret = gpio_pin_configure_dt(&cfg->ps_c,GPIO_OUTPUT_INACTIVE);
    if (ret)
    {
        LOG_ERR("can not configura printer solenoid c pin, error code %d",ret);
        return -ENODEV;
    }
    ret = gpio_pin_configure_dt(&cfg->ps_d,GPIO_OUTPUT_INACTIVE);
    if (ret)
    {
        LOG_ERR("can not configura printer solenoid d pin, error code %d",ret);
        return -ENODEV;
    }
    ret = gpio_pin_configure_dt(&cfg->res_d,GPIO_INPUT);
    if (ret)
    {
        LOG_ERR("can not configura reset detector pin, error code %d",ret);
        return -ENODEV;
    }
    ret = gpio_pin_configure_dt(&cfg->tim_d,GPIO_INPUT);
    if (ret)
    {
        LOG_ERR("can not configura timing detector pin, error code %d",ret);
        return -ENODEV;
    }
    LOG_DBG("gpio configura is ok");
    /*init exit call back*/
    gpio_init_callback(&data->res_d_callback,
                    m150ii_printer_res_d_callback,
                    BIT(cfg->res_d.pin));
    gpio_init_callback(&data->tim_d_callback,
                    m150ii_printer_timing_d_callback,
                    BIT(cfg->tim_d.pin));
    LOG_DBG("exit callback init compliet");
    if (gpio_add_callback(cfg->res_d.port, &data->res_d_callback) < 0)
    {
        LOG_ERR("reset pin add call back faile");
        return -ENOSYS;
    }
    if (gpio_add_callback(cfg->tim_d.port, &data->tim_d_callback) < 0)
    {
        LOG_ERR("reset pin add call back faile");
        return -ENOSYS;
    }
    LOG_DBG("EXIT callback add ok");
    /*config ok*/
    LOG_INF("Printer Init complet");
    return 0;
}

/*
    @brief this fuction use for check triger time out
    @param time_out triger
    @return 0 for sussec
            -ETIMEDOUT for timeout
*/
static inline int check_triger_timeout (bool *triger_ptr)
{
    long couter = 0;
    *triger_ptr = false;
    while (!*triger_ptr)
    {
        /*time out init*/
        k_sleep(K_USEC(5));
        couter++;
        /*one seconed timeout*/
        if (couter > 2e5)
        {
            LOG_ERR("check triger time out");
            return -ETIMEDOUT;
        }
    }
    return 0;
}

/*
    @brief this function use for get buff off a coodinate form offset
    @details
        when buff not global, we have to face cut a small area form all page
        --------------------------------------
        --------------+++++++++++++++---------
        --------------------------------------
        in that time, program will check where x coodinate here, and how offset
        buff use, to get right bit print
    @param
        x_coordinate  x axis coordinate
        x_offset      x axis coordinate of buff zero
        y_coordinate  y axis coordinate
        y_offset      y axis coordinate of buff zero
        buff_width    one line buff width (bytes)
        buff_prt      buff point
    @return
        false   not to pin
        true    pin
*/
static inline bool check_buff_bit (uint8_t x_coordinate, uint8_t x_offset, uint16_t y_coordinate, uint16_t y_offset, const uint8_t buff_width, uint8_t *buff_ptr)
{
    /*not check buff beyond boundary, if beyond ,system will crash*/
    uint8_t xs = x_coordinate - x_offset;
    uint16_t ys = y_coordinate - y_offset;

#if defined(CONFIG_PRINTER_FRAME_FORMAT_REVERSED)
    /*if won to use low byte first*/
    uint8_t bit_offset = xs % 8;
#else
    /*all firme is Big-endian*/
    uint8_t bit_offset = 7 - (xs % 8);
#endif // CONFIG_PRINTER_FRAME_FORMAT_REVERSED
    /*buff coordinate equal buff_ptr [(ys * buff_width) + (xs / 8)] >> (bit offset)*/
    if (buff_ptr[((ys * buff_width) + (xs / 8))] >> bit_offset & 0x1)
    {
        return true;
    }
    return false;
}

/********************************************************************************************************************************************************************* */
/*API Fnction*/
/*
    @brief this function use for write buff to printer ,this diffrent to nomal display
            it cant replace drowing area
            all write will drow new area below laset drow,
    @details
            there are some diffrent to nomall display fuction, dot printer is an creent
            printer, it mean you can not use Partial refresh to this "screen"
            every use write function will "create" a new screen to use
            so, the x 0 and y 0 not fix, a simple page, jues like this

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

            the buff have to align to 8bit, but x or width or both dos not align that
            in this situation, desc->pitch must be align to 8bit, one line buff size 
            use this to count.
            in this case, line num is desc->buf_size/pitch/8, printer will start form
            0,0 ,but user can use buff smaller then all page
            such as dorw area width is 15 pixel,not align to 8bit,but buff must use uint_8
            so, desc->pich must be set to 16, to mark the oneline buff have 2 bytes
    @param

    @return
*/
static int m150ii_printer_write(const struct device *dev,
                            const uint16_t x,
                            const uint16_t y,
                            const struct display_buffer_descriptor *desc,
                            const void *buf)
{  
    /*check data ok*/
    LOG_DBG("START write and driver printer use");
    int ret = 0;
    /*pitch must be align 8 and bigger than width*/
    if (desc->pitch % 8 || desc->pitch < desc->width)
    {
        LOG_ERR("drow desc error, pitch illegal, pitch = %d, widtch = %d",desc->pitch,desc->width);
        return -ENOTSUP;
    }
    /*whidth and pitch not bigger then max cow*/
    if (desc->width > 96 || desc->pitch > 96)
    {
        LOG_ERR("witch %d or pitch %d can not use",desc->width,desc->pitch);
        return -ENOTSUP;
    }
    /*cow drow boundary can not beyond page size*/
    if (x < 0 || y < 0 || x + desc->width > 96)
    {
        LOG_ERR("drow boundary beyond page size, x = %d, y = %d,width = %d",x,y,desc->width);
        return -ENOTSUP;
    }
    /*verfy buff size and x width*/
    if (desc->width == 0 || desc->buf_size == 0)
    {
        LOG_ERR("buff can not empty,or width is 0, width = %d, buf_size = %d",desc->width, desc->buf_size);
        return -ENOTSUP;
    }
    LOG_DBG("data check ok");
    struct m150ii_gpio_data *data = (struct m150ii_gpio_data *)dev->data;
    const struct m150ii_gpio_config *cfg = (const struct m150ii_gpio_config *)dev->config;
    
    /*regest gpio external intrrput callback*/
    ret = gpio_pin_interrupt_configure_dt(&cfg->res_d,
                                GPIO_INT_EDGE_TO_ACTIVE);
    if(ret) 
    {
        LOG_ERR("gpio res interrrupt configure faile ,code %d",ret);
        goto exit;
    }
    ret = gpio_pin_interrupt_configure_dt(&cfg->tim_d,
#ifdef CONFIG_PRINTER_PREDICTION_TIMING
                                GPIO_INT_EDGE_TO_ACTIVE);
#else
                                GPIO_INT_EDGE_BOTH);
#endif
    if(ret) 
    {
        LOG_ERR("gpio timing interrrupt configure faile ,code %d",ret);
        goto exit;
    }
    LOG_DBG("gpio interrupt configure ok");
    /*statrt printer*/
    int16_t line = 0,row = 0;
    uint8_t format = 0;
    /*init buf ptr*/
    uint8_t *buf_ptr = (uint8_t *)buf;
    /*get print buf lin num*/
    const uint8_t cow_num = desc->pitch / 8;
    const uint16_t line_num = desc->buf_size / cow_num;
    switch (data->pixel_format)
    {
    case PIXEL_FORMAT_MONO01:
        format = 1;
        break;
    case PIXEL_FORMAT_MONO10:
        format = 0;
        break;
    default:
        LOG_ERR("pixel format steup error");
        goto exit;
        break;
    }
    ret = gpio_pin_set_dt(&cfg->motor_c,1);
    if (ret)
    {
        LOG_ERR("cannot set motor_c pin,error code %d",ret);
        goto exit;
    }
    LOG_DBG("set motor on");
    k_sleep(K_MSEC(cfg->start_delay));/*STATRT DELAY*/
    LOG_DBG("clear res tiger msgq");
    for (line = 0; line < y + desc->height; line++)
    {
        /*in one line end, check all pin must take back, otherwise, pin will swipe across the paper, then break it*/
        /*it also will consumes a lot of current*/
        /*so, check it, very very important*/
        ret |= gpio_pin_set_dt(&cfg->ps_a,0);
        ret |= gpio_pin_set_dt(&cfg->ps_b,0);
        ret |= gpio_pin_set_dt(&cfg->ps_c,0);
        ret |= gpio_pin_set_dt(&cfg->ps_d,0);
        if (ret)
        {
            LOG_ERR("cant not set gpio pin");
            goto exit;
        }
        if (check_triger_timeout(&data->res_tri))goto exit;
        k_sleep(K_USEC(cfg->zero_tri_delay));    /*ENTER DELAY*/
        for (row = 0; row < 96; row++)
        {
            uint8_t pin_or_not = 0;
            /*line is one by one working, but cow dosen*/
            /*so, for use not buff use, we must be check where pin x coodinate now*/
            uint8_t z = row % 4;
            uint8_t x_coordinate = (((z) * 24) + ((row / 4) % 24));
            /*in that time, program will check this coordinate in buf or not*/
            /*desc struct define x and y, this is buff start*/
            /*but not in buffs data, will choice pixel frime,if mono01*/
            /*do nothing, but if mono10, will print color to paper*/
            /*check this coordinate on buff*/
            if ((x_coordinate >= x && x_coordinate < (desc->width + x)) && line >= y && line <(line_num))
            {
                /*in buff*/
                pin_or_not = check_buff_bit(x_coordinate,x,line,y,cow_num,buf_ptr) ? format : (!format) & 0x1;
            }
            else
            {
                /*not in buff*/
                pin_or_not = (!format) & 0x1;
            }
            LOG_DBG("pin_or_not = %d line = %d row = %d x_coordinate =%d",pin_or_not,line,row,x_coordinate);
#if !defined(CONFIG_PRINTER_PREDICTION_TIMING)
            if (check_triger_timeout(&data->timing_tri))goto exit;
#endif // 
            switch (z)
            {
            case 0:
                gpio_pin_set_dt(&cfg->ps_a,pin_or_not);
                gpio_pin_set_dt(&cfg->ps_b,0);
                gpio_pin_set_dt(&cfg->ps_c,0);
                gpio_pin_set_dt(&cfg->ps_d,0);
                break;
            case 1:
                gpio_pin_set_dt(&cfg->ps_a,0);
                gpio_pin_set_dt(&cfg->ps_b,pin_or_not);
                gpio_pin_set_dt(&cfg->ps_c,0);
                gpio_pin_set_dt(&cfg->ps_d,0);
                break;
            case 2:
                gpio_pin_set_dt(&cfg->ps_a,0);
                gpio_pin_set_dt(&cfg->ps_b,0);
                gpio_pin_set_dt(&cfg->ps_c,pin_or_not);
                gpio_pin_set_dt(&cfg->ps_d,0);
                break;
            case 3:
                gpio_pin_set_dt(&cfg->ps_a,0);
                gpio_pin_set_dt(&cfg->ps_b,0);
                gpio_pin_set_dt(&cfg->ps_c,0);
                gpio_pin_set_dt(&cfg->ps_d,pin_or_not);
                break;
            default:
                LOG_ERR("math error, check program");
                goto exit;
                break;
            }
            /*delay time until next triger*/
#ifdef CONFIG_PRINTER_PREDICTION_TIMING
            k_sleep(data->printer_delay);
#endif
        }
        /*one line last point*/
        if (check_triger_timeout(&data->timing_tri))goto exit;
        /*line form 0 to height*/
    }
    if (check_triger_timeout(&data->res_tri))goto exit;
    /*clean handle*/
    LOG_INF("write ok");
    exit:
    ret |= gpio_pin_set_dt(&cfg->motor_c,0);
    ret |= gpio_pin_set_dt(&cfg->ps_a,0);
    ret |= gpio_pin_set_dt(&cfg->ps_b,0);
    ret |= gpio_pin_set_dt(&cfg->ps_c,0);
    ret |= gpio_pin_set_dt(&cfg->ps_d,0);
    ret |= gpio_pin_interrupt_configure_dt(&cfg->res_d,
                                    GPIO_INT_DISABLE);
    ret |= gpio_pin_interrupt_configure_dt(&cfg->tim_d,
                                    GPIO_INT_DISABLE);
    return ret;
}

/*
    @brief this function use for change pixel format
            youcan change format to black or white use
            background
    @param
    @return
*/
static int m150ii_printer_set_pixel_format(const struct device *dev,
                            const enum display_pixel_format pixel_format)
{
    struct m150ii_gpio_data *data = (struct m150ii_gpio_data *)dev->data;
    switch (pixel_format)
    {
    case PIXEL_FORMAT_MONO01:
        /*write 0 black 1*/
        data->pixel_format = PIXEL_FORMAT_MONO01;
        break;
    case PIXEL_FORMAT_MONO10:
        /*inver*/
        data->pixel_format = PIXEL_FORMAT_MONO10;
        break;
    default:
        LOG_INF("pixel format inver");
        return -ENOTSUP;
        break;
    }
    return 0;
}

/***************************************************************************************************************************************************************** */
/*api bindding*/
const static DEVICE_API(display, m150ii_printer_driver_api) = {
    .write = m150ii_printer_write,
    .set_pixel_format = m150ii_printer_set_pixel_format,
};

/*Expansion macro magic*/
#define M150II_PRINTER_DEFINE(inst)                                                     \
    static struct m150ii_gpio_data m150ii_gpio_data_##inst = {                          \
        .pixel_format = PIXEL_FORMAT_MONO01,                                            \
    };                                                                                  \
                                                                                        \
    /*create an instance of the consig struct, populate with DT calues*/                \
    static const struct m150ii_gpio_config m150ii_gpio_config_##inst = {                \
        .motor_c    = GPIO_DT_SPEC_INST_GET(inst, motor_gpios),                         \
        .res_d      = GPIO_DT_SPEC_INST_GET(inst, reset_gpios),                         \
        .ps_a       = GPIO_DT_SPEC_INST_GET(inst, print_a_gpios),                       \
        .ps_b       = GPIO_DT_SPEC_INST_GET(inst, print_b_gpios),                       \
        .ps_c       = GPIO_DT_SPEC_INST_GET(inst, print_c_gpios),                       \
        .ps_d       = GPIO_DT_SPEC_INST_GET(inst, print_d_gpios),                       \
        .tim_d      = GPIO_DT_SPEC_INST_GET(inst, timing_gpios),                        \
        .start_delay = DT_INST_PROP_OR(inst, start_delay, 50),                          \
        .zero_tri_delay = DT_INST_PROP_OR(inst, zero_tiger_delay, 20)                   \
    };                                                                                  \
                                                                                        \
    /*create a device instance form devicetree node indetifier and*/                    \
    /*registers the init fuction to run during boot*/                                   \
    DEVICE_DT_INST_DEFINE(inst,                                                         \
                                &m150ii_printer_init,                                   \
                                NULL,                                                   \
                                &m150ii_gpio_data_##inst,                               \
                                &m150ii_gpio_config_##inst,                             \
                                POST_KERNEL,                                            \
                                CONFIG_DISPLAY_INIT_PRIORITY,                           \
                                &m150ii_printer_driver_api);

/*this micro use for init and define all strcut from DT*/
DT_INST_FOREACH_STATUS_OKAY(M150II_PRINTER_DEFINE)

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
LOG_MODULE_REGISTER(M150II, 4);

/*define an private function*/
struct m150ii_gpio_config{
    struct gpio_dt_spec res_d,motor_c,ps_d,ps_c,ps_b,ps_a,tim_d;
    const int start_delay;
};
struct m150ii_gpio_data{
    /*use for privare and chansmit data*/
    enum display_pixel_format pixel_format;
    /*set for timing trigger*/
    const struct device *dev;
    struct gpio_callback res_d_callback,tim_d_callback;
    /*use for msgq*/
    struct k_msgq res_tri_msgq,tim_tri_msgq;
    char res_tri_msgq_buff[sizeof(bool)];
    char tim_tri_msgq_buff[sizeof(bool)];
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
    bool tri = 1;
    if (k_msgq_put(&data->res_tri_msgq,(void *)tri,K_NO_WAIT))
    {
        k_msgq_purge(&data->res_tri_msgq);
        k_msgq_put(&data->res_tri_msgq,(void *)tri,K_NO_WAIT);
    }
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
    bool tri = 1;
    if (k_msgq_put(&data->tim_tri_msgq,(void *)tri,K_NO_WAIT))
    {
        k_msgq_purge(&data->tim_tri_msgq);
        k_msgq_put(&data->tim_tri_msgq,(void *)tri,K_NO_WAIT);
    }
}

/*********************************************************************************************************************************************************************************** */
/*private function*/
static int m150ii_printer_init(const struct device *dev)
{
    struct m150ii_gpio_config *cfg = (struct m150ii_gpio_config *)dev->config;
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
    return 0;
}

/********************************************************************************************************************************************************************* */
/*API Fnction*/
/*
    @brief this function use for write buff to printer ,this diffrent to nomal display
            it cant replace drowing area
            all write will drow new area below laset drow,
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
    if (x && x % 8)
    {
        LOG_ERR("pxiel x %d ilegal, x have to algin to 8",x);
        return -ENOTSUP;
    }
    if (desc->width > 96 || desc->pitch > 96)
    {
        LOG_ERR("witch %d or pitch %d can not use",desc->width,desc->pitch);
        return -ENOTSUP;
    }
    if (desc->width % 8 && desc->pitch % 8)
    {
        LOG_ERR("witch %d or pitch %d not aligin 8",desc->width,desc->pitch);
    }
    LOG_DBG("data check ok");
    /*create new data buff*/
    uint8_t buff[12][desc->height];
    struct m150ii_gpio_data *data = (struct m150ii_gpio_data *)dev->data;
    struct m150ii_gpio_config *cfg = (struct m150ii_gpio_config *)dev->config;
    switch (data->pixel_format)
    {
    case PIXEL_FORMAT_MONO01:
        /*SET ALL BUFF 0*/
        memset(buff,0x00,sizeof(buff));
        LOG_DBG("set buff 0");
        break;
    case PIXEL_FORMAT_MONO10:
        /*set all buff 0xff*/
        memset(buff,0xff,sizeof(buff));
        LOG_DBG("set buff 1");
        break;
    default:
        LOG_ERR("pixel format not init");
        return -ENOTSUP;
        break;
    }
    /*fill buf data to buff*/
    /*x form 0 and y from zero*/
    uint8_t *buf_prt = (uint8_t *)buf;
    for (uint16_t buff_y = y; buff_y < desc->height; buff_y++)
    {
        for (uint8_t buff_x = x / 8; buff_x < desc->pitch / 8; buff_x++)
        {
            switch (data->pixel_format)
            {
            case PIXEL_FORMAT_MONO01:
                buff[x][y] = *buf_prt;
                break;
            case PIXEL_FORMAT_MONO10:
                buff[x][y] = !*buf_prt;
                break;
            default:
                LOG_ERR("pixel format not init");
                return -ENOTSUP;
                break;
            }
            buf_prt++;
        }
    }
    LOG_DBG("buff fill ready");
    /*init msgq*/
    k_msgq_init(&data->res_tri_msgq,data->res_tri_msgq_buff,sizeof(bool),1);
    // if (ret) 
    // {
    //     LOG_ERR("no mem to create msgq error code: %d",ret);
    //     goto exit;
    // }
    k_msgq_init(&data->tim_tri_msgq,data->res_tri_msgq_buff,sizeof(bool),1);
    // if (ret)
    // {
    //     LOG_ERR("no mem to create msgq error code: %d",ret);
    //     goto exit;
    // }
    LOG_DBG("msgq init ok");
    /*regest exin callback*/
    ret = gpio_pin_interrupt_configure_dt(&cfg->res_d,
                                GPIO_INT_EDGE_TO_ACTIVE);
    if(ret) 
    {
        LOG_ERR("gpio res interrrupt configure faile ,code %d",ret);
        goto exit;
    }
    ret = gpio_pin_interrupt_configure_dt(&cfg->tim_d,
                                GPIO_INT_EDGE_BOTH);
    if(ret) 
    {
        LOG_ERR("gpio timing interrrupt configure faile ,code %d",ret);
        goto exit;
    }
    LOG_DBG("gpio interrupt configure ok");
    /*statrt printer*/
    int16_t line = 0,row = 0;
    ret = gpio_pin_set_dt(&cfg->motor_c,1);
    if (ret)
    {
        LOG_ERR("cannot set motor_c pin,error code %d",ret);
        goto exit;
    }
    LOG_DBG("set motor on");
    k_sleep(K_MSEC(cfg->start_delay));
    k_msgq_purge(&data->res_tri_msgq);
    LOG_DBG("clear res tiger msgq");
    bool tri;
    while (line < desc->height)
    {
        if (k_msgq_get(&data->res_tri_msgq,&tri,K_SECONDS(3)))
        {
            LOG_ERR("can not detector res signal");
            goto exit;
        }
        k_sleep(K_USEC(100));
        k_msgq_purge(&data->tim_tri_msgq);
        while (row < 96)
        {
            if (k_msgq_get(&data->tim_tri_msgq,&tri,K_SECONDS(1)))
            {
                LOG_ERR("can not detector timing signal");
                goto exit;
            }
            uint8_t z = row % 4;
            uint8_t a = row / 4 % 8;
            uint8_t b = row / 24;
            LOG_DBG("couter z = %d a = %d b = %d",z,a,b);
            uint8_t pin_or_not = ((buff[(z * 3) + b][line] >> a) > 0 ? 1 : 0);
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
                gpio_pin_set_dt(&cfg->ps_c,pin_or_not);
                gpio_pin_set_dt(&cfg->ps_d,0);
                break;
            default:
                LOG_ERR("math error, check program");
                goto exit;
                break;
            }
            /*delay time*/
            k_sleep(K_USEC(600));
            ret = gpio_pin_set_dt(&cfg->ps_a,0);
            if (ret)
            {
                LOG_ERR("cant not set gpio pin");
                goto exit;
            }
            ret = gpio_pin_set_dt(&cfg->ps_b,0);
            if (ret)
            {
                LOG_ERR("cant not set gpio pin");
                goto exit;
            }
            ret = gpio_pin_set_dt(&cfg->ps_c,0);
            if (ret)
            {
                LOG_ERR("cant not set gpio pin");
                goto exit;
            }
            ret = gpio_pin_set_dt(&cfg->ps_d,0);
            if (ret)
            {
                LOG_ERR("cant not set gpio pin");
                goto exit;
            }
            row++;
        }
        /*line form 0 to height*/
        line++;
    }
    if (k_msgq_get(&data->res_tri_msgq,&tri,K_SECONDS(3)))
    {
        LOG_ERR("can not detector res signal");
        goto exit;
    }
    /*clean handle*/
    exit:
    gpio_pin_set_dt(&cfg->motor_c,0);
    gpio_pin_set_dt(&cfg->ps_a,0);
    gpio_pin_set_dt(&cfg->ps_b,0);
    gpio_pin_set_dt(&cfg->ps_c,0);
    gpio_pin_set_dt(&cfg->ps_d,0);
    gpio_pin_interrupt_configure_dt(&cfg->res_d,
                                GPIO_INT_DISABLE);
    gpio_pin_interrupt_configure_dt(&cfg->tim_d,
                                GPIO_INT_DISABLE);
    k_msgq_cleanup(&data->res_tri_msgq);
    k_msgq_cleanup(&data->tim_tri_msgq);
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

/*
    @brief this function use for set printer pick pepor out a litte bit
            do nothing
    @param
    @return
*/
int m150ii_printer_clear (const struct device *dev)
{
    return 0;
}

/***************************************************************************************************************************************************************** */
/*api bindding*/
const static DEVICE_API(display, m150ii_printer_driver_api) = {
    .write = m150ii_printer_write,
    .set_pixel_format = m150ii_printer_set_pixel_format,
    .clear = m150ii_printer_clear,
};

/*Expansion macro magic*/
#define M150II_PRINTER_DEFINE(node_id)                                                  \
    static const struct m150ii_gpio_data m150ii_gpio_data_##node_id = {                 \
        .pixel_format = PIXEL_FORMAT_MONO01,                                            \
    };                                                                                  \
                                                                                        \
    /*create an instance of the consig struct, populate with DT calues*/                \
    static const struct m150ii_gpio_config m150ii_gpio_config_##node_id = {             \
        .motor_c    = GPIO_DT_SPEC_GET(node_id, motor_gpios),                           \
        .res_d      = GPIO_DT_SPEC_GET(node_id, reset_gpios),                           \
        .ps_a       = GPIO_DT_SPEC_GET(node_id, print_a_gpios),                         \
        .ps_b       = GPIO_DT_SPEC_GET(node_id, print_b_gpios),                         \
        .ps_c       = GPIO_DT_SPEC_GET(node_id, print_c_gpios),                         \
        .ps_d       = GPIO_DT_SPEC_GET(node_id, print_d_gpios),                         \
        .tim_d      = GPIO_DT_SPEC_GET(node_id, timing_gpios),                          \
        .start_delay = DT_PROP_OR(node_id, start_delay, 500),                           \
    };                                                                                  \
                                                                                        \
    /*create a device instance form devicetree node indetifier and*/                    \
    /*registers the init fuction to run during boot*/                                   \
    DEVICE_DT_DEFINE(node_id,                                                           \
                                &m150ii_printer_init,                                   \
                                NULL,                                                   \
                                &m150ii_gpio_data_##node_id,                            \
                                &m150ii_gpio_config_##node_id,                          \
                                POST_KERNEL,                                            \
                                CONFIG_DISPLAY_INIT_PRIORITY,                           \
                                &m150ii_printer_driver_api);

/*this micro use for init and define all strcut from DT*/
DT_FOREACH_STATUS_OKAY(epson_m150ii, M150II_PRINTER_DEFINE)
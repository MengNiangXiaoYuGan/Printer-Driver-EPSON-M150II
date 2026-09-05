/*
 * Custom DIYer 
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT zephyr_encode_gpio

#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

#include "encode-gpio.h"

/*enable logging at CONFIG_SENSOR_LOG_LEVEL*/
LOG_MODULE_REGISTER(Q_ENDCODE, CONFIG_SENSOR_LOG_LEVEL);

/*************************************************************************************** */
/*define and private fuction*/
struct encode_gpio_config {
    struct gpio_dt_spec gpio_a,gpio_b;
    int ppr,spp;
    bool polling_mode;
    uint32_t id;
};
struct encode_gpio_data {
    int32_t count;
    int32_t revolutions;
    int32_t revolutions_dot;
    /*set to trigger*/
    const struct device *drv;
    struct gpio_callback gpio_a_cb,gpio_b_cb;
    struct k_msgq encode_msgq;
};

struct encode_msgq_msg {
    enum {
        /*trigger form a pin*/
        TRIG_PHA,
        /*trigger form b pin*/
        TRIG_PHB
    }trigger_phase;
    enum {
        /*orhter pin is 0*/
        LEVEL_0,
        /*orther pin is 1*/
        LEVEL_1
    }phase_level;
};
#define MSGQ_BUFF_LENG  50
char __aligned(4) encode_msgq_buff[MSGQ_BUFF_LENG * sizeof(struct encode_msgq_msg)];

static int encode_gpio_init(const struct device *drv);
static int encode_gpio_channel_get(const struct device *drv,
                        enum sensor_channel chan,
                        struct sensor_value *val);
/*************************************************************************************** */

/*
    @brief gpio call balck
*/
static void encode_gpio_a_callback(const struct device *drv,
                        struct gpio_callback *cb,
                        uint32_t pins)
{
    struct encode_gpio_data *drv_data = 
        CONTAINER_OF(cb, struct encode_gpio_data, gpio_a_cb);
    const struct encode_gpio_config *cfg = (struct encode_gpio_config *) drv_data->drv->config;
    ARG_UNUSED(pins);

    /*use for a phase*/
    struct encode_msgq_msg msg = {
        .trigger_phase = TRIG_PHA,
    };
    if (gpio_pin_get_dt(&cfg->gpio_b))
    {
        msg.phase_level = LEVEL_1;
        LOG_DBG("INFO : a level ++\r\n");
    }
    else
    {
        msg.phase_level = LEVEL_0;
        LOG_DBG("INFO : a level --\r\n");
    }
    if (k_msgq_put(&drv_data->encode_msgq,&msg,K_NO_WAIT))
    {
        LOG_ERR("error : msgq full!!!");
    }
    gpio_pin_interrupt_configure_dt(&cfg->gpio_a,
                            GPIO_INT_DISABLE);
    gpio_pin_interrupt_configure_dt(&cfg->gpio_b,
                            GPIO_INT_DISABLE);
}
static void encode_gpio_b_callback(const struct device *drv,
                        struct gpio_callback *cb,
                        uint32_t pins)
{
    struct encode_gpio_data *drv_data = 
        CONTAINER_OF(cb, struct encode_gpio_data, gpio_b_cb);
    const struct encode_gpio_config *cfg = (struct encode_gpio_config *) drv_data->drv->config;
    ARG_UNUSED(pins);

    /*use for b phase*/
    struct encode_msgq_msg msg = {
        .trigger_phase = TRIG_PHB,
    };
    if (gpio_pin_get_dt(&cfg->gpio_a))
    {
        msg.phase_level = LEVEL_1;
        LOG_DBG("INFO : b level ++\r\n");
    }
    else
    {
        msg.phase_level = LEVEL_0;
        LOG_DBG("INFO : b level --\r\n");
    }
    if (k_msgq_put(&drv_data->encode_msgq,&msg,K_NO_WAIT))
    {
        LOG_ERR("error : msgq full!!!");
    }
    gpio_pin_interrupt_configure_dt(&cfg->gpio_a,
                            GPIO_INT_DISABLE);
    gpio_pin_interrupt_configure_dt(&cfg->gpio_b,
                            GPIO_INT_DISABLE);
}

/*
    @brief thread for couter
*/
#define STACK_FOR_THREAD_SIZE 512
struct k_thread thread_for_conter;
K_THREAD_STACK_DEFINE(thread_for_conter_stack,STACK_FOR_THREAD_SIZE);
static void thread_for_conter_thread(void *arg1, void *arg2, void *arg3)
{
    struct device *drv = (struct device *)arg1;
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);
    struct encode_gpio_config *cfg = (struct encode_gpio_config *)drv->config;
    struct encode_gpio_data *data = (struct encode_gpio_data *)drv->data;
    static struct encode_msgq_msg msg,msg_lass;
    static bool trig;
    while (1)
    {
        k_msgq_get(&data->encode_msgq,&msg,K_FOREVER);
        LOG_DBG("info : get msg\r\n");
        /*disable inttrupt used for detach sink*/
        // gpio_pin_interrupt_configure_dt(&cfg->gpio_a,
        //                         GPIO_INT_DISABLE);
        // gpio_pin_interrupt_configure_dt(&cfg->gpio_b,
        //                         GPIO_INT_DISABLE);
        /*check lass turn*/
        // if (cfg->spp != 1)
        // {
            if      (msg.trigger_phase == TRIG_PHA && msg.phase_level == LEVEL_1) data->count --;
            else if (msg.trigger_phase == TRIG_PHA && msg.phase_level == LEVEL_0) data->count ++;
            else if (msg.trigger_phase == TRIG_PHB && msg.phase_level == LEVEL_1) data->count ++;
            else if (msg.trigger_phase == TRIG_PHB && msg.phase_level == LEVEL_0) data->count --;
        // }
        // else
        // {
        //     if      (msg.trigger_phase == TRIG_PHA && msg.phase_level == LEVEL_1 && msg_lass.trigger_phase == TRIG_PHB && msg_lass.phase_level == LEVEL_0) data->count --;
        //     else if (msg.trigger_phase == TRIG_PHA && msg.phase_level == LEVEL_0 && msg_lass.trigger_phase == TRIG_PHB && msg_lass.phase_level == LEVEL_1) data->count ++;
        //     else if (msg.trigger_phase == TRIG_PHB && msg.phase_level == LEVEL_1 && msg_lass.trigger_phase == TRIG_PHA && msg_lass.phase_level == LEVEL_0) data->count ++;
        //     else if (msg.trigger_phase == TRIG_PHB && msg.phase_level == LEVEL_0 && msg_lass.trigger_phase == TRIG_PHA && msg_lass.phase_level == LEVEL_1) data->count --;
        //     else
        //     {
        //         LOG_DBG("info : no math,skip this msg\r\n");
        //     }
        // }
        // msg_lass = msg;
        k_msleep(50);
        /*enable intrrupt*/
        gpio_pin_interrupt_configure_dt(&cfg->gpio_a,
                                GPIO_INT_EDGE_TO_ACTIVE);
        gpio_pin_interrupt_configure_dt(&cfg->gpio_b,
                                GPIO_INT_EDGE_TO_ACTIVE);
    }
    return;
}

/*
    @brief this fuction used for init endcode and set interrupt(if dont use polling mode)
*/
static int encode_gpio_init(const struct device *drv)
{
    int ret = 0;

    /*get dt information*/
    const struct encode_gpio_config *cfg = (const struct encode_gpio_config *)drv->config;

    /*get the encode struct and instrans ID from the config*/
    const struct gpio_dt_spec *gpio_a = &cfg->gpio_a;
    const struct gpio_dt_spec *gpio_b = &cfg->gpio_b;
    
    /*print log to output*/
    LOG_DBG("Initializing encode gpio (istance ID: %u)\r\n",cfg->id);

    /*check gpio is ready*/
    if (!gpio_is_ready_dt(gpio_a) || !gpio_is_ready_dt(gpio_b))
    {
        LOG_ERR("ENDCODE GPIO is not ready!\r\n");
        return -ENODEV;
    }

    /*set the gpio as input*/
    ret = gpio_pin_configure_dt(gpio_a, GPIO_INPUT);
    if (ret < 0)
    {
        LOG_ERR("Could not configure GPIO as inout\r\n");
        return -ENODEV;
    }
    ret = gpio_pin_configure_dt(gpio_b, GPIO_INPUT);
    if (ret < 0)
    {
        LOG_ERR("Could not configure GPIO as inout\r\n");
        return -ENODEV;
    }

    /*init positong*/
    struct encode_gpio_data *data = (struct encode_gpio_data *)drv->data;
    data->count = 0;
    data->revolutions = 0;
    data->revolutions_dot = 0;
    data->drv = drv;

    /*init msgq*/
    k_msgq_init(&data->encode_msgq,
                encode_msgq_buff,
                sizeof(struct encode_msgq_msg),
                MSGQ_BUFF_LENG);

    /*if polling mode init a new thread to get ab phase statues*/
    if (cfg->polling_mode)
    {
        /* code */
        LOG_DBG(" device in to polling mode\r\n");
    }
    else
    {
        LOG_DBG(" device in to intrrupt mode\r\n");
        /*else just set ab pings to intrupt*/
        /*we set two intrruput cb same */
        gpio_init_callback(&data->gpio_a_cb,
                    encode_gpio_a_callback,
                    BIT(cfg->gpio_a.pin));
        
        if (gpio_add_callback(cfg->gpio_a.port, &data->gpio_a_cb) < 0)
        {
            LOG_ERR("Failed to set gpio a call back\r\n");
            return -EIO;
        }

        gpio_init_callback(&data->gpio_b_cb,
                    encode_gpio_b_callback,
                    BIT(cfg->gpio_b.pin));
        
        if (gpio_add_callback(cfg->gpio_b.port, &data->gpio_b_cb) < 0)
        {
            LOG_ERR("Failed to set gpio b call back\r\n");
            return -EIO;
        }
        gpio_pin_interrupt_configure_dt(&cfg->gpio_a,
                                GPIO_INT_EDGE_TO_ACTIVE);
        // gpio_pin_interrupt_configure_dt(&cfg->gpio_b,
        //                         GPIO_INT_EDGE_TO_ACTIVE);
        
        /*using thread to get step*/
        k_thread_create(&thread_for_conter,
                        thread_for_conter_stack,
                        K_THREAD_STACK_SIZEOF(thread_for_conter_stack),
                        thread_for_conter_thread,
                        (void *)drv,
                        NULL,
                        NULL,
                        75,
                        0,
                        K_NO_WAIT);
    }
    return ret;
}

static int encode_gpio_channel_get(const struct device *drv,
                        enum sensor_channel chan,
                        struct sensor_value *val)
{
    struct sensor_value *ret = val;
    struct encode_gpio_data *data = (struct encode_gpio_data *) drv->data;
    const struct encode_gpio_config *cfg = (struct encode_gpio_config *) drv->config;
    int32_t resolution_revolustion = cfg->ppr * cfg->spp;
    data->revolutions = data->count / resolution_revolustion;
    data->revolutions_dot = data->count % resolution_revolustion * 10e6 / resolution_revolustion;
    switch (chan)
    {
    case SENSOR_CHAN_ENCODER_COUNT:
    /* get recouce count*/
        ret->val2 = 0;
        ret->val1 = data->count;
        break;
    case SENSOR_CHAN_ENCODER_REVOLUTIONS:
    /*get revolution*/
        ret->val1 = data->revolutions;
        ret->val2 = data->revolutions_dot;   
        break;
    default:
        return -ENOTSUP;
        break;
    }
    return 0;
}

static DEVICE_API(sensor, encode_gpio_driver_api) = {
    .channel_get = encode_gpio_channel_get,
};

/*Expansion macro to define driver instances*/
#define ENCODE_GPIO_DEFINE(inst)                                            \
    static struct encode_gpio_data encode_gpio_data_##inst;                 \
                                                                            \
    /*create an instance of the config struct, populate with DT calues*/    \
    static const struct encode_gpio_config encode_gpio_config_##inst = {    \
        .gpio_a = GPIO_DT_SPEC_INST_GET(inst, a_gpios),                     \
        .gpio_b = GPIO_DT_SPEC_INST_GET(inst, b_gpios),                     \
        .id = inst,                                                         \
        .ppr = DT_INST_PROP(inst, ppr),                                     \
        .spp = DT_INST_PROP(inst, spp),                                     \
        .polling_mode = DT_INST_PROP_OR(inst, polling_mode, false)          \
    };                                                                      \
                                                                            \
    /*create a device instance from a devicetree node indetifier and*/      \
    /* registers the init fuction to run during boot*/                      \
    SENSOR_DEVICE_DT_INST_DEFINE(inst,                                      \
                                encode_gpio_init,                           \
                                NULL,                                       \
                                &encode_gpio_data_##inst,                   \
                                &encode_gpio_config_##inst,                 \
                                POST_KERNEL,                                \
                                CONFIG_SENSOR_INIT_PRIORITY,                \
                                &encode_gpio_driver_api);                   \
    /*EACH device defined in the device tree*/
DT_INST_FOREACH_STATUS_OKAY(ENCODE_GPIO_DEFINE)

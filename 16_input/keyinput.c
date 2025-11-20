#include <linux/module.h>
#include <linux/errno.h> 
#include <linux/of.h> 
#include <linux/platform_device.h> 
#include <linux/of_gpio.h> 
#include <linux/input.h> 
#include <linux/timer.h> 
#include <linux/of_irq.h> 
#include <linux/interrupt.h> 

#define KEYINPUT_NAME  "keyinput"       /*名字 */

struct key_dev{
    struct input_dev *idev; /* 输入设备结构体 */
    struct timer_list timer;    /* 定时器 */
    int gpio_key;              /* 按键所使用的GPIO编号 */
    int irq_key;              /* 按键所使用的中断号 */
};

struct key_dev key; /* 按键设备 */

static irqreturn_t key_interrupt(int irq, void *dev_id)
{
    struct key_dev *keydev = (struct key_dev *)dev_id;

    /* 启动定时器，延时10ms后去处理按键扫描 */
    mod_timer(&keydev->timer, jiffies + msecs_to_jiffies(10));

    return IRQ_HANDLED;
}

static int key_gpio_init(struct platform_device *pdev)
{
    int ret;
    int irqflags;

    /* 获取设备树中的gpio属性，得到按键所使用的GPIO编号 */
    key.gpio_key = of_get_named_gpio(pdev->dev.of_node, "key-gpio", 0);
    if(key.gpio_key < 0){
        printk("can't get key gpio\r\n");
        return -EINVAL;
    }

    /* 申请GPIO */
    ret = gpio_request(key.gpio_key, "key-gpio");
    if(ret){
        printk("gpio_request failed!\r\n");
        return ret;
    }

    /* 设置GPIO为输入 */
    gpio_direction_input(key.gpio_key);

    // 获取中断号
    key.irq_key = gpio_to_irq(key.gpio_key);
    if(key.irq_key < 0){
        printk("gpio_to_irq failed!\r\n");
        gpio_free(key.gpio_key);
        return -EINVAL;
    }
    //获取设备树中的中断触发类型
    irqflags = of_irq_get_trigger_type(pdev->dev.of_node, 0);
    if(irqflags < 0){
        printk("of_irq_get_trigger_type failed!\r\n");
        gpio_free(key.gpio_key);
        return -EINVAL;
    }
    //申请中断
    ret = request_irq(key.irq_key, key_interrupt, irqflags, "key-interrupt", &key);
    if(ret){
        printk("request_irq failed!\r\n");
        gpio_free(key.gpio_key);
        return ret;
    }


    return 0;
}

static void key_timer_function(struct timer_list *t)
{
    struct key_dev *keydev = from_timer(keydev, t, timer);
    int key_state;

    /* 读取按键状态 */
    key_state = gpio_get_value(keydev->gpio_key);

    if(key_state == 0){
        /* 按键按下 */
        input_report_key(keydev->idev, KEY_SPACE, 1);
        input_sync(keydev->idev);
    }else{
        /* 按键释放 */
        input_report_key(keydev->idev, KEY_SPACE, 0);
        input_sync(keydev->idev);
    }
}

static int keyinput_probe(struct platform_device *pdev)
{
    int ret;

    ret = key_gpio_init(pdev->dev.of_node);
    if(ret < 0){
        return ret;
    }

    timer_setup(&key.timer, key_timer_function, 0);

    /* 分配输入设备结构体 */
    key.idev = input_allocate_device();
    if(!key.idev){
        printk("input_allocate_device failed!\r\n");
        goto fail_irq;  
    }
    key.idev->name = KEYINPUT_NAME;
#if 0
__set_bit(EV_KEY, key.idev->evbit);
    __set_bit(KEY_SPACE, key.idev->keybit);
#endif

#if 0
    key.idev->evbit[0] = BIT_MASK(EV_KEY);
    key.idev->keybit[BIT_WORD(KEY_SPACE)] = BIT_MASK(KEY_SPACE);
#endif
    key.idev->evbit[0] = BIT_MASK(EV_KEY);
    key.idev->keybit[BIT_WORD(KEY_SPACE)] = BIT_MASK(KEY_SPACE);

    ret = input_register_device(key.idev);
    if(ret){
        printk("input_register_device failed!\r\n");
        goto fail_free_idev;
    }
    return 0;
fail_free_idev:
    input_free_device(key.idev);
fail_irq:
    free_irq(key.irq_key, &key);
    gpio_free(key.gpio_key);
    return -EIO;
}

static int keyinput_remove(struct platform_device *pdev)
{
    input_unregister_device(key.idev);
    free_irq(key.irq_key, &key);
    gpio_free(key.gpio_key);
    del_timer_sync(&key.timer);
    printk("key device removed!\r\n");
    return 0;
}

static struct of_device_id key_of_match[] = {
    {.compatible = "rk3588-key"},
    {/* Sentinel */}
};
MODULE_DEVICE_TABLE(of, key_of_match);

static struct platform_driver keyinput_driver = {
    .probe      = keyinput_probe,
    .remove     = keyinput_remove,
    .driver     = {
        .name   = "rk3588-key",
        .owner  = THIS_MODULE,
        .of_match_table = key_of_match,
    },
};


module_platform_driver(keyinput_driver);
MODULE_LICENSE("GPL");


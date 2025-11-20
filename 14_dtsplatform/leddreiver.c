#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <linux/fcntl.h>
//#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define LEDDEV_CNT   1               /*设备号个数 */
#define LEDDEV_NAME  "dtsplatled"       /*名字 */

#define LEDOFF 	0				        /* 关灯 */
#define LEDON 	1				        /* 开灯 */

struct leddev_dev{
    dev_t devid;            /* 设备号 */
    struct cdev cdev;       /* cdev */
    struct class *class;    /* 类 */
    struct device *device;  /* 设备 */
    struct device_node *nd; /* 设备节点 */
    int gpio_led;         /* led所使用的GPIO编号 */
    int major;              /* 主设备 */
    int minor;              /* 次设备号 */

};

struct leddev_dev leddev; /* led设备 */


void led_switch(u8 sta)
{
    u32 val = 0;
    if(sta == LEDON) {
        gpio_set_value(leddev.gpio_led, 0);
    }else if(sta == LEDOFF) { 
        gpio_set_value(leddev.gpio_led, 1);
    }	
}

static int led_gpio_init(struct platform_device *pdev)
{
    int ret;

    /* 获取设备树中的gpio属性，得到LED所使用的GPIO编号 */
    leddev.gpio_led = of_get_named_gpio(pdev->dev.of_node, "led-gpio", 0);
    if(leddev.gpio_led < 0){
        printk("can't get led gpio\r\n");
        return -EINVAL;
    }

    /* 申请GPIO */
    ret = gpio_request(leddev.gpio_led, "led-gpio");
    if(ret){
        printk("gpio_request failed %d\r\n", ret);
        return -EINVAL;
    }

    /* 设置GPIO为输出，并且默认输出高电平，关闭LED */
    ret = gpio_direction_output(leddev.gpio_led, 1);
    if(ret){
        printk("gpio_direction_output failed %d\r\n", ret);
        goto fail_gpio;
    }

    return 0;
fail_gpio:
    gpio_free(leddev.gpio_led);
    return -EINVAL;
}

static int leddev_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t leddev_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int retvalue = 0;
    u8 ledstat;

    retvalue = copy_from_user(&ledstat, buf, cnt);
    if(retvalue < 0)
    {
        printk("leddev write failed!\r\n");
        return -EFAULT;
    }
    ledstat = buf[0];
    if(ledstat == LEDON)
    {
        led_switch(LEDON);  
    }
    else if(ledstat == LEDOFF)
    {
        led_switch(LEDOFF); 
    }   
    return 0;
}

static struct file_operations leddev_fops = {
    .owner = THIS_MODULE,
    .open = leddev_open,
    .write = leddev_write,
};

static int led_prob(struct platform_device *pdev)
{
    int ret;

    printk("led device probed!\r\n");

    /* 初始化LED所使用的GPIO */
    ret = led_gpio_init(pdev);
    if(ret < 0){
        return ret;
    }
    /* 注册字符设备驱动 */
    /* 1、创建设备号*/
    ret =alloc_chrdev_region(&leddev.devid, 0, LEDDEV_CNT, LEDDEV_NAME);
    if(ret < 0){
        pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n",LEDDEV_NAME, ret);
        goto fail_gpio;
    }

    /* 2、初始化cdev */
    leddev.cdev.owner = THIS_MODULE;
    cdev_init(&leddev.cdev, &leddev_fops);
    ret = cdev_add(&leddev.cdev, leddev.devid, LEDDEV_CNT);
    if(ret < 0){
        pr_err("%s cdev_add failed, ret=%d\r\n", LEDDEV_NAME, ret);
        goto fail_unregister;
    }
    /* 3、创建类 */
    leddev.class = class_create(THIS_MODULE, LEDDEV_NAME);
    if(IS_ERR(leddev.class)){
        pr_err("%s class_create failed!\r\n", LEDDEV_NAME);
        goto del_cdev;
    }
    /* 4、创建设备 */
    leddev.device = device_create(leddev.class, NULL, leddev.devid,
                                    NULL, LEDDEV_NAME);
    if(IS_ERR(leddev.device)){
        pr_err("%s device_create failed!\r\n", LEDDEV_NAME);
        goto destroy_class;
    }
    return 0;
destroy_class:
    class_destroy(leddev.class);
del_cdev:
    cdev_del(&leddev.cdev);
fail_unregister:
    unregister_chrdev_region(leddev.devid, LEDDEV_CNT);
fail_gpio:
    gpio_free(leddev.gpio_led);
    return -EIO;
}

static int led_remove(struct platform_device *pdev)
{
    /* 注销字符设备驱动 */
    device_destroy(leddev.class, leddev.devid);
    class_destroy(leddev.class);
    cdev_del(&leddev.cdev);
    unregister_chrdev_region(leddev.devid, LEDDEV_CNT);
    /* 释放GPIO */
    gpio_free(leddev.gpio_led);
    return 0;
}

static struct of_device_id led_of_match[] = {
    {.compatible = "rk3588-led"},
    {/* Sentinel */}
};
MODULE_DEVICE_TABLE(of, led_of_match);

static struct platform_driver leddriver = {
    .probe      = led_prob,
    .remove     = led_remove,
    .driver     = {
        .name   = "rk3588-led",
        .owner  = THIS_MODULE,
        .of_match_table = led_of_match,
    },
};

static int __init leddriver_init(void)
{
    return platform_driver_register(&leddriver);
}

static void __exit leddriver_exit(void)
{
    platform_driver_unregister(&leddriver);
}

module_init(leddriver_init);
module_exit(leddriver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("LED Driver for RK3588");
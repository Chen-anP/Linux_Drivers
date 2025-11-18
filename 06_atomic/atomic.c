#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
//#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/match/map.h>
#include <asm/io.h>

#define GPIOLED_CNT  1               /*设备号个数 */
#define GPIOLED_NAME "gpioled"       /*名字 */
#define LEDOFF 	0				        /* 关灯 */
#define LEDON 	1				        /* 开灯 */

struct gpioled_dev{
    dev_t devid;            /* 设备号 */
    struct cdev cdev;       /* cdev */
    struct class *class;    /* 类 */
    struct device *device;  /* 设备 */
    int major;              /* 主设备 */
    int minor;              /* 次设备号 */
    struct device_node *nd; /* 设备节点 */
    int led_gpio;           /* led所使用的GPIO编号 */
    atomic_t lock;          /* 原子变量，用于同步 */
};

struct gpioled_dev gpioled; /* led设备 */

static int led_open(struct inode *inode, struct file *filp)
{
    if(!atomic_dec_and_test(&gpioled.lock)){
        atomic_inc(&gpioled.lock);
        return -EBUSY;   /* 设备忙 */
    }

    filp->private_data = &gpioled;      /*设置私有数据 */
    return 0;
}

static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int retvalue;
    unsigned char databuf[1];
    unsigned char ledstat;
    struct gpioled_dev *dev = filp->private_data;

    retvalue = copy_from_user(databuf, buf, cnt);
    if(retvalue < 0)
    {
        printk("kernel write failed!\r\n");
        return -EFAULT; 
    }

    ledstat = databuf[0];
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

static int led_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static struct file_operations led_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .read = led_read,
    .write = led_write,
    .release = led_release,
};

static int __init led_init(void)
{
    int ret = 0;
    const char *str;

    //初始化原子变量
    gpioled.lock = ATOMIC_INIT(1);

    //原子变量初始值设为1，表示设备空闲
    atomic_set(&gpioled.lock, 1);
    

    /* 获取设备树中的属性数据 */
    gpioled.nd = of_find_node_by_path("/gpioled");
    if(gpioled.nd == NULL){
        printk("gpioled node not found!\r\n");
        return -EINVAL;
    }else{
        printk("gpioled node found!\r\n");
    }
    //获取status属性内容
    ret = of_property_read_string(gpioled.nd, "status", &str);
    if(ret < 0){
        printk("status read failed!\r\n");
    }
    else{
        printk("status = %s\r\n", str);
    }
    //获取compatible属性内容
    ret = of_property_read_string(gpioled.nd, "compatible", &str);
    if(ret < 0){
        printk("compatible read failed!\r\n");
    }
    else{
        printk("compatible = %s\r\n", str);
    }

    //获取led所使用的GPIO编号
    gpioled.led_gpio = of_get_named_gpio(gpioled.nd, "led-gpio", 0);
    if(gpioled.led_gpio < 0){
        printk("can't get led gpio\r\n");
        return -EINVAL;
    }
    printk("led gpio num = %d\r\n", gpioled.led_gpio);

    //申请GPIO
    ret = gpio_request(gpioled.led_gpio, "led-gpio");
    if(ret){
        printk("gpio_request failed %d\r\n", ret);
        return -EINVAL;
    }

    ret = gpio_direction_output(gpioled.led_gpio, 1); //默认关闭LED
    if(ret){
        printk("gpio_direction_output failed %d\r\n", ret);
        goto fail_gpio;
    }

    /* 注册字符设备驱动 */
    /* 1、创建设备号*/
    if(gpioled.major){
        gpioled.devid = MKDEV(gpioled.major, 0);   
        ret = register_chrdev_region(gpioled.devid, GPIOLED_CNT, GPIOLED_NAME);
        if(ret < 0){
            pr_err("cannot register %s char driver [ret=%d]\n",GPIOLED_NAME, GPIOLED_CNT);
            goto fail_gpio;
        }
    }else{         
        ret = alloc_chrdev_region(&gpioled.devid, 0, GPIOLED_CNT, GPIOLED_NAME);
        if(ret < 0){
            pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n",GPIOLED_NAME, ret);
            goto fail_gpio;
        }
        gpioled.major = MAJOR(gpioled.devid);   
        gpioled.minor = MINOR(gpioled.devid);   
    }
    printk("gpioled major=%d,minor=%d\r\n",gpioled.major, gpioled.minor);

    /* 2、初始化cdev */
    gpioled.cdev.owner = THIS_MODULE;
    cdev_init(&gpioled.cdev, &led_fops);
    ret = cdev_add(&gpioled.cdev, gpioled.devid, GPIOLED_CNT);
    if(ret < 0){
        pr_err("cdev_add failed, ret=%d\r\n", ret);
        goto fail_gpio;
    }
    /* 4、创建类 */
    gpioled.class = class_create(THIS_MODULE, GPIOLED_NAME);
    if(IS_ERR(gpioled.class)){
        ret = PTR_ERR(gpioled.class);
        pr_err("class_create failed, ret=%d\r\n", ret);
        goto del_cdev;
    }
    /* 5、创建设备 */
    gpioled.device = device_create(gpioled.class, NULL, gpioled.devid, NULL, GPIOLED_NAME);
    if(IS_ERR(gpioled.device)){
        ret = PTR_ERR(gpioled.device);
        pr_err("device_create failed, ret=%d\r\n", ret);
        goto destroy_class;
    }

    fail_gpio:
    return 0;
    destroy_class:
    class_destroy(gpioled.class);
    del_cdev:
    cdev_del(&gpioled.cdev);   
    unregister_chrdev_region(gpioled.devid, GPIOLED_CNT);
    return -EIO;
}

static void __exit led_exit(void)
{
    //关闭LED
    led_switch(LEDOFF);
    //释放GPIO
    gpio_free(gpioled.led_gpio);
    //注销字符设备驱动
    device_destroy(gpioled.class, gpioled.devid);
    class_destroy(gpioled.class);
    cdev_del(&gpioled.cdev);
    unregister_chrdev_region(gpioled.devid, GPIOLED_CNT);
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");

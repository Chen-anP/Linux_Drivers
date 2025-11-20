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

#define LEDDEV_CNT  1               /*设备号个数 */
#define LEDDEV_NAME "platled"     /*名字 */
#define LEDOFF 	0				        /* 关灯 */
#define LEDON 	1				        /* 开灯 */

static void __iomem *PMU2_IOC_GPIO0C_IOMUX_SEL_H_VA;
static void __iomem *GPIO_SWPORT_DR_H_VA;
static void __iomem *GPIO_SWPORT_DDR_H_VA;

struct leddev_dev{
    dev_t devid;            /* 设备号 */
    struct cdev cdev;       /* cdev */
    struct class *class;    /* 类 */
    struct device *device;  /* 设备 */
    int major;              /* 主设备 */
    int minor;              /* 次设备号 */
};

struct leddev_dev leddev; /* led设备 */

void led_switch(u8 sta)
{
    u32 val = 0;
    if(sta == LEDON) {
        val = readl(GPIO_SWPORT_DR_H_VA);
        val &= ~(0X20<<0);
        val |= (0X20<<16) | (0X20<<0);
        writel(val, GPIO_SWPORT_DR_H_VA);
    }else if(sta == LEDOFF) { 
        writel(val, GPIO_SWPORT_DR_H_VA);
        val = readl(GPIO_SWPORT_DR_H_VA);
        val &= ~(0X20<<0);
        val |= (0X20<<16);
        writel(val, GPIO_SWPORT_DR_H_VA);
    }	
}


void led_remap(void)
{
    PMU2_IOC_GPIO0C_IOMUX_SEL_H_VA = ioremap(PMU2_IOC_GPIO0C_IOMUX_SEL_H, 4);
    GPIO_SWPORT_DR_H_VA = ioremap(GPIO_SWPORT_DR_H, 4);
    GPIO_SWPORT_DDR_H_VA = ioremap(GPIO_SWPORT_DDR_H, 4);
}

void led_unmap(void)
{
    iounmap(PMU2_IOC_GPIO0C_IOMUX_SEL_H_VA);
    iounmap(GPIO_SWPORT_DR_H_VA);
    iounmap(GPIO_SWPORT_DDR_H_VA);
}

static int leddev_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &leddev;      /*设置私有数据 */
    return 0;
}

static ssize_t leddev_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int retvalue = 0;
    u8 ledstat;

    struct leddev_dev *dev = filp->private_data;

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
    int ret = 0;
    u32 val = 0;

    leddev.major = 0; /* 动态申请设备号 */

    /* 初始化LED */
    /* 1、寄存器地址映射 */
    led_remap();

    val = readl(PMU2_IOC_GPIO0C_IOMUX_SEL_H_VA);
    val &= ~(0x00F0<<0);
    val |= (0x00F0<<0) | (0x0<<0);

    writel(val, PMU2_IOC_GPIO0C_IOMUX_SEL_H_VA);
    /* 2、设置gpio为输出功能 */
    val = readl(GPIO_SWPORT_DDR_H_VA);
    val &= ~(0X20<<0);
    val |= (0X20<<16) | (0X20<<0);
    writel(val, GPIO_SWPORT_DDR_H_VA);

    /* 3、设置gpio默认输出高电平，led关闭 */
    val = readl(GPIO_SWPORT_DR_H_VA);
    val &= ~(0X20<<0);
    val |= (0X20<<16);
    writel(val, GPIO_SWPORT_DR_H_VA);

    /* 注册字符设备驱动 */
    /* 1、创建设备号*/
    if(leddev.major){
        leddev.devid = MKDEV(leddev.major, 0);    /*  定义了设备号 */
        ret = register_chrdev_region(leddev.devid, LEDDEV_CNT, LEDDEV_NAME);
        if(ret < 0){
            pr_err("cannot register %s char driver [ret=%d]\n",LEDDEV_NAME, LEDDEV_CNT);
            goto fail_map;
        }
    }else{          /* 没有定义设备号 */
        ret = alloc_chrdev_region(&leddev.devid, 0, LEDDEV_CNT, LEDDEV_NAME);
        if(ret < 0){
            pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n",LEDDEV_NAME, ret);
            goto fail_map;
        }
        leddev.major = MAJOR(leddev.devid);   /* 获取主设备号 */
        leddev.minor = MINOR(leddev.devid);   /* 获取次设备号 */
    }
    printk("leddev major=%d,minor=%d\r\n",leddev.major, leddev.minor);
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
fail_map:
    led_unmap();
    return -EIO;
}

static int led_remove(struct platform_device *pdev)
{
    /* 注销字符设备驱动 */
    device_destroy(leddev.class, leddev.devid);
    class_destroy(leddev.class);
    cdev_del(&leddev.cdev);
    unregister_chrdev_region(leddev.devid, LEDDEV_CNT);
    /* 取消寄存器重映射 */
    led_unmap();
    return 0;
}

static struct platform_driver leddriver = {
    .probe      = led_prob,
    .remove     = led_remove,
    .driver     = {
        .name   = "rk3588-led",
        .owner  = THIS_MODULE,
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
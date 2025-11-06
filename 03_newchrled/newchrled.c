#include <linux/types.h> 
#include <linux/kernel.h>
#include <linux/delay.h> 
#include <linux/ide.h> 
#include <linux/init.h> 
#include <linux/module.h> 
#include <linux/errno.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>
#include <asm/io.h> 

// #define LED_MAJOR 200
// #define LED_NAME "led"

#define NEWCHRLED_CNT 1
#define NEWCHRLED_NAME "newchrled"

#define LEDOFF 0
#define LEDON 1

#define PMU_GRF_BASE                      (0xFD5F8000)
#define BUS_IOC_BASE                      (0xFD5F8000)
//#define VCCIO1_4_BASE                     (0xFD5F8000)
#define GPIO0_BASE                        (0xFD8A0000)


#define BUS_IOC_GPIO0C_IOMUX_SEL_L       (BUS_IOC_BASE + 0x0010) 
//#define VCCIO1_4_IOC_GPIO0C_DS_L        (VCCIO1_4_BASE + 0x0020) 
#define GPIO_SWPORT_DR_L                (GPIO0_BASE + 0X0000) 
#define GPIO_SWPORT_DDR_L               (GPIO0_BASE + 0X0008)


static void __iomem *BUS_IOC_GPIO0C_IOMUX_SEL_L_VA;
//static void __iomem *VCCIO1_4_IOC_GPIO0C_DS_L_VA;
static void __iomem *GPIO_SWPORT_DR_L_VA;
static void __iomem *GPIO_SWPORT_DDR_L_VA;

//newchrled设备结构体
struct newchrled_dev {
    devt_t devid;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    int major;
    int minor;
};

struct newchrled_dev newchrled;


void led_switch(u8 state)
{
    u32 val = 0;
    val = readl(GPIO_SWPORT_DR_L_VA);
    if(state == LEDON)
    {
        val &= ~(1<<12);
    }
    else if(state == LEDOFF)
    {
        val |= (1<<12);
    }
    writel(val, GPIO_SWPORT_DR_L_VA);
}

void led_remap(void)
{
    BUS_IOC_GPIO0C_IOMUX_SEL_L_VA = ioremap(BUS_IOC_GPIO0C_IOMUX_SEL_L, 4);
    //VCCIO1_4_IOC_GPIO0C_DS_L_VA = ioremap(VCCIO1_4_IOC_GPIO0C_DS_L, 4);
    GPIO_SWPORT_DR_L_VA = ioremap(GPIO_SWPORT_DR_L, 4);
    GPIO_SWPORT_DDR_L_VA = ioremap(GPIO_SWPORT_DDR_L, 4);
}

void led_unmap(void)
{
    iounmap(BUS_IOC_GPIO0C_IOMUX_SEL_L_VA);
    //iounmap(VCCIO1_4_IOC_GPIO0C_DS_L_VA);
    iounmap(GPIO_SWPORT_DR_L_VA);
    iounmap(GPIO_SWPORT_DDR_L_VA);
}


static int led_open(struct inode *inode, struct file *filp)
{
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
    u32 val = 0;
    int retvalue;
    //寄存器重映射
    led_remap();
    //设置gpio0C_12为gpio功能
    val = readl(BUS_IOC_GPIO0C_IOMUX_SEL_L_VA);
    val &= ~(0x3<<24);
    val |= (0x1<<24);

    writel(val, BUS_IOC_GPIO0C_IOMUX_SEL_L_VA);
    //设置gpio0C_12 40ohm的驱动能力
    // val = readl(VCCIO1_4_IOC_GPIO0C_DS_L_VA);
    // val &= ~(0x3<<24);
    // val |= (0x1<<24);

    // writel(val, VCCIO1_4_IOC_GPIO0C_DS_L_VA);
    //设置gpio0C_12为输出功能
    val = readl(GPIO_SWPORT_DDR_L_VA);
    val |= (1<<12);
    writel(val, GPIO_SWPORT_DDR_L_VA);


    //注册字符设备驱动
    if(newchrled.major)
    {
        newchrled.devid = MKDEV(newchrled.major, 0);
        retvalue = register_chrdev_region(newchrled.devid, NEWCHRLED_CNT, NEWCHRLED_NAME);
        if(retvalue < 0)
        {
            printk("led driver register failed!\r\n");
            goto fail_map; 
        }
    }
    else
    {
        retvalue = alloc_chrdev_region(&newchrled.devid, 0, NEWCHRLED_CNT, NEWCHRLED_NAME);
        if(retvalue < 0)
        {
            printk("led driver register failed!\r\n");
            goto fail_map; 
        }
        newchrled.major = MAJOR(newchrled.devid);
        newchrled.minor = MINOR(newchrled.devid);
    }

    // retvalue = register_chrdev(LED_MAJOR, LED_NAME, &led_fops);
    // if(retvalue < 0)
    // {
    //     printk("led driver register failed!\r\n");
    //     goto fail_map; 
    // }

    printk("led driver register successed! major=%d minor=%d\r\n", newchrled.major, newchrled.minor);

    //字符设备初始化
    newchrled.cdev.owner = THIS_MODULE;
    cdev_init(&newchrled.cdev, &led_fops);

    //添加一个字符设备
    retvalue = cdev_add(&newchrled.cdev, newchrled.devid, NEWCHRLED_CNT);
    if(retvalue < 0)
    {
        printk("led driver add failed!\r\n");
        goto fail_unregister;
    }

    //创建类
    newchrled.class = class_create(THIS_MODULE, NEWCHRLED_NAME);
    if(IS_ERR(newchrled.class))
    {
        printk("led driver class create failed!\r\n");
        retvalue = PTR_ERR(newchrled.class);
        goto fail_cdev;
    }

    //创建设备
    newchrled.device = device_create(newchrled.class, NULL, newchrled.devid, NULL, NEWCHRLED_NAME);
    if(IS_ERR(newchrled.device))
    {
        printk("led driver device create failed!\r\n");
        retvalue = PTR_ERR(newchrled.device);
        goto fail_class;
    }


    return 0;
    destroy_class:
        class_destroy(newchrled.class);
    del_cdev:
        cdev_del(&newchrled.cdev);
    del_unregister:
        unregister_chrdev_region(newchrled.devid, NEWCHRLED_CNT);
    fail_map:
        led_unmap();
        return -EFAULT;
}



static void __exit led_exit(void)
{
    // //注销字符设备驱动
    // unregister_chrdev(LED_MAJOR, LED_NAME);
    //取消寄存器重映射
    led_unmap();

    //注销设备
    cdev_del(&newchrled.cdev);
    unregister_chrdev_region(newchrled.devid, NEWCHRLED_CNT);
    device_destroy(newchrled.class, newchrled.devid);
    class_destroy(newchrled.class);
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("chen");
module_INFO(intree,"Y");
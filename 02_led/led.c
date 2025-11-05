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

#define LED_MAJOR 200
#define LED_NAME "led"

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

    retvalue = register_chrdev(LED_MAJOR, LED_NAME, &led_fops);
    if(retvalue < 0)
    {
        printk("led driver register failed!\r\n");
        goto fail_map; 
    }
    return 0;
}

fail_map:
    led_unmap();
    return -EFAULT;
}


static void __exit led_exit(void)
{
    //注销字符设备驱动
    unregister_chrdev(LED_MAJOR, LED_NAME);
    //取消寄存器重映射
    led_unmap();
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("chen");
module_INFO(intree,"Y");
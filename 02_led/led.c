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

#define PMU_GRF_BASE                       (0xFD5F8000)
#define PMU2_IOC                           (0xFD5F4000)
//#define VCCIO1_4_BASE                     (0xFD5F8000)
#define GPIO0_BASE                         (0xFD8A0000)


#define PMU2_IOC_GPIO0C_IOMUX_SEL_H       (PMU2_IOC + 0x0008) 
//#define VCCIO1_4_IOC_GPIO0C_DS_L        (VCCIO1_4_BASE + 0x0020) 
#define GPIO_SWPORT_DR_H                  (GPIO0_BASE + 0X0004) 
#define GPIO_SWPORT_DDR_H                 (GPIO0_BASE + 0X000C)


static void __iomem *PMU2_IOC_GPIO0C_IOMUX_SEL_H_VA;
//static void __iomem *VCCIO1_4_IOC_GPIO0C_DS_L_VA;
static void __iomem *GPIO_SWPORT_DR_H_VA;
static void __iomem *GPIO_SWPORT_DDR_H_VA;


void led_switch(u8 state)
{
    u32 val = 0;

    if(state == LEDON)
    {
        val = readl(GPIO_SWPORT_DR_H_VA);
        val &= ~(0X20<<0);
        val |= (0X20<<16) | (0X20<<0);
        writel(val, GPIO_SWPORT_DR_H_VA);
    }
    else if(state == LEDOFF)
    {
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
    //VCCIO1_4_IOC_GPIO0C_DS_L_VA = ioremap(VCCIO1_4_IOC_GPIO0C_DS_L, 4);
    GPIO_SWPORT_DR_H_VA = ioremap(GPIO_SWPORT_DR_H, 4);
    GPIO_SWPORT_DDR_H_VA = ioremap(GPIO_SWPORT_DDR_H, 4);
}

void led_unmap(void)
{
    iounmap(PMU2_IOC_GPIO0C_IOMUX_SEL_H_VA);
    //iounmap(VCCIO1_4_IOC_GPIO0C_DS_L_VA);
    iounmap(GPIO_SWPORT_DR_H_VA);
    iounmap(GPIO_SWPORT_DDR_H_VA);
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
    //设置为gpio功能
    val = readl(PMU2_IOC_GPIO0C_IOMUX_SEL_H_VA);
    val &= ~(0x00F0<<0);
    val |= (0x00F0<<0) | (0x0<<0);

    writel(val, PMU2_IOC_GPIO0C_IOMUX_SEL_H_VA);
    //设置gpio 40ohm的驱动能力
    // val = readl(VCCIO1_4_IOC_GPIO0C_DS_L_VA);
    // val &= ~(0x3<<24);
    // val |= (0x1<<24);

    // writel(val, VCCIO1_4_IOC_GPIO0C_DS_L_VA);
    //设置gpio为输出功能
    val = readl(GPIO_SWPORT_DDR_H_VA);
    val &= ~(0X20<<0);
    val |= (0X20<<16) | (0X20<<0);
    writel(val, GPIO_SWPORT_DDR_H_VA);

    //设置gpio默认输出低电平，led开启
    val = readl(GPIO_SWPORT_DR_H_VA);
    val &= ~(0X20<<0);
    val |= (0X20<<16);
    writel(val, GPIO_SWPORT_DR_H_VA);

    retvalue = register_chrdev(LED_MAJOR, LED_NAME, &led_fops);
    if(retvalue < 0)
    {
        printk("led driver register failed!\r\n");
        goto fail_map; 
    }
    return 0;
    
fail_map:
    led_unmap();
    return -EIO;

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
MODULE_INFO(intree, "Y");
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

#define PMU_GRF_BASE                       (0xFD5F8000)
#define PMU2_IOC                           (0xFD5F4000)
//#define VCCIO1_4_BASE                     (0xFD5F8000)
#define GPIO0_BASE                         (0xFD8A0000)


#define PMU2_IOC_GPIO0C_IOMUX_SEL_H       (PMU2_IOC + 0x0008) 
//#define VCCIO1_4_IOC_GPIO0C_DS_L        (VCCIO1_4_BASE + 0x0020) 
#define GPIO_SWPORT_DR_H                  (GPIO0_BASE + 0X0004) 
#define GPIO_SWPORT_DDR_H                 (GPIO0_BASE + 0X000C)

static void led_release(void)
{
    printk("led driver release!\r\n");
}


//define resource structure
static struct resource led_resource[] = {
    [0] = {
        .start  = PMU2_IOC_GPIO0C_IOMUX_SEL_H,
        .end    = PMU2_IOC_GPIO0C_IOMUX_SEL_H + 4 - 1,
        .flags  = IORESOURCE_MEM,
    },
    
    [1] = {
        .start  = GPIO_SWPORT_DR_H,
        .end    = GPIO_SWPORT_DR_H + 4 - 1,
        .flags  = IORESOURCE_MEM,
    },
    [2] = {
        .start  = GPIO_SWPORT_DDR_H,
        .end    = GPIO_SWPORT_DDR_H + 4 - 1,
        .flags  = IORESOURCE_MEM,
    },  
};

//platform driver structure
static struct platform_driver leddevice = {
    
    .name = "rk3588-led",
    .id = -1,
    .driver = {
        .owner = THIS_MODULE,
    },
    .num_resources = ARRAY_SIZE(led_resource),
    .resource = led_resource,
};

static int __init leddevice_init(void)
{
    return platform_driver_register(&leddevice);
}

static void __exit leddevice_exit(void)
{
    platform_driver_unregister(&leddevice);
}

module_init(leddevice_init);
module_exit(leddevice_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("chen");
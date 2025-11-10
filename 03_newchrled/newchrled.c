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
#include <asm/io.h>
/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
文件名		: led.c
作者	  	: 正点原子
版本	   	: V1.0
描述	   	: LED驱动文件。
其他	   	: 无
论坛 	   	: www.openedv.com
日志	   	: 初版V1.0 2022/12/02 正点原子团队创建
***************************************************************/
#define NEWCHRLED_CNT   1               /*设备号个数 */
#define NEWCHRLED_NAME  "newchrled"     /*名字 */
#define LEDOFF 	0				        /* 关灯 */
#define LEDON 	1				        /* 开灯 */

// #define BUS_IOC_BASE						(0xFD5F8000)
// #define BUS_IOC_GPIO1A_IOMUX_SEL_L			(BUS_IOC_BASE + 0x0020)
// #define VCCIO1_4_BASE						(0xFD5F9000)
// #define VCCIO1_4_IOC_GPIO1A_DS_L			(VCCIO1_4_BASE + 0x0020)

// #define GPIO1_BASE						(0xFEC20000)
// #define GPIO_SWPORT_DR_L 				(GPIO1_BASE + 0X0000)
// #define GPIO_SWPORT_DDR_L				(GPIO1_BASE + 0X0008)

// /* 映射后的寄存器虚拟地址指针 */
// static void __iomem *BUS_IOC_GPIO1A_IOMUX_SEL_L_PI;
// static void __iomem *VCCIO1_4_IOC_GPIO1A_DS_L_PI;
// static void __iomem *GPIO_SWPORT_DR_L_PI;
// static void __iomem *GPIO_SWPORT_DDR_L_PI;


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

/* newchrled设备结构体*/
struct newchrled_dev{
    dev_t devid;            /* 设备号 */
    struct cdev cdev;       /* cdev */
    struct class *class;    /* 类 */
    struct device *device;  /* 设备 */
    int major;              /* 主设备 */
    int minor;              /* 次设备号 */
};

struct newchrled_dev newchrled; /* led设备 */

/*
 * @description		: LED打开/关闭
 * @param - sta 	: LEDON(0) 打开LED，LEDOFF(1) 关闭LED
 * @return 			: 无
 */
void led_switch(u8 sta)
{
	u32 val = 0;
	if(sta == LEDON) {
		// val = readl(GPIO_SWPORT_DR_H_VA);
		// val &= ~(0X8 << 0); /* bit3 清零*/
		// val |= ((0X8 << 16) | (0X8 << 0));	/* bit19 置1，允许写bit3，
	 	// 				   				   bit3，高电平*/

        val = readl(GPIO_SWPORT_DR_H_VA);
        val &= ~(0X20<<0);
        val |= (0X20<<16) | (0X20<<0);
		writel(val, GPIO_SWPORT_DR_H_VA);
	}else if(sta == LEDOFF) { 
		// writel(val, GPIO_SWPORT_DR_H_VA);
		// val = readl(GPIO_SWPORT_DR_H_VA);
		// val &= ~(0X8 << 0); /* bit3 清零*/
		// val |= (0X8 << 16);	/* bit19 置1，允许写bit3，
	 	// 				   				   bit3，低电平	*/
		// writel(val, GPIO_SWPORT_DR_H_VA);

        writel(val, GPIO_SWPORT_DR_H_VA);
        val = readl(GPIO_SWPORT_DR_H_VA);
        val &= ~(0X20<<0);
        val |= (0X20<<16);
        writel(val, GPIO_SWPORT_DR_H_VA);
	}	
}

/*
 * @description		: 物理地址映射
 * @return 			: 无
 */
void led_remap(void)
{
  	PMU2_IOC_GPIO0C_IOMUX_SEL_H_VA = ioremap(PMU2_IOC_GPIO0C_IOMUX_SEL_H, 4);
    //VCCIO1_4_IOC_GPIO0C_DS_L_VA = ioremap(VCCIO1_4_IOC_GPIO0C_DS_L, 4);
    GPIO_SWPORT_DR_H_VA = ioremap(GPIO_SWPORT_DR_H, 4);
    GPIO_SWPORT_DDR_H_VA = ioremap(GPIO_SWPORT_DDR_H, 4);
}

/*
 * @description		: 取消映射
 * @return 			: 无
 */
void led_unmap(void)
{
	/* 取消映射 */
	iounmap(PMU2_IOC_GPIO0C_IOMUX_SEL_H_VA);
	//iounmap(VCCIO1_4_IOC_GPIO0C_DS_L_VA);
	iounmap(GPIO_SWPORT_DR_H_VA);
	iounmap(GPIO_SWPORT_DDR_H_VA);
}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int led_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &newchrled;      /*设置私有数据 */
	return 0;
}

/*
 * @description		: 从设备读取数据 
 * @param - filp 	: 要打开的设备文件(文件描述符)
 * @param - buf 	: 返回给用户空间的数据缓冲区
 * @param - cnt 	: 要读取的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 读取的字节数，如果为负值，表示读取失败
 */
static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	return 0;
}

/*
 * @description		: 向设备写数据 
 * @param - filp 	: 设备文件，表示打开的文件描述符
 * @param - buf 	: 要写给设备写入的数据
 * @param - cnt 	: 要写入的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	int retvalue;
	unsigned char databuf[1];
	unsigned char ledstat;

	retvalue = copy_from_user(databuf, buf, cnt);
	if(retvalue < 0) {
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	ledstat = databuf[0];		/* 获取状态值 */

	if(ledstat == LEDON) {	
		led_switch(LEDON);		/* 打开LED灯 */
	} else if(ledstat == LEDOFF) {
		led_switch(LEDOFF);		/* 关闭LED灯 */
	}
	return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int led_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* 设备操作函数 */
static struct file_operations newchrled_fops = {
	.owner = THIS_MODULE,
	.open = led_open,
	.read = led_read,
	.write = led_write,
	.release = 	led_release,
};

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init led_init(void)
{
	int ret = 0;
	u32 val = 0;

	/* 初始化LED */
	/* 1、寄存器地址映射 */
	led_remap();

	// /* 2、设置GPIO1_A3为GPIO功能。*/
	// val = readl(BUS_IOC_GPIO1A_IOMUX_SEL_L_PI);
	// val &= ~(0XF000 << 0);	/* bit15:12，清零 */
	// val |= ((0XF000 << 16) | (0X0 << 0));	/* bit31:28 置1，允许写bit15:12，
	//  					   				   bit15:12：0，用作GPIO1_A3	*/
	// writel(val, BUS_IOC_GPIO1A_IOMUX_SEL_L_PI);

	// /* 3、设置GPIO0_C0驱动能力为40ohm */
	// val = readl(VCCIO1_4_IOC_GPIO1A_DS_L_PI);
	// val &= ~(0X7000 << 0);	/* bit14:12清零*/
	// val |= ((0X7000 << 16) | (0X6000 << 0));	/* bit30:28 置1，允许写bit14:12，
	//  					   				   bit14:12：110，用作GPIO1_A3	*/
	// writel(val, VCCIO1_4_IOC_GPIO1A_DS_L_PI);

	// /* 4、设置GPIO1_A3为输出 */
	// val = readl(GPIO_SWPORT_DDR_L_PI);
	// val &= ~(0X8 << 0); /* bit3 清零*/
	// val |= ((0X8 << 16) | (0X8 << 0));	/* bit19 置1，允许写bit3，
	//  					   				   bit3，高电平	*/
	// writel(val, GPIO_SWPORT_DDR_L_PI);

	// /* 5、设置GPIO1_A3为低电平，关闭LED灯。*/
	// val = readl(GPIO_SWPORT_DR_L_PI);
	// val &= ~(0X8 << 0); /* bit3 清零*/
	// val |= (0X8 << 16);	/* bit19 置1，允许写bit3，
	//  					   				   bit3，低电平	*/
	// writel(val, GPIO_SWPORT_DR_L_PI);

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

    //设置gpio默认输出高电平，led关闭
    val = readl(GPIO_SWPORT_DR_H_VA);
    val &= ~(0X20<<0);
    val |= (0X20<<16);
    writel(val, GPIO_SWPORT_DR_H_VA);

	/* 注册字符设备驱动 */
    /* 1、创建设备号*/
    if(newchrled.major){
        newchrled.devid = MKDEV(newchrled.major, 0);    /*  定义了设备号 */
        ret = register_chrdev_region(newchrled.devid, NEWCHRLED_CNT, NEWCHRLED_NAME);
        if(ret < 0){
            pr_err("cannot register %s char driver [ret=%d]\n",NEWCHRLED_NAME, NEWCHRLED_CNT);
            goto fail_map;
        }
    }else{          /* 没有定义设备号 */
        ret = alloc_chrdev_region(&newchrled.devid, 0, NEWCHRLED_CNT, NEWCHRLED_NAME);
        if(ret < 0){
            pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n",NEWCHRLED_NAME, ret);
            goto fail_map;
        }
        newchrled.major = MAJOR(newchrled.devid);   /* 获取主设备号 */
        newchrled.minor = MINOR(newchrled.devid);   /* 获取次设备号 */
    }
    printk("newchrled major=%d,minor=%d\r\n",newchrled.major, newchrled.minor);

    /* 2、初始化cdev */
    newchrled.cdev.owner = THIS_MODULE;
    cdev_init(&newchrled.cdev, &newchrled_fops);

    /* 3、添加一个cdev */
    ret = cdev_add(&newchrled.cdev, newchrled.devid, NEWCHRLED_CNT);
    if(ret < 0){
        goto del_unregister;
    }

    /* 4、创建类 */
    newchrled.class = class_create(THIS_MODULE, NEWCHRLED_NAME);
    if(IS_ERR(newchrled.class)){
        goto del_cdev;
    }

    /* 5、创建设备 */
    newchrled.device = device_create(newchrled.class, NULL, newchrled.devid, NULL, NEWCHRLED_NAME);
    if(IS_ERR(newchrled.device)){
        goto destroy_class;
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
    return -EIO;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit led_exit(void)
{
	/* 取消映射 */
	led_unmap();

	/* 注销字符设备驱动 */
    cdev_del(&newchrled.cdev);
    unregister_chrdev_region(newchrled.devid, NEWCHRLED_CNT);
    device_destroy(newchrled.class, newchrled.devid);
    class_destroy(newchrled.class);
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("CHEN");
MODULE_INFO(intree, "Y");
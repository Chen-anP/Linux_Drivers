#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/moduleparam.h>

#define NEWCHRLED_CNT 1
#define NEWCHRLED_NAME "newchrled"

#define LEDOFF 0
#define LEDON 1

/* Physical addresses (board-specific). Confirm these for your board/BSP. */
#define PMU_GRF_BASE                       (0xFD5F8000)
#define PMU2_IOC                           (0xFD5F4000)
#define GPIO0_BASE                         (0xFD8A0000)

#define PMU2_IOC_GPIO0C_IOMUX_SEL_H       (PMU2_IOC + 0x0008)
#define GPIO_SWPORT_DR_H                  (GPIO0_BASE + 0x0004)
#define GPIO_SWPORT_DDR_H                 (GPIO0_BASE + 0x000C)

/* use bit 5 as LED pin (mask) - adjust if your board uses another pin */
#define LED_PIN_SHIFT 5
#define LED_PIN_MASK  (1U << LED_PIN_SHIFT)

static void __iomem *PMU2_IOC_GPIO0C_IOMUX_SEL_H_VA;
static void __iomem *GPIO_SWPORT_DR_H_VA;
static void __iomem *GPIO_SWPORT_DDR_H_VA;

struct newchrled_dev {
    dev_t devid;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    int major; /* settable as module parameter (0 for dynamic) */
    int minor;
};

static struct newchrled_dev newchrled;
module_param(newchrled.major, int, S_IRUGO);
MODULE_PARM_DESC(newchrled.major, "Major device number (0 = dynamic)");

static void led_unmap(void)
{
    if (PMU2_IOC_GPIO0C_IOMUX_SEL_H_VA) {
        iounmap(PMU2_IOC_GPIO0C_IOMUX_SEL_H_VA);
        release_mem_region(PMU2_IOC_GPIO0C_IOMUX_SEL_H, 4);
        PMU2_IOC_GPIO0C_IOMUX_SEL_H_VA = NULL;
    }
    if (GPIO_SWPORT_DR_H_VA) {
        iounmap(GPIO_SWPORT_DR_H_VA);
        release_mem_region(GPIO_SWPORT_DR_H, 4);
        GPIO_SWPORT_DR_H_VA = NULL;
    }
    if (GPIO_SWPORT_DDR_H_VA) {
        iounmap(GPIO_SWPORT_DDR_H_VA);
        release_mem_region(GPIO_SWPORT_DDR_H, 4);
        GPIO_SWPORT_DDR_H_VA = NULL;
    }
}

static int led_remap(void)
{
    if (!request_mem_region(PMU2_IOC_GPIO0C_IOMUX_SEL_H, 4, NEWCHRLED_NAME)) {
        pr_err("newchrled: request_mem_region PMU2_IOC_GPIO0C_IOMUX_SEL_H failed\n");
        goto err;
    }
    PMU2_IOC_GPIO0C_IOMUX_SEL_H_VA = ioremap(PMU2_IOC_GPIO0C_IOMUX_SEL_H, 4);
    if (IS_ERR(PMU2_IOC_GPIO0C_IOMUX_SEL_H_VA) || !PMU2_IOC_GPIO0C_IOMUX_SEL_H_VA) {
        pr_err("newchrled: ioremap PMU2_IOC_GPIO0C_IOMUX_SEL_H failed\n");
        goto err;
    }

    if (!request_mem_region(GPIO_SWPORT_DR_H, 4, NEWCHRLED_NAME)) {
        pr_err("newchrled: request_mem_region GPIO_SWPORT_DR_H failed\n");
        goto err;
    }
    GPIO_SWPORT_DR_H_VA = ioremap(GPIO_SWPORT_DR_H, 4);
    if (IS_ERR(GPIO_SWPORT_DR_H_VA) || !GPIO_SWPORT_DR_H_VA) {
        pr_err("newchrled: ioremap GPIO_SWPORT_DR_H failed\n");
        goto err;
    }

    if (!request_mem_region(GPIO_SWPORT_DDR_H, 4, NEWCHRLED_NAME)) {
        pr_err("newchrled: request_mem_region GPIO_SWPORT_DDR_H failed\n");
        goto err;
    }
    GPIO_SWPORT_DDR_H_VA = ioremap(GPIO_SWPORT_DDR_H, 4);
    if (IS_ERR(GPIO_SWPORT_DDR_H_VA) || !GPIO_SWPORT_DDR_H_VA) {
        pr_err("newchrled: ioremap GPIO_SWPORT_DDR_H failed\n");
        goto err;
    }

    return 0;
err:
    led_unmap();
    return -ENOMEM;
}

static void led_switch(u8 state)
{
    u32 val;

    if (!GPIO_SWPORT_DR_H_VA)
        return;

    val = readl(GPIO_SWPORT_DR_H_VA);

    if (state == LEDON) {
        /* clear output bit then set output value and set write-enable (bit16) pattern */
        val &= ~LED_PIN_MASK;
        /* set bit16 write-mask and set bit value to 1 */
        val |= ((LED_PIN_MASK) << 16) | LED_PIN_MASK;
        writel(val, GPIO_SWPORT_DR_H_VA);
    } else { /* LEDOFF */
        val &= ~LED_PIN_MASK;
        /* set write-mask only (bit16) leaving output 0 -> high or off depending on hw wiring */
        val |= ((LED_PIN_MASK) << 16);
        writel(val, GPIO_SWPORT_DR_H_VA);
    }
}

static int led_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &newchrled;
    return 0;
}

static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int ret;
    unsigned char databuf[1];

    if (cnt == 0)
        return -EINVAL;

    if (copy_from_user(databuf, buf, 1)) {
        pr_err("newchrled: copy_from_user failed\n");
        return -EFAULT;
    }

    if (databuf[0] == LEDON)
        led_switch(LEDON);
    else
        led_switch(LEDOFF);

    return 1; /* number of bytes consumed */
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

    pr_info("newchrled: init start\n");

    /* remap registers */
    ret = led_remap();
    if (ret) {
        pr_err("newchrled: led_remap failed\n");
        return ret;
    }
    pr_info("newchrled: ioremap OK\n");

    /* configure mux to gpio function - check pointer */
    {
        u32 val = readl(PMU2_IOC_GPIO0C_IOMUX_SEL_H_VA);
        val &= ~(0x00F0U << 0);
        val |= (0x00F0U << 0) | (0x0U << 0);
        writel(val, PMU2_IOC_GPIO0C_IOMUX_SEL_H_VA);
    }

    /* set gpio as output */
    {
        u32 val = readl(GPIO_SWPORT_DDR_H_VA);
        val &= ~LED_PIN_MASK;
        val |= ((LED_PIN_MASK) << 16) | LED_PIN_MASK;
        writel(val, GPIO_SWPORT_DDR_H_VA);
    }

    /* set default output (LED off) */
    {
        u32 val = readl(GPIO_SWPORT_DR_H_VA);
        val &= ~LED_PIN_MASK;
        val |= ((LED_PIN_MASK) << 16);
        writel(val, GPIO_SWPORT_DR_H_VA);
    }

    /* register chrdev region */
    if (newchrled.major) {
        newchrled.devid = MKDEV(newchrled.major, 0);
        ret = register_chrdev_region(newchrled.devid, NEWCHRLED_CNT, NEWCHRLED_NAME);
        if (ret < 0) {
            pr_err("newchrled: register_chrdev_region failed %d\n", ret);
            goto out_unmap;
        }
    } else {
        ret = alloc_chrdev_region(&newchrled.devid, 0, NEWCHRLED_CNT, NEWCHRLED_NAME);
        if (ret < 0) {
            pr_err("newchrled: alloc_chrdev_region failed %d\n", ret);
            goto out_unmap;
        }
        newchrled.major = MAJOR(newchrled.devid);
        newchrled.minor = MINOR(newchrled.devid);
    }
    pr_info("newchrled: major=%d, minor=%d\n", newchrled.major, newchrled.minor);

    /* cdev setup */
    cdev_init(&newchrled.cdev, &led_fops);
    newchrled.cdev.owner = THIS_MODULE;
    ret = cdev_add(&newchrled.cdev, newchrled.devid, NEWCHRLED_CNT);
    if (ret) {
        pr_err("newchrled: cdev_add failed %d\n", ret);
        goto out_unregister;
    }

    /* class: use a unique class name based on the assigned major to avoid name collisions */
    {
        char cls_name[32];
        snprintf(cls_name, sizeof(cls_name), NEWCHRLED_NAME "%d", newchrled.major);
        newchrled.class = class_create(THIS_MODULE, cls_name);
        if (IS_ERR(newchrled.class)) {
            pr_err("newchrled: class_create failed %d\n", (int)PTR_ERR(newchrled.class));
            ret = PTR_ERR(newchrled.class);
            goto out_del_cdev;
        }
    }

    /* device: keep device node name as NEWCHRLED_NAME so /dev/newchrled is created */
    newchrled.device = device_create(newchrled.class, NULL, newchrled.devid, NULL, NEWCHRLED_NAME);
    if (IS_ERR(newchrled.device)) {
        pr_err("newchrled: device_create failed %ld\n", PTR_ERR(newchrled.device));
        ret = PTR_ERR(newchrled.device);
        goto out_destroy_class;
    }

    pr_info("newchrled: device created OK\n");
    return 0;

out_destroy_class:
    class_destroy(newchrled.class);
out_del_cdev:
    cdev_del(&newchrled.cdev);
out_unregister:
    unregister_chrdev_region(newchrled.devid, NEWCHRLED_CNT);
out_unmap:
    led_unmap();
    return ret ?: -EIO;
}

static void __exit led_exit(void)
{
    pr_info("newchrled: exit\n");

    if (newchrled.device && !IS_ERR(newchrled.device))
        device_destroy(newchrled.class, newchrled.devid);
    if (newchrled.class && !IS_ERR(newchrled.class))
        class_destroy(newchrled.class);

    cdev_del(&newchrled.cdev);
    unregister_chrdev_region(newchrled.devid, NEWCHRLED_CNT);

    led_unmap();
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("chen");
MODULE_DESCRIPTION("Safe newchrled character driver with checks and debug prints");

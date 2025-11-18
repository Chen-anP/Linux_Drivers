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


#define KEY_CNT   1               /*设备号个数 */
#define KEY_NAME  "key"           /*名字 */

#define KEY0VALUE	0xF0		/* 按键0按下对应的键值 */
#define INVAKEY     0x00		/* 无效的键值 */

struct key_dev{
    dev_t devid;            /* 设备号 */
    struct cdev cdev;       /* cdev */
    struct class *class;    /* 类 */
    struct device *device;  /* 设备 */
    int major;              /* 主设备 */
    int minor;              /* 次设备号 */
    int key_gpio;           /* 按键所使用的GPIO编号 */
    atomic_t keyvalue;      /* 原子变量，按键值 */
};

struct key_dev keydev; /* key设备 */

static int keyio_init(void)
{
    int ret;
    
    const char *str;

    /* 获取设备节点 */
    keydev.nd = of_find_node_by_path("/key");
    if(keydev.nd == NULL){
        printk("key node not found!\r\n");
        return -EINVAL;
    }else{
        printk("key node found!\r\n");
    }
    
    //获取status属性内容
    ret = of_property_read_string(keydev.nd, "status", &str);
    if(ret < 0){
        printk("status read failed!\r\n");
    }
    else{
        printk("status = %s\r\n", str);
    }
    //获取compatible属性内容
    ret = of_property_read_string(keydev.nd, "compatible", &str);
    if(ret < 0){    
        printk("compatible read failed!\r\n");
    }
    else{
        printk("compatible = %s\r\n", str);
    }
    //获取key所使用的GPIO编号
    keydev.key_gpio = of_get_named_gpio(keydev.nd, "key-gpio", 0);
    if(keydev.key_gpio < 0){
        printk("can't get key gpio\r\n");
        return -EINVAL;
    }
    printk("key gpio num = %d\r\n", keydev.key_gpio);
    //申请GPIO
    ret = gpio_request(keydev.key_gpio, "key-gpio");
    if(ret){
        printk("gpio_request failed %d\r\n", ret);
        return -EINVAL;
    }
    //设置GPIO为输入
    ret = gpio_direction_input(keydev.key_gpio);
    if(ret){
        printk("gpio_direction_input failed %d\r\n", ret);
        goto fail_gpio;
    }

    return 0;
fail_gpio:
    gpio_free(keydev.key_gpio);
    return -EINVAL;
}

static int key_open(struct inode *inode, struct file *filp)
{
    int ret =- 0;
    filp->private_data = &keydev; /* 设置私有数据 */
    
    ret = keyio_init();
    if(ret < 0){
        return ret;
    }
    return 0;
}

static ssize_t key_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    int ret = 0;
    int value;
    struct key_dev *dev = (struct key_dev *)filp->private_data;

    if(gpio_get_value(dev->key_gpio) == 1){  /* 按键按下 */
        while(gpio_get_value(dev->key_gpio) == 1); /* 按键消抖 */
        atomic_set(&dev->keyvalue, KEY0VALUE);
    }else{
        atomic_set(&dev->keyvalue, INVAKEY);
    }
    value = atomic_read(&dev->keyvalue);

    ret = copy_to_user(buf, &value, sizeof(value));
    if(ret < 0){
        return -EFAULT;
    }
    return ret;
}

static ssize_t key_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    return 0;
}

static int key_release(struct inode *inode, struct file *filp)
{
    struct key_dev *dev = (struct key_dev *)filp->private_data;
    gpio_free(dev->key_gpio);
    return 0;
}

static struct file_operations key_fops = {
    .owner = THIS_MODULE,
    .open = key_open,
    .read = key_read,
    .write = key_write,
    .release = key_release,
};

static int __init key_init(void)
{
    int ret = 0;

    atomic_set(&keydev.keyvalue, INVAKEY);

    /* 注册字符设备驱动 */
    /* 1、创建设备号*/
    if(keydev.major){
        keydev.devid = MKDEV(keydev.major, 0);    /*  定义了设备号 */
        ret = register_chrdev_region(keydev.devid, KEY_CNT, KEY_NAME);
        if(ret < 0){
            pr_err("cannot register %s char driver [ret=%d]\n",KEY_NAME, KEY_CNT);
            goto fail;
        }
    }else{          /* 没有定义设备号 */
        ret = alloc_chrdev_region(&keydev.devid, 0, KEY_CNT, KEY_NAME);
        if(ret < 0){
            pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n",KEY_NAME, ret);
            goto fail;
        }
        keydev.major = MAJOR(keydev.devid);   /* 获取主设备号 */
        keydev.minor = MINOR(keydev.devid);   /* 获取次设备号 */
    }
    printk("key major=%d,minor=%d\r\n",keydev.major, keydev.minor);

    /* 2、初始化cdev */
    keydev.cdev.owner = THIS_MODULE;
    cdev_init(&keydev.cdev, &key_fops);

    /* 3、添加一个cdev */
    ret = cdev_add(&keydev.cdev, keydev.devid, KEY_CNT);
    if(ret < 0){
        goto del_unregister;
    }

    /* 4、创建类 */
    keydev.class = class_create(THIS_MODULE, KEY_NAME);
    if(IS_ERR(keydev.class)){
        goto del_cdev;
    }

    /* 5、创建设备 */
    keydev.device = device_create(keydev.class, NULL, keydev.devid, NULL, KEY_NAME);
    if(IS_ERR(keydev.device)){
        goto destroy_class;
    }

    return 0;
destroy_class:
    class_destroy(keydev.class);
del_cdev:
    cdev_del(&keydev.cdev);
del_unregister:
    unregister_chrdev_region(keydev.devid, KEY_CNT);
fail:
    return -EIO;
}

static void __exit key_exit(void)
{
    /* 注销字符设备驱动 */
    device_destroy(keydev.class, keydev.devid);
    class_destroy(keydev.class);
    cdev_del(&keydev.cdev);
    unregister_chrdev_region(keydev.devid, KEY_CNT);
}

module_init(key_init);
module_exit(key_exit);
MODULE_LICENSE("GPL");

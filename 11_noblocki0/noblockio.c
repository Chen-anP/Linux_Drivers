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
//#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define KEY_CNT   1               /*设备号个数 */
#define KEY_NAME  "key"           /*名字 */

enum key_status{
    KEY_PRESS = 0,
    KEY_RELEASE,
    KEY_KEEP,
};


struct key_dev{
    dev_t devid;            /* 设备号 */
    struct cdev cdev;       /* cdev */
    struct class *class;    /* 类 */
    struct device *device;  /* 设备 */
    int major;              /* 主设备 */
    int minor;              /* 次设备号 */
    int key_gpio;           /* 按键所使用的GPIO编号 */
    struct device_node *nd; /* 设备节点 */
    struct timer_list timer; /* 按键定时器 */
    int irq_num;          /* 中断号 */
    spinlock_t spinlock;      /* 自旋锁 */

    atomic_t status;        /* 按键状态 */
    wait_queue_head_t r_wait; /* 等待队列头 */
};


static struct key_dev key; /* key设备 */
static int status = KEY_KEEP;


static irqreturn_t key_interrupt(int irq, void *dev_id)
{
    /*防抖*/
    mod_timer(&key.timer, jiffies + msecs_to_jiffies(15));
    
    return IRQ_HANDLED;
}


static int key_parse_dt(void)
{
    int ret;
    const char *str;

    /* 获取设备节点 */
    key.nd = of_find_node_by_path("/key");
    if(key.nd == NULL){
        printk("key node not found!\r\n");
        return -EINVAL;
    }else{
        printk("key node found!\r\n");
    }
    
    //获取status属性内容
    ret = of_property_read_string(key.nd, "status", &str);
    if(ret < 0){
        printk("status read failed!\r\n");
    }
    else{
        printk("status = %s\r\n", str);
    }
    //获取compatible属性内容
    ret = of_property_read_string(key.nd, "compatible", &str);
    if(ret < 0){
        printk("compatible read failed!\r\n");
    }
    else{
        printk("compatible = %s\r\n", str);
    }

    //获取key所使用的GPIO编号
    key.key_gpio = of_get_named_gpio(key.nd, "key-gpio", 0);
    if(key.key_gpio < 0){
        printk("can't get key gpio\r\n");
        return -EINVAL;
    }

    //获取中断号
    key.irq_num = irq_of_parse_and_map(key.nd, 0);
    if(key.irq_num < 0){
        printk("irq_of_parse_and_map failed!\r\n");
        return -EINVAL;
    }

    printk("key gpio num = %d\r\n", key.key_gpio);

    return 0;
}

static int key_gpio_init(void)
{
    int ret;
    unsigned long irq_flags;

    //申请GPIO
    ret = gpio_request(key.key_gpio, "key-gpio");
    if(ret){
        printk("gpio_request failed %d\r\n", ret);
        return -EINVAL;
    }
    //设置GPIO为输入
    ret = gpio_direction_input(key.key_gpio);
    if(ret){
        printk("gpio_direction_input failed %d\r\n", ret);
        goto fail_gpio;
    }

    //获取设备树中指定的中断触发类型
    irq_flags = irq_get_trigger_type(key.irq_num);
    //申请中断
    ret = request_irq(key.irq_num, key_interrupt, irq_flags, "key-gpio", NULL);
    if(ret){
        printk("request_irq failed %d\r\n", ret);
        goto fail_gpio;
    }

    return 0;
fail_gpio:
    gpio_free(key.key_gpio);
    return -EINVAL;
}

static void key_timer_function(struct timer_list *t)
{
    static int last_val = 0;
    int current_val;

    current_val = gpio_get_value(key.key_gpio);
    if (1 ==current_val && !last_val)
    {
        atomic_set(&key.status, KEY_PRESS);
        wake_up_interruptible(&key.r_wait);
    }
    else if (0 == current_val && last_val)
    {
        atomic_set(&key.status, KEY_RELEASE);
        wake_up_interruptible(&key.r_wait);
    }
    else
    {
        atomic_set(&key.status, KEY_KEEP);
    }
    last_val = current_val;
}

static int key_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &key;

    return 0;
}

static ssize_t key_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    int ret;

    if (filp->flags & O_NOBLOCK)
    {
        if (atomic_read(&key.status) == KEY_KEEP)
        {
            return -EAGAIN;
        }
    }
    else
    {
        /* 阻塞方式 */
        ret = wait_event_interruptible(key.r_wait, atomic_read(&key.status) != KEY_KEEP);
        if (ret)
        {
            return ret;
        }
    }
    

    ret = copy_to_user(buf, &key.status, sizeof(key.status));
    atomic_set(&key.status, KEY_KEEP);
    if(ret < 0){
        return -EFAULT;
    }
    return sizeof(key.status);
}


static unsigned int key_poll(struct file *filp, struct poll_table_struct *wait)
{
    unsigned int mask = 0;
    struct key_dev *dev = filp->private_data;

    poll_wait(filp, &dev->r_wait, wait);

    if (atomic_read(&dev->status) != KEY_KEEP)
    {
        mask |= POLLIN | POLLRDNORM; /* 可读 */
    }

    return mask;
}

static ssize_t key_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    return 0;
}

static int key_release(struct inode *inode, struct file *filp)
{
    struct key_dev *dev = (struct key_dev *)filp->private_data;
    free_irq(dev->irq_num, NULL);
    gpio_free(dev->key_gpio);
    return 0;
}

static struct file_operations key_fops = {
    .owner = THIS_MODULE,
    .open = key_open,
    .read = key_read,
    .write = key_write,
    .release = key_release,
    .poll = key_poll,
};


static int __init mykey_init(void)
{
    int ret = 0;

    init_waitqueue_head(&key.r_wait);

    atomic_set(&key.keyvalue, INVAKEY);

    

    /* 解析设备树 */
    ret = key_parse_dt();
    if(ret < 0){
        return ret;
    }

    /* 初始化按键所使用的GPIO及中断 */
    ret = key_gpio_init();
    if(ret < 0){
        return ret;
    }

    /* 初始化定时器 */
    timer_setup(&key.timer, key_timer_function, 0);

    /* 注册字符设备驱动 */
    /* 1、创建设备号*/
    if(key.major){
        key.devid = MKDEV(key.major, 0);    /*  定义了设备号 */
        ret = register_chrdev_region(key.devid, KEY_CNT, KEY_NAME);
        if(ret < 0){
            pr_err("cannot register %s char driver [ret=%d]\n",KEY_NAME, KEY_CNT);
            goto fail;
        }
    }else{          /* 没有定义设备号 */
        ret = alloc_chrdev_region(&key.devid, 0, KEY_CNT, KEY_NAME);
        if(ret < 0){
            pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n",KEY_NAME, ret);
            goto fail;
        }
        key.major = MAJOR(key.devid);   /* 获取主设备号 */
        key.minor = MINOR(key.devid);   /* 获取次设备号 */
    }
    printk("key major=%d,minor=%d\r\n",key.major, key.minor);

    /* 2、初始化cdev */
    key.cdev.owner = THIS_MODULE;
    cdev_init(&key.cdev, &key_fops);

    /* 3、添加一个cdev */
    ret = cdev_add(&key.cdev, key.devid, KEY_CNT);
    if(ret < 0){
        goto del_unregister;
    }

    /* 4、创建类 */
    key.class = class_create(THIS_MODULE, KEY_NAME);
    if(IS_ERR(key.class)){
        goto del_cdev;
    }

    /* 5、创建设备 */
    key.device = device_create(key.class, NULL, key.devid, NULL, KEY_NAME);
    if(IS_ERR(key.device)){
        goto destroy_class;
    }

    spin_lock_init(&key.spinlock);

    return 0;
destroy_class:
    class_destroy(key.class);
del_cdev:
    cdev_del(&key.cdev);
del_unregister:
    unregister_chrdev_region(key.devid, KEY_CNT);
fail:
    del_timer_sync(&key.timer);
    free_irq(key.irq_num, NULL);
    gpio_free(key.key_gpio);
    return -EIO;
}

static void __exit key_exit(void)
{
    /* 注销字符设备驱动 */
    device_destroy(key.class, key.devid);
    class_destroy(key.class);
    cdev_del(&key.cdev);
    unregister_chrdev_region(key.devid, KEY_CNT);

    del_timer_sync(&key.timer);
    free_irq(key.irq_num, NULL);
    gpio_free(key.key_gpio);
}


module_init(mykey_init);
module_exit(key_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chen");

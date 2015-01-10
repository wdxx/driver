#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <mach/regs-gpio.h>
#include <linux/uaccess.h>		/* For copy_to_user/put_user/... */

#include <plat/devs.h>
#include <plat/gpio-cfg.h>

struct button_irq_desc{
	int irq;	//irq number
	unsigned long flags;	// interrupt flags
	char *name;	//interrupt name
};

static struct button_irq_desc button_irqs[] = {
	{IRQ_EINT0, IRQF_TRIGGER_FALLING, "KEY4"},
	{IRQ_EINT1, IRQF_TRIGGER_FALLING, "KEY1"},
	{IRQ_EINT2, IRQF_TRIGGER_FALLING, "KEY3"},
	{IRQ_EINT4, IRQF_TRIGGER_FALLING, "KEY2"},	
};


#define TQ2440_BUTTONS_NAME		"tq_buttons"
static int buttons_major = 0;
static struct class *tq_btn_class;

static struct cdev cdev_btn = {
	.owner = THIS_MODULE,
};

static volatile int ev_press = 0;
static int press_cnt[4];
//static struct wait_queue_head_t wq;
static DECLARE_WAIT_QUEUE_HEAD(wq);
irqreturn_t tq2440_buttons_irq(int irq, void *dev_id)
{
	volatile int *press_cnt = (volatile int *)dev_id;

	*press_cnt = *press_cnt+1;
	ev_press = 1;
	wake_up_interruptible(&wq);
	return IRQ_HANDLED;;
}

int tq_buttons_release(struct inode *inode, struct file *fp)
{
	int i = 0;
	for (i=0; i < sizeof(button_irqs)/sizeof(button_irqs[0]); i++)
		free_irq(button_irqs[i].irq, (void *)&press_cnt[i]);
	return 0;
}

ssize_t tq_buttons_read(struct file *fp, char __user *buffer, size_t count, loff_t *loft)
{
	//int key_val = 0;
	int err = 0;
	wait_event_interruptible(wq, ev_press);
	ev_press = 0;

	err = copy_to_user(buffer, (void *)press_cnt, min(sizeof(press_cnt), count));

	memset((void *)press_cnt, 0, sizeof(press_cnt));

	return err ? -EFAULT : 0;
}

int tq_buttons_open(struct inode *inode, struct file *fp)
{
	int err = 0;
	err = request_irq(button_irqs[0].irq, tq2440_buttons_irq, button_irqs[0].flags, button_irqs[0].name, &press_cnt[0]);
	if (err < 0){
		printk("failed request irq_eint0 on %s,in line:%d\n", __FUNCTION__, __LINE__);
		goto request_irq_eint0_error;
	}
	err = request_irq(button_irqs[1].irq, tq2440_buttons_irq, button_irqs[1].flags, button_irqs[1].name, &press_cnt[1]);
	if (err < 0){
		printk("failed request irq_eint1 on %s,in line:%d\n", __FUNCTION__, __LINE__);
		goto request_irq_eint1_error;
	}
	err = request_irq(button_irqs[2].irq, tq2440_buttons_irq, button_irqs[2].flags, button_irqs[2].name, &press_cnt[2]);
	if (err < 0){
		printk("failed request irq_eint2 on %s,in line:%d\n", __FUNCTION__, __LINE__);
		goto request_irq_eint2_error;
	}
	err = request_irq(button_irqs[3].irq, tq2440_buttons_irq, button_irqs[3].flags, button_irqs[3].name, &press_cnt[3]);
	if (err < 0){
		printk("failed request irq_eint4 on %s,in line:%d\n", __FUNCTION__, __LINE__);
		goto request_irq_eint4_error;
	}

request_irq_eint0_error:
	return err;
request_irq_eint1_error:
	free_irq(button_irqs[0].irq, (void *)&press_cnt[0]);
	return err;
request_irq_eint2_error:
	free_irq(button_irqs[0].irq, (void *)&press_cnt[0]);
	free_irq(button_irqs[1].irq, (void *)&press_cnt[1]);
	return err;
request_irq_eint4_error:
	free_irq(button_irqs[0].irq, (void *)&press_cnt[0]);
	free_irq(button_irqs[1].irq, (void *)&press_cnt[1]);
	free_irq(button_irqs[2].irq, (void *)&press_cnt[2]);
	return err;

}

static struct file_operations fops_btn = {
	.owner 		= THIS_MODULE,
	.open		= tq_buttons_open,
	.read 		= tq_buttons_read,
	.release 	= tq_buttons_release,
};


static int tq2440_buttons_init(void)
{
	int err = 0;
	int devt = MKDEV(buttons_major, 0);

	s3c_gpio_cfgpin(S3C2410_GPF(0),S3C2410_GPIO_IRQ);
	s3c_gpio_cfgpin(S3C2410_GPF(1),S3C2410_GPIO_IRQ);
	s3c_gpio_cfgpin(S3C2410_GPF(2),S3C2410_GPIO_IRQ);
	s3c_gpio_cfgpin(S3C2410_GPF(4),S3C2410_GPIO_IRQ);
	
	if (buttons_major){
		err = register_chrdev_region(devt, 1, TQ2440_BUTTONS_NAME);
		if (err < 0)
		{
			printk("Can't register dev_t on %s,in line:%d\n", __FUNCTION__, __LINE__);
			return err;
		}

	}else
	{
		err = alloc_chrdev_region(&devt, 0, 1, TQ2440_BUTTONS_NAME);
		if (err < 0)
		{
			printk("Can't register dev_t on %s,in line:%d\n", __FUNCTION__, __LINE__);
			return err;
		}
		buttons_major = MAJOR(devt);
	}



	cdev_init(&cdev_btn, &fops_btn);
	err = cdev_add(&cdev_btn, devt, 1);
	if (err < 0)
	{
		printk("failed to add cdev on %s,in line:%d\n", __FUNCTION__, __LINE__);
		goto cdev_add_error;
	}

	tq_btn_class = class_create(THIS_MODULE, "tq_btn_class");
	if (IS_ERR(tq_btn_class)){
		printk("failed to create class on %s,in line:%d\n", __FUNCTION__, __LINE__);
		goto create_class_error;
	}

	if (IS_ERR(device_create(tq_btn_class, NULL, devt, NULL, "tq_buttons"))){
		printk("failed to create device on %s,in line:%d\n", __FUNCTION__, __LINE__);
		goto create_device_error;
	};
	
	return 0;
	
cdev_add_error:
	unregister_chrdev_region(devt, 1);
	return err;
create_class_error:
	unregister_chrdev_region(devt, 1);
	cdev_del(&cdev_btn);
	return -1;
create_device_error:
	unregister_chrdev_region(devt, 1);
	cdev_del(&cdev_btn);
	class_destroy(tq_btn_class);
	return -1;
}

static void tq2440_buttons_exit(void)
{
	device_destroy(tq_btn_class, MKDEV(buttons_major, 0));
	class_destroy(tq_btn_class);
	cdev_del(&cdev_btn);
	unregister_chrdev_region(MKDEV(buttons_major, 0), 1);	
}

module_init(tq2440_buttons_init);
module_exit(tq2440_buttons_exit);

MODULE_LICENSE("GPL");


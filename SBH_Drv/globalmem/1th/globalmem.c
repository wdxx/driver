#include <linux/fs.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <asm/io.h>
#include <asm/uaccess.h>

/* 全局内存大小:4KB */
#define GLOBALMEM_SIZE			0x1000
#define MEM_CLEAR				0x01
/* 主设备号 */
#define GLOBALMEM_MAJOR		254

static int globalmem_major = GLOBALMEM_MAJOR;

struct globalmem_dev
{
	struct cdev cdev;
	unsigned char mem[GLOBALMEM_SIZE];
};

struct globalmem_dev dev;

/* glaobalmem device 读操作函数 */
static ssize_t globalmem_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;
	int ret t = 0;
	/* 判断访问是否越界 */
	if (p >= GLOBALMEM_SIZE) return count? -ENXIO : 0;
	if (count > GLOBALMEM_SIZE - p)
		count = GLOBALMEM_SIZE - p;

	if (copy_to_user(buf, (void *)(dev.mem + p), count))
	{
		ret = -EFAULT;
	}
	else
	{
		*ppos += count;
		ret = count;

		printk(KERN_INFO "read %d byte from %d \n", count, p);
	}

	return ret;
}

ssize_t globalmem_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;
	int ret = 0;
	/* 判断访问是否越界 */
	if (p >= GLOBALMEM_SIZE) return count? -ENXIO : 0;
	if (count > GLOBALMEM_SIZE - p)
		count = GLOBALMEM_SIZE - p;

	if (copy_from_user(dev.mem+p, buf, count))
	{
		ret = -EFAULT;
	}
	else
	{
		*ppos += count;
		ret = count;
		printk(KERN_INFO "write %d byte to %d \n", count, p);
	}

	return ret;
}

static loff_t globalmem_llseek(struct file *filp, loff_t offset, int orig)
{
	loff_t ret;
	switch (orig)
	{
		/* 从文件开头开始偏移 */
		case 0:
		{
			if (offset < 0)
			{
				ret = -EINVAL;
				break;
			}
			if ((unsigned int)offset > GLOBALMEM_SIZE)	//越界
			{
				ret = -EINVAL;
				break;
			}
			filp->f_pos = (unsigned int)offset;
			ret = filp->f_pos;
			break;
		}
		/* 当前位置开始偏移 */
		case 1:
		{
			if ((filp->f_pos + offset) > GLOBALMEM_SIZE)
			{
				ret = -EINVAL;
				break;
			}
			if ((filp->f_pos + offset) < 0)
			{
				ret = -EINVAL;
				break;
			}
			filp->f_pos = filp->f_pos + offset;
			ret = filp->f_pos;
			break;
		}
		default:
			ret = -EINVAL;
			break;
	}
	return ret;
}

static long globalmem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	switch (cmd)
	{
		case MEM_CLEAR:
		{
			memset(dev.mem, 0, GLOBALMEM_SIZE);
			printk(KERN_INFO "globalmem is set to zero\n");
			break;
		}
		default:
			return -EINVAL;
	}
	
	return 0;
}


static const struct file_operations globalmem_fops = {
	.owner 			= THIS_MODULE,
	.llseek 			= globalmem_llseek,
	.read 			= globalmem_read,
	.write 			= globalmem_write,
	/* 对于ioctl操作,优先执行f_op->unlocked_ioctl,如果没有unlocked_ioctl,那么执行f_op->ioctl */
	.unlocked_ioctl 	= globalmem_ioctl,
};
static globalmem_setup_cdev(void)
{
	int err, devno = MKDEV(globalmem_major, 0);
	cdev_init(&dev.cdev, &globalmem_fops);
	dev.cdev.owner = THIS_MODULE;
	dev.cdev.ops = &globalmem_fops;
	err = cdev_add(&dev.cdev, devno, 1);
	if (err)
	{
		printk(KERN_NOTICE "Error %d adding globalmem\n", err);
	}
}

static int globalmem_init(void)
{
	int result;
	dev_t devno = MKDEV(globalmem_major, 0);

	if (globalmem_major)
	{
		result = register_chrdev_region(devno, 1, "globalmem");
	}
	else
	{
		result = alloc_chrdev_region(&devno, 0, 1, "globalmem");
		globalmem_major = MAJOR(devno);
	}

	if (result < 0)
	{
		return result;
	}
	globalmem_setup_cdev();
	return 0;
	
}

static void globalmem_exit(void)
{
	cdev_del(&dev.cdev);	//删除cdev
	unregister_chrdev_region(MKDEV(globalmem_major, 0), 1);	//注销设备号
}

module_init(globalmem_init);
module_exit(globalmem_exit);

MODULE_LICENSE("GPL");


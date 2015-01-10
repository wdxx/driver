#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>

struct ft6x06_platform_data {
	unsigned int x_max;
	unsigned int y_max;
	unsigned long irqflags;
	unsigned int irq;
	unsigned int reset;
};

static ft6x06_platform_data ft6x06_platform_dat = {
	.x_max   = 480,
	.y_max   = 272,
	.irqflags = IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,
	.irq     = IRQ_EINT0,
};

#if 0
static struct i2c_board_info ft_ts_info = {
	I2C_BOARD_INFO("ft_ts", (0x70>>1)),
};
#endif

static struct i2c_client *ft_client;
static unsigned short const normal_i2c[] = {0x70, 0x38, I2C_CLIENT_END};
static int ft_ts_dev_init(void)
{
	struct i2c_adapter *i2c_adap;
	struct i2c_board_info info;

	i2c_adap = i2c_get_adapter(0);
	memset(&info, 0, sizeof(struct i2c_board_info));
	strlcpy(info.type, "ft_ts", I2C_NAME_SIZE);
	info.platform_data = (void *)&ft6x06_platform_dat;
	ft_client = i2c_new_probed_device(i2c_adap, &info, normal_i2c);
	i2c_put_adapter(i2c_adap);

	if (ft_client)
		return 0;
	else
		return -ENODEV;
}

static void ft_ts_drv_exit(void)
{
	i2c_unregister_device(ft_client);
}


module_init(ft_ts_dev_init);
module_exit(ft_ts_drv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sum_liu@163.com");




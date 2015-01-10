#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>

#define CFG_MAX_TOUCH_POINTS	2

#define PRESS_MAX	0xFF
#define FT_PRESS		0x7F

#define FT6X06_NAME 	"ft6x06_ts"

#define FT_MAX_ID	0x0F
#define FT_TOUCH_STEP	6
#define FT_TOUCH_X_H_POS		3
#define FT_TOUCH_X_L_POS		4
#define FT_TOUCH_Y_H_POS		5
#define FT_TOUCH_Y_L_POS		6
#define FT_TOUCH_EVENT_POS		3
#define FT_TOUCH_ID_POS			5

#define POINT_READ_BUF	(3 + FT_TOUCH_STEP * CFG_MAX_TOUCH_POINTS)

/*register address*/
#define FT6x06_REG_FW_VER		0xA6
#define FT6x06_REG_POINT_RATE	0x88
#define FT6x06_REG_THGROUP	0x80



struct ft6x06_platform_data {
	unsigned int x_max;
	unsigned int y_max;
	unsigned long irqflags;
	unsigned int irq;
	unsigned int reset;
};

struct ft6x06_ts_data {
	unsigned int irq;
	unsigned int x_max;
	unsigned int y_max;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct ts_event event;
	struct ft6x06_platform_data *pdata;
#ifdef CONFIG_PM
	struct early_suspend *early_suspend;
#endif
};


static struct i2c_device_id ft_idtable[] = {
	/* 这个是与i2c_board_info里面的name 匹配的 */
	{"ft_ts", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, ft_idtable);

#define FTS_POINT_UP		0x01
#define FTS_POINT_DOWN		0x00
#define FTS_POINT_CONTACT	0x02

/*release the point*/
static void ft6x06_ts_release(struct ft6x06_ts_data *data)
{
	input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 0);
	input_sync(data->input_dev);
}

/*Read touch point information when the interrupt  is asserted.*/
static int ft6x06_read_Touchdata(struct ft6x06_ts_data *data)
{
	struct ts_event *event = &data->event;
	unsigned char buf[POINT_READ_BUF] = { 0 };
	int ret = -1;
	int i = 0;
	unsigned char pointid = FT_MAX_ID;

	//ret = ft6x06_i2c_Read(data->client, buf, 1, buf, POINT_READ_BUF);
	ret = i2c_smbus_read_i2c_block_data(data->client, buf, POINT_READ_BUF, buf);
	if (ret < 0) {
		dev_err(&data->client->dev, "%s read touchdata failed.\n",
			__func__);
		return ret;
	}
	memset(event, 0, sizeof(struct ts_event));

	//event->touch_point = buf[2] & 0x0F;

	event->touch_point = 0;
	for (i = 0; i < CFG_MAX_TOUCH_POINTS; i++) {
	//for (i = 0; i < event->touch_point; i++) {
		pointid = (buf[FT_TOUCH_ID_POS + FT_TOUCH_STEP * i]) >> 4;
		if (pointid >= FT_MAX_ID)
			break;
		else
			event->touch_point++;
		event->au16_x[i] =
		    (s16) (buf[FT_TOUCH_X_H_POS + FT_TOUCH_STEP * i] & 0x0F) <<
		    8 | (s16) buf[FT_TOUCH_X_L_POS + FT_TOUCH_STEP * i];
		event->au16_y[i] =
		    (s16) (buf[FT_TOUCH_Y_H_POS + FT_TOUCH_STEP * i] & 0x0F) <<
		    8 | (s16) buf[FT_TOUCH_Y_L_POS + FT_TOUCH_STEP * i];
		event->au8_touch_event[i] =
		    buf[FT_TOUCH_EVENT_POS + FT_TOUCH_STEP * i] >> 6;
		event->au8_finger_id[i] =
		    (buf[FT_TOUCH_ID_POS + FT_TOUCH_STEP * i]) >> 4;
	}
	
	event->pressure = FT_PRESS;

	return 0;
}


/*
*report the point information
*/
static void ft6x06_report_value(struct ft6x06_ts_data *data)
{
	struct ts_event *event = &data->event;
	int i = 0;
	int up_point = 0;
	//int touch_point = 0;

	for (i = 0; i < event->touch_point; i++) {
		/* LCD view area */
		if (event->au16_x[i] < data->x_max
		    && event->au16_y[i] < data->y_max) {
			input_report_abs(data->input_dev, ABS_MT_POSITION_X,
					 event->au16_x[i]);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y,
					 event->au16_y[i]);
			//input_report_abs(data->input_dev, ABS_MT_PRESSURE,
					 event->pressure);
			input_report_abs(data->input_dev, ABS_MT_TRACKING_ID,
					 event->au8_finger_id[i]);
			if (event->au8_touch_event[i] == FTS_POINT_DOWN
			    || event->au8_touch_event[i] == FTS_POINT_CONTACT)\
				input_report_abs(data->input_dev,
						 ABS_MT_TOUCH_MAJOR,
						 event->pressure);
			else {
				input_report_abs(data->input_dev,
						 ABS_MT_TOUCH_MAJOR, 0);
				up_point++;
			}
			//touch_point ++;
		}

		input_mt_sync(data->input_dev);
	}
	if (event->touch_point > 0)
		input_sync(data->input_dev);

	if (event->touch_point == 0)
		ft6x06_ts_release(data);

}


/*The ft6x06 device will signal the host about TRIGGER_FALLING.
*Processed when the interrupt is asserted.
*/
static irqreturn_t ft6x06_ts_interrupt(int irq, void *dev_id)
{
	struct ft6x06_ts_data *ft6x06_ts = dev_id;
	int ret = 0;
	disable_irq_nosync(ft6x06_ts->irq);

	ret = ft6x06_read_Touchdata(ft6x06_ts);
	if (ret == 0)
		ft6x06_report_value(ft6x06_ts);

	enable_irq(ft6x06_ts->irq);

	return IRQ_HANDLED;
}


int ft_probe(struct i2c_client *client, const struct i2c_device_id *device_id)
{
	struct ft6x06_ts_data *ft6x06_ts;
	struct input_dev *input_dev;
	int err = 0;
	unsigned char uc_reg_value;
	unsigned char uc_reg_addr;

	struct ft6x06_platform_data *pdata =
	    (struct ft6x06_platform_data *)client->dev.platform_data;

	printk("ft_ts device probed!\n");
	/*  设置硬件相关
        * 1. 中断引脚设置，注册中断
        * 2. RST引脚设置
	 */
	s3c2410_gpio_cfgpin(S3C2410_GPB5, S3C2410_GPIO_OUTPUT);
	s3c2410_gpio_setpin(S3C2410_GPB5, 0);
	msleep(5);
	s3c2410_gpio_setpin(S3C2410_GPB5, 1);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	ft6x06_ts = kzalloc(sizeof(struct ft6x06_ts_data), GFP_KERNEL);

	if (!ft6x06_ts) {
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	i2c_set_clientdata(client, ft6x06_ts);
	ft6x06_ts->irq = client->irq;
	ft6x06_ts->client = client;
	ft6x06_ts->pdata = pdata;
	ft6x06_ts->x_max = pdata->x_max - 1;
	ft6x06_ts->y_max = pdata->y_max - 1;

#ifdef CONFIG_PM
	err = gpio_request(pdata->reset, "ft6x06 reset");
	if (err < 0) {
		dev_err(&client->dev, "%s:failed to set gpio reset.\n",
			__func__);
		goto exit_request_reset;
	}
#endif

	err = request_threaded_irq(client->irq, NULL, ft6x06_ts_interrupt,
					   pdata->irqflags, client->dev.driver->name,
					   ft6x06_ts);

	if (err < 0) {
		dev_err(&client->dev, "ft6x06_probe: request irq failed\n");
		goto exit_irq_request_failed;
	}
	disable_irq(client->irq);

	/* input_device设置 */
	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		dev_err(&client->dev, "failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}

	ft6x06_ts->input_dev = input_dev;

	set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
	set_bit(ABS_MT_POSITION_X, input_dev->absbit);
	set_bit(ABS_MT_POSITION_Y, input_dev->absbit);
	//set_bit(ABS_MT_PRESSURE, input_dev->absbit);

	input_set_abs_params(input_dev,
			     ABS_MT_POSITION_X, 0, ft6x06_ts->x_max, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_POSITION_Y, 0, ft6x06_ts->y_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, PRESS_MAX, 0, 0);
	//input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, PRESS_MAX, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_TRACKING_ID, 0, CFG_MAX_TOUCH_POINTS, 0, 0);

	set_bit(EV_KEY, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);

	input_dev->name = FT6X06_NAME;
	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev,
			"ft6x06_ts_probe: failed to register input device: %s\n",
			dev_name(&client->dev));
		goto exit_input_register_device_failed;
	}
	/*make sure CTP already finish startup process */
	msleep(150);

		/*get some register information */
	uc_reg_addr = FT6x06_REG_FW_VER;
	//ft6x06_i2c_Read(client, &uc_reg_addr, 1, &uc_reg_value, 1);
	i2c_smbus_read_i2c_block_data(client, &uc_reg_addr, 1, &uc_reg_value);
	dev_dbg(&client->dev, "[FTS] Firmware version = 0x%x\n", uc_reg_value);

	uc_reg_addr = FT6x06_REG_POINT_RATE;
	//ft6x06_i2c_Read(client, &uc_reg_addr, 1, &uc_reg_value, 1);
	i2c_smbus_read_i2c_block_data(client, &uc_reg_addr, 1, &uc_reg_value);
	dev_dbg(&client->dev, "[FTS] report rate is %dHz.\n",
		uc_reg_value * 10);

	uc_reg_addr = FT6x06_REG_THGROUP;
	//ft6x06_i2c_Read(client, &uc_reg_addr, 1, &uc_reg_value, 1);
	i2c_smbus_read_i2c_block_data(client, &uc_reg_addr, 1, &uc_reg_value);
	dev_dbg(&client->dev, "[FTS] touch threshold is %d.\n",
		uc_reg_value * 4);
#ifdef SYSFS_DEBUG
	ft6x06_create_sysfs(client);
#endif

#ifdef FTS_CTL_IIC
	if (ft_rw_iic_drv_init(client) < 0)
		dev_err(&client->dev, "%s:[FTS] create fts control iic driver failed\n",
				__func__);
#endif
	enable_irq(client->irq);
	return 0;

exit_input_register_device_failed:
	input_free_device(input_dev);

exit_input_dev_alloc_failed:
	free_irq(client->irq, ft6x06_ts);
#ifdef CONFIG_PM
exit_request_reset:
	gpio_free(ft6x06_ts->pdata->reset);
#endif

exit_irq_request_failed:
	i2c_set_clientdata(client, NULL);
	kfree(ft6x06_ts);

exit_alloc_data_failed:
exit_check_functionality_failed:
	return err;
	
}

static int ft_remove(struct i2c_client *client)
{
	struct ft6x06_ts_data *ft6x06_ts;
	ft6x06_ts = i2c_get_clientdata(client);
	input_unregister_device(ft6x06_ts->input_dev);
	#ifdef CONFIG_PM
	gpio_free(ft6x06_ts->pdata->reset);
	#endif

	#ifdef SYSFS_DEBUG
	ft6x06_release_sysfs(client);
	#endif
	#ifdef FTS_CTL_IIC
	ft_rw_iic_drv_exit();
	#endif
	free_irq(client->irq, ft6x06_ts);
	kfree(ft6x06_ts);
	i2c_set_clientdata(client, NULL);
	return 0;
}

#ifdef CONFIG_PM
static void ft_suspend(struct early_suspend *handler)
{
	struct ft6x06_ts_data *ts = container_of(handler, struct ft6x06_ts_data,
						early_suspend);

	dev_dbg(&ts->client->dev, "[FTS]ft6x06 suspend\n");
	disable_irq(ts->pdata->irq);
}

static void ft_resume(struct early_suspend *handler)
{
	struct ft6x06_ts_data *ts = container_of(handler, struct ft6x06_ts_data,
						early_suspend);

	dev_dbg(&ts->client->dev, "[FTS]ft6x06 resume.\n");
	gpio_set_value(ts->pdata->reset, 0);
	msleep(20);
	gpio_set_value(ts->pdata->reset, 1);
	enable_irq(ts->pdata->irq);
}
#else
#define ft_suspend	NULL
#define ft_resume		NULL
#endif


static struct i2c_driver ft_driver = {
	.driver = {
		.name	= "ft_ts",
		.owner  = THIS_MODULE,
	},

	.id_table	= ft_idtable,
	.probe		= ft_probe,
	.remove		= ft_remove,
	/* if device autodetection is needed: */
	//.class		= I2C_CLASS_SOMETHING,
	//.detect		= foo_detect,
	//.address_data	= &addr_data,

	//.shutdown	= foo_shutdown,	/* optional */
	.suspend	= ft_suspend,	/* optional */
	.resume		= ft_resume,	/* optional */
	//.command	= foo_command,	/* optional, deprecated */
}


static int ft_ts_drv_init(void)
{
	int i;
	i = i2c_add_driver(&ft_driver);
	if (i != 0){
		printk(KERN_ERR "Can't register i2c_driver in line %d\n", __LINE__);
		return i;
	}
	return 0;
}

static void ft_ts_drv_exit(void)
{
	i2c_del_driver(&ft_driver);
}

module_init(ft_ts_drv_init);
module_exit(ft_ts_drv_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("sum_liu@163.com");



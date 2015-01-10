#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>

#include <plat/adc.h>
#include <plat/regs-adc.h>
//#include <plat/ts.h>


static struct input_dev *s3c_ts_dev;
static struct timer_list ts_timer;


#define S3C2440_ADC_PA	0x58000000
#define S3C_TS_UP_DOWN_MASK	(1 << 15)

struct s3c_adc_regs{
	unsigned long adccon;
	unsigned long adctsc;
	unsigned long adcdly;
	unsigned long adcdat0;
	unsigned long adcdat1;
	unsigned long adcupdn;
};

volatile static struct s3c_adc_regs *s3c_adc_regs;

static void enter_wait_pen_up_mode(void)
{
	s3c_adc_regs->adctsc = 0x1d3;
}

static void enter_wait_pen_down_mode(void)
{
	s3c_adc_regs->adctsc = 0xd3;
}

static void enter_measure_xy_mode(void)
{
	s3c_adc_regs->adctsc = (1 << 2) | (1 << 3);
}

static irqreturn_t pen_up_down_irq(int irq, void *dev_id)
{
	/* pen down */
	if (!(s3c_adc_regs->adcdat0 & S3C_TS_UP_DOWN_MASK) && !(s3c_adc_regs->adcdat1 & S3C_TS_UP_DOWN_MASK)){

		//enter_wait_pen_up_mode();
		enter_measure_xy_mode();
		/* start adc */
		s3c_adc_regs->adccon |= (1 << 0);
		
	}
	/* pen up */
	else{
		enter_wait_pen_down_mode();
	}
	
	return IRQ_HANDLED;
}

static int s3c_filter_ts(int *x, int *y)
{
#define ERR_LIMIT 10

	int avr_x, avr_y;
	int det_x, det_y;

	avr_x = (x[0] + x[1])/2;
	avr_y = (y[0] + y[1])/2;

	det_x = (avr_x > x[2]) ? (avr_x - x[2]) : (x[2] - avr_x);
	det_y = (avr_y > y[2]) ? (avr_y - y[2]) : (y[2] - avr_y);

	if (det_x > ERR_LIMIT || det_y > ERR_LIMIT)
		return 0;

	avr_x = (x[1] + x[2])/2;
	avr_y = (y[1] + y[2])/2;

	det_x = (avr_x > x[3]) ? (avr_x - x[3]) : (x[3] - avr_x);
	det_y = (avr_y > y[3]) ? (avr_y - y[3]) : (y[3] - avr_y);

	if (det_x > ERR_LIMIT || det_y > ERR_LIMIT)
		return 0;

	return 1;
}

static irqreturn_t ts_adc_irq(int irq, void *dev_id)
{
	static int cnt = 0;
	static int x[4], y[4];
	int adcdat0,adcdat1;
	
	adcdat0 = s3c_adc_regs->adcdat0;
	adcdat1 = s3c_adc_regs->adcdat1;

	/* pen down */
	if (!(s3c_adc_regs->adcdat0 & S3C_TS_UP_DOWN_MASK) && !(s3c_adc_regs->adcdat1 & S3C_TS_UP_DOWN_MASK)){
		x[cnt] = adcdat0 & 0x3ff;
		y[cnt] = adcdat1 & 0x3ff;
		++cnt;
		if (cnt == 4){
			if (s3c_filter_ts(x, y)){
				input_report_abs(s3c_ts_dev, ABS_X, (x[0]+x[1]+x[2]+x[3])/4);
				input_report_abs(s3c_ts_dev, ABS_Y, (y[0]+y[1]+y[2]+y[3])/4);
				input_report_abs(s3c_ts_dev, ABS_PRESSURE, 1);
				input_report_abs(s3c_ts_dev, BTN_TOUCH, 1);
				input_sync(s3c_ts_dev);
			}
			cnt = 0;
			enter_wait_pen_up_mode();

			/* 加入定时器处理长按，滑动问题 */
			mod_timer(&ts_timer, jiffies + HZ/100);
		}
		else{
			enter_measure_xy_mode();
			/* start adc */
			s3c_adc_regs->adccon |= (1 << 0);	
			
		}
	}
	/* pen up */
	else{
		cnt = 0;
		input_report_abs(s3c_ts_dev, ABS_PRESSURE, 0);
		input_report_abs(s3c_ts_dev, BTN_TOUCH, 0);
		input_sync(s3c_ts_dev);
		enter_wait_pen_down_mode();
	}
	return IRQ_HANDLED;
}



void s3c_ts_timer_function(unsigned long dat){
	/* pen down */
	if (!(s3c_adc_regs->adcdat0 & S3C_TS_UP_DOWN_MASK) && !(s3c_adc_regs->adcdat1 & S3C_TS_UP_DOWN_MASK)){
		enter_measure_xy_mode();
		/* start adc */
		s3c_adc_regs->adccon |= (1 << 0);
	}
	else{
		/* pen up, touch release */
		input_report_abs(s3c_ts_dev, ABS_PRESSURE, 0);
		input_report_abs(s3c_ts_dev, BTN_TOUCH, 0);
		input_sync(s3c_ts_dev);
		enter_wait_pen_down_mode();
	}
}

static int s3c_ts_init(void)
{
	struct clk *clk;
	clk = clk_get(NULL,"adc");
	clk_enable(clk);

	s3c_ts_dev = input_allocate_device();
	input_register_device(s3c_ts_dev);
	set_bit(EV_SYN,s3c_ts_dev->evbit);
	set_bit(EV_KEY,s3c_ts_dev->evbit);
	set_bit(EV_ABS,s3c_ts_dev->evbit);
	set_bit(BTN_TOUCH,s3c_ts_dev->keybit);
	input_set_abs_params(s3c_ts_dev,ABS_X,0,0x3ff,0,0);
	input_set_abs_params(s3c_ts_dev,ABS_Y,0,0x3ff,0,0);
	input_set_abs_params(s3c_ts_dev,ABS_PRESSURE,0,1,0,0);

	s3c_adc_regs = ioremap(S3C2440_ADC_PA,sizeof(s3c_adc_regs));
	s3c_adc_regs->adccon = (1 << 14) | (49 << 6);
	s3c_adc_regs->adcdly = 0xffff;

	request_irq(IRQ_TC, pen_up_down_irq, IRQF_SAMPLE_RANDOM, "ts_pen", NULL);
	request_irq(IRQ_ADC, ts_adc_irq, IRQF_SAMPLE_RANDOM, "ts_adc", NULL);

	init_timer(&ts_timer);
	ts_timer.function = s3c_ts_timer_function;
	add_timer(&ts_timer);

	enter_wait_pen_down_mode();
	return 0;
}



static void s3c_ts_exit(void)
{
	del_timer(&ts_timer);
	free_irq(IRQ_ADC, NULL);
	free_irq(IRQ_TC, NULL);
	iounmap(s3c_adc_regs);
	input_unregister_device(s3c_ts_dev);
	//input_free_device(s3c_ts_dev);
}

module_init(s3c_ts_init);
module_exit(s3c_ts_exit);

MODULE_LICENSE("GPL");



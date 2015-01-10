#include <linux/input.h>
#include <linux/module.h>
#include <linux/init.h>

#include <asm/irq.h>
#include <asm/io.h>


#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <mach/regs-gpio.h>
#include <linux/timer.h>
#include <plat/gpio-cfg.h>



static struct input_dev *button_dev;
static struct timer_list button_timer;

#define BTN_A_IRQ			IRQ_EINT0
#define BTN_B_IRQ			IRQ_EINT1
#define BTN_C_IRQ			IRQ_EINT2
#define BTN_ENTER_IRQ		IRQ_EINT4

struct button_desc{
	unsigned int pin;
	unsigned int irq_number;
	unsigned int key_val;
	unsigned long flag;
	unsigned char *name;
};
static struct button_desc *p_btn_desc;
static struct button_desc tq_btn_desc[4]={
	{S3C2410_GPF0, BTN_A_IRQ, KEY_A, IRQF_TRIGGER_FALLING, "button1"},
	{S3C2410_GPF1, BTN_B_IRQ, KEY_B, IRQF_TRIGGER_FALLING, "button2"},
	{S3C2410_GPF2, BTN_C_IRQ, KEY_C, IRQF_TRIGGER_FALLING, "button3"},
	{S3C2410_GPF4, BTN_ENTER_IRQ, KEY_ENTER, IRQF_TRIGGER_FALLING, "button4"},
};
static irqreturn_t button_interrupt(int irq, void *dummy)
{
	p_btn_desc = (struct button_desc *)dummy;
	mod_timer(&button_timer,jiffies+HZ/100);
	return IRQ_HANDLED;
}


static void button_timer_function(unsigned long dat)
{
	unsigned int pinval;
	pinval = s3c2410_gpio_getpin(p_btn_desc->pin);

	if (!pinval){
		input_report_key(button_dev, p_btn_desc->key_val, 1);
		input_sync(button_dev);
	}else{
		input_report_key(button_dev, p_btn_desc->key_val, 0);
		input_sync(button_dev);
		//return;
	}
}

static int __init button_init(void)
{
	int error;
	int i;
	/* register the interrupt number that the button use */
	for (i = 0; i < sizeof(tq_btn_desc)/sizeof(tq_btn_desc[1]); i++){
		if (request_irq(tq_btn_desc[i].irq_number, button_interrupt, tq_btn_desc[i].flag, tq_btn_desc[i].name, &tq_btn_desc[i])) {
	    	printk(KERN_ERR "button_input.c: Can't allocate irq %d\n", tq_btn_desc[i].irq_number);
			for (; i > 0; i--){
				free_irq(tq_btn_desc[i-1].irq_number, &tq_btn_desc[i-1]);
			}
	        return -EBUSY;
	    }
	}

	button_dev = input_allocate_device();
	if (!button_dev) {
		printk(KERN_ERR "button.c: Not enough memory\n");
		error = -ENOMEM;
		goto err_free_irq;
	}

	//button_dev->evbit[0] = BIT_MASK(EV_KEY);
	//button_dev->keybit[BIT_WORD(BTN_A)] = BIT_MASK(BTN_A);
	set_bit(EV_KEY, button_dev->evbit);//support key event
	set_bit(EV_REP, button_dev->evbit);//support repeat event

	for (i = 0; i < sizeof(tq_btn_desc)/sizeof(tq_btn_desc[1]); i++){
		set_bit(tq_btn_desc[i].key_val, button_dev->keybit);
	}
	
	error = input_register_device(button_dev);
	if (error) {
		printk(KERN_ERR "button.c: Failed to register device\n");
		goto err_free_dev;
	}

	/* set the timer to avoid key unstable */
	memset(&button_timer,0,sizeof(button_timer));
	init_timer(&button_timer);
	button_timer.function = button_timer_function;
	add_timer(&button_timer);
	

	return 0;

 err_free_dev:
	input_free_device(button_dev);
 err_free_irq:
 	for (i = 0; i < sizeof(tq_btn_desc)/sizeof(tq_btn_desc[1]); i++){
		free_irq(tq_btn_desc[i].irq_number, &tq_btn_desc[i]);
 	}
	return error;
}

static void __exit button_exit(void)
{
	int i;
	del_timer(&button_timer);
    input_unregister_device(button_dev);
	input_free_device(button_dev);	
	for (i = 0; i < sizeof(tq_btn_desc)/sizeof(tq_btn_desc[1]); i++){
		free_irq(tq_btn_desc[i].irq_number, &tq_btn_desc[i]);
 	}
}

module_init(button_init);
module_exit(button_exit);

MODULE_LICENSE("GPL");





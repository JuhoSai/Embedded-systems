#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <inttypes.h>

//nappi konfiguraatio
#define BUTTON_0 DT_ALIAS(sw0)
static const struct gpio_dt_spec button_0 = GPIO_DT_SPEC_GET_OR(BUTTON_0, gpios, {0});
static struct gpio_callback button_0_data;
// Led pin configurations
static const struct gpio_dt_spec red = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec blue = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

// Red led thread initialization
#define STACKSIZE 500
#define PRIORITY 5
void red_led_task(void *, void *, void*);
K_THREAD_DEFINE(red_thread,STACKSIZE,red_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(yellow_thread,STACKSIZE,yellow_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(green_thread,STACKSIZE,green_led_task,NULL,NULL,NULL,PRIORITY,0,0);

int last_state = 0; //muistaa aikaisemman tilan
int led_state = 0;
// 1 = Red
// 2 = Yellow
// 3 = Green
// 4 = Pause

// Main program
int main(void)
{
	/*Tavoittelen 2 pistettä. Ohjelmassa on pause nappi ja ledit*/
	init_led();
	int ret = init_button();
	if (ret < 0) {
		return 0;
	}

	led_state = 1;
	return 0;
}

// Initialize leds
int  init_led() {

	int ret;
	// Led pin initialization
	ret = gpio_pin_configure_dt(&red, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printk("Error: Red Led configure failed\n");		
		return ret;
	}
	ret = gpio_pin_configure_dt(&green, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printk("Error: Green Led configure failed\n");		
		return ret;
	}
	
	// set led off
	gpio_pin_set_dt(&red,0);

	printk("Led initialized ok\n");
	
	return 0;
}

// Button initialization
int init_button() {

	int ret;
	if (!gpio_is_ready_dt(&button_0)) {
		printk("Error: button 0 is not ready\n");
		return -1;
	}

	ret = gpio_pin_configure_dt(&button_0, GPIO_INPUT);
	if (ret != 0) {
		printk("Error: failed to configure pin\n");
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&button_0, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error: failed to configure interrupt on pin\n");
		return -1;
	}

	gpio_init_callback(&button_0_data, button_0_handler, BIT(button_0.pin));
	gpio_add_callback(button_0.port, &button_0_data);
	printk("Set up button 0 ok\n");
	
	return 0;
}


// Button interrupt handler
void button_0_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	if(led_state == 4){
		led_state = last_state;
	} else {
		last_state = led_state;
		led_state = 4;
	}
	
	printk("Button pressed\n");
}

// Task to handle red led
void red_led_task(void *, void *, void*) {
	
	printk("Red led thread started\n");
	while (true) {
		if (led_state == 1) {
			// 1. set led on 
			gpio_pin_set_dt(&red,1);
			printk("Red on\n");
			// 2. sleep for 2 seconds
			k_sleep(K_SECONDS(1));
			//pysäyttää jos led_state on 4
			if (led_state == 4) {
					gpio_pin_set_dt(&red, 0);
					continue; 
				}

			// 3. set led off
			gpio_pin_set_dt(&red,0);
			printk("Red off\n");
			// 4. sleep for 2 seconds
			k_sleep(K_SECONDS(1));
			led_state = 2;
		}
	}
}

void yellow_led_task(void *, void *, void*) {
	
	printk("Red led thread started\n");
	while (true) {
		if (led_state == 2) {
			// 1. set led on 
			gpio_pin_set_dt(&red,1);
			gpio_pin_set_dt(&green,1);
			printk("Yellow on\n");
			// 2. sleep for 2 seconds
			k_sleep(K_SECONDS(1));

			//pysäyttää jos led_state on 4
			if (led_state == 4) {
					gpio_pin_set_dt(&red, 0);
					gpio_pin_set_dt(&green, 0);
					continue; 
				}


			// 3. set led off
			gpio_pin_set_dt(&red,0);
			gpio_pin_set_dt(&green,0);
			printk("Yellow off\n");
			// 4. sleep for 2 seconds
			k_sleep(K_SECONDS(1));
			led_state = 3; 
		}
	}
}
void green_led_task(void *, void *, void*) {
	
	printk("Green led thread started\n");
	while (true) {
		if (led_state == 3) {
			// 1. set led on 
			gpio_pin_set_dt(&green,1);
			printk("Green on\n");
			// 2. sleep for 2 seconds
			k_sleep(K_SECONDS(1));

			//pysäyttää jos led_state on 4
			if (led_state == 4) {
					gpio_pin_set_dt(&green, 0);
					continue; 
				}


			// 3. set led off
			gpio_pin_set_dt(&green,0);
			printk("Green off\n");
			// 4. sleep for 2 seconds
			k_sleep(K_SECONDS(1));
			led_state = 1;
		}
	}
}

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <inttypes.h>
#include <zephyr/drivers/uart.h>
#include <stdlib.h>

// UART initialization
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

//nappi konfiguraatio
#define BUTTON_0 DT_ALIAS(sw0)
static const struct gpio_dt_spec button_0 = GPIO_DT_SPEC_GET_OR(BUTTON_0, gpios, {0});
static struct gpio_callback button_0_data;
// Led pin configurations
static const struct gpio_dt_spec red = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
//static const struct gpio_dt_spec blue = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

// Red led thread initialization
#define STACKSIZE 500
#define PRIORITY 5
void red_led_task(void *, void *, void*);
void yellow_led_task(void *, void *, void*);
void green_led_task(void *, void *, void*);
K_THREAD_DEFINE(red_thread,STACKSIZE,red_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(yellow_thread,STACKSIZE,yellow_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(green_thread,STACKSIZE,green_led_task,NULL,NULL,NULL,PRIORITY,0,0);

// Create dispatcher FIFO buffer
K_FIFO_DEFINE(dispatcher_fifo);

int last_state = 0; //muistaa aikaisemman tilan
int led_state = 0;
// 1 = Red
// 2 = Yellow
// 3 = Green
// 4 = Pause

struct data_t {
	void *fifo_reserved;
	char msg[20];
};

int init_uart(void) {
	// UART initialization
	if (!device_is_ready(uart_dev)) {
		return 1;
	} 
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

// Main program
int main(void)
{
	/*yhtä pistettä */
	
	init_led();
	init_button();
        int ret = init_uart();
	if (ret < 0) {
		return 0;
	}

	led_state = 1;

        char rc=0;
	while (true) {
		// Ask UART if data available
		if (uart_poll_in(uart_dev,&rc) == 0) {
			printk("Received: %c\n",rc);
		}
		k_yield();
        }
	return 0;
}

static void uart_task(void *unused1, void *unused2, void *unused3)
{
	// Received character from UART
	char rc=0;
	// Message from UART
	char uart_msg[20];
	memset(uart_msg,0,20);
	int uart_msg_cnt = 0;

	while (true) {
		// Ask UART if data available
		if (uart_poll_in(uart_dev,&rc) == 0) {
			// printk("Received: %c\n",rc);
			// If character is not newline, add to UART message buffer
			if (rc != '\r') {
				uart_msg[uart_msg_cnt] = rc;
				uart_msg_cnt++;
			// Character is newline, copy dispatcher data and put to FIFO buffer
			} else {
				printk("UART msg: %s\n", uart_msg);
                
				struct data_t *buf = k_malloc(sizeof(struct data_t));
				if (buf == NULL) {
					return;
				}
				// Copy UART message to dispatcher data
                                memset(buf->msg, 0, sizeof(buf->msg));
				strncpy(buf->msg, uart_msg, sizeof(buf->msg)-1);
				

				// You need to:
				// Put dispatcher data to FIFO buffer
                                k_fifo_put(&dispatcher_fifo, buf);

				// Clear UART receive buffer
				uart_msg_cnt = 0;
				memset(uart_msg,0,20);

				// Clear UART message buffer
				uart_msg_cnt = 0;
				memset(uart_msg,0,sizeof(uart_msg));
			}
		}
		k_msleep(10);
	}
	return;
}

/********************
 * Dispatcher task
 */
static void dispatcher_task(void *unused1, void *unused2, void *unused3)
{
	while (true) {
		// Receive dispatcher data from uart_task fifo
		struct data_t *rec_item = k_fifo_get(&dispatcher_fifo, K_FOREVER);
		char sequence[20];
		memcpy(sequence,rec_item->msg,20);
		k_free(rec_item);

		printk("Dispatcher: %s\n", sequence);
        // You need to:
        // Parse color and time from the fifo data
        // Example
                char color = sequence[0];
		int time = atoi(sequence+1);    
		printk("Data: %c %d\n", color, time);
        // Send the parsed color information to tasks using fifo
                if (color == 'R') led_state = 1;
		else if (color == 'Y') led_state = 2;
		else if (color == 'G') led_state = 3;

        // Use release signal to control sequence or k_yield
                k_msleep(time * 1000);
	}
}

K_THREAD_DEFINE(dis_thread,STACKSIZE,dispatcher_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(uart_thread,STACKSIZE,uart_task,NULL,NULL,NULL,PRIORITY,0,0);


// Task to handle red led
void red_led_task(void *, void *, void*) {
	char msg = 0;
	printk("Red led thread started\n");
	while (true) {
		if (led_state == 1) {

                        struct data_t *buf = k_malloc(sizeof(struct data_t));
		        if (buf == NULL) {
			return;
		        }
                        *buf->msg = msg;
		        k_fifo_put(&dispatcher_fifo, buf);
		        printk("Red added to fifo: %s\n",buf->msg);
                        msg ++;

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
	struct data_t *received;
	printk("Red led thread started\n");
	while (true) {
		if (led_state == 2) {

            received = k_fifo_get(&dispatcher_fifo, K_FOREVER);
		    printk("Yellow received from fifo: %s\n", received->msg);
		    k_free(received);

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
	struct data_t *received;
	printk("Green led thread started\n");
	while (true) {
		if (led_state == 3) {

                received = k_fifo_get(&dispatcher_fifo, K_FOREVER);
		        printk("Green received from fifo: %s\n", received->msg);
		        k_free(received);

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

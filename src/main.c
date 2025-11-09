#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <inttypes.h>
#include <zephyr/drivers/uart.h>
#include <stdlib.h>
#include <zephyr/timing/timing.h>
#include <string.h>
#include "TimeParser.h"


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

// led thread initialization
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

K_FIFO_DEFINE(fifo_red);
K_FIFO_DEFINE(fifo_yellow);
K_FIFO_DEFINE(fifo_green);

K_FIFO_DEFINE(debug_fifo);

//error codes
#define COMMAND_OK
#define TIME_LEN_ERROR      -1
#define TIME_ARRAY_ERROR    -2
#define TIME_VALUE_ERROR    -3
#define TIME_ZERO_ERROR    -4
#define TIME_NONNUM_ERROR    -5

//parser
int time_parse(char *command);

// Timer initializations
struct k_timer timer;
void timer_handler(struct k_timer *timer_id);

int last_state = 0; //muistaa aikaisemman tilan
int led_state = 0;
// 1 = Red
// 2 = Yellow
// 3 = Green
// 4 = Pause

//ajan mittausta varten
static timing_t aika_aloitus;

struct data_t {
	void *fifo_reserved;
	char msg[20];
};

struct debug_msg_t {
    void *fifo_reserved;  
    char msg[80];         
};


int init_uart(void) {
	// UART initialization
	if (!device_is_ready(uart_dev)) {
		return 1;
	} 
	return 0;
}

void debug_task(void *a, void *b, void *c) {
	printk("debug_task käynnistyi\n");

    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    struct debug_msg_t *dbg;
    while (true) {
        dbg = k_fifo_get(&debug_fifo, K_FOREVER);
        if (dbg) {
            printk("%s\n", dbg->msg);
            k_free(dbg);
        }
        k_msleep(10);
    }
}

void timer_handler(struct k_timer *timer_id) {
	if (led_state == 1) {
		gpio_pin_set_dt(&red,1);
		led_state = true;
	} else {
		gpio_pin_set_dt(&red,0);
		led_state = false;
	}
}
int time_parse(char *time) {

	// how many seconds, default returns error
	int seconds = TIME_LEN_ERROR;

	// TODO: Check that string is not null
    if ( time == NULL) {
        return TIME_LEN_ERROR;
    }

    if (strlen(time) != 6){
        return TIME_LEN_ERROR;
    }

   
    
	// Parse values from time string
	// For example: 124033 -> 12hour 40min 33sec
    int values[3];
	values[2] = atoi(time+4); // seconds
	time[4] = 0;
	values[1] = atoi(time+2); // minutes
	time[2] = 0;
	values[0] = atoi(time); // hours
	// Now you have:
	// values[0] hour
	// values[1] minute
	// values[2] second

	// TODO: Add boundary check time values: below zero or above limit not allowed
	// limits are 59 for minutes, 23 for hours, etc

    if (values[0] < 0 || values[0] > 23 ||
        values[1] < 0 || values[1] > 59 ||
        values[2] < 0 || values[2] > 59) {
        return TIME_VALUE_ERROR;
    }
	// TODO: Calculate return value from the parsed minutes and seconds
	// Otherwise error will be returned!
	seconds = values[0] * 3600 + values[1] * 60 + values[2];

    if (values[0] == 0 && values[1] == 0 && values[2] == 0) {
    return TIME_ZERO_ERROR;  // määrittele TIME_ZERO_ERROR esim. -4
    }

	return seconds;
}


//debug taskin define
K_THREAD_DEFINE(debug_thread, 1024, debug_task, NULL, NULL, NULL, PRIORITY, 0, 0);

void send_debug(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    struct debug_msg_t *dbg = k_malloc(sizeof(struct debug_msg_t));
    if (!dbg) {
		printk("k_malloc epäonnistui\n");
        va_end(args);
        return;
    }
    memset(dbg->msg, 0, sizeof(dbg->msg));
    vsnprintf(dbg->msg, sizeof(dbg->msg), format, args);
    va_end(args);

    k_fifo_put(&debug_fifo, dbg);
}


// Initialize leds
int  init_led() {

	int ret;
	// Led pin initialization
	ret = gpio_pin_configure_dt(&red, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		send_debug("Error: Red Led configure failed\n");		
		return ret;
	}
	ret = gpio_pin_configure_dt(&green, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		send_debug("Error: Green Led configure failed\n");		
		return ret;
	}
	
	// set led off
	gpio_pin_set_dt(&red,0);
	gpio_pin_set_dt(&green, 0);

	send_debug("Led initialized ok\n");
	
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
	
	send_debug("Button pressed\n");
}
// Button initialization
int init_button() {

	int ret;
	if (!gpio_is_ready_dt(&button_0)) {
		send_debug("Error: button 0 is not ready\n");
		return -1;
	}

	ret = gpio_pin_configure_dt(&button_0, GPIO_INPUT);
	if (ret != 0) {
		send_debug("Error: failed to configure pin\n");
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&button_0, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		send_debug("Error: failed to configure interrupt on pin\n");
		return -1;
	}

	gpio_init_callback(&button_0_data, button_0_handler, BIT(button_0.pin));
	gpio_add_callback(button_0.port, &button_0_data);
	send_debug("Set up button 0 ok\n");
	
	return 0;
}

void uart_send_response(int code)
{
    char buf[10];
    snprintf(buf, sizeof(buf), "%dX", code);  // esim. -1X tai 0X
    for (int i = 0; buf[i] != '\0'; i++) {
        uart_poll_out(uart_dev, buf[i]);
    }
}


static void uart_task(void *unused1, void *unused2, void *unused3)
{
    char rc = 0;
    char uart_msg[20];
    int uart_msg_cnt = 0;

    memset(uart_msg, 0, sizeof(uart_msg));
    send_debug("UART task started\n");

    while (true) {
        if (uart_poll_in(uart_dev, &rc) == 0) {
            if (rc != 'X' && rc != '\n') {
                if (uart_msg_cnt < sizeof(uart_msg)-1) {
                    uart_msg[uart_msg_cnt++] = rc; // lisää merkki puskurin loppuun
                }
            } else if (uart_msg_cnt > 0) {
                uart_msg[uart_msg_cnt] = '\0'; // lopeta merkkijono
                send_debug("UART msg: %s", uart_msg);

                int seconds = time_parse(uart_msg);
                if (seconds >= 0) {
                    //send_debug("Parsed seconds: %d", seconds);
                    uart_send_response(seconds);
                    // Käynnistä timer tällä arvolla
					k_timer_start(&timer, K_SECONDS(seconds), K_SECONDS(seconds));
                } else {
                    send_debug("Time parse error: %d", seconds);
                    uart_send_response(seconds);
                }

                // nollaa puskuri seuraavaa viestiä varten
                uart_msg_cnt = 0;
                memset(uart_msg, 0, sizeof(uart_msg));
            }
        }
        k_msleep(10);
    }
}


/********************
 * Dispatcher task
 */
static void dispatcher_task(void *unused1, void *unused2, void *unused3)
{
	while (true) {
		// Receive dispatcher data from uart_task fifo
		struct data_t *rec_item = k_fifo_get(&dispatcher_fifo, K_FOREVER);
		if (!rec_item) {
			continue;
		}

		rec_item->msg[sizeof(rec_item->msg)-1] = '\0';
        send_debug("Dispatcher got: %s\n", rec_item->msg);
       
       	char color = rec_item->msg[0];
        int time_seconds = 0;
        if (strlen(rec_item->msg) > 1) {
            time_seconds = atoi(rec_item->msg + 1);
        }

		send_debug("Parsed: color=%c time=%d\n", color, time_seconds);

        // Send the parsed color information to tasks using fifo
          if (color == 'R') {
            led_state = 1;
            /* pass message to red fifo */
            k_fifo_put(&fifo_red, rec_item);
        } else if (color == 'Y') {
            led_state = 2;
            k_fifo_put(&fifo_yellow, rec_item);
        } else if (color == 'G') {
            led_state = 3;
            k_fifo_put(&fifo_green, rec_item);
		}
        // Use release signal to control sequence or k_yield
                k_msleep(10);
	}
}

K_THREAD_DEFINE(dis_thread,STACKSIZE,dispatcher_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(uart_thread,STACKSIZE,uart_task,NULL,NULL,NULL,PRIORITY,0,0);


// Task to handle red led
void red_led_task(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    send_debug("Red led thread started\n");
    while (true) {
        if (led_state == 1) {
            aika_aloitus = k_uptime_get(); // start of sequence
            uint64_t red_start = k_uptime_get();

            struct data_t *received = k_fifo_get(&fifo_red, K_MSEC(500));
            if (received) {
                send_debug("Red received from fifo: %s", received->msg);
                k_free(received);
            }

            gpio_pin_set_dt(&red, 1);
            k_sleep(K_SECONDS(1));
            if (led_state == 4) {
                gpio_pin_set_dt(&red, 0);
                continue; 
            }
            gpio_pin_set_dt(&red, 0);
            k_sleep(K_SECONDS(1));
            led_state = 2;

            uint64_t red_end = k_uptime_get();
            uint32_t s = (red_end - red_start) / 1000;
            uint32_t ms = (red_end - red_start) % 1000;
            //send_debug("Red task duration: %u.%03u s", s, ms);
        } else {
            k_yield();
        }
    }
}

void yellow_led_task(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    send_debug("Yellow led thread started\n");

    while (true) {
        if (led_state == 2) {
            uint64_t yellow_start = k_uptime_get();

            struct data_t *received = k_fifo_get(&fifo_yellow, K_MSEC(500));
            if (received) {
                send_debug("Yellow received from fifo: %s", received->msg);
                k_free(received);
            }

            gpio_pin_set_dt(&red, 1);
            gpio_pin_set_dt(&green, 1);
            k_sleep(K_SECONDS(1));

            if (led_state == 4) {
                gpio_pin_set_dt(&red, 0);
                gpio_pin_set_dt(&green, 0);
                continue;
            }

            gpio_pin_set_dt(&red, 0);
            gpio_pin_set_dt(&green, 0);
            k_sleep(K_SECONDS(1));
            led_state = 3;

            uint64_t yellow_end = k_uptime_get();
            uint32_t s = (yellow_end - yellow_start) / 1000;
            uint32_t ms = (yellow_end - yellow_start) % 1000;
           // send_debug("Yellow task duration: %u.%03u s", s, ms);
        } else {
            k_yield();
        }
    }
}

void green_led_task(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    send_debug("Green led thread started\n");
    while (true) {
        if (led_state == 3) {
            uint64_t green_start = k_uptime_get();

            struct data_t *received = k_fifo_get(&fifo_green, K_MSEC(500));
            if (received) {
                send_debug("Green received from fifo: %s", received->msg);
                k_free(received);
            }

            gpio_pin_set_dt(&green, 1);
            k_sleep(K_SECONDS(1));

            if (led_state == 4) {
                gpio_pin_set_dt(&green, 0);
                continue; 
            }

            gpio_pin_set_dt(&green, 0);
            k_sleep(K_SECONDS(1));
            led_state = 1;

            uint64_t green_end = k_uptime_get();
            uint32_t s = (green_end - green_start) / 1000;
            uint32_t ms = (green_end - green_start) % 1000;
           // send_debug("Green task duration: %u.%03u s", s, ms);

            uint64_t seq_end = k_uptime_get();
            uint32_t seq_s = (seq_end - aika_aloitus) / 1000;
            uint32_t seq_ms = (seq_end - aika_aloitus) % 1000;
           // send_debug("Sequence total duration: %u.%03u s", seq_s, seq_ms);
        } else {
            k_yield();
        }
    }
}

// Main program
int main(void) {
    printk("Main start\n");


    //2 pistettä sain testit toimimaan ja lisäsin pari lisää

    timing_init();
    timing_start();
    timing_t start_time = timing_counter_get();

    int ret;

    k_timer_init(&timer, timer_handler, NULL);
    k_timer_start(&timer, K_SECONDS(1), K_SECONDS(10));

    ret = init_uart();
    if (ret < 0) send_debug("init_uart failed (%d)", ret);

    ret = init_led();
    if (ret < 0) return ret;

    ret = init_button();
    if (ret < 0) return ret;

    led_state = 1;

    timing_t end_time = timing_counter_get();
    timing_stop();
    uint64_t timing_ns = timing_cycles_to_ns(timing_cycles_get(&start_time, &end_time));
    send_debug("Initialization: %lld", timing_ns);

    while (true) k_sleep(K_SECONDS(5));

    return 0;
}
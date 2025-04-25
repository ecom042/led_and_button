#include "button.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>

ZBUS_CHAN_DEFINE(chan_button_evt, struct msg_button_evt, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(.evt = BUTTON_EVT_UNDEFINED));

#define SLEEP_TIME_MS 1

/*
	Get button configuration from the devicetree sw0 alias. This is mandatory.
*/
#define SW0_NODE DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS_OKAY(SW0_NODE)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static struct gpio_callback button_cb_data;

//Set a timer that interrupts after 3 seconds and repeats after 0 cicles
struct k_timer longpress_timer;
void timer_function(struct k_timer *timer_id){

	struct msg_button_evt msg = {.evt = BUTTON_EVT_LONGPRESS};
	zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
}

K_TIMER_DEFINE(longpress_timer, timer_function, 0);
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};

	if (gpio_pin_get_dt(&button)) {
		msg.evt = BUTTON_EVT_PRESSED;
		k_timer_start(&longpress_timer ,K_SECONDS(3), K_NO_WAIT);
		printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
	} else {
		msg.evt = BUTTON_EVT_RELEASED;
		k_timer_stop(&longpress_timer);
		printk("Button released at %" PRIu32 "\n", k_cycle_get_32());
	}

	zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
}

int button_init(void)
{
	int ret;

	if (!gpio_is_ready_dt(&button)) {
		printk("Error: button device %s is not ready\n", button.port->name);
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n", ret, button.port->name,
		       button.pin);
		return ret;
	}

	return 0;
}

int button_enable_interrupts(void)
{
	int ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n", ret,
		       button.port->name, button.pin);
		return ret;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);

	return 0;
}

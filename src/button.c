#include "button.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>

/** 
 * @brief Define zbus channel for button events
 * Creates a channel to publish button events to subscribers
 */
ZBUS_CHAN_DEFINE(chan_button_evt, struct msg_button_evt, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(.evt = BUTTON_EVT_UNDEFINED));

/** Sleep time in milliseconds */		 
#define SLEEP_TIME_MS 1

/** Duration threshold for long press detection in milliseconds */
#define LONG_PRESS_TIME_MS 3000


/* 
 * Get button configuration from the devicetree sw0 alias. This is mandatory.
 */
#define SW0_NODE DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS_OKAY(SW0_NODE)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif

/** GPIO specification for the button */
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);

/** GPIO callback data structure */
static struct gpio_callback button_cb_data;

/** Timestamp of the last button press*/
static int64_t last_press_time = 0;

/**
 * @brief Button interrupt callback function
 *
 * Handles button press and release events, detects long presses,
 * and publishes appropriate messages to the zbus channel.
 *
 * @param dev Device that triggered the interrupt
 * @param cb Callback structure
 * @param pins Pin mask indicating which pins triggered the interrupt
 */
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};
	int64_t current_time = k_uptime_get();

	if (gpio_pin_get_dt(&button)) {
		msg.evt = BUTTON_EVT_PRESSED;
		last_press_time = current_time;
		printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
		zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
	} else {
		msg.evt = BUTTON_EVT_RELEASED;
		printk("Button released at %" PRIu32 "\n", k_cycle_get_32());
		zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
	}

	if(current_time - last_press_time >= LONG_PRESS_TIME_MS) {
		msg.evt = BUTTON_EVT_LONGPRESS;
		printk("Button long pressed at %" PRIu32 "\n", k_cycle_get_32());
		zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
	}
}

/**
 * @brief Initialize the button hardware
 *
 * Verifies that the button device is ready and configures
 * the GPIO pin as an input.
 *
 * @return 0 on success, negative error code on failure
 */
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


/**
 * @brief Enable interrupt handling for the button
 *
 * Configures the button pin to trigger interrupts on both rising and falling edges,
 * and registers the callback function to handle these interrupts.
 *
 * @return 0 on success, negative error code on failure
 */
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
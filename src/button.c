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
 * Get button configuration from the devicetree sw0 alias. This is mandatory.
 */
#define SW0_NODE DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS_OKAY(SW0_NODE)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static struct gpio_callback button_cb_data;

static uint32_t btn_press_milli = 0;

/**
 * @brief Button interrupt callback function
 *
 * Handles button press and release events, detects long presses,
 * and publishes events to the button event channel.
 *
 * @param dev Pointer to the device structure for the GPIO driver
 * @param cb Pointer to the GPIO callback structure
 * @param pins Pin mask indicating which pins triggered the callback
 */
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};

	bool was_longpress = false;

	if (gpio_pin_get_dt(&button)) {
		msg.evt = BUTTON_EVT_PRESSED;
		btn_press_milli = k_uptime_get_32()/1000;
		printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
	} else {
		msg.evt = BUTTON_EVT_RELEASED;
		
		const uint32_t btn_milli = k_uptime_get_32() - btn_press_milli;

		if (btn_milli >= LONG_PRESS_TIME_MILLI) {
			was_longpress = true;
			printk("Button long pressed at %" PRIu32 "\n", k_cycle_get_32());
		} 

		printk("Button released at %" PRIu32 "\n", k_cycle_get_32());
	}

	zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);

	if (was_longpress)
	{
		struct msg_button_evt longpress_msg = {.evt = BUTTON_EVT_LONGPRESS};
		zbus_chan_pub(&chan_button_evt, &longpress_msg, K_NO_WAIT);
	}
}

/**
 * @brief Initialize the button hardware
 *
 * Configures the button GPIO pin as an input. Checks if the 
 * GPIO device is ready and configures the pin.
 *
 * @return 0 if successful, negative errno code on failure:
 * -ENODEV if the GPIO device is not ready
 * or other error codes from gpio_pin_configure_dt()
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
 * @brief Enable button interrupt handling
 *
 * Configures interrupts for the button to detect both rising and falling edges,
 * and registers the callback function that will be invoked when the button
 * state changes.
 *
 * @return 0 if successful, negative errno code on failure from
 * gpio_pin_interrupt_configure_dt()
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

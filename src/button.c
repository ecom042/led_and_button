#include "button.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>
#include <stdint.h>

/** Defines the channel used in button */
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


/**
 * @brief Callback function for button press and release events.
 * 
 * @param dev Device structure pointer
 * @param cb Callback structure pointer
 * @param pins Pin number
 */
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	static uint32_t press_button = 0;
	struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};

	if (gpio_pin_get_dt(&button)) {

		press_button = k_uptime_get_32();
		msg.evt = BUTTON_EVT_PRESSED;
		printk("Button pressed at %" PRIu32 "\n", press_button);
		zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
	} else {
		uint32_t release_button = k_uptime_get_32() - press_button;

		msg.evt = BUTTON_EVT_RELEASED;
		printk("Button released at %" PRIu32 "\n", k_uptime_get_32());

		zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);

		if (release_button >= 3000) {
			struct msg_button_evt longpress_msg = {.evt = BUTTON_EVT_LONGPRESS};
			printk("Button long pressed at %" PRIu32 "ms\n", release_button);
			zbus_chan_pub(&chan_button_evt, &longpress_msg, K_NO_WAIT);
		}
	}
}

/**
 * @brief Function to initialize the button.
 * 
 * @retval 0 on success
 * @retval -ENODEV if the button device is not ready
 * @retval less than 0 on failure to configure the button pin
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
 * @brief Function to enable interrupts for the button.
 * 
 * @retval 0 on success
 * @retval less than 0 on failure to configure the interrupt
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

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

static int64_t press_start_time = 0;
/**
 * @brief GPIO callback for button state changes.
 *
 * This function is called on GPIO interrupts triggered by the button's state
 * transitions (press or release). It identifies the event type and publishes
 * a corresponding message to the zbus channel.
 *
 * @param dev Pointer to the device structure associated with the GPIO controller.
 * @param cb Pointer to the GPIO callback structure.
 * @param pins Bitmask identifying the GPIO pin(s) that triggered the interrupt.
 */

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};

	if (gpio_pin_get_dt(&button)) {
		msg.evt = BUTTON_EVT_PRESSED;
		press_start_time = k_uptime_get();
		printk("Button pressed at %" PRIu64 "\n", press_start_time);
		zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
	} else {
		int64_t release_time = k_uptime_get();
		int64_t press_duration = release_time - press_start_time;

		msg.evt = BUTTON_EVT_RELEASED;
		printk("Button released at %" PRIu64 " (held for %" PRIu64 " ms)\n", release_time, press_duration);
		zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);

		if (press_duration >= 3000) {
			msg.evt = BUTTON_EVT_LONGPRESS;
			printk("Long press detected\n");
			zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
		}
	}
}

/**
 * @brief Initialize the button GPIO configuration.
 *
 * Configures the GPIO pin associated with the button as an input.
 * Verifies that the GPIO device is ready before applying configuration.
 *
 * @return 0 on success, or a negative error code on failure.
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
 * @brief Enable GPIO interrupts for the button.
 *
 * Configures the button's GPIO pin to generate interrupts on both
 * rising and falling edges. Also registers the callback function
 * for handling button events.
 *
 * @return 0 on success, or a negative error code on failure.
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

#include "button.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>

/**
 * Defines the minimum duration (in milliseconds) to classify a press as a long press,
 * and stores timestamps for when the button is pressed and released.
 */
#define EVENT_PRESSED_TIME 3000

int64_t press_time = 0;
int64_t release_time = 0;

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
 * @brief GPIO callback triggered on button state change.
 *
 * This function is called whenever the button is pressed or released.
 * It publishes the appropriate event via Zbus.
 *
 * @param dev Pointer to the button device.
 * @param cb Pointer to the GPIO callback structure.
 * @param pins Bitmask of the pin(s) that triggered the interrupt.
 */
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};

	if (gpio_pin_get_dt(&button)) {
		press_time = k_uptime_get();

		msg.evt = BUTTON_EVT_PRESSED;
		printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
		zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
	} else {
		release_time = k_uptime_get();
		int64_t duration = release_time - press_time;

		msg.evt = BUTTON_EVT_RELEASED;
		printk("Button released at %" PRIu32 "\n", k_cycle_get_32());
		zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);

		if (duration >= EVENT_PRESSED_TIME) {
			struct msg_button_evt long_press_msg = {.evt = BUTTON_EVT_LONGPRESS};
			printk("Long press detected: %" PRId64 "ms\n", duration);
			zbus_chan_pub(&chan_button_evt, &long_press_msg, K_NO_WAIT);
		}
	}
}

/**
 * @brief Initializes the button pin as a GPIO input.
 *
 * Checks whether the button device is ready and configures the pin as input.
 *
 * @return 0 on success, or a non-zero error code on failure.
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
 * @brief Enables interrupts for the button on both rising and falling edges.
 *
 * Sets up the pin to trigger interrupts on both edges and registers the
 * `button_pressed` callback to handle the events.
 *
 * @return 0 on success, or a non-zero error code on failure.
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

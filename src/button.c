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
#define LONG_PRESS_THRESHOLD_MS 3000

/*
 * Get button configuration from the devicetree sw0 alias. This is mandatory.
 */
#define SW0_NODE DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS_OKAY(SW0_NODE)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static struct gpio_callback button_cb_data;

static int64_t pressed_time = 0;

/**
 * @brief GPIO callback handler for button state changes.
 *
 * Detects button press and release events, calculates the duration,
 * and publishes the corresponding event (pressed, released, long press)
 * to the ZBUS message channel.
 *
 * @param dev Pointer to the GPIO device.
 * @param cb Pointer to the registered callback structure.
 * @param pins Bitmask of the GPIO pins that triggered the interrupt.
 */
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};

	if (gpio_pin_get_dt(&button)) {
		pressed_time = k_uptime_get();
		msg.evt = BUTTON_EVT_PRESSED;
		printk("Button pressed at %" PRIu64 " ms\n", pressed_time);

		zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
	} else {
		int64_t released_time = k_uptime_get();
		int64_t pressed_duration = released_time - pressed_time;

		msg.evt = BUTTON_EVT_RELEASED;
		printk("Button released at %" PRIu64 " ms (duration: %" PRIu64 " ms)\n",
		       released_time, pressed_duration);
		
		zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);

		if (pressed_duration >= LONG_PRESS_THRESHOLD_MS) {
			msg.evt = BUTTON_EVT_LONGPRESS;
			printk("Button was long pressed\n");
			zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
		}
	}
}

/**
 * @brief Configures the button GPIO as input.
 *
 * Sets up the GPIO pin associated with the button as input based on
 * the devicetree configuration.
 *
 * @retval 0         Success.
 * @retval -ENODEV   Button device not ready.
 * @retval <0        Other error during GPIO configuration.
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
		printk("Error %d: failed to configure %s pin %d\n",
		       ret, button.port->name, button.pin);
		return ret;
	}

	return 0;
}

/**
 * @brief Enables GPIO interrupts for the button.
 *
 * Configures interrupts on both rising and falling edges
 * to detect press and release events. Registers and attaches
 * the button callback function.
 *
 * @retval 0    Success.
 * @retval <0   Error during interrupt setup or callback registration.
 */
int button_enable_interrupts(void)
{
	int ret;

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n",
		       ret, button.port->name, button.pin);
		return ret;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);

	return 0;
}

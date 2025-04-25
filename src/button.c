/**
 * @file button.c
 * @brief Button handling with press, release, and long-press events using Zephyr and ZBus.
 */

#include "button.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>

/**
 * @brief ZBus channel for button events.
 */
ZBUS_CHAN_DEFINE(chan_button_evt, struct msg_button_evt, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
				ZBUS_MSG_INIT(.evt = BUTTON_EVT_UNDEFINED));

/** @brief Delay time in milliseconds (unused in current code). */
#define SLEEP_TIME_MS 1

/** @brief Minimum press duration (ms) to consider a long press. */
#define LONG_PRESS_DURATION_MS 3000

/**
 * @brief Get button configuration from devicetree alias sw0.
 */
#define SW0_NODE DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS_OKAY(SW0_NODE)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif

/** @brief GPIO specification for the button. */
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);

/** @brief Structure to hold the GPIO callback data. */
static struct gpio_callback button_cb_data;

/** @brief Timestamp when the button was pressed. */
static int64_t button_press_time_ms = 0;

/**
 * @brief Callback triggered on button state change (press/release).
 *
 * Detects press and release events, and publishes them through ZBus.
 * If the button was held for more than 3 seconds, a long-press event is published right after release.
 *
 * @param dev Pointer to the device structure for the GPIO device.
 * @param cb Pointer to the GPIO callback structure.
 * @param pins Bitmap of the pin that triggered the callback.
 */
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};

	if (gpio_pin_get_dt(&button)) {
		msg.evt = BUTTON_EVT_PRESSED;
		printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
		button_press_time_ms = k_uptime_get();
	} else {
		msg.evt = BUTTON_EVT_RELEASED;
		printk("Button released at %" PRIu32 "\n", k_cycle_get_32());

		int64_t button_release_time_ms = k_uptime_get();
		int64_t button_press_duration_ms = button_release_time_ms - button_press_time_ms;

		if (button_press_duration_ms >= LONG_PRESS_DURATION_MS) {
			zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
			msg.evt = BUTTON_EVT_LONGPRESS;
			printk("Button long pressed for %" PRId64 " ms\n", button_press_duration_ms);
		}
	}

	zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
}

/**
 * @brief Initializes the button GPIO.
 *
 * Configures the button pin as an input using devicetree specification.
 *
 * @retval 0 on success.
 * @retval -ENODEV if the device is not ready.
 * @retval Other negative error codes on GPIO configuration failure.
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
 * @brief Enables interrupts for the button GPIO.
 *
 * Configures edge interrupts for both rising and falling edges, and registers the callback function.
 *
 * @retval 0 on success.
 * @retval Negative error code on failure.
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

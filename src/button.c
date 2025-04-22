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

static int64_t press_time = 0;
int64_t duration_ms;


/**
 * @brief Button pressed callback function.
 *
 * This function is called when the button is pressed or released.
 * It captures the timestamp of the press and release events and determines whether it
 * was a short or long press. It then publishes the corresponding
 * event to the zbus channel.
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param cb Pointer to the callback structure.
 * @param pins Bitmask of the pins that triggered the callback.
 */

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};

	if (gpio_pin_get_dt(&button)) {
		msg.evt = BUTTON_EVT_PRESSED;
		printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());

		press_time = k_uptime_get();
	} else {
		msg.evt = BUTTON_EVT_RELEASED;
		printk("Button released at %" PRIu32 "\n", k_cycle_get_32());

		int64_t release_time= k_uptime_get();
		duration_ms = release_time - press_time;
	}

	zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);

	if(duration_ms > LONG_PRESS_THRESHOLD_MS){
		msg.evt = BUTTON_EVT_LONGPRESS;
		zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
	}
}

/**
 * @brief Initialize the button GPIO pin.
 *
 * This function checks if the button device is ready and
 * configures the GPIO pin as an input.
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
 * @brief Enable interrupts for the button GPIO pin.
 *
 * This function configures the GPIO pin to trigger interrupts on both
 * rising and falling edges. It also registers the callback function
 * that handles the button press and release events.
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

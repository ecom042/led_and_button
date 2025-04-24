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
#define LONGPRESS_THRESHOLD_MS 3000 

/*
 * Get button configuration from the devicetree sw0 alias. This is mandatory.
 */
#define SW0_NODE DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS_OKAY(SW0_NODE)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static struct gpio_callback button_cb_data;
static int64_t button_press_time;

/**
 * @brief Callback function to handle button press and release events.
 *
 * This function is triggered when the button state changes (pressed or released).
 * It determines the type of button event (pressed, released, or long press) and
 * publishes the event to the zbus channel.
 *
 * @param dev Pointer to the device structure for the GPIO device.
 * @param cb Pointer to the GPIO callback structure.
 * @param pins Bitmask of pins that triggered the callback.
 */
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};

	if (gpio_pin_get_dt(&button)) {
		msg.evt = BUTTON_EVT_PRESSED;
		button_press_time = k_uptime_get();
		printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
		zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
	} else {
		msg.evt = BUTTON_EVT_RELEASED;
		int64_t press_duration = k_uptime_get() - button_press_time;
		
		printk("Button released at %" PRIu32 "\n", k_cycle_get_32());

		zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
		if (press_duration >= LONGPRESS_THRESHOLD_MS) {
			msg.evt = BUTTON_EVT_LONGPRESS;
			printk("Button long pressed for %" PRId64 " ms\n", press_duration);
			zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
		} 
	}

}

/**
 * @brief Initialize the button GPIO pin.
 *
 * This function configures the GPIO pin associated with the button as an input.
 * It first checks if the GPIO device is ready, and if not, logs an error message
 * and returns an error code. If the device is ready, it proceeds to configure
 * the pin as an input and returns the result of the configuration.
 *
 * @return 0 on success, or a negative error code on failure:
 *         - -ENODEV if the GPIO device is not ready.
 *         - Other negative error codes if the pin configuration fails.
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
 * This function configures the GPIO pin associated with the button to trigger
 * interrupts on both rising and falling edges. It also initializes and adds
 * a GPIO callback to handle the button press events.
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

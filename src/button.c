#include "button.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>

/* Define Zbus channel for button event communication */
ZBUS_CHAN_DEFINE(chan_button_evt, struct msg_button_evt, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(.evt = BUTTON_EVT_UNDEFINED));

#define SLEEP_TIME_MS 1

/* Get button configuration from the Devicetree sw0 alias */
#define SW0_NODE DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS_OKAY(SW0_NODE)
#error "Unsupported board: sw0 Devicetree alias is not defined"
#endif

/* Button GPIO specification */
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);

/* GPIO callback structure for button events */
static struct gpio_callback button_cb_data;

/* Timestamp for button press event */
static uint64_t button_press_timestamp = 0;

/* Timestamp for button release event */
static uint64_t button_release_timestamp = 0;

/** 
 * @brief Button interrupt callback function
 * 
 * Handles logic for press and release events, including long press detection.
 * 
 * @param dev Pointer to the device structure for the driver instance
 * @param cb Pointer to the gpio_callback structure
 * @param pins Bitmask of pins that triggered the callback
 */
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};

	if (gpio_pin_get_dt(&button)) {
		msg.evt = BUTTON_EVT_PRESSED;
		button_press_timestamp = k_uptime_get_32();
		printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
	} else {
		button_release_timestamp = k_uptime_get_32();
		msg.evt = BUTTON_EVT_RELEASED;
		printk("Button released at %" PRIu32 "\n", k_cycle_get_32());

		if ((button_release_timestamp - button_press_timestamp) >= 3000) {
			zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
			msg.evt = BUTTON_EVT_LONGPRESS;
		}
	}

	zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
}

/** 
 * @brief Initialize the button GPIO
 * 
 * Configures the GPIO pin as input and checks readiness of the device.
 * 
 * @return 0 if successful, negative error code otherwise
 */
int button_init(void)
{
	int ret;

	if (!gpio_is_ready_dt(&button)) {
		printk("Error: Button device %s is not ready\n", button.port->name);
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		printk("Error %d: Failed to configure %s pin %d\n", ret, button.port->name,
		       button.pin);
		return ret;
	}

	return 0;
}

/** 
 * @brief Enable button GPIO interrupts
 * 
 * Configures interrupts on button GPIO pin and sets the callback function.
 * 
 * @return 0 if successful, negative error code otherwise
 */
int button_enable_interrupts(void)
{
	int ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);
	if (ret != 0) {
		printk("Error %d: Failed to configure interrupt on %s pin %d\n", ret,
		       button.port->name, button.pin);
		return ret;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);

	return 0;
}

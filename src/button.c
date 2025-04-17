#include "button.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>
#include <zephyr/sys/time_units.h>

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

static uint32_t button_press_time = 0;

/**
 * @brief Button pressed callback function.
 *
 * This function is called when the button is pressed or released.
 * It sends a message to the zbus channel with the event type.
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param cb Pointer to the callback structure.
 * @param pins Bitmask of the pins that triggered the callback.
 */
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};

	bool was_long_pressed = 0;

	if (gpio_pin_get_dt(&button)) {
		msg.evt = BUTTON_EVT_PRESSED;
		button_press_time = k_cyc_to_sec_floor32(k_cycle_get_32());

		printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
	} else {
		uint32_t button_release_time = k_cyc_to_sec_floor32(k_cycle_get_32());
		msg.evt = BUTTON_EVT_RELEASED;

		was_long_pressed = (button_press_time - button_release_time) > 3;

		printk("Button released at %" PRIu32 "\n", k_cycle_get_32());
	}

	zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);

	if (was_long_pressed) {
		msg.evt = BUTTON_EVT_LONGPRESS;
		zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
	}
}

/**
 * @brief Initialize the button GPIO pin.
 *
 * This function configures the GPIO pin for the button as an input pin.
 * It also sets up the interrupt for the button press and release events.
 *
 * @return 0 on success, negative error code on failure.
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
 * This function configures the GPIO pin for the button to trigger interrupts
 * on both rising and falling edges. It also initializes the callback function
 * for handling button press and release events.
 *
 * @return 0 on success, negative error code on failure.
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

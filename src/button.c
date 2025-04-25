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
static int64_t press_timestamp = 0; 


/**
 * @brief Callback function for when the button is pressed
 * 
 * This function is called when the button generates an interruption, indicating
 * one of the three button events: press, release or long press. 
 * 
 * It determines the type of the event and then publishes its correspondent
 * event message to the zbus channel.
 *
 * @param dev Pointer to the device structure representing the GPIO hardware. 
 * @param cb Pointer to the GPIO callback structure associated with the interrupt.
 * @param pins Bitfield that indicates which GPIO pins triggered this callback.
 */
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};

	if (gpio_pin_get_dt(&button)) {
		press_timestamp = k_uptime_get(); 
		msg.evt = BUTTON_EVT_PRESSED;
		printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
		zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
	} else {
		int64_t release_timestamp = k_uptime_get();
		int64_t duration = release_timestamp - press_timestamp; 
		msg.evt = BUTTON_EVT_RELEASED;
		printk("Button released at %" PRIu32 "\n", k_cycle_get_32());

		zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);

		if (duration >= 3000) {
			msg.evt = BUTTON_EVT_LONGPRESS;
			printk("Button longpress detected at %" PRIu32 "\n", k_cycle_get_32());
			zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
		}

	}

}


/**
 * @brief Initialize the button
 * 
 * This function checks if the device is ready, then configure its pin as an input.
 * It throws error if it's not ready or if the configuration of the pin fails. 
 * 
 * @return 0 if successful or error code in case of failure.
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
 * @brief Enable interrupts for the button.
 * 
 * This function will configure the button pin to trigger the interrupts on both
 * edges (rising and falling).asm
 * 
 * Also it initializes and register the callback handler for the pin.
 *
 * @return 0 if successful or error code in case of failure.
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

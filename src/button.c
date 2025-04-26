#include "button.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>

// Define zbus channel for button events with initial undefined state
ZBUS_CHAN_DEFINE(chan_button_evt, struct msg_button_evt, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
         ZBUS_MSG_INIT(.evt = BUTTON_EVT_UNDEFINED));

#define SLEEP_TIME_MS 1

// Retrieve the configuration of the button from the device tree
#define SW0_NODE DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS_OKAY(SW0_NODE)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static struct gpio_callback button_cb_data;
static int64_t press_timestamp = 0;


/**
 * @brief Handler callback for changes in the button state
 * 
 * Processes the interrupts from the button in order to detect different patterns in the 
 * interactions as: press, release and longpress.
 * 
 * Then it publishes corresponding events to the channel when detected.
 *
 * @param dev GPIO device instance triggering the interrupt
 * @param cb Callback structure reference
 * @param pins Mask of GPIO pins that caused the interrupt
 */
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};

    if (gpio_pin_get_dt(&button)) {
        press_timestamp = k_uptime_get(); 
        msg.evt = BUTTON_EVT_PRESSED;
        printk("Button press detected at %" PRIu32 "\n", k_cycle_get_32());
        zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
    } else {
        int64_t release_timestamp = k_uptime_get();
        int64_t duration = release_timestamp - press_timestamp; 
        msg.evt = BUTTON_EVT_RELEASED;
        printk("Button release detected at %" PRIu32 "\n", k_cycle_get_32());

        zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);

        if (duration >= 3000) {
            msg.evt = BUTTON_EVT_LONGPRESS;
            printk("Long press detected at %" PRIu32 "\n", k_cycle_get_32());
            zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
        }
    }
}


/**
 * @brief Set up button hardware
 * 
 * This function verifies if the device is ready, then configuring its pin as an input. 
 * It return an error if the device is not ready or if there is some error in the configuration
 * of the pin.
 * 
 * @return 0 on success, negative error code on failure
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
 * @brief Activate button interrupt detection
 * 
 * Enables interrupts on both edges for both press and release of the button 
 * and also registers the callback handler.
 * 
 * @return 0 on success, negative error code on failure
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
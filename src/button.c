#include "button.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>

/*
 * This channel is used to communicate button events, such as press, release,
 * and long press, to other parts of the application.
 */
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

/*
 * This function is called whenever the button state changes.
 * It determines the type of event (press, release, or long press), logs the event,
 * and publishes it to the ZBUS channel.
 */
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};

    if (gpio_pin_get_dt(&button)) {
        msg.evt = BUTTON_EVT_PRESSED;

        button_press_time = k_uptime_get_32();

        printk("Button pressed at %" PRIu32 " ms\n", button_press_time);

        zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
    } else {
        uint32_t button_release_time = k_uptime_get_32();
        uint32_t duration = button_release_time - button_press_time;

        msg.evt = BUTTON_EVT_RELEASED;
        printk("Button released at %" PRIu32 " ms\n", button_release_time);


        zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);

        if (duration > 3000) {
            msg.evt = BUTTON_EVT_LONGPRESS;
            printk("Button long-pressed for %" PRIu32 " ms\n", duration);

            zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
        }
    }
}

/*
 * This function configures the button GPIO pin as an input. It also checks
 * if the button device is ready before proceeding.
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

/*
 * this function configures interrupts on the button GPIO pin for both rising
 * and falling edges. It also initializes and registers the GPIO callback.
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

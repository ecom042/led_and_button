#include "button.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>

/** @brief ZBus channel for button events.
 *
 * This channel is used to publish `msg_button_evt` type events
 * related to the physical button's state, such as pressed, released, or long press.
 */
ZBUS_CHAN_DEFINE(chan_button_evt, struct msg_button_evt, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(.evt = BUTTON_EVT_UNDEFINED));

/** @brief Sleep time (in milliseconds) used when necessary. */
#define SLEEP_TIME_MS 1

/** @brief Button node defined by the `sw0` alias in the Devicetree. */
#define SW0_NODE DT_ALIAS(sw0)

/*
 * Checks whether the `sw0` alias is defined and enabled in the Devicetree.
 * If not, compilation is stopped with an error.
 */
#if !DT_NODE_HAS_STATUS_OKAY(SW0_NODE)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif

/** @brief Button specification obtained via Devicetree. */
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);

/** @brief Callback structure associated with the button. */
static struct gpio_callback button_cb_data;

/** @brief Global flag indicating whether a long press was detected.
 *
 * This flag is set by the timer after 3 seconds of button press.
 * It is checked when the button is released to emit the BUTTON_EVT_LONGPRESS event.
 */
static bool longpress_detected = false;

/** @brief Timer callback for detecting long press.
 *
 * This function is automatically called after 3 seconds if the button remains pressed.
 * It simply sets the `longpress_detected` flag to true.
 *
 * @param timer_id Pointer to the timer that triggered (unused).
 */
void timer_button_cb(struct k_timer *timer_id)
{
	longpress_detected = true;
}

/** @brief Defines the timer used to detect long press. */
K_TIMER_DEFINE(timer_button, timer_button_cb, NULL);

/** @brief Button interrupt callback.
 *
 * This function is automatically called when the button state changes: pressed or released.
 * When pressed, it starts a 3-second timer. If released, it cancels the timer and sends the
 * corresponding events via ZBus. If the button was held for more than 3 seconds, it also sends
 * the BUTTON_EVT_LONGPRESS event after the BUTTON_EVT_RELEASED event.
 *
 * @param dev Pointer to the GPIO device.
 * @param cb Pointer to the GPIO callback structure.
 * @param pins Pin mask that triggered the interrupt.
 */
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};

	if (gpio_pin_get_dt(&button)) {
		msg.evt = BUTTON_EVT_PRESSED;
		longpress_detected = false;
		k_timer_start(&timer_button, K_SECONDS(3), K_NO_WAIT);
		printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
	} else {
		msg.evt = BUTTON_EVT_RELEASED;
		k_timer_stop(&timer_button);
		printk("Button released at %" PRIu32 "\n", k_cycle_get_32());
	}

	zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);

	if (longpress_detected) {
		msg.evt = BUTTON_EVT_LONGPRESS;
		zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
		printk("Button long press at %" PRIu32 "\n", k_cycle_get_32());
		longpress_detected = false;
	}
}

/** @brief Initializes the button from the Devicetree specification.
 *
 * Configures the button pin as a GPIO input.
 *
 * @retval 0 Success.
 * @retval -ENODEV If the button device is not ready.
 * @retval <0 GPIO pin configuration error code.
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

/** @brief Enables interrupts for the button.
 *
 * Configures interrupts on both rising and falling edges of the button signal.
 * Adds the interrupt callback to detect button press/release.
 *
 * @retval 0 Success.
 * @retval <0 Error code in case of failure to configure.
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

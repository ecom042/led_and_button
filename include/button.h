#ifndef _BUTTON_H_
#define _BUTTON_H_
#include <zephyr/zbus/zbus.h>

/**
 * @brief Time in milliseconds to consider a button press as a long press
 */
#define LONG_PRESS_TIME_MILLI 3000

/**
 * @brief Enumeration of button event types
 *
 * Defines the different types of button events that can be detected.
 */
enum button_evt_type {
	BUTTON_EVT_UNDEFINED, /**< Undefined event */
	BUTTON_EVT_PRESSED,   /**< Button press event */
	BUTTON_EVT_RELEASED,  /**< Button release event */
	BUTTON_EVT_LONGPRESS, /**< Long press event */
};

/**
 * @brief Button event message structure
 *
 * Contains the type of button event that occurred.
 */
struct msg_button_evt {
	enum button_evt_type evt;
};

ZBUS_CHAN_DECLARE(chan_button_evt);

/**
 * @brief Initialize the button hardware
 *
 * Configures the button GPIO pin as an input.
 *
 * @return 0 if successful, negative errno code on failure
 */
int button_init(void);

/**
 * @brief Enable button interrupt handling
 *
 * Configures interrupts for the button to detect both rising and falling edges,
 * and registers the callback function.
 *
 * @return 0 if successful, negative errno code on failure
 */
int button_enable_interrupts(void);

#endif /* _BUTTON_H_ */

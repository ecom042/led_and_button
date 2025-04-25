#ifndef _BUTTON_H_
#define _BUTTON_H_

#include <zephyr/zbus/zbus.h>

/**
 * @file
 * @brief Button event definitions and API declarations.
 *
 * This header defines the button event types and declares the API functions
 * for initializing and enabling button interrupts.
 */

/**
 * @enum button_evt_type
 * @brief Enumeration of possible button event types.
 */
enum button_evt_type {
    BUTTON_EVT_UNDEFINED, /**< Undefined button event. */
    BUTTON_EVT_PRESSED,   /**< Button pressed event. */
    BUTTON_EVT_RELEASED,  /**< Button released event. */
    BUTTON_EVT_LONGPRESS  /**< Button long press event. */
};

/**
 * @struct msg_button_evt
 * @brief Structure representing a button event message.
 */
struct msg_button_evt {
    enum button_evt_type evt; /**< Type of the button event. */
};

/**
 * @brief Declaration of the Zbus channel for button events.
 */
ZBUS_CHAN_DECLARE(chan_button_evt);

/**
 * @brief Initialize the button GPIO.
 *
 * Configures the button GPIO pin as input and checks the readiness of the device.
 *
 * @return 0 if successful, or a negative error code on failure.
 */
int button_init(void);

/**
 * @brief Enable button GPIO interrupts.
 *
 * Configures interrupts on the button GPIO pin and sets the callback function.
 *
 * @return 0 if successful, or a negative error code on failure.
 */
int button_enable_interrupts(void);

#endif /* _BUTTON_H_ */

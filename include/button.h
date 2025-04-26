/**
 * @file button.h
 * @brief Interface for the button events.
 * 
 * This file declares the button event types, the structure of the 
 * event message, the zbus channel for the events and the button.c functions.
 *
 */

#ifndef _BUTTON_H_
#define _BUTTON_H_
#include <zephyr/zbus/zbus.h>

enum button_evt_type {
	BUTTON_EVT_UNDEFINED, /** Button event undefined */
	BUTTON_EVT_PRESSED, /** Button press event */
	BUTTON_EVT_RELEASED, /** Button release event */
	BUTTON_EVT_LONGPRESS, /** Button longpress event */
};

/** Struct of the message with the button event information */
struct msg_button_evt {
	enum button_evt_type evt;
};

/** Deckaration of the button event channel in zbus */
ZBUS_CHAN_DECLARE(chan_button_evt);

/**
 * @brief Initialization of the button
 * 
 * Checks if the device is ready, if it is then it configures its pin as an input.
 * 
 * @return Returns 0 if successful or an error code if it fails.
 */
int button_init(void);
/**
 * @brief Enabling of the interrupts for the button.
 * 
 * Configures the interrupts for both rising and falling edges and 
 * registers the callback handler.
 *
 * @return Return 0 if successful or an error code if it fails.
 */
int button_enable_interrupts(void);

#endif /* _BUTTON_H_ */

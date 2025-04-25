/**
 * @file button.h
 * @brief Interface for the button events.
 * 
 * This header file declares the button event types, the structure of message
 * used to communicated said events, the zbus channel for said events and initializes
 * the functions declared in button.c.
 *
 */

#ifndef _BUTTON_H_
#define _BUTTON_H_
#include <zephyr/zbus/zbus.h>

/** The types of button events */
enum button_evt_type {
	BUTTON_EVT_UNDEFINED, /** Event not defined */
	BUTTON_EVT_PRESSED, /** Button press */
	BUTTON_EVT_RELEASED, /** Button release */
	BUTTON_EVT_LONGPRESS, /** Button long press detected */
};

/** Structure of the message the carries button event information */
struct msg_button_evt {
	enum button_evt_type evt;
};

/** The zbus channel for publishing the events */
ZBUS_CHAN_DECLARE(chan_button_evt);

/**
 * @brief Initialize the button
 * 
 * This function checks the decive readiness, and then if it is ready it 
 * configures its pin as an input.
 * 
 * @return 0 if successful or error code in case of failure.
 */
int button_init(void);

/**
 * @brief Enable the interrupts for the button.
 * 
 * This function will configure the interrupts for both edges (rising and falling)
 * and initializes and registers the callback function/handler.
 *
 * @return 0 if successful or error code in case of failure.
 */
int button_enable_interrupts(void);

#endif /* _BUTTON_H_ */

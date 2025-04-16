#ifndef _BUTTON_H_
#define _BUTTON_H_
#include <zephyr/zbus/zbus.h>

enum button_evt_type {
	BUTTON_EVT_UNDEFINED,
	BUTTON_EVT_PRESSED,
	BUTTON_EVT_RELEASED,
	BUTTON_EVT_LONGPRESS,
};

struct msg_button_evt {
	enum button_evt_type evt;
};

ZBUS_CHAN_DECLARE(chan_button_evt);

int button_init(void);
int button_enable_interrupts(void);

#endif /* _BUTTON_H_ */

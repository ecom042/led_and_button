/*
 * Copyright (c) 2025 Rodrigo Peixoto <rodrigopex@ic.ufal.br>
 * SPDX-License-Identifier: Apache-2.0
 */

 /*
  * This test suite verifies the functionality of the button driver implementation.
  * It uses GPIO emulation to simulate button presses and releases, and validates
  * that the correct button events are generated and published via zbus.
  */
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include "button.h"

/*
 *Subscribe to button events channel to receive notifications.
 */
ZBUS_MSG_SUBSCRIBER_DEFINE(msub_button_evt);
ZBUS_CHAN_ADD_OBS(chan_button_evt, msub_button_evt, 3);

/*
 * Test fixture containing button GPIO specification.
 */
static struct button_fixture {
	const struct gpio_dt_spec button_gpio;
} fixture = {
	.button_gpio = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios),
};

/*
 * Sets the GPIO pin to low (active) and waits for debounce
 */
#define BUTTON_PRESS(_fixture)                                                                     \
	do {                                                                                       \
		gpio_emul_input_set(_fixture->button_gpio.port, _fixture->button_gpio.pin, 0);     \
		k_msleep(80);                                                                      \
	} while (0)

/*
 * Sets the GPIO pin to high (inactive) and waits for debounce
 */
#define BUTTON_RELEASE(_fixture)                                                                   \
	do {                                                                                       \
		gpio_emul_input_set(_fixture->button_gpio.port, _fixture->button_gpio.pin, 1);     \
		k_msleep(80);                                                                      \
	} while (0)

/*
 * Initializes the button driver, verifies GPIO port is available,
 * and sets the initial button state to released.
 */
static void *button_test_setup(void)
{
	zassert_not_null(fixture.button_gpio.port);

	button_init();

	gpio_emul_input_set(fixture.button_gpio.port, fixture.button_gpio.pin, 1);

	button_enable_interrupts();

	return &fixture;
}

/*
 * Verifies that pressing and releasing the button generates
 * the correct PRESSED and RELEASED events.
 */
ZTEST_F(button, test_01_single_press)
{
	const struct zbus_channel *chan;
	struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};

	/* Press the button and verify PRESSED event */
	BUTTON_PRESS(fixture);

	zbus_sub_wait_msg(&msub_button_evt, &chan, &msg, K_SECONDS(1));

	zassert_true(msg.evt == BUTTON_EVT_PRESSED);

	k_msleep(500);

	/* Release the button and verify RELEASED event */
	BUTTON_RELEASE(fixture);

	zbus_sub_wait_msg(&msub_button_evt, &chan, &msg, K_SECONDS(1));

	zassert_true(msg.evt == BUTTON_EVT_RELEASED);
}

/*
 * Verifies that pressing the button for more than 3 seconds generates
 * PRESSED, RELEASED and LONGPRESS events in the correct sequence.
 */
ZTEST_F(button, test_02_long_press)
{
	const struct zbus_channel *chan;
	struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};

	/* Press the button and verify PRESSED event */
	BUTTON_PRESS(fixture);

	zbus_sub_wait_msg(&msub_button_evt, &chan, &msg, K_SECONDS(1));

	zassert_true(msg.evt == BUTTON_EVT_PRESSED);

	/* Hold for 3 seconds to trigger long press detection */
	k_msleep(3000);

	/* Release the button and verify RELEASED followed by LONGPRESS events */
	BUTTON_RELEASE(fixture);

	zbus_sub_wait_msg(&msub_button_evt, &chan, &msg, K_SECONDS(1));

	zassert_true(msg.evt == BUTTON_EVT_RELEASED);

	zbus_sub_wait_msg(&msub_button_evt, &chan, &msg, K_SECONDS(1));

	zassert_true(msg.evt == BUTTON_EVT_LONGPRESS);
}

/*
 * The test suite consists of setup, test cases, and optional teardown.
 */
ZTEST_SUITE(button, NULL, button_test_setup, NULL, NULL, NULL);

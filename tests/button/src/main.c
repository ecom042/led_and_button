/*
 * Copyright (c) 2025 Rodrigo Peixoto <rodrigopex@ic.ufal.br>
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/drivers/gpio/gpio_emul.h>

#include "button.h"

/** @brief Subscriber definition for button events.
 *
 * This macro defines a subscriber object that listens for messages
 * on the button event channel. The subscriber receives button-related
 * events (e.g., press, release, long press).
 */
ZBUS_MSG_SUBSCRIBER_DEFINE(msub_button_evt);

/** @brief Channel observer definition for button event channel.
 *
 * This macro defines an observer for the button event channel.
 * The observer listens to the `msub_button_evt` subscriber and can
 * receive up to 3 messages before being blocked (configured with 3).
 */
ZBUS_CHAN_ADD_OBS(chan_button_evt, msub_button_evt, 3);

/** @brief Fixture to test the button.
 *
 * This structure contains the button specification from the Devicetree,
 * which is used in test functions to emulate button actions.
 */
static struct button_fixture {
	const struct gpio_dt_spec button_gpio;
} fixture = {
	.button_gpio = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios),
};

/** @brief Macros to press and release the button during tests. */
#define BUTTON_PRESS(_fixture)                                                                     \
	do {                                                                                       \
		gpio_emul_input_set(_fixture->button_gpio.port, _fixture->button_gpio.pin, 0);     \
		k_msleep(80);                                                                      \
	} while (0)

#define BUTTON_RELEASE(_fixture)                                                                   \
	do {                                                                                       \
		gpio_emul_input_set(_fixture->button_gpio.port, _fixture->button_gpio.pin, 1);     \
		k_msleep(80);                                                                      \
	} while (0)

/** @brief Button test setup function.
 *
 * Initializes the required resources to test the button, including
 * configuring interrupts and setting the initial button state.
 *
 * @retval pointer To the test fixture.
 */
static void *button_test_setup(void)
{
	zassert_not_null(fixture.button_gpio.port);

	button_init();

	gpio_emul_input_set(fixture.button_gpio.port, fixture.button_gpio.pin, 1);

	button_enable_interrupts();

	return &fixture;
}

/** @brief Test for a single button press.
 *
 * This test checks the behavior of the button when it is pressed and released.
 * The `BUTTON_EVT_PRESSED` event should be triggered when the button is pressed.
 * The `BUTTON_EVT_RELEASED` event should be triggered when the button is released.
 */
ZTEST_F(button, test_01_single_press)
{
	const struct zbus_channel *chan;
	struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};

	BUTTON_PRESS(fixture);

	zbus_sub_wait_msg(&msub_button_evt, &chan, &msg, K_SECONDS(1));

	zassert_true(msg.evt == BUTTON_EVT_PRESSED);

	k_msleep(500);

	BUTTON_RELEASE(fixture);

	zbus_sub_wait_msg(&msub_button_evt, &chan, &msg, K_SECONDS(1));

	zassert_true(msg.evt == BUTTON_EVT_RELEASED);
}

/** @brief Test for a long button press.
 *
 * This test checks the button's behavior for a long press
 * (longer than 3 seconds). The `BUTTON_EVT_LONGPRESS` event should be triggered
 * after the `BUTTON_EVT_RELEASED` event, if the button was held for more than 3 seconds.
 */
ZTEST_F(button, test_02_long_press)
{
	const struct zbus_channel *chan;
	struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};

	BUTTON_PRESS(fixture);

	zbus_sub_wait_msg(&msub_button_evt, &chan, &msg, K_SECONDS(1));

	zassert_true(msg.evt == BUTTON_EVT_PRESSED);

	k_msleep(3000);

	BUTTON_RELEASE(fixture);

	zbus_sub_wait_msg(&msub_button_evt, &chan, &msg, K_SECONDS(1));

	zassert_true(msg.evt == BUTTON_EVT_RELEASED);

	zbus_sub_wait_msg(&msub_button_evt, &chan, &msg, K_SECONDS(1));

	zassert_true(msg.evt == BUTTON_EVT_LONGPRESS);
}

/** @brief Test to ensure the BUTTON_EVT_LONGPRESS is correctly triggered.
 *
 * This test verifies that when the button is held for more than 3 seconds and then released,
 * the BUTTON_EVT_LONGPRESS event is triggered immediately after the BUTTON_EVT_RELEASED event.
 */
ZTEST_F(button, test_03_long_press_event)
{
	const struct zbus_channel *chan;
	struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};

	BUTTON_PRESS(fixture);

	zbus_sub_wait_msg(&msub_button_evt, &chan, &msg, K_SECONDS(1));
	zassert_true(msg.evt == BUTTON_EVT_PRESSED);

	k_msleep(3000);

	BUTTON_RELEASE(fixture);

	zbus_sub_wait_msg(&msub_button_evt, &chan, &msg, K_SECONDS(1));
	zassert_true(msg.evt == BUTTON_EVT_RELEASED);

	zbus_sub_wait_msg(&msub_button_evt, &chan, &msg, K_SECONDS(1));
	zassert_true(msg.evt == BUTTON_EVT_LONGPRESS);
}

/** @brief Ensures that BUTTON_EVT_LONGPRESS is not emitted if released before 3 seconds. */
ZTEST_F(button, test_04_no_long_press_if_short_duration)
{
	const struct zbus_channel *chan;
	struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};

	BUTTON_PRESS(fixture);

	zbus_sub_wait_msg(&msub_button_evt, &chan, &msg, K_SECONDS(1));
	zassert_true(msg.evt == BUTTON_EVT_PRESSED);

	k_msleep(2000);

	BUTTON_RELEASE(fixture);

	zbus_sub_wait_msg(&msub_button_evt, &chan, &msg, K_SECONDS(1));
	zassert_true(msg.evt == BUTTON_EVT_RELEASED);

	bool received = zbus_sub_wait_msg(&msub_button_evt, &chan, &msg, K_MSEC(500)) == 0;
	zassert_false(received && msg.evt == BUTTON_EVT_LONGPRESS, "Unexpected LONGPRESS event");
}

/** @brief Ensures correct event order: PRESSED -> RELEASED -> LONGPRESS on long press. */
ZTEST_F(button, test_05_event_order_on_long_press)
{
	const struct zbus_channel *chan;
	struct msg_button_evt msg1, msg2, msg3;

	BUTTON_PRESS(fixture);
	zbus_sub_wait_msg(&msub_button_evt, &chan, &msg1, K_SECONDS(1));
	k_msleep(3000);
	BUTTON_RELEASE(fixture);
	zbus_sub_wait_msg(&msub_button_evt, &chan, &msg2, K_SECONDS(1));
	zbus_sub_wait_msg(&msub_button_evt, &chan, &msg3, K_SECONDS(1));

	zassert_equal(msg1.evt, BUTTON_EVT_PRESSED);
	zassert_equal(msg2.evt, BUTTON_EVT_RELEASED);
	zassert_equal(msg3.evt, BUTTON_EVT_LONGPRESS);
}

/** @brief Button test suite configuration.
 *
 * Defines the setup and teardown for the button tests.
 */
ZTEST_SUITE(button, NULL, button_test_setup, NULL, NULL, NULL);

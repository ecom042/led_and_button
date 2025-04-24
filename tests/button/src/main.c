/*
 * Copyright (c) 2025 Rodrigo Peixoto <rodrigopex@ic.ufal.br>
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/drivers/gpio/gpio_emul.h>

#include "button.h"

ZBUS_MSG_SUBSCRIBER_DEFINE(msub_button_evt);

ZBUS_CHAN_ADD_OBS(chan_button_evt, msub_button_evt, 3);

static struct button_fixture {
	const struct gpio_dt_spec button_gpio;
} fixture = {
	.button_gpio = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios),
};

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

static void *button_test_setup(void)
{
	zassert_not_null(fixture.button_gpio.port);

	button_init();

	gpio_emul_input_set(fixture.button_gpio.port, fixture.button_gpio.pin, 1);

	button_enable_interrupts();

	return &fixture;
}

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

ZTEST_SUITE(button, NULL, button_test_setup, NULL, NULL, NULL);

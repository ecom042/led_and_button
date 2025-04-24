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

/**
 * @brief Test fixture structure for button tests
 *
 * Contains the GPIO device specification for the button used in tests.
 */
static struct button_fixture {
	const struct gpio_dt_spec button_gpio;
} fixture = {
	.button_gpio = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios),
};

/**
 * @brief Simulates a button press by setting the GPIO pin to low
 *
 * @param _fixture Pointer to the button_fixture structure
 */
#define BUTTON_PRESS(_fixture)                                                                     \
	do {                                                                                       \
		gpio_emul_input_set(_fixture->button_gpio.port, _fixture->button_gpio.pin, 0);     \
		k_msleep(80);                                                                      \
	} while (0)

/**
 * @brief Simulates a button release by setting the GPIO pin to high
 *
 * @param _fixture Pointer to the button_fixture structure
 */
#define BUTTON_RELEASE(_fixture)                                                                   \
	do {                                                                                       \
		gpio_emul_input_set(_fixture->button_gpio.port, _fixture->button_gpio.pin, 1);     \
		k_msleep(80);                                                                      \
	} while (0)

/**
 * @brief Setup function that runs before each test
 *
 * Initializes the button, sets its initial state, and enables interrupts.
 *
 * @return Pointer to the test fixture
 */
static void *button_test_setup(void)
{
	zassert_not_null(fixture.button_gpio.port);

	button_init();

	gpio_emul_input_set(fixture.button_gpio.port, fixture.button_gpio.pin, 1);

	button_enable_interrupts();

	return &fixture;
}

/**
 * @brief Test case for a single button press and release
 *
 * Verifies that the correct button events (pressed, released) are
 * published when a button is pressed and released.
 *
 * @param fixture Pointer to the button_fixture structure
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

/**
 * @brief Test case for a long button press
 *
 * Verifies that the correct button events (pressed, released, longpress)
 * are published when a button is pressed for a long time and then released.
 *
 * @param fixture Pointer to the button_fixture structure
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

/**
 * @brief Button test suite definition
 * 
 * Defines the test suite with setup and teardown functions.
 * NULL arguments indicate that the default functions or no functions
 * are used for the corresponding setup/teardown steps.
 */
ZTEST_SUITE(button, NULL, button_test_setup, NULL, NULL, NULL);

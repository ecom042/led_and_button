/*
 * Copyright (c) 2025 Rodrigo Peixoto <rodrigopex@ic.ufal.br>
 * SPDX-License-Identifier: Apache-2.0
 */

/** 
* @file test_button.c
* @brief Verification tests for button driver functionality
*
* This file has automated test cases for validating the button press
* detection logic. 
* 
* It uses GPIO emulation to simulate the real/physical button interactions
* without requiring the real hardware. 
*
*/

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/drivers/gpio/gpio_emul.h>

#include "button.h"

/* Message subscriber for receiving button event notifications */
ZBUS_MSG_SUBSCRIBER_DEFINE(msub_button_evt);

/* Register subscriber to monitor button event channel */
ZBUS_CHAN_ADD_OBS(chan_button_evt, msub_button_evt, 3);

/**
 * @struct button_fixture
 * @brief Container for test configuration and GPIO specifications
 */
static struct button_fixture {
	const struct gpio_dt_spec button_gpio;
} fixture = {
	.button_gpio = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios),
};

/* Simulate physical button press action */
#define BUTTON_PRESS(_fixture)                                                                     \
	do {                                                                                       \
		gpio_emul_input_set(_fixture->button_gpio.port, _fixture->button_gpio.pin, 0);     \
		k_msleep(80); /* Debounce delay */                                                \
	} while (0)

/* Simulate physical button release action */	
#define BUTTON_RELEASE(_fixture)                                                                   \
	do {                                                                                       \
		gpio_emul_input_set(_fixture->button_gpio.port, _fixture->button_gpio.pin, 1);     \
		k_msleep(80); /* Debounce delay */                                                \
	} while (0)

/**
 * @brief Test environment initialization
 * 
 * Prepares test conditions by: validating the GPIO device availability, configuring the
 * interface of the button, setting the initial button state and enabling the interrupt 
 * detection. 
 *
 * @return Pointer to initialized test fixture
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
 * @brief Validate basic press/release detection
 * 
 * Verifies if the event are being generated correctly for button press detection
 * and a subsequent release detection.
 * 
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
 * @brief Validates the longpress detection
 * 
 * This function simulates the longpress of the button (press longer than 3 seconds)
 * and then verifies if the correspondent correct events are published on the channel.
 *
 */
ZTEST_F(button, test_02_long_press)
{
	const struct zbus_channel *chan;
	struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};

	BUTTON_PRESS(fixture);

	zbus_sub_wait_msg(&msub_button_evt, &chan, &msg, K_SECONDS(1));

	zassert_true(msg.evt == BUTTON_EVT_PRESSED);

	/* Maintain the pressed state to trigger long-press */
	k_msleep(3000);

	BUTTON_RELEASE(fixture);

	zbus_sub_wait_msg(&msub_button_evt, &chan, &msg, K_SECONDS(1));

	zassert_true(msg.evt == BUTTON_EVT_RELEASED);

	zbus_sub_wait_msg(&msub_button_evt, &chan, &msg, K_SECONDS(1));

	zassert_true(msg.evt == BUTTON_EVT_LONGPRESS);
}

/* Register test suite with the ztest framework */
ZTEST_SUITE(button, NULL, button_test_setup, NULL, NULL, NULL);
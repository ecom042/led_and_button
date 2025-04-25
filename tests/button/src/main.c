/*
 * Copyright (c) 2025 Rodrigo Peixoto <rodrigopex@ic.ufal.br>
 * SPDX-License-Identifier: Apache-2.0
 */

/** 
* @file test_button.c
* @brief Unit tests for the button driver.
*
* This file contain tests used to verify if the button handling code is behaving
* correctly.
*
* It will use emulation to simulate the inputs from the button without real
* hardware.
*/




#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/drivers/gpio/gpio_emul.h>

#include "button.h"

/* Define an instance of a zbus subscriber to receive the button events */
ZBUS_MSG_SUBSCRIBER_DEFINE(msub_button_evt);

/* Adds the zubs subscriber as an observer of the event channel */
ZBUS_CHAN_ADD_OBS(chan_button_evt, msub_button_evt, 3);

/**
 * @struct button_fixture
 * @brief Test fixture containing configuration for the button device.
 */
static struct button_fixture {
	const struct gpio_dt_spec button_gpio;
} fixture = {
	.button_gpio = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios),
};

/* These are macros used to simulate button press and release using the GPIO emulator */
/* 'Press' will set the pin low and will wait for debounce */
#define BUTTON_PRESS(_fixture)                                                                     \
	do {                                                                                       \
		gpio_emul_input_set(_fixture->button_gpio.port, _fixture->button_gpio.pin, 0);     \
		k_msleep(80);                                                                      \
	} while (0)

	
/* 'Release' will set the pin high and wait for debounce */
#define BUTTON_RELEASE(_fixture)                                                                   \
	do {                                                                                       \
		gpio_emul_input_set(_fixture->button_gpio.port, _fixture->button_gpio.pin, 1);     \
		k_msleep(80);                                                                      \
	} while (0)

/**
 * @brief Function of setup to run before each test.
 *
 * Initializes the button device, configures it as input, and enables interrupts.
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
 * @brief Test button press and release events.
 *
 * This will simulate both press and release of the button and verify
 * if the correct events are published on the zbus channel.
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
 * @brief Test long press event.
 * 
 * This will simulate when the holding of a button longer than the defined
 * press threshold (in this case, 3 seconds) and verifies if the correct
 * events are published on the zbus channel.
 *
 */
ZTEST_F(button, test_02_long_press)
{
	const struct zbus_channel *chan;
	struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};

	BUTTON_PRESS(fixture);

	zbus_sub_wait_msg(&msub_button_evt, &chan, &msg, K_SECONDS(1));

	zassert_true(msg.evt == BUTTON_EVT_PRESSED);

	/* Holding button for 3 seconds to trigger the long press */
	k_msleep(3000);

	BUTTON_RELEASE(fixture);

	zbus_sub_wait_msg(&msub_button_evt, &chan, &msg, K_SECONDS(1));

	zassert_true(msg.evt == BUTTON_EVT_RELEASED);

	zbus_sub_wait_msg(&msub_button_evt, &chan, &msg, K_SECONDS(1));

	zassert_true(msg.evt == BUTTON_EVT_LONGPRESS);
}
/** Bind the test suite to the ztest framework for automated testing */
ZTEST_SUITE(button, NULL, button_test_setup, NULL, NULL, NULL);

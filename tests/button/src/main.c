/**
 * @file test_button.c
 * @brief Unit tests for button handling using Zephyr's ZTest and ZBus.
 *
 * Tests include single press and long press detection using GPIO emulation.
 */

/*
 * Copyright (c) 2025 Rodrigo Peixoto <rodrigopex@ic.ufal.br>
 * SPDX-License-Identifier: Apache-2.0
 */

 #include <zephyr/kernel.h>
 #include <zephyr/ztest.h>
 #include <zephyr/zbus/zbus.h>
 #include <zephyr/drivers/gpio/gpio_emul.h>
 
 #include "button.h"
 
 /// Subscriber for button events
 ZBUS_MSG_SUBSCRIBER_DEFINE(msub_button_evt);
 
 /// Register subscriber to the button event channel
 ZBUS_CHAN_ADD_OBS(chan_button_evt, msub_button_evt, 3);
 
 /**
  * @brief Test fixture for button tests.
  *
  * Holds GPIO device specification for the button.
  */
 static struct button_fixture {
	 const struct gpio_dt_spec button_gpio;
 } fixture = {
	 .button_gpio = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios),
 };
 
 /// Simulates button press
 #define BUTTON_PRESS(_fixture)                                                                     \
	 do {                                                                                       \
		 gpio_emul_input_set(_fixture->button_gpio.port, _fixture->button_gpio.pin, 0);     \
		 k_msleep(80);                                                                      \
	 } while (0)
 
 /// Simulates button release
 #define BUTTON_RELEASE(_fixture)                                                                   \
	 do {                                                                                       \
		 gpio_emul_input_set(_fixture->button_gpio.port, _fixture->button_gpio.pin, 1);     \
		 k_msleep(80);                                                                      \
	 } while (0)
 
 /**
  * @brief Setup function for button test suite.
  *
  * Initializes the button and sets up GPIO emulator.
  *
  * @return Pointer to the initialized fixture.
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
  * @brief Test that a single button press and release triggers correct events.
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
  * @brief Test that a long button press triggers a long press event.
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
 
 /// Defines the test suite for button functionality
 ZTEST_SUITE(button, NULL, button_test_setup, NULL, NULL, NULL);
 
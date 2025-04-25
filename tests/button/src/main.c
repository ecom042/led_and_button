/*
 * Copyright (c) 2025 Rodrigo Peixoto <rodrigopex@ic.ufal.br>
 * SPDX-License-Identifier: Apache-2.0
 */

 #include <zephyr/kernel.h>
 #include <zephyr/ztest.h>
 #include <zephyr/zbus/zbus.h>
 #include <zephyr/drivers/gpio/gpio_emul.h>
 
 #include "button.h"
 
 /* 
  * Define um subscriber de mensagens Zbus para receber eventos do botão.
  */
 ZBUS_MSG_SUBSCRIBER_DEFINE(msub_button_evt);
 
 /* 
  * Adiciona o subscriber como observador do canal de eventos do botão.
  */
 ZBUS_CHAN_ADD_OBS(chan_button_evt, msub_button_evt, 3);
 
 /*
  * Estrutura de teste com acesso ao GPIO do botão emulado.
  */
 static struct button_fixture {
	 const struct gpio_dt_spec button_gpio;
 } fixture = {
	 .button_gpio = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios),
 };
 
 /*
  * Simula o pressionamento do botão no hardware emulado.
  * Define o pino GPIO como 0 (pressionado) e espera 80ms.
  */
 #define BUTTON_PRESS(_fixture)                                                                     \
	 do {                                                                                       \
		 gpio_emul_input_set(_fixture->button_gpio.port, _fixture->button_gpio.pin, 0);     \
		 k_msleep(80);                                                                      \
	 } while (0)
 
 /*
  * Simula a liberação do botão no hardware emulado.
  * Define o pino GPIO como 1 (liberado) e espera 80ms.
  */
 #define BUTTON_RELEASE(_fixture)                                                                   \
	 do {                                                                                       \
		 gpio_emul_input_set(_fixture->button_gpio.port, _fixture->button_gpio.pin, 1);     \
		 k_msleep(80);                                                                      \
	 } while (0)
 
 /*
  * Função de setup executada antes dos testes.
  * Inicializa o botão, configura o estado inicial como "não pressionado"
  * e ativa as interrupções do botão.
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
  * Teste 01 — Verifica se um pressionamento curto gera os eventos esperados:
  * 1. BUTTON_EVT_PRESSED ao pressionar.
  * 2. BUTTON_EVT_RELEASED ao soltar.
  * Esse teste garante o funcionamento básico do sistema de eventos de botão.
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
 
 /*
  * Teste 02 — Verifica se um pressionamento longo (> 3 segundos) gera os eventos esperados:
  * 1. BUTTON_EVT_PRESSED ao pressionar.
  * 2. BUTTON_EVT_RELEASED ao soltar.
  * 3. BUTTON_EVT_LONGPRESS imediatamente após o release.
  * Esse teste valida o comportamento esperado para long press.
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
 
 /*
  * Define a suíte de testes do botão.
  * Usa o setup "button_test_setup" e nenhuma função extra de before/after.
  */
 ZTEST_SUITE(button, NULL, button_test_setup, NULL, NULL, NULL);
 

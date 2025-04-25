#include "button.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>

ZBUS_CHAN_DEFINE(chan_button_evt, struct msg_button_evt, NULL, NULL,
		 ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(.evt = BUTTON_EVT_UNDEFINED));

#define SLEEP_TIME_MS 1

/* Referência ao nó do botão no device tree */
#define SW0_NODE DT_ALIAS(sw0)

#if !DT_NODE_HAS_STATUS_OKAY(SW0_NODE)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif

/* Especificação do botão e callback */
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static struct gpio_callback button_cb_data;

/* Tempo em que o botão foi pressionado */
static int64_t button_press_time = 0;

/**
 * @brief Handler de interrupção para eventos de botão.
 *
 * Detecta quando o botão é pressionado ou solto e publica os
 * eventos correspondentes no canal zbus. Se o tempo pressionado
 * for maior que 3 segundos, emite evento de long press.
 */
void button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	struct msg_button_evt msg = {.evt = BUTTON_EVT_UNDEFINED};

	if (gpio_pin_get_dt(&button)) {
		/* Botão pressionado */
		msg.evt = BUTTON_EVT_PRESSED;
		zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
		button_press_time = k_uptime_get();
	} else {
		/* Botão solto */
		msg.evt = BUTTON_EVT_RELEASED;
		zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);

		int64_t duration = k_uptime_get() - button_press_time;

		if (duration >= 3000) {
			msg.evt = BUTTON_EVT_LONGPRESS;
			zbus_chan_pub(&chan_button_evt, &msg, K_NO_WAIT);
		}
	}
}

/**
 * @brief Inicializa o botão.
 *
 * Configura o pino definido no device tree como entrada.
 */
int button_init(void)
{
	int ret;

	if (!gpio_is_ready_dt(&button)) {
		printk("Error: button device %s is not ready\n", button.port->name);
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n",
		       ret, button.port->name, button.pin);
		return ret;
	}

	return 0;
}

/**
 * @brief Habilita interrupções no botão.
 *
 * Configura interrupções de borda para detectar pressionamentos e liberações.
 */
int button_enable_interrupts(void)
{
	int ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n",
		       ret, button.port->name, button.pin);
		return ret;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);

	return 0;
}

/*
 * Copyright (c) 2019 Foundries.io
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_MODULE_NAME fota_timer
#define LOG_LEVEL CONFIG_FOTA_LOG_LEVEL

#include <logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#include <zephyr.h>
#include <gpio.h>
#include <net/lwm2m.h>

static void set_timer_gpio(bool state)
{
	struct device *gpio;

	gpio = device_get_binding(DT_SW_TIMER0_GPIO_CONTROLLER);
	if (!gpio) {
		LOG_ERR("GPIO device not found; you must configure the GPIO\n"
			"for the timer to activate in DTS.  See dt-sw-timer0\n"
			"in boards/nrf52_blenano2.overlay");
		return;
	}

	LOG_DBG("STATE:%d", state);
	gpio_pin_configure(gpio, DT_SW_TIMER0_GPIO_PIN,
			   DT_SW_TIMER0_GPIO_FLAGS);
	gpio_pin_write(gpio, DT_SW_TIMER0_GPIO_PIN, state);
}

static int timer_digital_state_post_write_cb(u16_t obj_inst_id,
					     u8_t *data, u16_t data_len,
					     bool last_block, size_t total_size)
{
	bool *digital_state = (bool *)data;

	/* adjust GPIO based on check the state */
	if (*digital_state) {
		set_timer_gpio(true);
	} else {
		set_timer_gpio(false);
	}

	return 0;
}

int init_timer_control(void)
{
	float64_value_t delay_duration;
	int ret;

	/* Only one instance (ID 0) is supported. */
	ret = lwm2m_engine_create_obj_inst("3340/0");
	if (ret < 0) {
		goto fail;
	}

	/* register for timer output state changes */
	ret = lwm2m_engine_register_post_write_callback("3340/0/5543",
			timer_digital_state_post_write_cb);
	if (ret < 0) {
		goto fail;
	}

	/* set initial delay duration: .5 second */
	delay_duration.val1 = 0LL;
	delay_duration.val2 = 500000000LL;
	ret = lwm2m_engine_set_float64("3340/0/5521", &delay_duration);
	if (ret < 0) {
		goto fail;
	}

	/* set application type */
	ret = lwm2m_engine_set_string("3340/0/5750", DT_SW_TIMER0_LABEL);
	if (ret < 0) {
		goto fail;
	}

	return 0;

fail:
	return ret;
}

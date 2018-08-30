/*
 * Copyright (c) 2018 Open Source Foundries
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Light control routines for PWMing individual color channels.
 */

#include <string.h>

#define SYS_LOG_DOMAIN "fota/light"
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_FOTA_LEVEL
#include <logging/sys_log.h>

#include <device.h>
#include <pwm.h>
#include <init.h>

#include "light_control_priv.h"

/* Color Unit used by the IPSO object */
#define COLOR_UNIT	"hex"
#define COLOR_WHITE	"#FFFFFF"

/* Initial dimmer value (%) */
#define DIMMER_INITIAL	50

/* 100 is more than enough for it to be flicker free */
#define PWM_PERIOD (USEC_PER_SEC / 100)

/* Private data type for struct ipso_light_ctl. */
struct pwm_data {
	u8_t color_rgb[3];
/* Support for up to 4 PWM devices */
#if defined(CONFIG_APP_PWM_RED)
	struct device *red;
#endif
#if defined(CONFIG_APP_PWM_GREEN)
	struct device *green;
#endif
#if defined(CONFIG_APP_PWM_BLUE)
	struct device *blue;
#endif
#if defined(CONFIG_APP_PWM_WHITE)
	struct device *white;
	u8_t white_current;
#endif
};

static u32_t scale_pulse(u8_t level, u8_t ceiling)
{
	if (level && ceiling) {
		/* Scale level based on ceiling and return period */
		return PWM_PERIOD / 255 * level * ceiling / 255;
	}

	return 0;
}

static int write_pwm_pin(struct device *pwm_dev, u32_t pwm_pin,
			 u8_t level, u8_t ceiling)
{
	u32_t pulse = scale_pulse(level, ceiling);

	SYS_LOG_DBG("Set PWM %d: level %d, ceiling %d, pulse %lu",
		    pwm_pin, level, ceiling, pulse);

	return pwm_pin_set_usec(pwm_dev, pwm_pin, PWM_PERIOD, pulse);
}

static int light_control_pwm_set_color(struct ipso_light_ctl *ilc, u8_t rgb[3])
{
	struct pwm_data *data = ilc->data;
	int ret = 0;

#if defined(CONFIG_APP_PWM_WHITE)
	u8_t white = 0;

	/* If a dedicated PWM is used for white, zero RGB */
	if (rgb[0] == rgb[1] && rgb[1] == rgb[2]) {
		white = rgb[0];
		rgb[0] = rgb[1] = rgb[2] = 0;
	}

	/*
	 * If switching from white->color we first need to disable white to
	 * avoid consuming 4 PWM pins (required for nRF5 devices).
	 */
	if (!white && data->white_current) {
		data->white_current = 0;
		ret = write_pwm_pin(data->white, CONFIG_APP_PWM_WHITE_PIN,
				    0, 0);
		if (ret) {
			SYS_LOG_ERR("Failed to update white PWM");
			return ret;
		}
	}
#endif

#if defined(CONFIG_APP_PWM_RED)
	ret = write_pwm_pin(data->red, CONFIG_APP_PWM_RED_PIN,
			    rgb[0], CONFIG_APP_PWM_RED_PIN_CEILING);
	if (ret) {
		SYS_LOG_ERR("Failed to update red PWM");
		return ret;
	}
#endif

#if defined(CONFIG_APP_PWM_GREEN)
	ret = write_pwm_pin(data->green, CONFIG_APP_PWM_GREEN_PIN,
			    rgb[1], CONFIG_APP_PWM_GREEN_PIN_CEILING);
	if (ret) {
		SYS_LOG_ERR("Failed to update green PWM");
		return ret;
	}
#endif

#if defined(CONFIG_APP_PWM_BLUE)
	ret = write_pwm_pin(data->blue, CONFIG_APP_PWM_BLUE_PIN,
			    rgb[2], CONFIG_APP_PWM_BLUE_PIN_CEILING);
	if (ret) {
		SYS_LOG_ERR("Failed to update blue PWM");
		return ret;
	}
#endif

#if defined(CONFIG_APP_PWM_WHITE)
	if (white != data->white_current) {
		data->white_current = white;
		ret = write_pwm_pin(data->white, CONFIG_APP_PWM_WHITE_PIN,
				    white, CONFIG_APP_PWM_WHITE_PIN_CEILING);
		if (ret) {
			SYS_LOG_ERR("Failed to update white PWM");
			return ret;
		}
	}
#endif

	return ret;
}

static int light_control_pwm_update(struct ipso_light_ctl *ilc, u8_t dimmer)
{
	struct pwm_data *data = ilc->data;
	u8_t rgb[3];
	int ret, i;

	memcpy(&rgb, data->color_rgb, 3);

	if (dimmer > 100) {
		dimmer = 100;
	}

	/*
	 * Update individual color values based on dimmer.
	 *
	 * This is just a direct map between dimmer and PWM, but not the best
	 * way to control the light brightness, as the human eye perceives
	 * it differently, but good enough for now as it is a simple method.
	 */
	for (i = 0; i < 3; i++) {
		rgb[i] = rgb[i] * dimmer / 100;
	}

	ret = light_control_pwm_set_color(ilc, rgb);
	if (ret) {
		SYS_LOG_ERR("Failed to update color");
		return ret;
	}

	return ret;
}

int light_control_pwm_pre_init(struct ipso_light_ctl *ilc)
{
	struct pwm_data *data = ilc->data;

#if defined(CONFIG_APP_PWM_WHITE)
	data->white = device_get_binding(CONFIG_APP_PWM_WHITE_DEV);
	if (!data->white) {
		SYS_LOG_ERR("Failed to get PWM device used for white");
		return -ENODEV;
	}
#endif
#if defined(CONFIG_APP_PWM_RED)
	data->red = device_get_binding(CONFIG_APP_PWM_RED_DEV);
	if (!data->red) {
		SYS_LOG_ERR("Failed to get PWM device used for red");
		return -ENODEV;
	}
#endif
#if defined(CONFIG_APP_PWM_GREEN)
	data->green = device_get_binding(CONFIG_APP_PWM_GREEN_DEV);
	if (!data->green) {
		SYS_LOG_ERR("Failed to get PWM device used for green");
		return -ENODEV;
	}
#endif
#if defined(CONFIG_APP_PWM_BLUE)
	data->blue = device_get_binding(CONFIG_APP_PWM_BLUE_DEV);
	if (!data->blue) {
		SYS_LOG_ERR("Failed to get PWM device used for blue");
		return -ENODEV;
	}
#endif

	/* Initial color: white */
	memset(data->color_rgb, 0xFF, sizeof(data->color_rgb));

	return 0;
}

int light_control_pwm_post_init(struct ipso_light_ctl *ilc)
{
	int ret;

	ret = ilc_set_dimmer(ilc, DIMMER_INITIAL);
	if (ret < 0) {
		return ret;
	}

	ret = ilc_set_sensor_units(ilc, COLOR_UNIT);
	if (ret < 0) {
		return ret;
	}

	ret = ilc_set_color(ilc, COLOR_WHITE);
	if (ret < 0) {
		return ret;
	}

	/* Set initial light state based on DIMMER_INITIAL */
	if (DIMMER_INITIAL) {
		ilc_set_onoff(ilc, true);
	}

	return 0;
}

static int light_control_pwm_on_off_cb(struct ipso_light_ctl *ilc, bool on)
{
	int ret = 0;
	u8_t dimmer = 0;

	if (on) {
		ret = ilc_get_dimmer(ilc, &dimmer);
		if (ret) {
			goto fail;
		}
	}

	ret = light_control_pwm_update(ilc, dimmer);
	if (ret) {
		goto fail;
	}

	return 0;

fail:
	SYS_LOG_ERR("Failed to update light state");
	return ret;
}

static int light_control_pwm_dimmer_cb(struct ipso_light_ctl *ilc, u8_t dimmer)
{
	int ret = 0;
	bool on;

	/* Update PWM output if light is 'on' */
	ret = ilc_get_onoff(ilc, &on);
	if (ret) {
		SYS_LOG_ERR("Failed to get onoff");
		return ret;
	}

	if (!on) {
		return 0;
	}

	ret = light_control_pwm_update(ilc, dimmer);
	if (ret) {
		SYS_LOG_ERR("Failed to update dimmer");
		return ret;
	}

	return 0;
}

static int light_control_pwm_color_cb(struct ipso_light_ctl *ilc,
				      char *color, u16_t color_len)
{
	struct pwm_data *data = ilc->data;
	u8_t color_rgb[3];
	u8_t dimmer;
	bool on;
	int ret;

	ret = light_control_parse_rgb(color, color_len, color_rgb);
	if (ret) {
		return ret;
	}

	memcpy(data->color_rgb, color_rgb, sizeof(data->color_rgb));
	SYS_LOG_DBG("RGB color updated to #%02x%02x%02x", color_rgb[0],
					color_rgb[1], color_rgb[2]);

	/* Update PWM output if light is 'on' */
	ret = ilc_get_onoff(ilc, &on);
	if (ret) {
		SYS_LOG_ERR("Failed to get onoff: %d", ret);
		return ret;
	}

	if (!on) {
		return 0;
	}

	ret = ilc_get_dimmer(ilc, &dimmer);
	if (ret) {
		SYS_LOG_ERR("Failed to get dimmer");
		return ret;
	}

	ret = light_control_pwm_update(ilc, dimmer);
	if (ret) {
		SYS_LOG_ERR("Failed to update color");
		return ret;
	}

	return ret;
}

static struct pwm_data data;

static struct ipso_light_ctl ilc_pwm = {
	.pre_init = light_control_pwm_pre_init,
	.post_init = light_control_pwm_post_init,
	.on_off_cb = light_control_pwm_on_off_cb,
	.dimmer_cb = light_control_pwm_dimmer_cb,
	.color_cb = light_control_pwm_color_cb,
	.data = &data,
};

static int light_control_register_pwm(struct device *dev)
{
	int ret = light_control_register(&ilc_pwm);
	if (ret) {
		SYS_LOG_ERR("failed: %d", ret);
	} else {
		SYS_LOG_INF("success");
	}
	return ret;
}

SYS_INIT(light_control_register_pwm, APPLICATION,
	 CONFIG_APPLICATION_INIT_PRIORITY);

/*
 * Copyright (c) 2017-2018 Open Source Foundries Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#define SYS_LOG_DOMAIN "fota/light"
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_FOTA_LEVEL
#include <logging/sys_log.h>
#include <device.h>
#include <led_strip.h>
#include <init.h>

#include "light_control_priv.h"

#define WS2812_NUM_LEDS	CONFIG_WS2812_STRIP_MAX_PIXELS
#define WS2812_DEV_NAME	CONFIG_WS2812_STRIP_NAME
#define SPI_DEV_NAME		CONFIG_WS2812_STRIP_SPI_DEV_NAME

/* Color Unit used by the IPSO object */
#define COLOR_UNIT		"hex"
#define COLOR_WHITE		"#FFFFFF"

/* Initial dimmer value (%) */
#define DIMMER_INITIAL		25

/* Is the light initially on? */
#define ON_INITIAL              true

struct ws2812_data {
	struct device *ws2812;
	struct led_rgb ws2812_buf[WS2812_NUM_LEDS];
	struct led_rgb color;
};

static int light_control_ws2812_update(struct ipso_light_ctl *ilc, u8_t dimmer)
{
	struct ws2812_data *data = ilc->data;
	struct led_rgb *buf = data->ws2812_buf;
	struct led_rgb *color = &data->color;
	u8_t r, g, b;
	size_t i;
	int ret;

	r = color->r * dimmer / 100;
	g = color->g * dimmer / 100;
	b = color->b * dimmer / 100;

	for (i = 0; i < WS2812_NUM_LEDS; i++) {
		buf[i].r = r;
		buf[i].g = g;
		buf[i].b = b;
	}

	ret = led_strip_update_rgb(data->ws2812, buf, WS2812_NUM_LEDS);

	return ret;
}

static int light_control_ws2812_pre_init(struct ipso_light_ctl *ilc)
{
	struct ws2812_data *data = ilc->data;
	struct device *spi;

	/* Sanity-check the SPI configuration. */
	spi = device_get_binding(SPI_DEV_NAME);
	if (spi) {
		SYS_LOG_INF("Found SPI device %s", SPI_DEV_NAME);
	} else {
		SYS_LOG_ERR("SPI device not found; you must choose a SPI "
			    "device and configure its name to %s",
			    SPI_DEV_NAME);
		return -ENODEV;
	}

	/* Cache the actual LED strip device. */
	data->ws2812 = device_get_binding(WS2812_DEV_NAME);
	if (data->ws2812) {
		SYS_LOG_INF("Found LED strip device %s", WS2812_DEV_NAME);
	} else {
		SYS_LOG_ERR("LED strip device %s not found", WS2812_DEV_NAME);
		return -ENODEV;
	}

	memset(data->ws2812_buf, 0xff, sizeof(data->ws2812_buf));
	memset(&data->color, 0xff, sizeof(data->color));

	return 0;
}

static int light_control_ws2812_post_init(struct ipso_light_ctl *ilc)
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

	ret = ilc_set_onoff(ilc, ON_INITIAL);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

static int light_control_ws2812_on_off_cb(struct ipso_light_ctl *ilc, bool on)
{
	int ret = 0;
	u8_t dimmer = 0;

	if (on) {
		ret = ilc_get_dimmer(ilc, &dimmer);
		if (ret) {
			goto fail;
		}
	}

	ret = light_control_ws2812_update(ilc, dimmer);
	if (ret) {
		goto fail;
	}

	return 0;

fail:
	SYS_LOG_ERR("Failed to update light state");
	return ret;
}

static int light_control_ws2812_dimmer_cb(struct ipso_light_ctl *ilc,
					  u8_t dimmer)
{
	int ret = 0;
	bool on;

	/* Update output if light is 'on' */
	ret = ilc_get_onoff(ilc, &on);
	if (ret) {
		SYS_LOG_ERR("Failed to get onoff");
		return ret;
	}

	if (!on) {
		return 0;
	}

	ret = light_control_ws2812_update(ilc, dimmer);
	if (ret) {
		SYS_LOG_ERR("Failed to update dimmer");
		return ret;
	}

	return 0;
}

static int light_control_ws2812_color_cb(struct ipso_light_ctl *ilc,
					 char *color, u16_t color_len)
{
	struct ws2812_data *data = ilc->data;
	u8_t color_rgb[3];
	u8_t dimmer;
	bool on;
	int ret;

	ret = light_control_parse_rgb(color, color_len, color_rgb);
	if (ret) {
		return ret;
	}

	data->color.r = color_rgb[0];
	data->color.g = color_rgb[1];
	data->color.b = color_rgb[2];
	SYS_LOG_DBG("RGB color updated to #%02x%02x%02x", color_rgb[0],
					color_rgb[1], color_rgb[2]);

	/* Update output if light is 'on' */
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

	ret = light_control_ws2812_update(ilc, dimmer);
	if (ret) {
		SYS_LOG_ERR("Failed to update color");
		return ret;
	}

	return ret;
}

static int light_control_ws2812_flash(struct ipso_light_ctl *ilc,
				      u8_t r, u8_t g, u8_t b, s32_t duration)
{
	struct ws2812_data *data = ilc->data;
	struct led_rgb cache;
	u8_t dimmer;
	int ret;

	ret = ilc_get_dimmer(ilc, &dimmer);
	if (ret) {
		return ret;
	}

	memcpy(&cache, &data->color, sizeof(struct led_rgb));
	data->color.r = r;
	data->color.g = g;
	data->color.b = b;
	ret = light_control_ws2812_update(ilc, dimmer);
	if (ret) {
		goto fail;
	}
	k_sleep(duration);
	memcpy(&data->color, &cache, sizeof(struct led_rgb));
	ret = light_control_ws2812_update(ilc, dimmer);
	return ret;

fail:
	memcpy(&data->color, &cache, sizeof(struct led_rgb));
	(void)light_control_ws2812_update(ilc, dimmer);
	return ret;
}

static struct ws2812_data data;

static struct ipso_light_ctl ilc_ws2812 = {
	.pre_init = light_control_ws2812_pre_init,
	.post_init = light_control_ws2812_post_init,
	.on_off_cb = light_control_ws2812_on_off_cb,
	.dimmer_cb = light_control_ws2812_dimmer_cb,
	.color_cb = light_control_ws2812_color_cb,
	.flash = light_control_ws2812_flash,
	.data = &data,
};

static int light_control_register_ws2812(struct device *dev)
{
	int ret = light_control_register(&ilc_ws2812);
	if (ret) {
		SYS_LOG_ERR("failed: %d", ret);
	} else {
		SYS_LOG_INF("success");
	}
	return ret;
}

SYS_INIT(light_control_register_ws2812, APPLICATION,
	 CONFIG_KERNEL_INIT_PRIORITY_DEVICE);

/*
 * Copyright (c) 2016-2017 Linaro Limited
 * Copyright (c) 2017-2018 Open Source Foundries Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <net/lwm2m.h>

#include "light_control_priv.h"

/*
 * Singleton light controller in use.
 *
 * TODO: support multiple instances.
 */
static struct ipso_light_ctl *ilc;

static u8_t char_to_nibble(char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	}

	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10U;
	}

	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10U;
	}

	SYS_LOG_ERR("Invalid ascii hex value (%c), assuming 0xF!", c);
	return 15U;
}

int light_control_parse_rgb(char *color, u16_t color_len, u8_t rgb[3])
{
	size_t i;

	/* Check if just HEX and #HEX */
	if (color_len < 6 || color_len > 7) {
		SYS_LOG_ERR("Invalid color length (%s)", color);
		return -EINVAL;
	}

	if (color_len == 7 && *color != '#') {
		SYS_LOG_ERR("Invalid color format (%s)", color);
		return -EINVAL;
	}

	/* Skip '#' if available */
	if (color_len == 7) {
		color += 1;
	}

	for (i = 0; i < 3; i++) {
		rgb[i] = (char_to_nibble(*color) << 4) |
				char_to_nibble(*(color + 1));
		color += 2;
	}

	return 0;
}

int light_control_register(struct ipso_light_ctl *light_control)
{
	if (ilc) {
		return -ENOMEM;
	} else {
		ilc = light_control;
		return 0;
	}
}

/* TODO: Move to a pre write hook that can handle ret codes once available */
static int on_off_cb(u16_t obj_inst_id, u8_t *data, u16_t data_len,
		     bool last_block, size_t total_size)
{
	bool on;
	int ret = 0;

	if (data_len != 1) {
		SYS_LOG_ERR("Length of on_off callback data incorrect! (%u)",
			    data_len);
		return -EINVAL;
	}

	on = *data;

	ret = ilc->on_off_cb(ilc, on);
	if (ret) {
		return ret;
	}

	if (!on) {
		ret = ilc_set_on_time(ilc, 0);
	}

	return ret;
}

/* TODO: Move to a pre write hook that can handle ret codes once available */
static int dimmer_cb(u16_t obj_inst_id, u8_t *data, u16_t data_len,
		     bool last_block, size_t total_size)
{
	u8_t dimmer = *data;

	if (dimmer > 100) {
		SYS_LOG_ERR("Invalid dimmer value %u, forcing it to 100",
			    dimmer);
		dimmer = 100;
	}

	return ilc->dimmer_cb(ilc, dimmer);
}

/* TODO: Move to a pre write hook that can handle ret codes once available */
static int color_cb(u16_t obj_inst_id, u8_t *data, u16_t data_len,
		    bool last_block, size_t total_size)
{
	return ilc->color_cb(ilc, (char *)data, data_len);
}

int init_light_control(void)
{
	int ret;

	if (!ilc) {
		ret = -ENODEV;
		goto fail;
	}

	/* Only one instance (ID 0) is supported. */
	ret = lwm2m_engine_create_obj_inst("3311/0");
	if (ret < 0) {
		goto fail;
	}

	ilc->inst_id = 0;
	ret = ilc->pre_init(ilc);
	if (ret < 0) {
		goto fail;
	}

	ret = lwm2m_engine_register_post_write_callback("3311/0/5850",
							on_off_cb);
	if (ret < 0) {
		goto fail;
	}

	ret = lwm2m_engine_register_post_write_callback("3311/0/5851",
							dimmer_cb);
	if (ret < 0) {
		goto fail;
	}

	ret = lwm2m_engine_register_post_write_callback("3311/0/5706",
							color_cb);
	if (ret < 0) {
		goto fail;
	}

	ret = ilc->post_init(ilc);
	if (ret < 0) {
		goto fail;
	}

	return 0;

fail:
	return ret;
}

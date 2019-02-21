/*
 * Copyright (c) 2016-2017 Linaro Limited
 * Copyright (c) 2017-2018 Foundries.io
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_MODULE_NAME fota_light
#define LOG_LEVEL CONFIG_FOTA_LOG_LEVEL

#include <logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#include <zephyr.h>
#include <net/lwm2m.h>

#include "light_control_priv.h"

/*
 * Singleton light controller in use.
 *
 * TODO: support multiple instances.
 */
static struct ipso_light_ctl *ilc;
static K_SEM_DEFINE(ilc_sem, 1, 1);

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

	LOG_ERR("Invalid ascii hex value (%c), assuming 0xF!", c);
	return 15U;
}

int light_control_parse_rgb(char *color, u16_t color_len, u8_t rgb[3])
{
	size_t i;

	/* Check if just HEX and #HEX */
	if (color_len < 6 || color_len > 7) {
		LOG_ERR("Invalid color length (%s)", color);
		return -EINVAL;
	}

	if (color_len == 7 && *color != '#') {
		LOG_ERR("Invalid color format (%s)", color);
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

	k_sem_take(&ilc_sem, K_FOREVER);

	if (data_len != 1) {
		LOG_ERR("Length of on_off callback data is incorrect! (%u)",
			data_len);
		goto out;
	}

	on = *data;

	ret = ilc->on_off_cb(ilc, on);
	if (ret) {
		goto out;
	}

	if (!on) {
		ret = ilc_set_on_time(ilc, 0);
	}

out:
	k_sem_give(&ilc_sem);
	return ret;
}

/* TODO: Move to a pre write hook that can handle ret codes once available */
static int dimmer_cb(u16_t obj_inst_id, u8_t *data, u16_t data_len,
		     bool last_block, size_t total_size)
{
	u8_t dimmer = *data;
	int ret;

	k_sem_take(&ilc_sem, K_FOREVER);

	if (dimmer > 100) {
		LOG_ERR("Invalid dimmer value %u, forcing it to 100",
			dimmer);
		dimmer = 100;
	}

	ret = ilc->dimmer_cb(ilc, dimmer);

	k_sem_give(&ilc_sem);
	return ret;
}

/* TODO: Move to a pre write hook that can handle ret codes once available */
static int color_cb(u16_t obj_inst_id, u8_t *data, u16_t data_len,
		    bool last_block, size_t total_size)
{
	int ret;

	k_sem_take(&ilc_sem, K_FOREVER);
	ret = ilc->color_cb(ilc, (char *)data, data_len);
	k_sem_give(&ilc_sem);

	return ret;
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

int light_control_flash(u8_t r, u8_t g, u8_t b, s32_t duration)
{
	int ret;

	k_sem_take(&ilc_sem, K_FOREVER);
	if (!ilc) {
		LOG_ERR("no light registered but flash called");
		ret = -ENODEV;
	} else if (!ilc->flash) {
		LOG_WRN("light control object doesn't support flashing");
		ret = -EINVAL;
	} else {
		ret = ilc->flash(ilc, r, g, b, duration);
	}
	k_sem_give(&ilc_sem);
	return ret;
}

#if defined(CONFIG_LWM2M_PERSIST_SETTINGS)
void light_control_persist(void)
{
	/* save on/off state */
	lwm2m_engine_set_persist("3311/0/5850");
	/* save dimmer state */
	lwm2m_engine_set_persist("3311/0/5851");
	/* saver color */
	lwm2m_engine_set_persist("3311/0/5706");
}
#endif

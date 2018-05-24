/*
 * Copyright (c) 2018 Open Source Foundries
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __FOTA_LIGHT_CONTROL_PRIV_H__
#define __FOTA_LIGHT_CONTROL_PRIV_H__

#include <zephyr/types.h>
#include <net/lwm2m.h>

#define IPSO_LIGHT_CTL_ONOFF     "5850"
#define IPSO_LIGHT_CTL_DIMMER    "5851"
#define IPSO_LIGHT_CTL_ON_TIME   "5852"
#define IPSO_LIGHT_CTL_CUM_PWR   "5805"
#define IPSO_LIGHT_CTL_PWR_FAC   "5820"
#define IPSO_LIGHT_CTL_COLOR     "5706"
#define IPSO_LIGHT_CTL_SNS_UNIT  "5701"

/**
 * Backend abstraction for an LWM2M-based IPSO light control object.
 *
 * Note: we can't easily migrate this into Zephyr's led.h API, as
 * that has no notion of color, and the IPSO object's color callback
 * takes opaque data in user-defined color spaces.
 */
struct ipso_light_ctl {
	/**
	 * Pre-initialize light controller
	 *
	 * This is invoked after the LWM2M object instance has been
	 * allocated, but before any of the following callbacks have
	 * been registered. It is an appropriate place to initialize
	 * internal resources, but not to call into the LWM2M engine.
	 *
	 * @param obj_inst_id LwM2M object instance ID
	 */
	int (*pre_init)(struct ipso_light_ctl *);

	/**
	 * Post-initialization setup routine.
	 *
	 * At this point, all of the following callbacks have been
	 * registered with the LwM2M engine.
	 */
	int (*post_init)(struct ipso_light_ctl *);

	/**
	 * This is invoked as part of the LWM2M callback installed for
	 * when the ON/OFF resource is written.
	 */
	int (*on_off_cb)(struct ipso_light_ctl *, bool on);
	/** This is invoked when dimmer resource is written. */
	int (*dimmer_cb)(struct ipso_light_ctl *, u8_t dimmer);
	/** This is invoked when color resource is written. */
	int (*color_cb)(struct ipso_light_ctl *, char *color, u16_t color_len);

	/** System hook for flashing an RGB color, if supported. */
	int (*flash)(struct ipso_light_ctl *, u8_t r, u8_t g, u8_t b,
		     s32_t duration);

	/**
	 * Buffer for resource paths.
	 *
	 * "3311" + "/" + <obj_inst_id> + "/" + <resource_id> + NUL
	 */
	char path_buf[4 + 1 + 4 + 1 + 4 + 1];

	/** Cached instance ID */
	u16_t inst_id;

	/** Private instance data. */
	void *data;
};

/*
 * Helpers for getting/setting commonly used IPSO light control object
 * resources.
 */

static inline char* _ilc_rsrc(struct ipso_light_ctl *ilc, const char *resource)
{
	snprintk(ilc->path_buf, sizeof(ilc->path_buf), "3311/%u/%s",
		 ilc->inst_id, resource);
	return ilc->path_buf;
}

static inline int ilc_get_onoff(struct ipso_light_ctl *ilc, bool *on)
{
	char *path = _ilc_rsrc(ilc, IPSO_LIGHT_CTL_ONOFF);
	return lwm2m_engine_get_bool(path, on);
}

static inline int ilc_set_onoff(struct ipso_light_ctl *ilc, bool on)
{
	char *path = _ilc_rsrc(ilc, IPSO_LIGHT_CTL_ONOFF);
	return lwm2m_engine_set_bool(path, on);
}

static inline int ilc_get_dimmer(struct ipso_light_ctl *ilc, u8_t *dimmer)
{
	char *path = _ilc_rsrc(ilc, IPSO_LIGHT_CTL_DIMMER);
	return lwm2m_engine_get_u8(path, dimmer);
}

static inline int ilc_set_dimmer(struct ipso_light_ctl *ilc, u8_t dimmer)
{
	char *path = _ilc_rsrc(ilc, IPSO_LIGHT_CTL_DIMMER);
	return lwm2m_engine_set_u8(path, dimmer);
}

static inline int ilc_set_on_time(struct ipso_light_ctl *ilc, s32_t on_time)
{
	char *path = _ilc_rsrc(ilc, IPSO_LIGHT_CTL_ON_TIME);
	return lwm2m_engine_set_s32(path, on_time);
}

static inline int ilc_set_sensor_units(struct ipso_light_ctl *ilc, char *units)
{
	char *path = _ilc_rsrc(ilc, IPSO_LIGHT_CTL_SNS_UNIT);
	return lwm2m_engine_set_string(path, units);
}

static inline int ilc_set_color(struct ipso_light_ctl *ilc, char *color)
{
	char *path = _ilc_rsrc(ilc, IPSO_LIGHT_CTL_COLOR);
	return lwm2m_engine_set_string(path, color);
}

/**
 * Install a backend object. This *must* be invoked by exactly one
 * backend during system initialization at a pre-application priority.
 */
int light_control_register(struct ipso_light_ctl *light_control);

/**
 * Helper for parsing an RGB triplet out of an ASCII color value.
 */
int light_control_parse_rgb(char *color, u16_t color_len, u8_t rgb[3]);

#endif	/* __FOTA_LIGHT_CONTROL_H__ */

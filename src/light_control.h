/*
 * Copyright (c) 2016-2017 Linaro Limited
 * Copyright (c) 2017-2018 Open Source Foundries
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FOTA_LIGHT_CONTROL_H__
#define FOTA_LIGHT_CONTROL_H__

int init_light_control(void);
void light_control_persist(void);
int light_control_flash(u8_t r, u8_t g, u8_t b, s32_t duration);

#endif	/* FOTA_LIGHT_CONTROL_H__ */

# Copyright (c) 2017 Linaro Limited
# Copyright (c) 2018 Open Source Foundries Limited
#
# SPDX-License-Identifier: Apache-2.0
#

# Selector for what type of light this is.
choice APP_LIGHT_TYPE
	prompt "Type of lighting technology which is in use"
	default APP_LIGHT_TYPE_PWM

config APP_LIGHT_TYPE_PWM
	bool "LEDs with PWM brightness"
	help
	  Select this option to use this application with up to four
	  LEDs (supporting red, green, blue, and white color channels, but
	  not requiring any of them).

config APP_LIGHT_TYPE_WS2812
	bool "LEDs from WS2812 strip"
	select SPI
	select LED_STRIP
	select WS2812_STRIP
	help
	  Select this to use WS2812 LED strips.

endchoice # APP_LIGHT_TYPE

rsource "Kconfig.app.pwm"

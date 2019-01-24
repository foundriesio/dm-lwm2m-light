# An LWM2M-based Smart Light Bulb

A simple open source light bulb project built on the Foundries.io
Zephyr microPlatform. We test it with (now discontinued) RedBear BLE
Nano 2 boards, but can be easily modified to work with essentially any
nRF52832 or nRF52840 based development board.

## Important Notice

**THERE ARE MANY TYPES OF NEOPIXEL RINGS.**

The firmware support defaults to [12 pixel RGBW rings from
AdaFruit](https://www.adafruit.com/product/2853) as shown below
left. RGBW pixels have a black coating. RGB pixels have a white
coating (below right). If you have more than 12 pixels or if you have
RGB pixels, you will need additional configuration changes as
described below before building the firmware.

![](/doc/pixel-types.jpg?raw=true)

## Build and Flash

To use this application, install the Zephyr microPlatform
as described here:

https://app.foundries.io/docs/latest/tutorial/index.html

You will also need a Linux microPlatform LWM2M gateway set up as
described in those pages.

There are 2 hardware variants supported by the dm-lwm2m-light application:

  * (default) WS2812-based hardware such as AdaFruit's NeoPixel products.
    Example: [https://www.adafruit.com/product/2853](https://www.adafruit.com/product/2853)
  * PWM-based LED hardware (off the shelf LED connected to BLE Nano2)

If you want to build this application using NeoPixel style hardware,
read this first:

- If you have RGB pixels: add this line to `dm-lwm2m-light/prj.conf`

  `CONFIG_WS2812_HAS_WHITE_CHANNEL=n`

- If you have more than 12 pixels in your ring or strip: add a line like
  this to `dm-lwm2m-light/prj.conf`:

  `CONFIG_WS2812_STRIP_MAX_PIXELS=YOUR_NUMBER_OF_PIXELS`

Then, instead of following the instructions to set up the standard
lwm2m system using `dm-lwm2m` firmware in
https://app.foundries.io/docs/latest/tutorial/basic-system.html, use
this instead:

```
./zmp build -b nrf52_blenano2 zephyr-fota-samples/dm-lwm2m-light
```

To build the application with PWM support instead (see `Kconfig.app.pwm`
for the relevant configuration options):

```
./zmp build -b nrf52_blenano2 --overlay-config=overlay-nrf52_blenano2-pwm.conf lwm2m-light/
```

Either way, flash the board like this:

```
./zmp flash -b nrf52_blenano2 zephyr-fota-samples/dm-lwm2m-light
```

## NeoPixel Wiring

Here is a simplified wiring schematic for using a strip of
NeoPixel-style LEDs, using a
[74AHCT125](https://www.adafruit.com/product/1787)
level shifter:

![](/doc/wiring-blenano2-neopixel.png?raw=true)

- Connect BLE Nano 2 pin `P0_2` (just above GND at bottom right in
  the picture above) to the "IN" pin on the pixel ring
- Supply VIN and GND to BLE Nano 2
- Supply PWR/GND to the NeoPixel ring
    - Ensure NeoPixel ring and BLE Nano 2 have a common ground
    - For easy operation but less brightness, connect BLE Nano 2 pin
      VDD to NeoPixel ring PWR input.
    - For 5V supply to the NeoPixel ring (max voltage; full brightness), a
      level shifter (like the 74AHCT125) from BLE Nano 2 P0_2 pin to
      NeoPixel IN pin is recommended. You may get away without one, but
      the pin's output high level is not guaranteed to work with 5V logic.

Here's the BLE Nano 2 pinout for reference:

![](/doc/blenano2-pinout.png?raw=true)

## Usage

On boot board will connect to the Leshan server at
https://mgmt.foundries.io/leshan as usual with LWM2M as described in
the microPlatform documentation. You'll need to pick your client from
the list based on its serial number.

The pixel ring will flash green briefly once Bluetooth is
connected. It will flash red if it ever gets a Bluetooth disconnect
(and it will reboot and retry to reconnect forever).

You can interact with the usual light control object in Leshan:

![](/doc/leshan-light.png?raw=true)

Actions:

- Write "true" to On/Off to turn on, "false" to turn off
- Write an integer from 0 (dimmest) to 100 (brightest) to Dimmer to
  set brightness
- Write a hexadecimal `RRGGBB` (all six characters must be present) to
  Colour to set the colour. Examples:
    - `00ff00` bright green
    - `007f00` dimmer green
    - `ff0000` bright red

You can also interact with the device using the Leshan JSON API.
Documentation is lacking; the best way to figure this out is to
intercept REST API calls in your browser console while interacting
with the web interface.

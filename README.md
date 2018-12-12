OS Light - the LWM2M Light
==========================

test

A simple open source lightbulb project built on the Foundries.io Zephyr microPlatform.  This sample is built against the latest subscriber release available at [Foundries.io Source](http://source.foundries.io).

**NOTE: You will need a valid subscription with [Foundries.io](https://app.foundries.io/) to perform the next steps.**

----------
To rebuild the OSLight applications, setup your environment according to Foundries.io [Zephyr microPlatform documentation](https://app.foundries.io/docs/latest/tutorial/installation-zephyr.html) and then you can use repo and the zmp command-line application to rebuild and flash.  The following samples work on MacOS and Linux.


Setup your workspace and get the code
```
#Create your workspace
mkdir oslight-workingdirectory
cd oslight-workingdirectory

#get the Zephyr microPlatform (you will be asked for your FIO subscriber token)
repo init -u https://source.foundries.io/zmp-manifest
repo sync

#get the lwm2m zephyr app
git clone https://github.com/oslight/lwm2m-light

```

There are 2 hardware variants supported by the lwm2m-light application:

  * FastLED hardware such as the WS2812-based NeoPixel ring products.  Example: [https://www.adafruit.com/product/2853](https://www.adafruit.com/product/2853)
  * PWM-based LED hardware (off the shelf LED modified to work with BLENano2)

Build the application(s) with FastLED support (the default) for BLENano2:
```
./zmp build -b nrf52_blenano2 lwm2m-light/
```
**Or**, build the application(s) with PWM support for the BLENano2:
```
./zmp build -b nrf52_blenano2 --overlay-config=overlay-nrf52_blenano2-pwm.conf lwm2m-light/
```

Flash the code to your device:
```
./zmp flash -b nrf52_blenano2 lwm2m-light/
```

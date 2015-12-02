# Configuration Mode

The firmware will enter configuration mode if the EEPROM storage is empty, or
if the button is held during reset.

The device will configure itself as an open access point with a name in the
format *ws2812-xxxxxx*.

Connect to the access point and browse to http://192.168.4.1/ to configure the
desired wifi settings and the number of LEDs to be addressed.

After updating the settings, reset the device to enter normal operation.

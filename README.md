# ArduinoPebbleSerial

This is an Arduino library for communicating with Pebble Time via the Smartstrap port. For more
information, see the
[Smartstrap guide on the Pebble developer site](https://developer.getpebble.com/guides/hardware/)

## Hardware Configuration

This Arduino library has both a software serial and hardware serial mode. The hardware serial mode
requires an external open-drain buffer as detailed in the smartstrap guide linked above, and also
requires board-specific support (see utility/board.h). The software serial mode requires only
a pull-up resistor and supports any AVR-based microcontroller.

## Examples ##

There a few example Arduino projects and an example Pebble app provided with library in the examples
folder.
* examples/PebbleApp/ - A Pebble app to use with the TeensyDemo and XadowDemo examples
* examples/TeensyDemo/ - A simple example sketch for the Teensy 2.0 board which will toggle the LED
when data is received and send the uptime to the watch, among other things. This demo runs with the
PebbleApp example and uses pin B1 (arduino pin 1) on the Teensy 2.0 board with the software serial
mode of the library.

# ArduinoPebbleSerial

This is an Arduino library for communicating with Pebble Time via the Smartstrap port. For more
information, see the
[Smartstrap guide on the Pebble developer site](https://developer.getpebble.com/guides/hardware/)

## Hardware Configuration

This Arduino library has both a software serial and hardware serial mode. The hardware serial mode
requires an external open-drain buffer as detailed in the smartstrap guide linked above, and also
requires board-specific support (see utility/board.h). The software serial mode requires only
a pull-up resistor and supports any AVR-based microcontroller.

## Tested Boards ##

| Board Name      | Tested in Software Mode | Tested in Hardware Mode                       |
| --------------- | ----------------------- | --------------------------------------------- |
| Teensy 2.0      | yes                     | yes (RX/TX pins shorted together)             |
| Teensy 3.0      | no                      | no, but supported                             |
| Teensy 3.1      | no                      | yes (pins 0[RX1] and 1[TX1] shorted together) |
| Arduino Uno     | yes                     | no, but supported                             |

## Examples ##

There an example Arduino project and an example Pebble app provided with library in the examples
folder.
* examples/PebbleApp/ - A Pebble app to use with the TeensyDemo examples
* examples/TeensyDemo/ - A simple example sketch for the Teensy 2.0 or Teensy 3.1 board which will
toggle the LED when data is received and send the uptime to the watch, among other things. This demo
runs with the PebbleApp example.

# ArduinoPebbleSerial

This is an Arduino library for communicating with Pebble Time via the Smartstrap port. For more
information, see the
[Smartstrap guide on the Pebble developer site](https://developer.getpebble.com/guides/hardware/)

## Installation ##

Below are instructions for installing this library with Arduino 1.6.x:

1. Download this repository as a .zip by clicking on the "Download ZIP" button on GitHub.
2. In Arduino, create a new project and go to "Sketch" -> "Include Library" -> "Add .ZIP Library..."
3. Select the .zip you just downloaded.
4. Arduino will automatically import the library and place the proper #include at the top.
5. In the future, you can skip step 1 and simply select the library within the "Sketch" ->
"Include Library" list for step 2.

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

There are two demos in the examples folder. Each demo consists of an Arduino project to be run on a
Teensy 2.0/3.x board (Demo2 only supports Teensy 2.0) and a Pebble app to be run on the watch.

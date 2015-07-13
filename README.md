# ArduinoPebbleSerial

This is an Arduino library for communicating with Pebble Time via the Smartstrap port.

## Hardware Configuration

The following boards and pin configurations are currently supported. The two pins listed must be
tied together and connected to the Smartstrap data pin.
* Arduino Uno (ATmega328p) - Serial0 - pins 0 and 1
* Arduino Mega 2560 (ATmega2560) - Serial1 - pins 18 and 19
* Xadow Main Board (ATmega32u4) - Serial1 - pins 0 and 1

To add support for more boards, see the top of the ArduinoPebbleSerial.cpp file.

## Initialization ##

	void ArduinoPebbleSerial::begin(uint8_t *buffer, size_t length);
This method starts the underlying serial connection. The caller must pass a buffer to be used by the
library as a receive buffer as well as its length.

## Feeding ##

	bool ArduinoPebbleSerial::feed(size_t *length, bool *is_read);
This method must be called continously in order to allow the library to process new bytes which have
come in over the serial port. It will return true when a complete frame has been received, in which
case it will set the passed `length` and `is_read` parameters with the length of the received
message and whether it was a read request or not respectively.

## Sending Responses ##

	bool ArduinoPebbleSerial::write(const uint8_t *payload, size_t length);
This method should only be called (once) after a frame with `is_read` set to true was received. The
specified payload of the specified length will be sent to the watch. Returns whether or not it was
send successfully.

## Other APIs ##

	bool ArduinoPebbleSerial::is_connected(void);
Returns true/false if we are currently connected to the watch.

	void ArduinoPebbleSerial::notify(void);
Sends a notification to the watch. Notifications can be used to tell the watch about an event.

## Demos ##

https://github.com/pebble/xadow-smartstrap-demo

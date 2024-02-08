
# Ratchet / Nano D++ communications protocol

This page describes the configuration and control protocol for the Nano D++ and Ratchet haptic devices.

The current implementation uses the device's serial port, via USB. 

This is the protocol used to configure and customize the device via the haptic configuration GUI tool, or it could be used to implement your own projects involving Nano D++ communications.

It is not the output protocol of the device for regular use. That would be either MIDI, or USB HID.

The MIDI communication is documented [here](midi.md).

The USB HID communication is described [here](hid.md).

## Serial transport

The Nano D++ and Ratchet devices have a USB 2.0 HS (10MBit) USB port. 

In addtion to presenting the Nano D++ or Ratchet as a HID (human input device - like a kind of keyboard) to a laptop or computer, the USB port allows you to open a Serial connection to the device as well.

The Serial speed is automatically negotiated when connecting via USB. The connection settings should be 8 bits, no parity and 1 stop bit (8N1).

You can connect with a serial terminal program like CoolTerm, TeraTerm or the unix "screen" command, or fire up the ZeroOne haptic configuration tool, which will automatically detect your devices.

## Serial protocol

The Serial protocol is simple: small JSON messages are sent in both directions, which encode commands or data to be exchanged with the device. The message formats are described below.

Each JSON message is separated from the next one by a newline character. Newline characters within the JSON structure are not permitted. Where newline characters are used within the transported field values of the JSON message, they should be text escaped as '\n'.

## Message Types

The message types are:

### type 'h' - haptic configuration

Use a haptic configuration message to set one or more values in the haptic configuration. For example, a message like:

`{ "p": { "id": "profileid", "attract_distance": 21, "bounce_strength": 3 } }`

sets the fields 'attract_distance' and 'bounce_strength' in the profile with id 'profileid'.

Send a message with no values to retrieve all the haptic parameters for the given program:

`{ "p": "profileid" }`

Response:

TODO add example of entire program.

Send a message with profile id #all to retrieve all the profile IDs the device knows about:

`{ "p": "#all" }`

Response:

`{ "profiles": ["default", "Profile 1", "My cool profile"] }`


### type 'l' - LED control

TODO describe, works similar to haptic commands

### type 'k' - key mapping configuration

TODO describe, works similar to haptic commands

### type 'e' - event

TODO finalize this, it will still change

The device sends these messages when internal state changes happen. There are different types of event data that can be attached to the event, the messages are formatted like this:

`{ "e": 1261847483, "a": 2.8845 "t": 17 }`

The possible event data fields are:

- e: event timestamp
- a: angle (float, in radians)
- t: full turns (integer)
- kd: key down (integer, key number)
- ku: key up (integer, key number)
- ks: keys state (array of 4 booleans, true when down, false when up)
- bd: button down (boolean)
- bf: button force (force on button)

An event will contain at least one data field, and may contain multiple data fields. No data field will be included more than once.

### type 'm' - system message

Send system messages. Usually from device to GUI tool.

`{ "m"="INFO: initialziation complete", l="I"}`
`{ "m"="DEBUG: value is 3", l="D"}`
`{ "m"="ALERT: low voltage detected", l="A"}`


### type 'R' - motor register

Interact with SimpleFOC and the motor driver using its register based configuration protocol. These messages are sent by the host to the device.

`{ "R": "R1=10.0" }`


### type 'r' - motor response

Receive the SimpleFOC driver level responses to register commands. These messages are sent by the device to the host.

`{ "r": "r1=10.0" }`


### type 'T' and 'H' - telemetry and telemetry header

TODO not implemented for MVP

Receive the SimpleFOC driver level telemetry. Off by default, can be switched on using registers R28 and R29. Sent from the device to the host.

Please consult the SimpleFOC Drivers documentation and SimpleFOC API documentation for further details on the telemetry data format.
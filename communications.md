
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

## Outgoing messages

**Error messages** are sent when errors occur in the system, or as response to erroneous command messages. **Debug messages** should be output to the console to aid in debugging.

```json
{ "error": "An error occurred." }
{ "error": "Another kind of error.", "msg": "This one comes with a detail message." }

{ "debug": "A message for the console." }
```

**Idle messages** are sent once per second by the device if no event messages have been sent recently. They include the idle time, the duration (ms) since the last event message (e.g. the last user interaction):

```json
{ "idle": 16233 }
```

**Event messages** are sent by the system on user interaction with the device. There are a few types depending on the event. Multiple events could arrive in the same JSON message.

```json
{ "kd": "A", "ks": "AbCd" }              // kd = key-down, ks = keys-state
{ "ku": "AC", "kd": "D", "ks": "abcD" }  // ku = key-up
{ "a": 4.16, "t": -2, "v": -7.78 }       // a = angle (rad),
                                         // t = nr of turns, 
                                         // v = velocity (rad/s)
```

Other outgoing message are sent in response to commands, and are described below.

## Commands

### Profile commands

Get a list of all profile names: 
```json
{ "p": "#all" }
```

Response:
```json
{ "profiles": ["default", "Fusion", "Fusion2", "Fusion2 copy", "Blender"] }
```

<hr>

Get a single profile's details:

```json
{ "p": "Blender" }
```

The response includes all profile fields, and would arrive in one line, but is shown here formatted for legibility:
```json
{ 
    "p": {
        // TODO
    } 
}
```

<hr>

Update one or more profile fields:

```json
{ "p": { "id": "Blender", "profileType": 2, "haptic_click_strength": 13.0, "ledEnable": false }}
```

TODO what would be the best response?

<hr>

Set the current profile:

```json
{ "current": "Fusion2" }
```

TODO what would be the best response?

<hr>

Delete or reorder the profiles, e.g. put the "Blender" profile first, the "default" profile last, and remove the "Fusion2 copy" profile:

```json
{ "profiles": ["Blender", "Fusion", "Fusion2", "default"] }
```

TODO what would be the best response?

### Motor commands

Set SimpleFOC registers:

```json
{ "R": "17=2.0 19=7.7" }
```

Reset motor calibration:

```json
{ "recalibrate": true }
```

### System commands

Get device settings:

```json
{ "settings": "?" }
```

Response (formatted on multiple lines for clarity):

```json
{ 
    "settings": {
        "debug": false,
        "ledMaxBrightness": 125,
        "maxVelocity": 50.0,
        "maxVoltage": 3.0,
        "firmwareVersion": "1.0.0",
        "deviceId": "A300F377",
        "deviceName": "Richard's Nano",
        // TODO document other settings
    }
}
```

<hr>

Enable or disable debug messages or set device options. One or more settings values can be set in the same message:

```json
{ "settings": { "debug": true, "ledMaxBrightness": 170, "maxVelocity": 45.0, "maxVoltage": 4.4 }}
```

<hr>

Show a message on the device screen:

```json
{ "message": { "title": "Hey!", "text": "Get some work done.", "duration": 5.0 }}
```


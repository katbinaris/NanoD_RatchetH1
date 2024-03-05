# Nano API Concept

This is a more or less comprehensive list of what the Nano API for the zeroone frontend encompasses.
It was created for and modified during discussion with @runger, here for future reference.

This API is not for the enduser but for quick communication with and configuration by the UI.
User endpoints can be built at a later point (bindings, sdk etc)..

Types of messages:

- ## Client -> Nano:
	- Profile management
		- Get
		- List
		- Reorder
		- Add
		- Delete
		- Rename
	- Profile settings (structure here as in UI, FW will deviate)
		- Current profile
			- Knob 
				- Haptics (?)
					- Motor
					- Transducers
				- Lighting
					- Ring mode
					- Brightness
				- Mapping (???)
			- Keys (xN) { 'key_a_light_pressed': 218378  }
				- Lighting
					- Colors
						- Default
						- Pressed
					- Brightness
				- Mapping (???)
			- MIDI Jacks
		- Specific profile
	- Trigger actions
		- Recalibration
		- Reboot
		- FW update (???)
		- Save state
	- (Override settings temporarily)
		- LEDs
		- Display
	- Device Settings
		- Device name
		- Max brightness
		- ...
## Nano -> Client
- Setting changed
	- Success
	- Failure
		- Reason
- Input Event
	- Knob
	- Keys
- Heartbeat (nothing for >1 sec -> crash)
- Saved state message

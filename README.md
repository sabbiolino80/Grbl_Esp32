forked from https://github.com/bdring/Grbl_Esp32

stripped down version of grbl to control a 2-axes programmable watering system


### Issues / Changes

1. **Direction pin delay** - Not implemented yet. Some drivers require a couple of microseconds after the direction pin is set before you start the step pulse. The original plan was to [use the RMT feature](http://www.buildlog.net/blog/?s=rmt), but that has issues when trying to use it in an Interrupt.  **This is typically a option in Grbl that is not used.**
2. **Limit Switch debouncing** is not supported yet. It does not seem to be a problem on my test rigs. The dev board uses R/C filters on all inputs.
3. **Step Pulse Invert:** This has not been fully tested. I suggest...leaving $2=0



### Using It

The code should be compiled using the latest Arduino IDE. [Follow instructions here](https://github.com/espressif/arduino-esp32) on how to setup ESP32 in the IDE. The choice was made to use the Arduino IDE over the ESP-IDF to make the code a little more accessible to novices trying to compile the code.

I use the ESP32 Dev Module version of the ESP32. I suggest starting with that if you don't have hardware yet.

For basic instructions on using Grbl use the [gnea/grbl wiki](https://github.com/gnea/grbl/wiki). That is the Arduino version of Grbl, so keep that in mind regarding hardware setup. If you have questions ask via the GitHub issue system for this project.

Be sure you have external pullup resistors on any GPIO34-39 that you use. These default to door, start, hold and reset functions.

Using Bluetooth:

- [See the Wiki page](https://github.com/bdring/Grbl_Esp32/wiki/Using-Bletooth)

### Credits

The original [Grbl](https://github.com/gnea/grbl) is an awesome project by Sungeon (Sonny) Jeon. I have known him for many years and he is always very helpful. I have used Grbl on many projects. I only ported because of the limitation of the processors it was designed for. The core engine design is virtually unchanged.

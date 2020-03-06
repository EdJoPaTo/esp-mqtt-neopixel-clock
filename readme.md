# ESP MQTT NeoPixel Clock

NodeMCU with a WS2812 Ring showing a clock.
Brightness and On/Off Status can be controlled via MQTT respecting MQTT-Smarthome.

Known Issues: The NTP Server is contacted on every update (4 times a second) which isnt that bad in this case as it is the router but still not ideal.
using delay(1000) or something like that isnt as precisely correct.
Requires a bit more time to do that right.

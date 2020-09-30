# ESP MQTT NeoPixel Clock

NodeMCU with a WS2812 Ring showing a clock.
Brightness and On/Off Status can be controlled via MQTT respecting MQTT-Smarthome.

The hue does show the current hour:
- 0° (red) is 0:00, 6:00, 12:00 or 18:00
- 120° (green) is 2:00, 8:00, 14:00 or 20:00
- 240° (green) is 4:00, 10:00, 16:00 or 22:00

That way you can quickly assume the time by color.

Minutes are the gap in the circle. The gap works exactly like normal clockhands. (Top is :00, Right is 0:15 and so on.)

Seconds are the little dot with the complementary color of the rest and works in the same manner as clockhands.

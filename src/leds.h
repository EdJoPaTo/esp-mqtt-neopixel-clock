// NEOPIXEL BEST PRACTICES for most reliable operation:
// - Add 1000 uF CAPACITOR between NeoPixel strip's + and - connections.
// - MINIMIZE WIRING LENGTH between microcontroller board and first pixel.
// - NeoPixel strip's DATA-IN should pass through a 300-500 OHM RESISTOR.
// - AVOID connecting NeoPixels on a LIVE CIRCUIT. If you must, ALWAYS
//   connect GROUND (-) first, then +, then data.
// - When using a 3.3V microcontroller with a 5V-powered NeoPixel strip,
//   a LOGIC-LEVEL CONVERTER on the data line is STRONGLY RECOMMENDED.
// (Skipping these may work OK on your workbench but can fail in the field)

#include <NeoPixelBus.h>

const int LED_COUNT = 60;

// Efficient connection via DMA on pin RDX0 GPIO3 RX
// See <https://github.com/Makuna/NeoPixelBus/wiki/FAQ-%231>
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(LED_COUNT);

// bitbanging (Fallback)
// const int LED_PIN = 13; // D7
// NeoPixelBus<NeoGrbFeature, NeoEsp8266BitBang800KbpsMethod> strip(LED_COUNT, LED_PIN);

void leds_setup()
{
	strip.Begin();
	strip.Show();
}

void leds_clear()
{
	strip.ClearTo(RgbColor(0,0,0));
}

void leds_show()
{
	strip.Show();
}

// hue 0.0 - 360.0
// sat 0.0 - 1.0
// bri 0.0 - 1.0
void leds_set_hsv(uint16_t pixel, float h, float s, float v)
{
	strip.SetPixelColor(pixel, HsbColor(h / 360.0f, s, v));
}

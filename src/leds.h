// NEOPIXEL BEST PRACTICES for most reliable operation:
// - Add 1000 uF CAPACITOR between NeoPixel strip's + and - connections.
// - MINIMIZE WIRING LENGTH between microcontroller board and first pixel.
// - NeoPixel strip's DATA-IN should pass through a 300-500 OHM RESISTOR.
// - AVOID connecting NeoPixels on a LIVE CIRCUIT. If you must, ALWAYS
//   connect GROUND (-) first, then +, then data.
// - When using a 3.3V microcontroller with a 5V-powered NeoPixel strip,
//   a LOGIC-LEVEL CONVERTER on the data line is STRONGLY RECOMMENDED.
// (Skipping these may work OK on your workbench but can fail in the field)

#include <Adafruit_NeoPixel.h>

const int LED_PIN = 13; // D7
const int LED_COUNT = 60;

// Declare our NeoPixel strip object:
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
// Argument 1 = Number of pixels in NeoPixel strip
// Argument 2 = pin number (most are valid)
// Argument 3 = Pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)

void leds_setup()
{
	strip.begin();
	strip.clear();
	strip.setBrightness(0);
}

// 0 - 255
void leds_set_brightness(uint8_t value)
{
	strip.setBrightness(value);
}

void leds_clear()
{
	strip.clear();
}

void leds_show()
{
	strip.show();
}

// see https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both
// hue 0.0 - 360.0
// sat 0.0 - 1.0
// bri 0.0 - 1.0
void leds_set_hsv(uint16_t pixel, float h, float s, float v)
{
	float hh, p, q, t, ff;
	long i;
	float r, g, b;

	if (s <= 0.0f) // < is bogus, just shuts up warnings
	{
		r = v;
		g = v;
		b = v;
	}
	else
	{
		hh = h;
		if (hh >= 360.0f)
			hh = 0.0f;
		hh /= 60.0f;
		i = (long)hh;
		ff = hh - i;
		p = v * (1.0f - s);
		q = v * (1.0f - (s * ff));
		t = v * (1.0f - (s * (1.0f - ff)));

		switch (i)
		{
		case 0:
			r = v;
			g = t;
			b = p;
			break;
		case 1:
			r = q;
			g = v;
			b = p;
			break;
		case 2:
			r = p;
			g = v;
			b = t;
			break;

		case 3:
			r = p;
			g = q;
			b = v;
			break;
		case 4:
			r = t;
			g = p;
			b = v;
			break;
		case 5:
		default:
			r = v;
			g = p;
			b = q;
			break;
		}
	}

	strip.setPixelColor(pixel, strip.Color(r * 255, g * 255, b * 255));
}

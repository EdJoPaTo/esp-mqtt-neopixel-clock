// NEOPIXEL BEST PRACTICES for most reliable operation:
// - Add 1000 uF CAPACITOR between NeoPixel strip's + and - connections.
// - MINIMIZE WIRING LENGTH between microcontroller board and first pixel.
// - NeoPixel strip's DATA-IN should pass through a 300-500 OHM RESISTOR.
// - AVOID connecting NeoPixels on a LIVE CIRCUIT. If you must, ALWAYS
//   connect GROUND (-) first, then +, then data.
// - When using a 3.3V microcontroller with a 5V-powered NeoPixel strip,
//   a LOGIC-LEVEL CONVERTER on the data line is STRONGLY RECOMMENDED.
// (Skipping these may work OK on your workbench but can fail in the field)

#include <AceTime.h>
#include <Adafruit_NeoPixel.h>
#include <EspMQTTClient.h>

using namespace ace_time;
using namespace ace_time::clock;

#define CLIENT_NAME "espNeopixelClock"

EspMQTTClient client(
  "WifiSSID",
  "WifiPassword",
  "192.168.1.100",  // MQTT Broker server ip
  "MQTTUsername",   // Can be omitted if not needed
  "MQTTPassword",   // Can be omitted if not needed
  CLIENT_NAME,     // Client name that uniquely identify your device
  1883              // The MQTT port, default to 1883. this line can be omitted
);

const bool mqtt_retained = true;

#define BASIC_TOPIC CLIENT_NAME "/"
#define BASIC_TOPIC_SET BASIC_TOPIC "set/"
#define BASIC_TOPIC_STATUS BASIC_TOPIC "status/"


// Which pin is connected to the NeoPixels?
const int LED_PIN = 13;

// How many NeoPixels are attached?
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

const int SECOND = 1;
const int MINUTE = 60 * SECOND;
const int HOUR = 60 * MINUTE;

const int SECONDS = 60;
const int MINUTES = 60 * SECONDS;
const int HOURS = 12 * MINUTES;

int32_t nowSeconds = 0;
static BasicZoneProcessor berlinProcessor;
TimeZone tz = TimeZone::forZoneInfo(&zonedb::kZoneEurope_Berlin, &berlinProcessor);
static NtpClock ntpClock("fritz.box");

boolean on = true;
double briFactor = 0.15;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  Serial.begin(115200);

  ntpClock.setup();

  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.clear();
  // strip.show();            // Turn OFF all pixels ASAP
  strip.setBrightness(100); // Set BRIGHTNESS to about 1/5 (max = 255)

  // Optional functionnalities of EspMQTTClient
  client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  client.enableLastWillMessage(BASIC_TOPIC "connected", "0", mqtt_retained);  // You can activate the retain flag by setting the third parameter to true
}

void onConnectionEstablished() {
  client.subscribe(BASIC_TOPIC_SET "bri", [](const String & payload) {
    int value = strtol(payload.c_str(), 0, 10);
    briFactor = max(0.02, value / 100.0);
    client.publish(BASIC_TOPIC_STATUS "bri", payload, mqtt_retained);
  });

  client.subscribe(BASIC_TOPIC_SET "on", [](const String & payload) {
    boolean value = payload != "0";
    on = value;
    client.publish(BASIC_TOPIC_STATUS "on", payload, mqtt_retained);
  });

  client.publish(BASIC_TOPIC "connected", "2", mqtt_retained);
  digitalWrite(LED_BUILTIN, HIGH);
}

int rArr[LED_COUNT];
int gArr[LED_COUNT];
int bArr[LED_COUNT];

const int INTERVALS = 4;

void loop() {
  client.loop();
  strip.clear();

  auto now = ntpClock.getNow();
  if (now != LocalDate::kInvalidEpochSeconds) {
    nowSeconds = now;
  }

  if (on) {
    initArray(gArr, LED_COUNT);

    // Hourly ticks
    int hourEach = 60 / 12;
    for (int i = 0; i < 12; i++) {
      gArr[i * hourEach] = 10;
    }

    initArray(rArr, LED_COUNT);
    initArray(bArr, LED_COUNT);

    auto tzTime = ZonedDateTime::forEpochSeconds(nowSeconds, tz);
    auto hour = tzTime.hour() % 12;
    auto minute = tzTime.minute();
    auto second = tzTime.second();

    int hourIn60 = (hour * 60 + minute) * 5 / 60;

    // clock hands
    bArr[hourIn60] = 255;
    rArr[minute] = 255;
    gArr[second] = 100;

    for (int i = 0; i < LED_COUNT; i++) {
      setRgb(i, rArr[i], gArr[i], bArr[i]);
    }
  }

  strip.show();
  delay(1000 / INTERVALS);
}

void setRgb(int clockIndex, int r, int g, int b) {
  int pixel = (clockIndex + 43) % LED_COUNT;
  strip.setPixelColor(pixel, strip.Color(r * briFactor, g * briFactor, b * briFactor));
}

void initArray(int array[], int length) {
  for (int i = 0; i < length; i++) {
    array[i] = 0;
  }
}

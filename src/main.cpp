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
#include <credentials.h>
#include <DHTesp.h>
#include <EspMQTTClient.h>
#include <MqttKalmanPublish.h>

#include "localtime.h"

#define CLIENT_NAME "espNeopixelClock"
const bool MQTT_RETAINED = true;

EspMQTTClient client(
    WIFI_SSID,
    WIFI_PASSWORD,
    MQTT_SERVER,
    MQTT_USERNAME,
    MQTT_PASSWORD,
    CLIENT_NAME,
    1883);

#define BASIC_TOPIC CLIENT_NAME "/"
#define BASIC_TOPIC_SET BASIC_TOPIC "set/"
#define BASIC_TOPIC_STATUS BASIC_TOPIC "status/"

// Which pin is connected to the NeoPixels?
const int LED_PIN = 13; // D7
const int DHTPIN = 12;  // D6

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

DHTesp dht;

MQTTKalmanPublish mkTemp(client, BASIC_TOPIC_STATUS "temp", MQTT_RETAINED, 12 * 2 /* every 2 min */, 0.2);
MQTTKalmanPublish mkHum(client, BASIC_TOPIC_STATUS "hum", MQTT_RETAINED, 12 * 5 /* every 5 min */, 2);
MQTTKalmanPublish mkRssi(client, BASIC_TOPIC_STATUS "rssi", MQTT_RETAINED, 12 * 5 /* every 5 min */, 10);

int lastConnected = 0;
boolean on = true;
uint8_t mqttBri = 1;
const int BRIGHTNESS_FACTOR = 5;
const uint8_t MAX_BACKGROUND_OFF_BRIGHTNESS = 4;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);

  strip.begin();
  strip.clear();
  strip.setBrightness(min(255, BRIGHTNESS_FACTOR * mqttBri * on));

  dht.setup(DHTPIN, DHTesp::DHT22);

  client.enableDebuggingMessages();
  client.enableHTTPWebUpdater();
  client.enableOTA();
  client.enableLastWillMessage(BASIC_TOPIC "connected", "0", MQTT_RETAINED);
}

void onConnectionEstablished() {
  client.subscribe(BASIC_TOPIC_SET "bri", [](const String &payload) {
    int value = strtol(payload.c_str(), 0, 10);
    mqttBri = max(1, min(255 / BRIGHTNESS_FACTOR, value));
    strip.setBrightness(min(255, BRIGHTNESS_FACTOR * mqttBri * on));
    client.publish(BASIC_TOPIC_STATUS "bri", String(mqttBri), MQTT_RETAINED);
  });

  client.subscribe(BASIC_TOPIC_SET "on", [](const String &payload) {
    boolean value = payload != "0";
    on = value;
    strip.setBrightness(min(255, BRIGHTNESS_FACTOR * mqttBri * on));
    client.publish(BASIC_TOPIC_STATUS "on", String(on), MQTT_RETAINED);
  });

  client.publish(BASIC_TOPIC_STATUS "bri", String(mqttBri), MQTT_RETAINED);
  client.publish(BASIC_TOPIC_STATUS "on", String(on), MQTT_RETAINED);
  client.publish(BASIC_TOPIC "git-version", GIT_VERSION, MQTT_RETAINED);
  client.publish(BASIC_TOPIC "connected", "1", MQTT_RETAINED);
  lastConnected = 1;
}

void setHsv(int clockIndex, uint16_t hue, uint8_t sat, uint8_t bri) {
  uint16_t pixel = (clockIndex + 43) % LED_COUNT;
  strip.setPixelColor(pixel, strip.ColorHSV(hue * 182, sat * 2.55, bri));
}

void displayTime() {
  // Dont update when time / mqtt is not initialized yet
	if (!localtime_isKnown() || !client.isConnected()) {
		return;
	}

  strip.clear();

  if (on) {
		auto tzTime = localtime_getDateTime();
    auto hour = tzTime.hour();
    auto minute = tzTime.minute();
    auto second = tzTime.second();

    auto minuteOfDay = (hour * 60) + minute;
    uint16_t hue = minuteOfDay % 360;

    Serial.printf(
        "displayTime %2d:%02d:%02d  hue %3d  at %3ld (ideal: %3ld)\n",
        hour, minute, second, hue, millis() % 1000, referenceMillis % 1000);

    uint16_t hueArr[LED_COUNT];
    uint8_t satArr[LED_COUNT];
    uint8_t briArr[LED_COUNT];
    for (int i = 0; i < LED_COUNT; i++) {
      hueArr[i] = hue;
      satArr[i] = 100;
      briArr[i] = 255 / 20;
    }

    // Hourly ticks
    const int HOUR_EVERY_N_LEDS = LED_COUNT / 12;
    for (int i = 0; i < 12; i++) {
      auto led = i * HOUR_EVERY_N_LEDS;

      if (mqttBri <= MAX_BACKGROUND_OFF_BRIGHTNESS || led != second) {
        briArr[led] = i % 3 == 0 ? 255 / 5 : 255 / 10;
      }

      if (mqttBri > MAX_BACKGROUND_OFF_BRIGHTNESS) {
        satArr[led] = 75;
      }
    }

    // clock hands

    if (mqttBri > MAX_BACKGROUND_OFF_BRIGHTNESS) {
      for (int i = -2; i <= 2; i++) {
        uint8_t resulting_minute = (minute + 60 + i) % 60;
        if (resulting_minute != second) {
          briArr[resulting_minute] = 0;
        }
      }

      hueArr[second] = (hue + 180) % 360;
    } else {
      briArr[minute] = 255;
      satArr[minute] = 100;
    }

    for (int i = 0; i < LED_COUNT; i++) {
      setHsv(i, hueArr[i], satArr[i], briArr[i]);
    }
  }

  strip.show();
}

void loop() {
  client.loop();
  digitalWrite(LED_BUILTIN, client.isConnected() ? HIGH : LOW);

  static unsigned long nextMeasure = 0;
  if (millis() >= nextMeasure) {
    nextMeasure = millis() + 5000; // every 5 seconds

    float t = dht.getTemperature();
    float h = dht.getHumidity();

    boolean readSuccessful = dht.getStatus() == DHTesp::ERROR_NONE;
    if (client.isConnected()) {
      int nextConnected = readSuccessful ? 2 : 1;
      if (nextConnected != lastConnected) {
        Serial.printf("set /connected from %d to %d\n", lastConnected, nextConnected);
        lastConnected = nextConnected;
        client.publish(BASIC_TOPIC "connected", String(nextConnected), MQTT_RETAINED);
      }
    }

    if (readSuccessful) {
      float avgT = mkTemp.addMeasurement(t);
      Serial.printf("Temperature in Celsius: %5.1f Average: %6.2f\n", t, avgT);

      float avgH = mkHum.addMeasurement(h);
      Serial.printf("Humidity    in Percent: %5.1f Average: %6.2f\n", h, avgH);
    } else {
      Serial.print("Failed to read from DHT sensor! ");
      Serial.println(dht.getStatusString());
    }

    if (client.isWifiConnected()) {
      long rssi = WiFi.RSSI();
      float avgRssi = mkRssi.addMeasurement(rssi);
      Serial.printf("RSSI        in     dBm: %5ld Average: %6.2f\n", rssi, avgRssi);
    }
  }

	if (client.isWifiConnected() && localtime_updateNeeded() && localtime_update()) {
		// update successful
    displayTime();
	}

	if (localtime_isKnown()) {
		auto distance = localtime_millisUntilNextSecond();

		// If there is much time left, let others use it
		// Otherwise make sure the clock will be updated on time
		if (distance > 200) {
			delay(7);
		} else {
			delay(distance);
			displayTime();
			delay(50);
		}
	}
}

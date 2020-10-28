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
#include <DHTesp.h>
#include <EspMQTTClient.h>
#include <MqttKalmanPublish.h>

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

const bool MQTT_RETAINED = true;
int lastConnected = 0;

#define BASIC_TOPIC CLIENT_NAME "/"
#define BASIC_TOPIC_SET BASIC_TOPIC "set/"
#define BASIC_TOPIC_STATUS BASIC_TOPIC "status/"


// Which pin is connected to the NeoPixels?
const int LED_PIN = 13; // D7
const int DHTPIN = 12; // D6

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

const int HOUR_EVERY_N_LEDS = LED_COUNT / 12;

const int UPDATE_EVERY_SECONDS = 5 * MINUTE;

DHTesp dht;

MQTTKalmanPublish mkTemp(client, BASIC_TOPIC_STATUS "temp", MQTT_RETAINED, 12 * 2 /* every 2 min */, 0.2);
MQTTKalmanPublish mkHum(client, BASIC_TOPIC_STATUS "hum", MQTT_RETAINED, 12 * 5 /* every 5 min */, 2);
MQTTKalmanPublish mkRssi(client, BASIC_TOPIC_STATUS "rssi", MQTT_RETAINED, 12 * 5 /* every 5 min */, 10);

int32_t nowSeconds = 0;
int referenceMillis = 0;
boolean timeUnknown = true;
static BasicZoneProcessor berlinProcessor;
TimeZone tz = TimeZone::forZoneInfo(&zonedb::kZoneEurope_Berlin, &berlinProcessor);
static NtpClock ntpClock("fritz.box");

boolean on = true;
uint8_t mqttBri = 1;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  Serial.begin(115200);

  ntpClock.setup();

  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.clear();
  // strip.show();            // Turn OFF all pixels ASAP

  dht.setup(DHTPIN, DHTesp::DHT22);

  // Optional functionnalities of EspMQTTClient
  client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  client.enableLastWillMessage(BASIC_TOPIC "connected", "0", MQTT_RETAINED);
}

void onConnectionEstablished() {
  client.subscribe(BASIC_TOPIC_SET "bri", [](const String & payload) {
    int value = strtol(payload.c_str(), 0, 10);
    mqttBri = max(1, min(50, value));
    client.publish(BASIC_TOPIC_STATUS "bri", String(mqttBri), MQTT_RETAINED);
  });

  client.subscribe(BASIC_TOPIC_SET "on", [](const String & payload) {
    boolean value = payload != "0";
    on = value;
    client.publish(BASIC_TOPIC_STATUS "on", String(on), MQTT_RETAINED);
  });

  client.publish(BASIC_TOPIC "connected", "1", MQTT_RETAINED);
  client.publish(BASIC_TOPIC_STATUS "bri", String(mqttBri), MQTT_RETAINED);
  client.publish(BASIC_TOPIC_STATUS "on", String(on), MQTT_RETAINED);
  lastConnected = 1;
}

uint16_t hueArr[LED_COUNT];
uint8_t satArr[LED_COUNT];
uint8_t briArr[LED_COUNT];
const int BASE_BRIGHTNESS_FACTOR = 6;

void setHsv(int clockIndex, uint16_t hue, uint8_t sat, uint8_t bri) {
  uint16_t pixel = (clockIndex + 43) % LED_COUNT;
  strip.setPixelColor(pixel, strip.ColorHSV(hue * 182, sat * 2.55, bri));
}

void displayTime() {
  strip.clear();

  if (on) {
    auto tzTime = ZonedDateTime::forEpochSeconds(nowSeconds, tz);
    auto hour = tzTime.hour() % 12;
    auto minute = tzTime.minute();
    auto second = tzTime.second();

    auto totalMinuteOfHalfDay = (hour * 60) + minute;
    uint16_t hue = totalMinuteOfHalfDay % 360;

    for (int i = 0; i < LED_COUNT; i++) {
      hueArr[i] = hue;
      satArr[i] = 100;
      briArr[i] = mqttBri / BASE_BRIGHTNESS_FACTOR;
    }

    // Hourly ticks
    for (int i = 0; i < 12; i++) {
      briArr[i * HOUR_EVERY_N_LEDS] = mqttBri;
      if (mqttBri >= BASE_BRIGHTNESS_FACTOR) {
        satArr[i * HOUR_EVERY_N_LEDS] = 90;
      }
    }

    // clock hands

    if (mqttBri >= BASE_BRIGHTNESS_FACTOR) {
      for (int i = -2; i <= 2; i++) {
        uint8_t resulting_minute = (minute + 60 + i) % 60;
        if (resulting_minute != second) {
          briArr[resulting_minute] = 0;
        }
      }

      hueArr[second] = (hue + 180) % 360;
    } else {
      briArr[minute] = min(255, mqttBri * 5);
      satArr[minute] = 100;
    }

    for (int i = 0; i < LED_COUNT; i++) {
      setHsv(i, hueArr[i], satArr[i], briArr[i]);
    }
  }

  strip.show();
}

void updateTime() {
  auto start = millis();
  auto first = ntpClock.getNow();
  if (first == LocalDate::kInvalidEpochSeconds) {
    return;
  }

  int32_t second = first;
  int32_t tmp;
  while (second == first) {
    delay(20);

    tmp = ntpClock.getNow();
    if (tmp != LocalDate::kInvalidEpochSeconds) {
      second = tmp;
    }
  }

  referenceMillis = millis() % 1000;
  nowSeconds = second;
  timeUnknown = false;

  auto took = millis() - start;

  Serial.print("updateTime finished at ");
  Serial.print(referenceMillis);
  Serial.print(" and took ");
  Serial.print(took);
  Serial.println("ms");
}

const int INTERVALS = 4;
const int INTERVAL_WAIT = 1000 / INTERVALS;
int interval = 0;

void loop() {
  if (!client.isConnected()) {
    lastConnected = 0;
  }

  client.loop();
  digitalWrite(LED_BUILTIN, client.isConnected() ? HIGH : LOW);

  displayTime();

  if (interval == 0 && nowSeconds % 5 == 0) {
    float t = dht.getTemperature();
    float h = dht.getHumidity();

    boolean readSuccessful = dht.getStatus() == DHTesp::ERROR_NONE;
    if (client.isConnected()) {
      int nextConnected = readSuccessful ? 2 : 1;
      if (nextConnected != lastConnected) {
        Serial.print("set /connected from ");
        Serial.print(lastConnected);
        Serial.print(" to ");
        Serial.println(nextConnected);
        lastConnected = nextConnected;
        client.publish(BASIC_TOPIC "connected", String(nextConnected), MQTT_RETAINED);
      }
    }

    if (readSuccessful) {
      float avgT = mkTemp.addMeasurement(t);
      Serial.print("Temperature in Celsius: ");
      Serial.print(String(t).c_str());
      Serial.print(" Average: ");
      Serial.println(String(avgT).c_str());

      float avgH = mkHum.addMeasurement(h);
      Serial.print("Humidity    in Percent: ");
      Serial.print(String(h).c_str());
      Serial.print(" Average: ");
      Serial.println(String(avgH).c_str());
    } else {
      Serial.print("Failed to read from DHT sensor! ");
      Serial.println(dht.getStatusString());
    }

    if (client.isConnected()) {
      long rssi = WiFi.RSSI();
      float avgRssi = mkRssi.addMeasurement(rssi);
      Serial.print("RSSI        in dBm:     ");
      Serial.print(String(rssi).c_str());
      Serial.print("   Average: ");
      Serial.println(String(avgRssi).c_str());
    }
  }

  if (++interval < INTERVALS) {
    delay(INTERVAL_WAIT);
  } else {
    nowSeconds++;
    interval = 0;

    // Update every 5 min -> update on 4:55 so 5:00 will be accurate
    if (nowSeconds % UPDATE_EVERY_SECONDS == UPDATE_EVERY_SECONDS - 5) {
      timeUnknown = true;
    }

    if (timeUnknown && client.isConnected()) {
      updateTime();
    } else {
      auto current = millis() % 1000;
      auto distance = (1000 + referenceMillis - current) % 1000;
      delay(distance);
    }

    Serial.print("now ");
    Serial.print(nowSeconds % 60);
    Serial.print(" at ");
    Serial.println(millis() % 1000);
  }
}

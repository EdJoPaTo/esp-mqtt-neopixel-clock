#include <credentials.h>
#include <DHTesp.h>
#include <EspMQTTClient.h>
#include <MqttKalmanPublish.h>

#include "leds.h"
#include "localtime.h"

const bool MQTT_RETAINED = true;

const int DHTPIN = 12; // D6

EspMQTTClient mqttClient(
	WIFI_SSID,
	WIFI_PASSWORD,
	MQTT_SERVER,
	MQTT_USERNAME,
	MQTT_PASSWORD,
	CLIENT_NAME,
	1883);

#define BASE_TOPIC CLIENT_NAME "/"
#define BASE_TOPIC_SET BASE_TOPIC "set/"
#define BASE_TOPIC_STATUS BASE_TOPIC "status/"

#ifdef ESP8266
	#define LED_BUILTIN_ON LOW
	#define LED_BUILTIN_OFF HIGH
#else // for ESP32
	#define LED_BUILTIN_ON HIGH
	#define LED_BUILTIN_OFF LOW
#endif

DHTesp dht;

MQTTKalmanPublish mkTemp(mqttClient, BASE_TOPIC_STATUS "temp", MQTT_RETAINED, 12 * 2 /* every 2 min */, 0.2);
MQTTKalmanPublish mkHum(mqttClient, BASE_TOPIC_STATUS "hum", MQTT_RETAINED, 12 * 5 /* every 5 min */, 2);
MQTTKalmanPublish mkRssi(mqttClient, BASE_TOPIC_STATUS "rssi", MQTT_RETAINED, 12 * 5 /* every 5 min */, 10);

int lastConnected = 0;
bool on = false;
uint16_t mqttHue = 120; // green
float mqttSat = 1.0;
uint8_t mqttBri = 0;
bool ledsNeedUpdate = false;

void setup() {
	pinMode(LED_BUILTIN, OUTPUT);
	Serial.begin(115200);

	leds_setup();

	dht.setup(DHTPIN, DHTesp::DHT22);

	mqttClient.enableDebuggingMessages();
	mqttClient.enableHTTPWebUpdater();
	mqttClient.enableOTA();
	mqttClient.enableLastWillMessage(BASE_TOPIC "connected", "0", MQTT_RETAINED);
}

void onConnectionEstablished() {
	mqttClient.subscribe(BASE_TOPIC_SET "hue", [](const String &payload) {
		auto value = strtol(payload.c_str(), 0, 10);
		mqttHue = abs(value) % 360;
		ledsNeedUpdate = true;
		mqttClient.publish(BASE_TOPIC_STATUS "hue", String(mqttHue), MQTT_RETAINED);
	});

	mqttClient.subscribe(BASE_TOPIC_SET "sat", [](const String &payload) {
		auto value = strtof(payload.c_str(), 0) / 100.0f;
		if (!isfinite(value)) return;
		mqttSat = max(0.0f, min(1.0f, value));
		ledsNeedUpdate = true;
		mqttClient.publish(BASE_TOPIC_STATUS "sat", String(mqttSat * 100, 1), MQTT_RETAINED);
	});

	mqttClient.subscribe(BASE_TOPIC_SET "bri", [](const String &payload) {
		auto value = strtol(payload.c_str(), 0, 10);
		mqttBri = max(1l, min(50l, value)); // 255 is probably too dangerous with power consumption
		ledsNeedUpdate = true;
		mqttClient.publish(BASE_TOPIC_STATUS "bri", String(mqttBri), MQTT_RETAINED);
	});

	mqttClient.subscribe(BASE_TOPIC_SET "on", [](const String &payload) {
		on = payload == "1" || payload == "true";
		ledsNeedUpdate = true;
		mqttClient.publish(BASE_TOPIC_STATUS "on", String(on), MQTT_RETAINED);
	});

	mqttClient.publish(BASE_TOPIC "git-remote", GIT_REMOTE, MQTT_RETAINED);
	mqttClient.publish(BASE_TOPIC "git-version", GIT_VERSION, MQTT_RETAINED);
	mqttClient.publish(BASE_TOPIC "connected", "1", MQTT_RETAINED);
	lastConnected = 1;
}

void displayTime() {
	if (on) {
		auto tzTime = localtime_getDateTime();
		auto hour = tzTime.hour();
		auto minute = tzTime.minute();
		auto second = tzTime.second();

		Serial.printf(
				"displayTime %2d:%02d:%02d  at %3ld (ideal: %3ld)\n",
				hour, minute, second, millis() % 1000, referenceMillis % 1000);

		uint16_t hueArr[LED_COUNT];
		float satArr[LED_COUNT];
		float briArr[LED_COUNT];
		auto brightnessPercentage = mqttBri / 255.0f;
		for (int i = 0; i < LED_COUNT; i++) {
			hueArr[i] = mqttHue;
			satArr[i] = mqttSat;
			briArr[i] = brightnessPercentage;
		}

		// Hourly ticks
		const int HOUR_EVERY_N_LEDS = LED_COUNT / 12;
		for (int i = 0; i < 12; i++) {
			auto led = i * HOUR_EVERY_N_LEDS;
			if (led != second && led != minute) {
				briArr[led] *= i % 3 == 0 ? 4 : 2;
				satArr[led] *= 0.80;
			}
		}

		// clock hands
		uint8_t hourish = (hour * 5) + (minute / 12);
		briArr[(hourish + 58) % 60] = 0;
		briArr[(hourish + 59) % 60] = 0;
		briArr[hourish % 60] = 0;
		briArr[(hourish + 1) % 60] = 0;
		briArr[(hourish + 2) % 60] = 0;

		briArr[(minute + 59) % 60] = 0;
		briArr[minute] = 0;
		briArr[(minute + 1) % 60] = 0;

		hueArr[second] = (mqttHue + 180) % 360;
		satArr[second] = mqttSat;
		briArr[second] = brightnessPercentage;

		for (int i = 0; i < LED_COUNT; i++) {
			uint16_t pixel = (i + 43) % LED_COUNT;
			leds_set_hsv(pixel, hueArr[i], satArr[i], briArr[i]);
		}
	} else {
		leds_clear();
	}

	leds_show();
}

void loop() {
	mqttClient.loop();
	digitalWrite(LED_BUILTIN, mqttClient.isConnected() ? LED_BUILTIN_OFF : LED_BUILTIN_ON);

	static unsigned long nextMeasure = 0;
	if (millis() >= nextMeasure) {
		nextMeasure = millis() + 5000; // every 5 seconds

		float t = dht.getTemperature();
		float h = dht.getHumidity();

		bool readSuccessful = dht.getStatus() == DHTesp::ERROR_NONE;
		int nextConnected = readSuccessful ? 2 : 1;
		if (nextConnected != lastConnected && mqttClient.isConnected()) {
			bool successful = mqttClient.publish(BASE_TOPIC "connected", String(nextConnected), MQTT_RETAINED);
			if (successful) {
				Serial.printf("set /connected from %d to %d\n", lastConnected, nextConnected);
				lastConnected = nextConnected;
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

		if (mqttClient.isWifiConnected()) {
			long rssi = WiFi.RSSI();
			float avgRssi = mkRssi.addMeasurement(rssi);
			Serial.printf("RSSI        in     dBm: %5ld Average: %6.2f\n", rssi, avgRssi);
		}
	}

	if (mqttClient.isWifiConnected() && localtime_update()) {
		// update successful
		ledsNeedUpdate = true;
	}

	if (localtime_isKnown()) {
		if (ledsNeedUpdate) {
			displayTime();
			ledsNeedUpdate = false;
		} else {
			// If there is much time left, let others use it
			// Otherwise make sure the clock will be updated on time
			auto distance = localtime_millisUntilNextSecond();
			if (distance < 200) {
				delay(distance);
				displayTime();
			}
		}
	}
}

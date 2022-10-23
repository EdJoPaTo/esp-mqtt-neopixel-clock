#pragma once

#include <AceTimeClock.h>

using namespace ace_time;
using namespace ace_time::clock;

const acetime_t UPDATE_TIME_EVERY_SECONDS = 60 * 5; // 5 minutes

acetime_t epochSecondsOnUpdate = 0;
unsigned long referenceMillis = 0;
static NtpClock ntpClock("fritz.box");

bool localtime_isKnown() { return epochSecondsOnUpdate > 0; }

acetime_t localtime_getEpochSeconds()
{
	auto ago = millis() - referenceMillis;
	acetime_t epochSeconds = epochSecondsOnUpdate + (ago / 1000);
	return epochSeconds;
}

ZonedDateTime localtime_getDateTime()
{
	static BasicZoneProcessor berlinProcessor;
	static TimeZone tz = TimeZone::forZoneInfo(&zonedb::kZoneEurope_Berlin, &berlinProcessor);

	auto epochSeconds = localtime_getEpochSeconds();
	return ZonedDateTime::forEpochSeconds(epochSeconds, tz);
}

unsigned long localtime_millisUntilNextSecond()
{
	auto current = millis() % 1000;
	return (referenceMillis - current) % 1000;
}

bool localtime_updateNeeded()
{
	if (epochSecondsOnUpdate == 0)
		return true;

	if (millis() >= referenceMillis + UPDATE_TIME_EVERY_SECONDS * 1000)
		return true;

	auto epochSeconds = localtime_getEpochSeconds();

	// Update every 5 min -> update on 4:55 so 5:00 will be accurate
	if (epochSeconds % UPDATE_TIME_EVERY_SECONDS == UPDATE_TIME_EVERY_SECONDS - 5)
		return true;

	return false;
}

/// @brief updates the time
/// @return Successful time update
bool localtime_update()
{
	if (!ntpClock.isSetup())
		ntpClock.setup();

	auto start = millis();
	auto first = ntpClock.getNow();
	if (first == LocalDate::kInvalidEpochSeconds)
	{
		Serial.println("localtime_update failed first is invalid");
		return false;
	}

	acetime_t second;
	size_t attempt = 0;
	do
	{
		if (attempt++ > 50)
		{
			Serial.println("localtime_update failed second timed out");
			return false;
		}
		delay(20);
		second = ntpClock.getNow();
	} while (second <= first || second == LocalDate::kInvalidEpochSeconds);

	referenceMillis = millis();
	epochSecondsOnUpdate = second;

	auto took = referenceMillis - start;
	Serial.printf("localtime_update finished at %3ld and took %ldms\n", referenceMillis % 1000, took);
	return true;
}

#pragma once

#include <AceTimeClock.h>

using namespace ace_time;
using namespace ace_time::clock;

const acetime_t UPDATE_TIME_EVERY_SECONDS = 60 * 29; // 29 minutes

acetime_t epochSecondsOnUpdate = 0;
unsigned long referenceMillis = 0;

bool localtime_isKnown() { return referenceMillis > 0; }

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

/// @brief updates the time
/// @return Successful time update
bool localtime_update()
{
	// Only update when needed
	static unsigned long nextUpdateAttempt = 0;
	if (millis() < nextUpdateAttempt)
		return false;

	NtpClock ntpClock("fritz.box");
	ntpClock.setup();

	auto start = millis();
	auto first = ntpClock.getNow();
	if (first == LocalDate::kInvalidEpochSeconds)
	{
		Serial.println("localtime_update failed first is invalid");
		nextUpdateAttempt = millis() + 5000;
		return false;
	}

	acetime_t second;
	size_t attempt = 0;
	do
	{
		if (attempt++ > 50)
		{
			Serial.println("localtime_update failed second timed out");
			nextUpdateAttempt = millis() + 5000;
			return false;
		}
		delay(20);
		second = ntpClock.getNow();
	} while (second <= first || second == LocalDate::kInvalidEpochSeconds);

	referenceMillis = millis();
	epochSecondsOnUpdate = second;

	auto took = referenceMillis - start;
	Serial.printf("localtime_update finished at %3ld and took %ldms\n", referenceMillis % 1000, took);

	nextUpdateAttempt = referenceMillis + UPDATE_TIME_EVERY_SECONDS * 1000;

	return true;
}

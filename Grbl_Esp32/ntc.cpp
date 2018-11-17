#include "ntc.h"
#include <WiFi.h>
#include "time.h"

//https://github.com/espressif/arduino-esp32/blob/master/cores/esp32/esp32-hal-time.c

/*
  struct tm
  Time structure
  Structure containing a calendar date and time broken down into its components.
  The structure contains nine members of type int (in any order), which are:
  Member  Type  Meaning Range
  tm_sec  int seconds after the minute  0-60*
  tm_min  int minutes after the hour  0-59
  tm_hour int hours since midnight  0-23
  tm_mday int day of the month  1-31
  tm_mon  int months since January  0-11
  tm_year int years since 1900
  tm_wday int days since Sunday 0-6
  tm_yday int days since January 1  0-365
  tm_isdst  int Daylight Saving Time flag

*/


ntc::ntc()
{

}

void ntc::printLocalTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void ntc::initLocalTime()
{
  //Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  configTime(hourOffset * 3600, 3600, ntp_server);
  printLocalTime();

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

// plan bytes: 0 start minute, 1 start hour, 2 day of week mask, 3 duration in minutes
bool ntc::checkScheduler(uint8_t *plan)
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return false;
  }

  if (timeinfo.tm_sec == 0 && timeinfo.tm_min == plan[0] && timeinfo.tm_hour == plan[1] && (1 << timeinfo.tm_wday) & plan[2])
  {
    active = true;
    startMillis = millis();
    return true;
  }

  if (active)
  {
    currentMillis = millis();
    if (currentMillis - startMillis >= (unsigned long)plan[3] * 60000UL)
    {
      active = false;
    }
  }
  return active;

  //  if (!done && timeinfo.tm_sec == 0 && timeinfo.tm_min == plan[0] && timeinfo.tm_hour == plan[1] && (1 << timeinfo.tm_wday) & plan[2])
  //  {
  //    done = true;
  //    return true;
  //  }
  //  if (done && timeinfo.tm_sec == 1 && timeinfo.tm_min == plan[0] && timeinfo.tm_hour == plan[1] && (1<<timeinfo.tm_wday) & plan[2])
  //  {
  //    done = false;
  //  }

}

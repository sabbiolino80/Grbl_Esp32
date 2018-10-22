#include "ntc.h"
#include <WiFi.h>
#include "time.h"

ntc::ntc()
{
  
}

void ntc::printLocalTime()
{
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
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

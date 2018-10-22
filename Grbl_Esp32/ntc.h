#ifndef ntc_h
#define ntc_h

#include "Arduino.h"


class ntc{
  public:
    ntc();
    //~ntc();
    void printLocalTime();
    void initLocalTime();
  private:
    const char* ssid       = "BARALDI_EXT";
    const char* password   = "ambarabaciccicocco";
    const char* ntp_server = "pool.ntp.org";
    const float hourOffset = 1.0;
};


#endif

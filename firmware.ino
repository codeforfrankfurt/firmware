#define USE_GPS     0
#define USE_LORA    0

#include <TimerTCC0.h>
int sec = 0;

#ifdef USE_GPS
#include "TinyGPS++.h"
TinyGPSPlus gps;
boolean readGPS = false;
#endif

#if USE_LORA
#include <LoRaWan.h>

#define DEV_ADDR  "foo"
#define DEV_EUI   "foo"
#define APP_EUI   "foo"

#define NWK_S_KEY "foo"

#define APP_S_KEY "foo"
#define APP_KEY   "foo"


const float EU_channels[8] =    {868.1, 868.3, 868.5, 867.1, 867.3, 867.5, 867.7, 867.9}; //rx 869.525
#define UPLINK_DATA_RATE_MIN    DR0 //The min uplink data rate for all countries / plans is DR0
#define UPLINK_DATA_RATE_MAX_EU DR5
#define MAX_EIRP_NDX_EU         5
#define DOWNLINK_DATA_RATE_EU   DR3
#define FREQ_RX_WNDW_SCND_EU    869.525

#define DEFAULT_RESPONSE_TIMEOUT 5
unsigned char frame_counter = 1;
char buffer[256];
#endif



void setup() {
    SerialUSB.begin(115200);
    while(!SerialUSB);
        
#if USE_GPS
    char c;
    bool locked;

    Serial.begin(9600);     // open the GPS
    locked = false;

    // For S&G, let's get the GPS fix now, before we start running arbitary
    // delays for the LoRa section

    while (!gps.location.isValid()) {
      while (Serial.available() > 0) {
        if (gps.encode(c=Serial.read())) {
          displayGPSInfo();
          if (gps.location.isValid()) {
//            locked = true;
            break;
          }
        }
//        SerialUSB.print(c);
      }

//      if (locked)
//        break;

      if (millis() > 15000 && gps.charsProcessed() < 10)
      {
        SerialUSB.println(F("No GPS detected: check wiring."));
        SerialUSB.println(gps.charsProcessed());
        while(true);
      } 
      else if (millis() > 20000) {
        SerialUSB.println(F("Not able to get a fix in alloted time."));     
        break;
      }
    }
#endif

#if USE_LORA
    lora.init();
    lora.setDeviceDefault();    

    memset(buffer, 0, 256);
    lora.getVersion(buffer, 256, 1);
    SerialUSB.print(buffer); 

    memset(buffer, 0, 256);
    lora.getId(buffer, 256, 1);
    SerialUSB.print(buffer);
    
    lora.setId(DEV_ADDR, DEV_EUI, APP_EUI);
    lora.setKey(NWK_S_KEY, APP_S_KEY, APP_KEY);

    lora.setDeciveMode(LWABP);
    lora.setDataRate(DR0, EU868);   // 0 = SF12 / 125 kHz / 440bps
                                    // 1 = SF11 / 125 kHz / 440bps
    lora.setPower(7);               // 2 = Max EIRP –  4dB
                                    // 5 = Max EIRP - 10dB - MAX_EIRP_NDX_EU_433
                                    // 7 = Max EIRP - 14dB - MAX_EIRP_NDX_EU_863

    setChannelsForTTN(EU_channels);
    lora.setReceiceWindowFirst(7, 867.9);
    lora.setReceiceWindowSecond(FREQ_RX_WNDW_SCND_EU, DOWNLINK_DATA_RATE_EU);
   
//    lora.setDutyCycle(false);
//    lora.setJoinDutyCycle(false);
#endif

    //TimerTcc0.initialize(60000000); 1 Minute
    TimerTcc0.initialize(1000000); //1 second
    TimerTcc0.attachInterrupt(timerIsr);
}

//interrupt routine
void timerIsr(void) {
    sec = (sec + 1) % 6;   
    SerialUSB.println(sec);

    if (sec == 1) {
      Serial.write("h"); //Turn on GPS
    }
    if (sec == 3) {
      readGPS = true;
    }
    if (sec == 5) {
      displayGPSInfo();
      readGPS = false;
      Serial.write("$PMTK161,0*28\r\n");
    }
}


void loop(void) {
#if USE_GPS
  if(readGPS) {
      while (Serial.available() > 0){
          char currChar = Serial.read();
          gps.encode(currChar);
      }
  }
#endif
}

void displayGPSInfo()
{
  SerialUSB.print(F("Location: ")); 
  if (gps.location.isValid())
  {
    SerialUSB.print(gps.location.lat(), 6);
    SerialUSB.print(F(","));
    SerialUSB.print(gps.location.lng(), 6);
  }
  else
  {
    SerialUSB.print(F("INVALID"));
  }

  SerialUSB.print(F("  Date/Time: "));
  if (gps.date.isValid())
  {
    SerialUSB.print(gps.date.month());
    SerialUSB.print(F("/"));
    SerialUSB.print(gps.date.day());
    SerialUSB.print(F("/"));
    SerialUSB.print(gps.date.year());
  }
  else
  {
    SerialUSB.print(F("INVALID"));
  }

  SerialUSB.print(F(" "));
  if (gps.time.isValid())
  {
    if (gps.time.hour() < 10) SerialUSB.print(F("0"));
    SerialUSB.print(gps.time.hour());
    SerialUSB.print(F(":"));
    if (gps.time.minute() < 10) SerialUSB.print(F("0"));
    SerialUSB.print(gps.time.minute());
    SerialUSB.print(F(":"));
    if (gps.time.second() < 10) SerialUSB.print(F("0"));
    SerialUSB.print(gps.time.second());
    SerialUSB.print(F("."));
    if (gps.time.centisecond() < 10) SerialUSB.print(F("0"));
    SerialUSB.print(gps.time.centisecond());
  }
  else
  {
    SerialUSB.print(F("INVALID"));
  }

  SerialUSB.println();
}

#define USE_GPS     0
#define USE_LORA    0
#define USE_SDS011  1

#include <TimerTCC0.h>
int sec = 0;
int secSinceStart = 0;

#if USE_SDS011
#include "SDS011.h"
SDS011 sdsSensor;
#define SDS_PIN_TX 5
#define SDS_PIN_RX 6
boolean readSDS;
float pm25;
float pm10;
int sdsErrorCode;
#endif

#if USE_GPS
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
unsigned int frame_counter = 1;
char buffer[256];
boolean sendPacket = false;
#endif



void setup() {
    SerialUSB.begin(115200);
    while(!SerialUSB);

#if USE_SDS011
    sdsSensor.begin(SDS_PIN_RX, SDS_PIN_TX);
#endif

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
                                    // 2 = Max EIRP –  4dB
                                    // 5 = Max EIRP - 10dB - MAX_EIRP_NDX_EU_433
                                    // 7 = Max EIRP - 14dB - MAX_EIRP_NDX_EU_863

    // power is signal strength as well, might override above data rate setting
    lora.setPower(15);

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
    secSinceStart += 1;

#if USE_LORA
    // every minute
    if (secSinceStart % 60 == 0) {
      SerialUSB.println("LORA sendPacket=true");
      sendPacket = true;
    } else {
      sendPacket = false;
    }
#endif

    // sec is forced to a range of 0-5, so we can decide to do something in either of those seconds
    sec = (sec + 1) % 6;   
    SerialUSB.println(sec);

#if USE_GPS
    if (sec == 1) {
      Serial.write("h"); //Turn on GPS
    }
#endif
#if USE_SDS011
    if (sec == 2) {
      readSDS = true;
    }
#endif
#if USE_GPS
    if (sec == 3) {
      readGPS = true;
    }
#endif
    if (sec == 5) {
#if USE_SDS011
      readSDS = false;
#endif
#if USE_GPS
      displayGPSInfo();
      readGPS = false;
      Serial.write("$PMTK161,0*28\r\n");
#endif
    }
}


void loop(void) {
#if USE_SDS011
  if(readSDS) {
    // return code is 0, if new values were read, and 1 if there were no new values.
    sdsErrorCode = sdsSensor.read(&pm25, &pm10);
    if (!sdsErrorCode) {
        SerialUSB.println("PM2.5: " + String(pm25));
        SerialUSB.println("PM10:  " + String(pm10));
    } else {
        SerialUSB.print(F("SDS Error Code: "));
        SerialUSB.println(sdsErrorCode);
    }
    readSDS = false;
  }
#endif

#if USE_GPS
  if(readGPS) {
      while (Serial.available() > 0){
          char currChar = Serial.read();
          gps.encode(currChar);
      }
  }
#endif

#if USE_LORA
  if(sendPacket) {
    SerialUSB.print("sendAndReceiveLoRaPacket(): ");
    SerialUSB.println(frame_counter);
    sendAndReceiveLoRaPacket();
    frame_counter += 1;
  }
#endif
}

#if USE_GPS
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
#endif

#if USE_LORA
void setChannelsForTTN(const float* channels) {
    for(int i = 0; i < 8; i++){
        // DR0 is the min data rate
        // UPLINK_DATA_RATE_MAX_EU = DR5 is the max data rate for the EU
        if(channels[i] != 0){
          lora.setChannel(i, channels[i], UPLINK_DATA_RATE_MIN, UPLINK_DATA_RATE_MAX_EU);
        }
    }
}

void sendAndReceiveLoRaPacket() {
    bool result = false;
    char payload[256];
    memset(payload, 0, 256);
    String(frame_counter).toCharArray(payload, 256);

    result = lora.transferPacket(payload, 10);

    if(result) {
        short length;
        short rssi;

        memset(buffer, 0, 256);
        length = lora.receivePacket(buffer, 256, &rssi);

        if(length) {
            SerialUSB.print("Length is: ");
            SerialUSB.println(length);
            SerialUSB.print("RSSI is: ");
            SerialUSB.println(rssi);
            SerialUSB.print("Data is: ");
            for(unsigned char i = 0; i < length; i ++) {
                SerialUSB.print("0x");
                SerialUSB.print(buffer[i], HEX);
                SerialUSB.print(" ");
            }
            SerialUSB.println();
        }
    }
}
#endif

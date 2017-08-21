/*
 * The file is Licensed under the Apache License, Version 2.0
 * (c) 2017 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 */

#include "PinMap.h"
#include <arduino-mbed.h>
#include <sx1276-mbed-hal.h>
#include <RadioShuttle.h>
#include <RadioStatus.h>
#include <RadioSecurity.h>
#include <arduino-util.h>
#include <RTCZero.h>

#include "PM.h"


#ifdef FEATURE_LORA


// #define RADIO_SERVER  1

#ifdef RADIO_SERVER
bool server = true;
#else
bool server = false;
#endif
bool usePassword = false; // password the can used indepenend of AES
bool useAES = false;      // AES needs the usePassword option on

enum SensorsIDs { // Must be unique world wide.
    myPMSensorApp = 10,
#ifdef RADIO_SERVER
    myDeviceID = 1,
    myCode = 0x20EE91D6
    remoteDeviceID = 9,
#else
    myDeviceID = 9,
    //myCode = 0x20EE91DE, // Atmel Board
    myCode = 0x112B92ED, //Board r6.3 green pcb, red tactile
    // myCode = 0x194F6298, //Board r6.3 green pcb, black tactile
    remoteDeviceID = 1,
#endif
};

unsigned char samplePassword[] = { "RadioShuttleFly" };

/*
 * For details review: SX1276GenericLib/sx1276/sx1276.h
 * Supported spreading factors SF 7,8, 9, 10, 11, (12 does not work well)
 * Working frequencies using the 125000 bandwidth which leaves
 * sufficient distance to the neighbour channel
 * EU: 868.1, 868.3, 868.5 (Default LoRaWAN EU channels)
 * EU: 865.1, 865.3, 865.5, 865.7, 865.9 (additional channels)
 * EU: 866.1, 866.3, 866.5, 866.7, 866.9 (additional channels)
 * EU: 867.1, 867.3, 867.5, 867.7, 867.9 (additional channels)
 * Utilisation of these channels should not exceed 1% per hour per node
 * Bandwidth changes other than 125k requires different channels distances
 */
const RadioShuttle::RadioProfile myProfile[] =  {
    /*
     * Our default profile
     * frequency, bandwidth, TX power, spreading factor
     */
    { 868100000, 125000, 14, 7 },
    { 0, 0, 0, 0 },
};


struct sensor {
      uint8_t  version;
      uint8_t  padding;
      uint16_t pm25;
      uint16_t pm10;
      uint16_t id;
} PMAppData;


void PMSensorRecvHandler(int AppID, RadioShuttle::devid_t stationID, int msgID, int status, void *buffer, int length)
{
    switch(status) {
        case RadioShuttle::MS_SentCompleted:  // A SendMsg has been sent.
            dprintf("MSG_SentCompleted: id=%d  %d bytes", msgID, length);
            break;
        case RadioShuttle::MS_SentCompletedConfirmed:// A SendMsg has been sent and confirmed
            dprintf("MSG_SentCompletedConfirmed: id=%d %d bytes", msgID, length);
            break;
        case RadioShuttle::MS_SentTimeout:    // A timeout occurred, number of retires exceeded
            dprintf("MSG_SentTimeout ID: %d", msgID);
            break;

        case RadioShuttle::MS_RecvData:     // a simple input message
            dprintf("MSG_RecvData ID: %d, len=%d", msgID, length);
            // dump("MSG_RecvData", buffer, length);
            {
              struct sensor *p = (sensor *)buffer;
              if (length == sizeof(struct sensor) && p->version == 1) {
                dprintf("ParticalData: PM10: %.1f (μg/m3)   PM2.5: %.1f (μg/m3)    ID: %d", (float)p->pm10 / 10.0, (float)p->pm25 / 10.0, p->id);
              }
            }
            // dump("MSG_RecvData", buffer, length);
            break;
        case RadioShuttle::MS_RecvDataConfirmed:  // received a confirmed message
            dprintf("MSG_RecvDataConfirmed ID: %d, len=%d", msgID, length);
            // dump("MSG_RecvDataConfirmed", buffer, length);
            break;
        case RadioShuttle::MS_NoStationFound:
            dprintf("MSG_NoStationFound");
            break;
        case RadioShuttle::MS_NoStationSupportsApp:
            dprintf("MSG_NoStationSupportsApp");
            break;
        case RadioShuttle::MS_AuthenicationRequired: // the password does not match.
            dprintf("MSG_AuthenicationRequired");
            break;

        case RadioShuttle::MS_StationConnected: // a confirmation that the connection was accepted
            dprintf("MSG_StationConnected");
            break;
        case RadioShuttle::MS_StationDisconnected:  // a confirmation that the disconnect was accepted
            dprintf("MSG_StationDisconnected");
            break;
        default:
            break;
    }
}


DigitalOut led(LED);
#ifdef BOOSTER_EN50
DigitalOut boost50(BOOSTER_EN50);
#endif
#ifdef BOOSTER_EN33
DigitalOut boost33(BOOSTER_EN33);
#endif
InterruptIn intr(SW0);
volatile int pressedCount = 0;

RTCZero rtc;

uint32_t lastPMReadTime;
const int PM_READ_INTERVAL = 60; // 60*30 secs, ususally it can be every 30 minutes


void SwitchInput(void) {
   dprintf("SwitchInput");
   led = !led;
   pressedCount++;
}

void alarmMatch()
{
  int interval = 5;
  int secs = rtc.getSeconds() + interval;
  if (secs >= 58)
    secs = interval;
  rtc.setAlarmSeconds(secs);
  rtc.enableAlarm(rtc.MATCH_SS);
}

Radio *radio;
RadioShuttle *rs;
PMSensor pm;

void setup() {
  MYSERIAL.begin(230400);
  InitSerial(&MYSERIAL, 3000); // wait 2000ms that the Serial Monitor opens, otherwise turn off USB.
  SPI.begin();
  rtc.begin();

  rtc.setTime(00, 00, 00);
  rtc.setDate(00, 00, 17);
  rtc.attachInterrupt(alarmMatch);
  alarmMatch();

  dprintf("USBStatus: %s", SerialUSB_active == true ? "SerialUSB_active" : "SerialUSB_disbaled");
  if (!SerialUSB_active) {
      for (int i = 0; i < 10; i++) { // lets link the LED to show that SerialUSB is off.
        led = 1;
        delay(80);
        led = 0;
        delay(80);        
      }
  }

  led = 1;
  intr.mode(PullUp);
  intr.fall(callback(&SwitchInput));

  pm.SensorInit(&Serial1);
  lastPMReadTime = rtc.getEpoch();

#if 0 // enable this to test the sensor
  boost50 = 1;
  while(true)
    pm.ReadRecord();
#endif

  RSCode err;
  RadioStatusInterface *statusIntf = NULL;
  RadioSecurityInterface *securityIntf = NULL;

  // RFM95
  radio = new SX1276Generic(NULL, RFM95_SX1276,
            LORA_SPI_MOSI, LORA_SPI_MISO, LORA_SPI_SCLK, LORA_CS, LORA_RESET,
            LORA_DIO0, LORA_DIO1, LORA_DIO2, LORA_DIO3, LORA_DIO4, LORA_DIO5);
 
  statusIntf = new MyRadioStatus();

  securityIntf = new RadioSecurity();

  rs = new RadioShuttle("MyPMSensorRadio");

  rs->EnablePacketTrace(RadioShuttle::DEV_ID_ANY, true, true);
  
  err = rs->AddLicense(myDeviceID, myCode);
  if (err)
    return;  

  err = rs->AddRadio(radio, MODEM_LORA, myProfile);
  if (err)
    return;
  rs->AddRadioStatus(statusIntf);
  if (err)
    return;
  rs->AddRadioSecurity(securityIntf);
  if (err)
    return;
  /*
   * The password parameter can be skipped if no password is required
   */
   if (usePassword) {
    err = rs->RegisterApplication(myPMSensorApp, &PMSensorRecvHandler, samplePassword, sizeof(samplePassword)-1);
   } else {
    err = rs->RegisterApplication(myPMSensorApp, &PMSensorRecvHandler);
   }

  if (err)
    return;
  if (server) {
    err = rs->Startup(RadioShuttle::RS_Station_Basic);
    dprintf("Startup as a Server: Station_Basic ID=%d", myDeviceID);
  } else {
    //err = rs->Startup(RadioShuttle::RS_Node_Online/*RadioShuttle::RS_Node_Offline*/);
    err = rs->Startup(RadioShuttle::RS_Node_Offline);
    dprintf("Startup as a Node: RS_Node_Offline ID=%d", myDeviceID);
    if (rs->AppRequiresAuthentication(myPMSensorApp) == RS_PasswordSet) {
      err = rs->Connect(myPMSensorApp, remoteDeviceID);
    }
  }
  if (err)
    return;
 }


void loop() {
  static int cnt = 0;

  uint32_t rtctime = rtc.getEpoch();
  if (!server && rtctime > (lastPMReadTime + PM_READ_INTERVAL)) {
      lastPMReadTime = rtctime;
      cnt++; // it is time to read the sensor data.
  }

  if (cnt != pressedCount) {
      int flags = 0;
      // flags |= RadioShuttle::MF_NeedsConfirm; // optional
      if (usePassword && useAES)
        flags |= RadioShuttle::MF_Encrypted; // optional not needed for PMSensor data

      if (server) {
        rs->SendMsg(myPMSensorApp, NULL, 0, flags, remoteDeviceID);
      } else {
#ifdef BOOSTER_EN50
        boost50 = 1;     
#endif
        uint32_t start = s_getTicker();
        bool gotData = false;
        while (s_getTicker() < (start+pm.getWarmUpSeconds())) {
          if (pm.ReadRecord())
            gotData = true;
       }
#ifdef BOOSTER_EN50
        boost50 = 0;
#endif
        if (gotData) {
          memset(&PMAppData, 0, sizeof(PMAppData));
          PMAppData.version = 1;
          PMAppData.pm25 = pm.getPM25();
          PMAppData.pm10 = pm.getPM10();
          PMAppData.id = pm.getPMid();
          rs->SendMsg(myPMSensorApp, &PMAppData, sizeof(PMAppData), flags, remoteDeviceID);
        }
     }
    cnt = pressedCount;
  }

  led = 0;    
  if (!SerialUSB_active && rs->Idle() && rs->GetRadioType() == RadioShuttle::RS_Node_Offline) {
    /*
     * In deepsleep() the CPU is turned off, lowest power mode.
     * we receive a wakeup every 5 seconds to allow to work
     * in sleep the SW0 handler gets called.
     * in deepsleep we need to check the state of the SW0 switch 
     * and call the SwitchInput handler manually.
     */
    deepsleep();
    if (digitalRead(SW0) == LOW) {
      SwitchInput();
    }
  } else {
    sleep();  // timer and radio interrupts will wakeup us
  }
  led = 1;
  rs->RunShuttle(); // process all pending events
}

#endif // FEATURE_LORA

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


#define CHECK_ERROR_RET(func, err) { \
    if (err) { \
      dprintf("Error in %s: %s", func, rs->StrError(err)); \
      return err; \
    } \
  }

// #define RADIO_SERVER  1

#ifdef RADIO_SERVER
bool server = true;
#else
bool server = false;
#endif
bool usePassword = false; // password the can used indepenend of AES
bool useAES = false;      // AES needs the usePassword option on
bool useNodeOffline = false;  // when idle turns the radio off and enters deelsleep

enum SensorsIDs { // Must be unique world wide.
  myPMSensorApp = 10,
#ifdef RADIO_SERVER
  myDeviceID = 1,
  myCode = 0x20EE91D6
  remoteDeviceID = 9,
#else
  myDeviceID = 9,
  myCode = 0x112B92ED, //Board r6.3 green pcb, red tactile
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
   * frequency, bandwidth, TX power, spreading factor, frequency-offset
   */
  { 868100000, 125000, 14, 7, 0 },
  { 0, 0, 0, 0, 0 },
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
  switch (status) {
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
          String pm10 =  String((float)p->pm10 / 10.0, 1);
          String pm25 =  String((float)p->pm25 / 10.0, 1);
          dprintf("ParticalData: PM10: %s (μg/m3)   PM2.5: %s (μg/m3)    ID: %d", pm10.c_str(), pm25.c_str(), p->id);
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
  static uint32_t lastInterrupt = 0;
  uint32_t ticks_ms = ms_getTicker();
  if (!lastInterrupt || ticks_ms > (lastInterrupt + 300)) { // debounce 300ms.
    dprintf("SwitchInput");
    led = !led;
    pressedCount++;
    lastInterrupt = ticks_ms;
  }
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


Radio *radio;                         // the LoRa network interface
RadioShuttle *rs;                     // the RadioShutlle protocol
RadioStatusInterface *statusIntf;     // the optional status interface
RadioSecurityInterface *securityIntf; // the optional security interface
PMSensor pm;

int InitRadio()
{
  RSCode err;

  // RFM95
  radio = new SX1276Generic(NULL, RFM95_SX1276,
                            LORA_SPI_MOSI, LORA_SPI_MISO, LORA_SPI_SCLK, LORA_CS, LORA_RESET,
                            LORA_DIO0, LORA_DIO1, LORA_DIO2, LORA_DIO3, LORA_DIO4, LORA_DIO5);

  statusIntf = new MyRadioStatus();

  securityIntf = new RadioSecurity();

  rs = new RadioShuttle("MyRadioShuttle");

  rs->EnablePacketTrace(RadioShuttle::DEV_ID_ANY, true, true);

  err = rs->AddLicense(myDeviceID, myCode);
  CHECK_ERROR_RET("AddLicense", err);

  err = rs->AddRadio(radio, MODEM_LORA, myProfile);
  CHECK_ERROR_RET("AddRadio", err);
  dprintf("Radio: %d Hz, SF%d, %d kHz", myProfile[0].Frequency, myProfile[0].SpreadingFaktor, myProfile[0].Bandwidth / 1000);

  rs->AddRadioStatus(statusIntf);
  CHECK_ERROR_RET("AddRadioStatus", err);

  rs->AddRadioSecurity(securityIntf);
  CHECK_ERROR_RET("AddRadioSecurity", err);

  /*
   *  The password parameter can be skipped if no password is required
   */
  if (usePassword) {
    err = rs->RegisterApplication(myPMSensorApp, &PMSensorRecvHandler, samplePassword, sizeof(samplePassword) - 1);
  } else {
    err = rs->RegisterApplication(myPMSensorApp, &PMSensorRecvHandler);
  }
  CHECK_ERROR_RET("RegisterApplication", err);

  if (server) {
    err = rs->Startup(RadioShuttle::RS_Station_Basic);
    dprintf("Startup as a Server: Station_Basic ID=%d", myDeviceID);
  } else {
    if (useNodeOffline) {
      err = rs->Startup(RadioShuttle::RS_Node_Offline);
      dprintf("Startup as a Node: RS_Node_Offline ID=%d", myDeviceID);
    } else {
      err = rs->Startup(RadioShuttle::RS_Node_Online);
      dprintf("Startup as a Node: RS_Node_Online ID=%d", myDeviceID);
    }
    if (!err && rs->AppRequiresAuthentication(myPMSensorApp) == RS_PasswordSet) {
      err = rs->Connect(myPMSensorApp, remoteDeviceID);
    }
  }
  CHECK_ERROR_RET("Startup", err);
  return 0;
}


void DeInitRadio()
{
  if (securityIntf) {
    delete securityIntf;
    securityIntf = NULL;
  }
  if (statusIntf) {
    delete statusIntf;
    statusIntf = NULL;
  }
  if (rs) {
    delete rs;
    rs = NULL;
  }
  if (radio) {
    delete radio;
    radio = NULL;
  }
}


void setup() {
  intr.mode(PullUp);
  MYSERIAL.begin(230400);
  InitSerial(&MYSERIAL, 5000, &led, intr.read()); // wait 5000ms that the Serial Monitor opens, otherwise turn off USB.
  SPI.begin();
  rtc.begin();
  rtc.attachInterrupt(alarmMatch);
  alarmMatch();
  if (rtc.getYear() == 0)
    rtc.setDate(01, 01, 17);
  dprintf("RTC Clock: %d/%d/%d %02d:%02d:%02d", rtc.getDay(), rtc.getMonth(), rtc.getYear() + 2000, rtc.getHours(), rtc.getMinutes(), rtc.getSeconds());

#ifdef BOOSTER_EN50
  boost50 = 0;
  boost33 = 0;
#endif

  led = 1;
  if (!SerialUSB_active && useNodeOffline)
    intr.low(callback(&SwitchInput)); // in deepsleep only low/high are supported.
  else
    intr.fall(callback(&SwitchInput));
  dprintf("Welcome to RadioShuttle - PMSensor");

  pm.SensorInit(&Serial1);        // Serial1 = D0(RX), D1(TX)
  lastPMReadTime = rtc.getEpoch();

#if 0 // enable this to test the sensor
  boost50 = 1;
  while (true)
    pm.ReadRecord();
#endif

  if (InitRadio() != 0)
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
      while (s_getTicker() < (start + pm.getWarmUpSeconds())) {
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
     */
    deepsleep();
  } else {
    sleep();  // timer and radio interrupts will wakeup us
  }
  led = 1;
  rs->RunShuttle(); // process all pending events
}

#endif // FEATURE_LORA

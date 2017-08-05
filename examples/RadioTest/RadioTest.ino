/*
 * The file is Licensed under the Apache License, Version 2.0
 * (c) 2017 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 */

#include "PinMap.h"
#include "arduino-mbed.h"
#include "sx1276-mbed-hal.h"
#include "RadioShuttle.h"
#include "RadioStatus.h"
#include "RadioSecurity.h"
#include "arduino-util.h"

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
    myTempSensorApp = 0x0001,
#ifdef RADIO_SERVER
    myDeviceD = 1,
    myCode = 0x20EE91D6
    remoteDeviceID = 9,
#else
    myDeviceD = 9,
    myCode = 0x20EE91DE,
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


void TempSensorRecvHandler(int AppID, RadioShuttle::devid_t stationID, int msgID, int status, void *buffer, int length)
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
InterruptIn intr(SW0);
volatile int pressedCount = 0;

void SwitchInput(void) {
   dprintf("SwitchInput");  
   led = !led;
   pressedCount++;
}

Radio *radio;
RadioShuttle *rs;

void setup() {
  MYSERIAL.begin(230400);
  InitSerial(&MYSERIAL);
  SPI.begin();

  led = 0;
  intr.mode(PullUp);
  intr.fall(callback(&SwitchInput));
  
  dprintf("MyRadioShuttle");
  dump("MyDump", "Hello World\r\n", sizeof("Hello World\r\n")-1);


  RSCode err;
  RadioStatusInterface *statusIntf = NULL;
  RadioSecurityInterface *securityIntf = NULL;

  // RFM95
  radio = new SX1276Generic(NULL, RFM95_SX1276,
            LORA_SPI_MOSI, LORA_SPI_MISO, LORA_SPI_SCLK, LORA_CS, LORA_RESET,
            LORA_DIO0, LORA_DIO1, LORA_DIO2, LORA_DIO3, LORA_DIO4, LORA_DIO5);
 
  statusIntf = new MyRadioStatus();

  securityIntf = new RadioSecurity();

  rs = new RadioShuttle("MyRadioShuttle");

  rs->EnablePacketTrace(RadioShuttle::DEV_ID_ANY, true, true);
  
  err = rs->AddLicense(myDeviceD, myCode);
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
    err = rs->RegisterApplication(myTempSensorApp, &TempSensorRecvHandler, samplePassword, sizeof(samplePassword)-1);
   } else {
    err = rs->RegisterApplication(myTempSensorApp, &TempSensorRecvHandler);
   }

  if (err)
    return;
  if (server) {
    err = rs->Startup(RadioShuttle::RS_Station_Basic);
    dprintf("Startup as a Server: Station_Basic ID=%d", myDeviceD);
  } else {
    err = rs->Startup(RadioShuttle::RS_Node_Online/*RadioShuttle::RS_Node_Offline*/);
    dprintf("Startup as a Node: RS_Node_Online ID=%d", myDeviceD);
    if (rs->AppRequiresAuthentication(myTempSensorApp) == RS_PasswordSet) {
      err = rs->Connect(myTempSensorApp, remoteDeviceID);
    }
  }
  if (err)
    return;
 }


void loop() {
  static int cnt = 0;
  led = !led;

  if (cnt != pressedCount) {
    if (cnt > 0) {
      int flags = 0;
      flags |= RadioShuttle::MF_NeedsConfirm; // optional 
      if (usePassword && useAES)
        flags |= RadioShuttle::MF_Encrypted;

      if (server) {
        static char msg[] = "The server feels very good today";
        rs->SendMsg(myTempSensorApp, msg, sizeof(msg), flags, remoteDeviceID);
      } else {
        static char msg[] = "Hello, the temperature is 26 celsius";
        rs->SendMsg(myTempSensorApp, msg, sizeof(msg), flags, remoteDeviceID);
     }
    }
    cnt = pressedCount;
  }

  if (rs->GetRadioType() == RadioShuttle::RS_Node_Offline) {
    sleep();// deepsleep(); // CPU is turned off lowest power mode;
  } else {
    sleep();  // timer and radio interrupts will wakeup us
  }
  rs->RunShuttle(); // process all pending events
}

#endif // FEATURE_LORA

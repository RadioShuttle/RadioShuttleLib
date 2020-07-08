/*
 * The file is Licensed under the Apache License, Version 2.0
 * (c) 2017 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 */


 /*
  * When the USE_DEMOBOARD_PAIR is disabled the following properties must be set
  * once using the PropertyEditor (see Examples->Arduino-mbed-APIs):
  * For LongRa boards only (already factory preset on the ECO Power boards):
  *   LORA_DEVICE_ID (if not set, see on back of PCB, e.g. s10=123 (for ID 123)
  *   LORA_CODE_ID   (if not set, see on back of PCB, e.g. s11=0x6b17e559
  * Always:
  * LORA_RADIO_TYPE 
  *   - for the server s14=4 (RS_Station_Basic)
  *   - for the client s14=3 (RS_Node_Online) or s14=1 (RS_Node_Offline)
  * LORA_REMOTE_ID, e.g.: s12=123 (replace 123 with your remote board ID)
  * Optionally:
  * LORA_APP_PWD, e.g.: s20=Hello (must be identical for all boards using this app)
  * The password will only allow clients to communicate with the same password
  * For AES128-bit content encryption, in addition to a password, set useAES = true (in line 46)
  */

#include "PinMap.h"
#include <arduino-mbed.h>
#include <sx1276-mbed-hal.h>
#include <RadioShuttle.h>
#include <RadioStatus.h>
#include <RadioSecurity.h>
#include <arduino-util.h>


#ifdef FEATURE_LORA


#define CHECK_ERROR_RET(func, err) { \
    if (err) { \
      dprintf("Error in %s: %s", func, rs->StrError(err)); \
      return err; \
    } \
  }


bool server;                  // automatically being set if radioTypeMode RadioShuttle::RS_Station_Basic
bool useAES = false;          // AES needs the usePassword option on

#define myTempSensorApp 0x0001 // Must be unique world wide.
int myDeviceID;
int myCode;
int remoteDeviceID;
RadioShuttle::RadioType radioTypeMode;  // 1 = RS_Node_Offline, 3 = RS_Node_Online, 4 = RS_Station_Basic
const char *appPassword;

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
 * The last entry is a Freqency
 */
RadioShuttle::RadioProfile myProfile[] =  {
  /*
   * Our default profile
   * frequency, bandwidth, TX power, spreading factor, frequency-offset
   */
  { 868100000, 125000, 14, 7, 0},
  { 0, 0, 0, 0, 0 },
};


void TempSensorRecvHandler(int AppID, RadioShuttle::devid_t stationID, int msgID, int status, void *buffer, int length)
{
  UNUSED(stationID);
  UNUSED(buffer);
  UNUSED(AppID);

  switch (status) {
    case RadioShuttle::MS_SentCompleted:  // A SendMsg has been sent.
      dprintf("MSG_SentCompleted: id=%d  %d bytes", msgID, length);
      break;
    case RadioShuttle::MS_SentCompletedConfirmed:// A SendMsg has been sent and confirmed
      dprintf("MSG_SentCompletedConfirmed: id=%d %d bytes", msgID, length);
      break;
    case RadioShuttle::MS_SentTimeout:    // A timeout occurred, number of retries exceeded
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

/*
 * DigitalOut is a simple C++ wrapper around the GPIO ports (compatible with mbed.org).
 * It is easy to use and easier to understand, it allows reading and setting the object.
 * DigitalOut, DigitalIn and DigitalInOut are available.
 */
DigitalOut led(LED);

/*
 * InterruptIn allows input registration for a specific pin.
 * intr.mode() "PullUp" or "PullDown" is supported.
 * intr.fall, intr.rise, high, low allows registering an interrupt
 * handler to be called upon a pin change event.
 * A callback() wrapper is required to provide C or C++ handlers
 * (compatible with mbed.org).
 */
InterruptIn intr(SW0);
volatile int pressedCount = 0;

void SwitchInput(void) {
  dprintf("SwitchInput");
  led = !led;
  pressedCount++;
}


Radio *radio;                         // the LoRa network interface
RadioShuttle *rs;                     // the RadioShuttle protocol
RadioStatusInterface *statusIntf;     // the optional status interface
RadioSecurityInterface *securityIntf; // the optional security interface

int InitRadio()
{
  RSCode err;

#if defined(ARDUINO_HELTEC_WIFI_LORA_32_V2) || defined(ARDUINO_WIRELESS_STICK) || defined(ARDUINO_WIRELESS_STICK_LITE)
  // Vext power switch active
  radio = new SX1276Generic(NULL, HELTEC_L4_1276,
                            LORA_SPI_MOSI, LORA_SPI_MISO, LORA_SPI_SCLK, LORA_CS, LORA_RESET,
                            LORA_DIO0, LORA_DIO1, LORA_DIO2, LORA_DIO3, LORA_DIO4, LORA_DIO5, LORA_ANT_PWR);
#else
  // RFM95 compatible
  radio = new SX1276Generic(NULL, RFM95_SX1276,
                            LORA_SPI_MOSI, LORA_SPI_MISO, LORA_SPI_SCLK, LORA_CS, LORA_RESET,
                            LORA_DIO0, LORA_DIO1, LORA_DIO2, LORA_DIO3, LORA_DIO4, LORA_DIO5);
#endif

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
   * The password parameter can be NULL if no password is required
   */
  err = rs->RegisterApplication(myTempSensorApp, &TempSensorRecvHandler, (void *)appPassword);
 
  CHECK_ERROR_RET("RegisterApplication", err);

  if (server) {
    // usually RadioShuttle::RS_Station_Basic, set via properties
    err = rs->Startup(radioTypeMode);
    dprintf("Startup as a Server: %s ID=%d", rs->GetRadioName(rs->GetRadioType()), myDeviceID);
  } else {
    // usually RadioShuttle::RS_Node_Online or RadioShuttle, set via properties
    err = rs->Startup(radioTypeMode); 
    dprintf("Startup as a Node: %s ID=%d", rs->GetRadioName(rs->GetRadioType()), myDeviceID);
    if (!err && rs->AppRequiresAuthentication(myTempSensorApp) == RS_PasswordSet) {
      err = rs->Connect(myTempSensorApp, remoteDeviceID);
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
  intr.debounce();
  MYSERIAL.begin(115200);
  InitSerial(&MYSERIAL, 5000, &led, !intr.read()); // wait 5000ms that the Serial Monitor opens, otherwise turn off USB, use 0 for USB always on.
  SPI.begin();
  RTCInit(__DATE__, __TIME__);

  led = 1;
  
  bool nodeOffline = prop.GetProperty(prop.LORA_RADIO_TYPE, 0) == RadioShuttle::RS_Node_Offline;
  if (!SerialUSB_active && nodeOffline) {
#ifdef ARDUINO_SAMD_ZERO
    intr.low(callback(&SwitchInput)); // in deepsleep, D21 supports only low/high.
#else
    intr.fall(callback(&SwitchInput));
#endif
  } else {
   intr.fall(callback(&SwitchInput));
  }

  if (ESP32DeepsleepWakeup) {
    /*
     * We received a wakeup after an ESP32 deepsleep timeout or IO activity 
     * on registered pins. Here we can add some checking code and quickly enter
     * into deepsleep() again, when appropriate (nothing to do).
     * Omitting the additional startup code and messages will save energy 
     */
    // Your code goes here:
    // deepsleep();
  }

  dprintf("Welcome to RadioShuttle v%d.%d", RS_MAJOR, RS_MINOR);

  /*
   * The following are required properties for the operation
   */
  myDeviceID = prop.GetProperty(prop.LORA_DEVICE_ID, 0);
  myCode = prop.GetProperty(prop.LORA_CODE_ID, 0);
  radioTypeMode = (RadioShuttle::RadioType)prop.GetProperty(prop.LORA_RADIO_TYPE, 0);
  remoteDeviceID = 1;

#define USE_DEMOBOARD_PAIR
#ifdef USE_DEMOBOARD_PAIR
  /*
   * for a demo, we ship a pair of boards with odd/even numbers, e.g. IDs 13/14
   * 13 for a server, 14 for a node
   */
  if (myDeviceID & 0x01) { // odd demo board IDs are servers
    server = true;
    remoteDeviceID = myDeviceID + 1;
    radioTypeMode = RadioShuttle::RS_Station_Basic; 
  } else {
    server = false;
    remoteDeviceID = myDeviceID - 1;
    radioTypeMode = RadioShuttle::RS_Node_Online;
  }
#endif

  if (myDeviceID == 0 || myCode == 0 || radioTypeMode == 0) {
    dprintf("LORA_DEVICE_ID or LORA_CODE_ID or LORA_RADIO_TYPE not set, use PropertyEditor to set this!");
    return;
  }
  if (radioTypeMode >= RadioShuttle::RS_Station_Basic)
    server = true;

  /*
   * Here are optional properties for custom settings
   */
  int value;
  if ((value = prop.GetProperty(prop.LORA_REMOTE_ID, 0)) != 0)
    remoteDeviceID = value;
  if ((value = prop.GetProperty(prop.LORA_FREQUENCY, 0)) != 0)
    myProfile[0].Frequency = value;
  if ((value = prop.GetProperty(prop.LORA_BANDWIDTH, 0)) != 0)
    myProfile[0].Bandwidth = value;
  if ((value = prop.GetProperty(prop.LORA_SPREADING_FACTOR, 0)) != 0)
    myProfile[0].SpreadingFaktor = value;
  if ((value = prop.GetProperty(prop.LORA_TXPOWER, 0)) != 0)
    myProfile[0].TXPower = value;    
  if ((value = prop.GetProperty(prop.LORA_FREQUENCY_OFFSET, 0)) != 0)
    myProfile[0].FrequencyOffset = value;
  appPassword = prop.GetProperty(prop.LORA_APP_PWD, (const char *)NULL);
    
  if (InitRadio() != 0)
    return;
}


void loop() {
  static int cnt = 0;

  if (cnt != pressedCount) {
    int flags = 0;
    flags |= RadioShuttle::MF_NeedsConfirm; // optional
    if (useAES && appPassword)
      flags |= RadioShuttle::MF_Encrypted;

    if (server) {
      static char msg[] = "The server feels very good today";
      rs->SendMsg(myTempSensorApp, msg, sizeof(msg), flags, remoteDeviceID);
    } else {
      static char msg[] = "Hello, the temperature is 26 celsius";
      rs->SendMsg(myTempSensorApp, msg, sizeof(msg), flags, remoteDeviceID);
    }
    cnt = pressedCount;
  }

  led = 0;
  if (!SerialUSB_active && rs->Idle() && rs->GetRadioType() == RadioShuttle::RS_Node_Offline) {
    /*
     * In deepsleep() the CPU is turned off, lowest power mode.
     * On the D21, we receive a RTC wakeup every 5 seconds to allow working
     * On the ESP32 an RTC a deep sleep restarts every 10 secs
     */
    deepsleep();
  } else {
    sleep();  // timer and radio interrupts will wakeup us
  }
  led = 1;
  rs->RunShuttle(); // process all pending events
}

#endif // FEATURE_LORA

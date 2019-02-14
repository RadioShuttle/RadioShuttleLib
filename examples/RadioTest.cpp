/*
 * The file is Licensed under the Apache License, Version 2.0
 * (c) 2017 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 */

#include "mbed.h"
#include "PinMap.h"
#include "sx1276-mbed-hal.h"
#include "RadioShuttle.h"
#include "RadioStatus.h"
#include "RadioSecurity.h"
#include "main.h"
#ifdef  FEATURE_NVPROPERTY
#include <NVPropertyProviderInterface.h>
#include "NVProperty.h"
#endif


#if defined(FEATURE_LORA) && defined(FEATURE_RADIOTEST)

#define CHECK_ERROR_RET(func, err) { \
	if (err) { \
		dprintf("Error in %s: %s", func, rs->StrError(err)); \
		return err; \
	} \
}

#ifndef TARGET_STM32L4
 #define RADIO_SERVER	1
#endif


bool usePassword = false;	// password the can used indepenend of AES
bool server;				// automatically being set if radioTypeMode RadioShuttle::RS_Station_Basic
bool useAES = false;		// AES needs the usePassword option on
const char *appPassword;

static const int myTempSensorApp = 0x0001;  // Must be unique world wide.
#ifdef RADIO_SERVER
int myDeviceID = 1;
int remoteDeviceID = 14;
uint32_t myCode = 0;
RadioShuttle::RadioType radioTypeMode = RadioShuttle::RS_Station_Basic;  // 1 = RS_Node_Offline, 3 = RS_Node_Online, 4 = RS_Station_Basic
#else
int myDeviceID = 14;
int remoteDeviceID = 1;
uint32_t myCode = 0;
RadioShuttle::RadioType radioTypeMode = RadioShuttle::RS_Node_Offline;  // 1 = RS_Node_Offline, 3 = RS_Node_Online, 4 = RS_Station_Basic
#endif


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
RadioShuttle::RadioProfile myProfile[] =  {
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


void TempSensorRecvHandler(int AppID, RadioShuttle::devid_t stationID, int msgID, int status, void *buffer, int length)
{
    switch(status) {
        case RadioShuttle::MS_SentCompleted:	// A SendMsg has been sent.
            dprintf("MSG_SentCompleted: id=%d  %d bytes", msgID, length);
            break;
        case RadioShuttle::MS_SentCompletedConfirmed:// A SendMsg has been sent and confirmed
            dprintf("MSG_SentCompletedConfirmed: id=%d %d bytes", msgID, length);
            break;
        case RadioShuttle::MS_SentTimeout:		// A timeout occurred, number of retries exceeded
            dprintf("MSG_SentTimeout ID: %d", msgID);
            break;

        case RadioShuttle::MS_RecvData:			// a simple input message
            dprintf("MSG_RecvData ID: %d, len=%d", msgID, length);
            // dump("MSG_RecvData", buffer, length);
#ifdef PM_SENSOR
        	{
            	struct sensor *p = (sensor *)buffer;
            	if (length == sizeof(struct sensor) && p->version == 1) {
            		dprintf("ParticalData: PM10: %.1f (μg/m3)   PM2.5: %.1f (μg/m3)    ID: %d", (float)p->pm10 / 10.0, (float)p->pm25 / 10.0, p->id);
            	}
        	}
#endif
            break;
        case RadioShuttle::MS_RecvDataConfirmed:	// received a confirmed message
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

        case RadioShuttle::MS_StationConnected:	// a confirmation that the connection was accepted
            dprintf("MSG_StationConnected");
            break;
        case RadioShuttle::MS_StationDisconnected:	// a confirmation that the disconnect was accepted
            dprintf("MSG_StationDisconnected");
            break;
        default:
            break;
    }
}


Radio *radio;
RadioShuttle *rs;
RadioStatusInterface *statusIntf;
RadioSecurityInterface *securityIntf;

int InitRadio()
{
    Radio *radio;
    RSCode err;
    
#ifdef  FEATURE_NVPROPERTY
	NVProperty prop;
	int value;
	
	myDeviceID = prop.GetProperty(prop.LORA_DEVICE_ID, 0);
	myCode = prop.GetProperty(prop.LORA_CODE_ID, 0);
	if ((value = prop.GetProperty(prop.LORA_RADIO_TYPE, 0)) != 0)
		radioTypeMode = (RadioShuttle::RadioType)value;
	remoteDeviceID = 1;
	
	if (myDeviceID == 0 || myCode == 0 || radioTypeMode == 0) {
		dprintf("LORA_DEVICE_ID or LORA_CODE_ID or LORA_RADIO_TYPE not set, use PropertyEditor to set this!");
		return -1;
	}
	/*
	 * Here are optional properties for custom settings
	 */
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
#endif

  if (radioTypeMode >= RadioShuttle::RS_Station_Basic)
    server = true;
	
#ifdef TARGET_DISCO_L072CZ_LRWAN1
    radio = new SX1276Generic(NULL, MURATA_SX1276,
                              LORA_SPI_MOSI, LORA_SPI_MISO, LORA_SPI_SCLK, LORA_CS, LORA_RESET,
                              LORA_DIO0, LORA_DIO1, LORA_DIO2, LORA_DIO3, LORA_DIO4, LORA_DIO5,
                              LORA_ANT_RX, LORA_ANT_TX, LORA_ANT_BOOST, LORA_TCXO);
#elif defined(HELTECL432_REV1)
    radio = new SX1276Generic(NULL, HELTEC_L4_1276,
                              LORA_SPI_MOSI, LORA_SPI_MISO, LORA_SPI_SCLK, LORA_CS, LORA_RESET,
                              LORA_DIO0, LORA_DIO1, LORA_DIO2, LORA_DIO3, LORA_DIO4, LORA_DIO5,
                              LORA_ANT_PWR);
#else // RFM95
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
    dprintf("Radio: %.1f MHz, SF%d, %.f kHz", (float)myProfile[0].Frequency/1000000.0, myProfile[0].SpreadingFaktor, (float)myProfile[0].Bandwidth/1000.0);
    
    rs->AddRadioStatus(statusIntf);
    CHECK_ERROR_RET("AddRadioStatus", err);
    
    rs->AddRadioSecurity(securityIntf);
    CHECK_ERROR_RET("AddRadioSecurity", err);
    
	/*
	 * The password parameter can be NULL if no password is required
   	 */
	err = rs->RegisterApplication(myTempSensorApp, &TempSensorRecvHandler,  (void *)appPassword);
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

/*
 * this is a example basic loop for RadioShuttle
 */
int RadioTest()
{
    extern volatile int pressedCount;
    
    if (InitRadio() != 0)
        return -1;
    

    for(;;) {
        static int cnt = 0;
        if (cnt != pressedCount) {
            if (cnt > 0) {
                int flags = 0;
                flags |= RadioShuttle::MF_NeedsConfirm;  // optional
   				 if (useAES && appPassword)
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
        
        if (rs->Idle() && rs->GetRadioType() == RadioShuttle::RS_Node_Offline) {
            sleep(); // uses deepsleep() when idle lowest power mode;
        } else {
            sleep();  // timer and radio interrupts will wakeup us
        }
        rs->RunShuttle(); // process all pending events
    }
    return 0;
}


int RadioUpdate(bool keyPressed)
{
    if (!rs)
        return 0;
        
    if (keyPressed) {
        int flags = 0;
        flags |= RadioShuttle::MF_NeedsConfirm;  // optional
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
    rs->RunShuttle();
    return 0;
}

bool RadioISIdle()
{
    if (!rs)
        return true;
    return rs->Idle();
}

void InitLoRaChipWithShutdown()
{
#ifdef LORA_CS
    if (LORA_CS == NC)
      return;
#ifdef HELTECL432_REV1
    Radio *radio = new SX1276Generic(NULL, HELTEC_L4_1276,
                            LORA_SPI_MOSI, LORA_SPI_MISO, LORA_SPI_SCLK, LORA_CS, LORA_RESET,
                            LORA_DIO0, LORA_DIO1, LORA_DIO2, LORA_DIO3, LORA_DIO4, LORA_DIO5, LORA_ANT_PWR);
#else
    Radio *radio = new SX1276Generic(NULL, RFM95_SX1276,
                            LORA_SPI_MOSI, LORA_SPI_MISO, LORA_SPI_SCLK, LORA_CS, LORA_RESET,
                            LORA_DIO0, LORA_DIO1, LORA_DIO2, LORA_DIO3, LORA_DIO4, LORA_DIO5);
#endif
	
    RadioEvents_t radioEvents;
    memset(&radioEvents, 0, sizeof(radioEvents));
    if (radio->Init(&radioEvents)) {
        radio->Sleep();
        delete radio;
    }
#endif
}

void RadioContinuesTX(void)
{
	Radio *radio;
	
#ifdef  FEATURE_NVPROPERTY
	NVProperty prop;
	int value;
	
	/*
	 * Here are optional properties for custom settings
	 */
	if ((value = prop.GetProperty(prop.LORA_FREQUENCY, 0)) != 0)
		myProfile[0].Frequency = value;
	if ((value = prop.GetProperty(prop.LORA_TXPOWER, 0)) != 0)
		myProfile[0].TXPower = value;
#endif

	
#ifdef TARGET_DISCO_L072CZ_LRWAN1
    radio = new SX1276Generic(NULL, MURATA_SX1276,
                              LORA_SPI_MOSI, LORA_SPI_MISO, LORA_SPI_SCLK, LORA_CS, LORA_RESET,
                              LORA_DIO0, LORA_DIO1, LORA_DIO2, LORA_DIO3, LORA_DIO4, LORA_DIO5,
                              LORA_ANT_RX, LORA_ANT_TX, LORA_ANT_BOOST, LORA_TCXO);
#elif defined(HELTECL432_REV1)
    radio = new SX1276Generic(NULL, HELTEC_L4_1276,
                              LORA_SPI_MOSI, LORA_SPI_MISO, LORA_SPI_SCLK, LORA_CS, LORA_RESET,
                              LORA_DIO0, LORA_DIO1, LORA_DIO2, LORA_DIO3, LORA_DIO4, LORA_DIO5,
                              LORA_ANT_PWR);
#else // RFM95
    radio = new SX1276Generic(NULL, RFM95_SX1276,
                              LORA_SPI_MOSI, LORA_SPI_MISO, LORA_SPI_SCLK, LORA_CS, LORA_RESET,
                              LORA_DIO0, LORA_DIO1, LORA_DIO2, LORA_DIO3, LORA_DIO4, LORA_DIO5);
#endif
		if ((value = prop.GetProperty(prop.LORA_SPREADING_FACTOR, 0)) != 0)
		myProfile[0].SpreadingFaktor = value;
	
	dprintf("RadioContinuesTX test, press reset to abort");
	while(true) {
		int secs = 10;
		radio->SetTxContinuousWave(myProfile[0].Frequency, myProfile[0].TXPower, secs);
		wait_ms(secs * 1000);
		rprintf(".");
	}
}

#endif // FEATURE_LORA

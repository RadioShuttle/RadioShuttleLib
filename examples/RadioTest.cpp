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

#ifdef FEATURE_LORA

#ifndef TARGET_STM32L4
 #define RADIO_SERVER	1
#endif

#ifdef RADIO_SERVER
bool server = true;
#else
bool server = false;
#endif

bool usePassword = false;	// password the can used indepenend of AES
bool useAES = false;		// AES needs the usePassword option on
bool useNodeOffline = false;// when idle turns the radio off and enters deelsleep

Timeout timeout;

void timoutFunc(void)
{
#ifdef TARGET_STM32L4
    WatchDogUpdate();
#endif
}

enum SensorsIDs { // Must be unique world wide.
    myTempSensorApp = 0x0001,
#ifdef RADIO_SERVER
    myDeviceID = 1,
    myCode = 0,
    remoteDeviceID = 9,
#else
    myDeviceID = 9,
    myCode = 0,
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


void TempSensorRecvHandler(int AppID, RadioShuttle::devid_t stationID, int msgID, int status, void *buffer, int length)
{
    switch(status) {
        case RadioShuttle::MS_SentCompleted:	// A SendMsg has been sent.
            dprintf("MSG_SentCompleted: id=%d  %d bytes", msgID, length);
            break;
        case RadioShuttle::MS_SentCompletedConfirmed:// A SendMsg has been sent and confirmed
            dprintf("MSG_SentCompletedConfirmed: id=%d %d bytes", msgID, length);
            break;
        case RadioShuttle::MS_SentTimeout:		// A timeout occurred, number of retires exceeded
            dprintf("MSG_SentTimeout ID: %d", msgID);
            break;

        case RadioShuttle::MS_RecvData:			// a simple input message
            dprintf("MSG_RecvData ID: %d, len=%d", msgID, length);
        {
            struct sensor *p = (sensor *)buffer;
            if (length == sizeof(struct sensor) && p->version == 1) {
            	dprintf("ParticalData: PM10: %.1f (μg/m3)   PM2.5: %.1f (μg/m3)    ID: %d", (float)p->pm10 / 10.0, (float)p->pm25 / 10.0, p->id);

            }
            // dump("MSG_RecvData", buffer, length);
        }
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

extern volatile int pressedCount;

int RadioTest()
{
    Radio *radio;
    RSCode err;
    RadioStatusInterface *statusIntf = NULL;
    RadioSecurityInterface *securityIntf = NULL;
    timeout.attach(&timoutFunc, 10);
    
    
#ifdef TARGET_DISCO_L072CZ_LRWAN1
    radio = new SX1276Generic(NULL, MURATA_SX1276,
            LORA_SPI_MOSI, LORA_SPI_MISO, LORA_SPI_SCLK, LORA_CS, LORA_RESET,
            LORA_DIO0, LORA_DIO1, LORA_DIO2, LORA_DIO3, LORA_DIO4, LORA_DIO5,
            LORA_ANT_RX, LORA_ANT_TX, LORA_ANT_BOOST, LORA_TCXO);
    
    statusIntf = new MyRadioStatus();
#else // RFM95
    radio = new SX1276Generic(NULL, RFM95_SX1276,
            LORA_SPI_MOSI, LORA_SPI_MISO, LORA_SPI_SCLK, LORA_CS, LORA_RESET,
            LORA_DIO0, LORA_DIO1, LORA_DIO2, LORA_DIO3, LORA_DIO4, LORA_DIO5);
#endif   

    securityIntf = new RadioSecurity();
    
    RadioShuttle *rs = new RadioShuttle("MyRadioShuttle");
    
    rs->EnablePacketTrace(RadioShuttle::DEV_ID_ANY, true, true);
    
    err = rs->AddLicense(myDeviceID, myCode);
    if (err)
        return err;
    err = rs->AddRadio(radio, MODEM_LORA, myProfile);
    if (err)
        return err;
    rs->AddRadioStatus(statusIntf);
    if (err)
        return err;
    rs->AddRadioSecurity(securityIntf);
    if (err)
        return err;
    /*
     * The password parameter can be skipped if no password is required
     */
    if (usePassword) {
	    err = rs->RegisterApplication(myTempSensorApp, &TempSensorRecvHandler, samplePassword, sizeof(samplePassword)-1);
    } else {
        err = rs->RegisterApplication(myTempSensorApp, &TempSensorRecvHandler);
        err = rs->RegisterApplication(10, &TempSensorRecvHandler);
    }
    if (err)
        return err;
    
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
        if (rs->AppRequiresAuthentication(myTempSensorApp) == RS_PasswordSet) {
        	err = rs->Connect(myTempSensorApp, remoteDeviceID);
        }
    }
    if (err)
        return err;

    for(;;) {
        static int cnt = 0;
        if (cnt != pressedCount) {
            if (cnt > 0) {
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
            cnt = pressedCount;
        }
        
        timeout.attach(&timoutFunc, 25);
        if (rs->Idle() && rs->GetRadioType() == RadioShuttle::RS_Node_Offline) {
            deepsleep(); // CPU is turned off lowest power mode;
        } else {
            sleep();  // timer and radio interrupts will wakeup us
        }
        rs->RunShuttle(); // process all pending events
#ifdef TARGET_STM32L4
        WatchDogUpdate();
#endif        
    }
    return 0;
}

#endif // FEATURE_LORA

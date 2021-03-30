/*
 * This is work copyright
 * (c) 2019 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 *
 * Use is granted to registered RadioShuttle licensees only.
 * Licensees must own a valid serial number and product code.
 * Details see: www.radioshuttle.de
 */

/*
 * Known Problems:
 * - For RS_Node_Checking find a receive message solution
 * - Add channel switching support
 * - Add a global AIR busy data table to calculate communication slots
 *	 for available response windows and sends
 * - winScale > 0 should multiply the respWindow value (and not shift)
 * - Add C++ alike callback for RegisterApplication() handler
 */

#ifdef ARDUINO
#include <Arduino.h>
#include "arduino-util.h"
#define	InterruptMSG(x)	void()
#endif
#ifdef __MBED__
#ifdef __ARMCC_VERSION
 #define _RWSTD_MINIMUM_NEW_CAPACITY _RWSTD_C::size_t (1) // default is 32
 #define _RWSTD_INCREASE_CAPACITY(x) (x) // default is 1.6x
#endif
#include "mbed.h"
#include "xPinMap.h"
#include "main.h"
#include "mbed-util.h"
#endif
#include "RadioShuttle.h"

#ifdef FEATURE_LORA

class RadioEntry;


static void RDTxDone(void *radio, void *userThisPtr, void *userData)
{
    InterruptMSG(INT_LORA);
    RadioShuttle *callbackClass = (RadioShuttle *)userThisPtr;
    callbackClass->RS_TxDone((Radio *)radio, userData);
}

static void RDRxDone(void *radio, void *userThisPtr, void *userData, uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr )
{
    InterruptMSG(INT_LORA);
    RadioShuttle *callbackClass = (RadioShuttle *)userThisPtr;
    callbackClass->RS_RxDone((Radio *)radio, userData, payload, size, rssi, snr);
}

static void RDTxTimeout(void *radio, void *userThisPtr, void *userData)
{
    InterruptMSG(INT_LORA);
    RadioShuttle *callbackClass = (RadioShuttle *)userThisPtr;
    callbackClass->RS_TxTimeout((Radio *)radio, userData);
}

static void RDRxTimeout(void *radio, void *userThisPtr, void *userData)
{
    InterruptMSG(INT_LORA);
    RadioShuttle *callbackClass = (RadioShuttle *)userThisPtr;
    callbackClass->RS_RxTimeout((Radio *)radio, userData);
}

static void RDRxError(void *radio, void *userThisPtr, void *userData)
{
    InterruptMSG(INT_LORA);
    RadioShuttle *callbackClass = (RadioShuttle *)userThisPtr;
    callbackClass->RS_RxError((Radio *)radio, userData);
}

static void RDCadDone(void *radio, void *userThisPtr, void *userData, bool channelActivityDetected)
{
    InterruptMSG(INT_LORA);
    RadioShuttle *callbackClass = (RadioShuttle *)userThisPtr;
    callbackClass->RS_CadDone((Radio *)radio, userData, channelActivityDetected);
}


const RadioShuttle::RadioProfile RadioShuttle::defaultProfile[] =  {
    /*
     * Our default profile
     * frequency, bandwidth, TX power, spreading factor, frequency-offset
     */         
    { 868100000, 125000, 14, 7, 0 },
    { 0, 0, 0, 0, 0 },
};


RadioShuttle::RadioShuttle(const char *deviceName)
{
    RadioHeader *p;

    STATIC_ASSERT(sizeof(RadioHeader) == 16, "RadioHeader");
    STATIC_ASSERT(sizeof(p->u.fullyv1) == 12, "RadioHeader fullyv1");
    STATIC_ASSERT(sizeof(p->u.packedv1) == 8, "RadioHeader packedv1");
    STATIC_ASSERT(sizeof(EncryptionHeader) == 8, "EncryptionHeader");
    
    
    busyInShuttle = false;
    _name = deviceName;
    _deviceID = ~0; // -1
    _maxMTUSize = 0;
    timer = new MyTimeout();
    prevWakeup = 0;
    SetTimerCount = 0;
    _statusIntf = NULL;
    _securityIntf = NULL;
	ticker = new MyTimer();
	ticker->start();
	_startupHandler = (AppStartupHandler)this;
    memset(&_wireDumpSettings, 0, sizeof(_wireDumpSettings));
}


RadioShuttle::~RadioShuttle()
{
    if (timer) {
        delete timer;
	}

	if (ticker) {
		delete ticker;
	}
    
    list<RadioEntry>::iterator re;
    for(re = _radios.begin(); re != _radios.end(); re++) {
        re->radio->Standby();
    }
    
    _radios.clear();

    list<SendMsgEntry>::iterator me;
    for(me = _sends.begin(); me != _sends.end(); me++) {
        if (me->releaseData)
            delete[] (uint8_t *)me->data;
    }
    _sends.clear();
    _recvs.clear();
    _airtimes.clear();
    _apps.clear();
    _connections.clear();
    _signals.clear();
}


RSCode
RadioShuttle::AddLicense(devid_t deviceID, uint32_t productCode)
{
	if (AddRSLicense(this, sizeof(*this), deviceID, productCode) != RS_NoErr)
		return RS_InvalidProductCode;
		
	_deviceID = deviceID;
	
    return RS_NoErr;
}


RSCode
RadioShuttle::AddRadio(Radio *radio, RadioModems_t modem, const struct RadioProfile *profile, int customRetryDelay_ms)
{
    if (!(modem == MODEM_FSK || modem == MODEM_LORA))
        return RS_UnknownModemType;
    
    struct RadioEntry r;
    memset(&r, 0, sizeof(r));
    r.radioEvents.TxDone = RDTxDone;
    r.radioEvents.RxDone = RDRxDone;
    r.radioEvents.RxError = RDRxError;
    r.radioEvents.TxTimeout = RDTxTimeout;
    r.radioEvents.RxTimeout = RDRxTimeout;
    r.radioEvents.CadDone = RDCadDone;
    r.radioEvents.userThisPtr = this;
    
    r.radio = radio;
    if (profile)
        r.profile = profile;
    else 
        r.profile = &defaultProfile[0];
    r.modem = modem;
    
    _radios.push_back(r);
    struct RadioEntry &re(_radios.back()); 	// Get the pointer to our record
    re.radioEvents.userData = (void *)&re;	// userData now points to us

    if (!radio->Init(&re.radioEvents))
        return RS_NoRadioAvailable;
    /*
     * Random cannot be called after the initialization because it
     * destroys the radio configuration. Otherwise receiving does not work anymore.
     */
    re.random = radio->Random();
    re.random2 = radio->Random();
	re.customRetryDelay_ms = customRetryDelay_ms;
    
    int radioMTUSize = radio->MaxMTUSize(modem);
    if (!_maxMTUSize || _maxMTUSize > radioMTUSize)
        _maxMTUSize = radioMTUSize;
    
    return RS_NoErr;
}


RSCode
RadioShuttle::AddRadioStatus(RadioStatusInterface *statusIntf)
{
    _statusIntf = statusIntf;
    return RS_NoErr;
}


RSCode
RadioShuttle::AddRadioSecurity(RadioSecurityInterface *securityIntf)
{
    _securityIntf = securityIntf;
    if (_wireDumpSettings.sents || _wireDumpSettings.recvs) {
#if 0 // only for testing.
        if (_securityIntf)
        	_securityIntf->EncryptTest();
#endif
    }
	return RS_NoErr;
}


RSCode
RadioShuttle::Startup(RadioType radioType, devid_t myID)
{
	if (myID)
		_deviceID = myID;
		
    if (_radios.size() < 1)
        return RS_NoRadioConfigured;
    
    _radioType = radioType;

    list<RadioEntry>::iterator re;
    for(re = _radios.begin(); re != _radios.end(); re++) {
        _initRadio(&*re);
        dprintf("RandomRetry: %d ms", re->retry_ms);
    }

	if (_startupHandler)
		_receiveHandler = _startupHandler;
	else
		_receiveHandler = NULL;

    if (_statusIntf) {
        _statusIntf->SetStationID(_deviceID);
        _statusIntf->SetRadioType(GetRadioName(radioType));
    }
    return RS_NoErr;
}


RSCode
RadioShuttle::UpdateNodeStartup(RadioType newRadioType)
{
    if (!(_radioType == RS_Node_Offline || _radioType == RS_Node_Online))
        return RS_InvalidParam;
    
    list<RadioEntry>::iterator re;
    for(re = _radios.begin(); re != _radios.end(); re++) {
        if (this->Idle() && newRadioType == RS_Node_Offline) {
            re->radio->Sleep();
        if (this->Idle() && newRadioType == RS_Node_Online)
            re->radio->Rx(RX_TIMEOUT_30MIN);
        }
    }
    _radioType = newRadioType;

    return RS_NoErr;
}


RSCode
RadioShuttle::_initRadio(RadioEntry *re)
{
    int LORA_SYMBOL_TIMEOUT = 5;
    int CODING_RATE_4_5 = 1;
    int LORA_PREAMBLE_LENGTH = 8;
    int LORA_NB_SYMB_HOP = 4;
    
    re->radio->SetChannel(re->profile->Frequency + re->profile->FrequencyOffset);
    if (_statusIntf)
        _statusIntf->SetRadioParams(re->profile->Frequency, re->profile->SpreadingFaktor);

    if (re->modem == MODEM_LORA) {
        re->radio->SetTxConfig(re->modem, re->profile->TXPower, 0, re->profile->Bandwidth,
                               re->profile->SpreadingFaktor, CODING_RATE_4_5,
                               LORA_PREAMBLE_LENGTH, false,
                               true, re->modem == MODEM_FSK, LORA_NB_SYMB_HOP,
                               false, 2000 );
        
        
        re->radio->SetRxConfig(re->modem, re->profile->Bandwidth, re->profile->SpreadingFaktor,
                               CODING_RATE_4_5, 0, LORA_PREAMBLE_LENGTH,
                               LORA_SYMBOL_TIMEOUT, false, 0,
                               true, re->modem == MODEM_FSK, LORA_NB_SYMB_HOP,
                               false, true);
    }
    if (re->modem == MODEM_FSK) {
        int FSK_FDEV = 25000;
        int FSK_DATARATE = 19200;
        int FSK_PREAMBLE_LENGTH = 5;
        int FSK_AFC_BANDWIDTH = 83333; // Hz
        
        
        re->radio->SetTxConfig(re->modem, re->profile->TXPower, FSK_FDEV, 0,
                               FSK_DATARATE, 0,
                               FSK_PREAMBLE_LENGTH, false,
                               true, 0, 0, 0, 2000 );
        
        re->radio->SetRxConfig(re->modem, re->profile->Bandwidth, FSK_DATARATE,
                               0, FSK_AFC_BANDWIDTH, FSK_PREAMBLE_LENGTH,
                               0, false, 0, true,
                               0, 0, false, true );
    }
    if (_radioType == RS_Node_Offline || _radioType == RS_Node_Checking)
        re->radio->Sleep();
    else
        re->radio->Rx(RX_TIMEOUT_30MIN);
    
    re->rStats.startupTime = time(NULL);
    re->maxTimeOnAir = re->radio->TimeOnAir(re->modem, re->radio->MaxMTUSize(re->modem));
    if (re->modem == MODEM_LORA) {
        // Init it again with the proper send timeout which could be rather large
        int tx_timeout = re->maxTimeOnAir + (re->maxTimeOnAir / 10); // add 10%

        re->radio->SetTxConfig(re->modem, re->profile->TXPower, 0, re->profile->Bandwidth,
                               re->profile->SpreadingFaktor, CODING_RATE_4_5,
                               LORA_PREAMBLE_LENGTH, false,
                               true, re->modem == MODEM_FSK, LORA_NB_SYMB_HOP,
                               false, tx_timeout );
    }
    re->retry_ms = re->maxTimeOnAir + (re->random % re->maxTimeOnAir);
	if (re->customRetryDelay_ms)
		re->retry_ms = re->customRetryDelay_ms;
    re->lastTxPower = re->profile->TXPower;
    re->timeOnAir12Bytes = re->radio->TimeOnAir(re->modem, 12);
    if (_wireDumpSettings.sents || _wireDumpSettings.recvs) {
        dprintf("TimeOnAir: 12 bytes (%d ms), 49 bytes (%d ms)",
                re->timeOnAir12Bytes, (int)re->radio->TimeOnAir(re->modem, 49));
    }

    return RS_NoErr;
}


RadioShuttle::RadioType
RadioShuttle::GetRadioType(void)
{
    return _radioType;
}


RSCode
RadioShuttle::RegisterApplication(int AppID, AppRecvHandler handler, void *password, int pwLen)
{
    if (!pwLen && password)
        pwLen = strlen((char *)password);
    map<int, AppEntry>::iterator it = _apps.find(AppID);
    if(it != _apps.end()) {
        return RS_DuplicateAppID;
    }

    struct AppEntry r;
    memset(&r, 0, sizeof(r));
    r.AppID = AppID;
    r.handler = handler;
    r.msgID = 1;
    r.password = password;
    r.pwLen = pwLen;
    r.pwdConnected = false;

    _apps.insert(std::pair<int,AppEntry> (AppID, r));
    
    
    return RS_NoErr;
}


RSCode
RadioShuttle::DeRegisterApplication(int AppID)
{
    map<int, AppEntry>::iterator it = _apps.find(AppID);
    if(it == _apps.end()) {
        return RS_AppID_NotFound;
    }
 
    list<SendMsgEntry>::iterator me;
    me = _sends.begin();
    while(me != _sends.end()) { // while loop to overcome erase list problem
        if (AppID == me->AppID) {
            if (me->releaseData)
                delete[] (uint8_t *)me->data;
            me = _sends.erase(me); // erase() returns the next list entry
        } else
            me++;
    }

    _apps.erase(it);

    return RS_NoErr;
}


RSCode
RadioShuttle::AppRequiresAuthentication(int AppID)
{
    map<int, AppEntry>::iterator it = _apps.find(AppID);
    if(it == _apps.end()) {
        return RS_AppID_NotFound;
    }
    if (it->second.password)
        return RS_PasswordSet;
    return RS_NoPasswordSet;
}


RSCode
RadioShuttle::Connect(int AppID, devid_t stationID)
{

    map<int, AppEntry>::iterator it = _apps.find(AppID);

    if (it == _apps.end()) {
	    return RS_AppID_NotFound;
    }
    if (!it->second.password)
        return RS_NoPasswordSet;
    
    if(!_securityIntf)
        return RS_NoSecurityInterface;
    
    map<pair<devid_t,int>, ConnectEntry>::iterator cit = _connections.find(pair<devid_t,int>(stationID, AppID));
   
    if (cit != _connections.end()) {
        return RS_DuplicateAppID;
    }
    
	struct ConnectEntry r;
	memset(&r, 0, sizeof(r));
    r.stationID = stationID;
    r.AppID = AppID;
    r.authorized = false;

    _connections.insert(std::make_pair(pair<devid_t, int>(stationID, AppID), r));

    SendMsg(AppID, NULL, _securityIntf->GetHashBlockSize(), MF_Connect|MF_NeedsConfirm, stationID);
    
    return RS_NoErr;
}


RSCode
RadioShuttle::SendMsg(int AppID, void *data, int len, int flags, devid_t stationID, int txPower, int *msgID)
{
    struct AppEntry *aep = NULL;
    ConnectEntry *cop = NULL;
    
    
    if (len > _maxMTUSize - (int)sizeof(RadioHeader))
        return RS_MessageSizeExceeded;
    
    map<int, AppEntry>::iterator it = _apps.find(AppID);
    
    if(it == _apps.end()) {
        return RS_AppID_NotFound;
    }
    aep = &it->second;
    
    if (!(flags & MF_Direct) && aep->password && !(flags & MF_Connect)) {
        map<pair<devid_t,int>, ConnectEntry>::iterator ce = _connections.find(pair<devid_t,int>(stationID, AppID));
        
        if (ce == _connections.end()) {
            return RS_StationNotConnected;
        }
        cop = &ce->second;
        /*
         * Try to connect again if no connect request is pending.
         */
        if (ce->second.authorized == false) {
            bool connectPending = false;
            
            list<SendMsgEntry>::iterator me;
            for(me = _sends.begin(); me != _sends.end(); me++) {
                if (AppID == me->AppID && me->flags & MF_Connect && me->stationID == stationID) {
                    connectPending = true;
                    break;
                }
            }
            if (!connectPending) { // try to connect again.
                SendMsg(AppID, NULL, _securityIntf->GetHashBlockSize(), MF_Connect|MF_NeedsConfirm, stationID);
            }
        }
    }
    
    struct SendMsgEntry r;
    memset(&r, 0, sizeof(r));
    r.AppID = AppID;
    if (flags & CF_CopyData) {
        uint8_t *newdata = new uint8_t[len];
        if (!newdata)
            return RS_OutOfMemory;
        memcpy(newdata, data, len);
        data = newdata;
        flags |= CF_FreeData;
    }
    r.data = data;
    r.len = len;
    r.flags = flags & MF_FlagsMask;
    if (flags & CF_FreeData)
        r.releaseData = true;
    r.stationID = stationID;
    r.txPower = txPower;
    r.msgID = aep->msgID++;
    if (msgID)
        *msgID = r.msgID;
    r.cep = cop;
    r.aep = aep;
    if (flags & MF_Direct)
    	r.pStatus =  PS_GotSendSlot;
    else
    	r.pStatus = PS_Queued;

    _sends.push_back(r);
    RunShuttle();
    return RS_NoErr;
}


RSCode
RadioShuttle::KillMsg(int AppID, int msgID)
{
    list<SendMsgEntry>::iterator me;
    for(me = _sends.begin(); me != _sends.end(); me++) {
        if (AppID == me->AppID && msgID == me->msgID) {
            if (me->releaseData)
                delete[] (uint8_t *)me->data;
            _sends.erase(me);
            return RS_NoErr;
        }
    }
    return RS_MsgID_NotFound;    
}


RSCode
RadioShuttle::UpdateRadioProfile(Radio *radio, RadioType radioType, const struct RadioProfile *profile)
{
    bool foundRadio = false;
    
    if (!profile || !radio)
        return RS_InvalidParam;
        
    list<RadioEntry>::iterator re;
    for(re = _radios.begin(); re != _radios.end(); re++) {
        if (re->radio == radio) {
            foundRadio = true;
            break;
        }
    }
    if (!foundRadio) {
        return RS_NoRadioAvailable;
    }

    _radioType = radioType;
    
	re->profile = profile;
	_initRadio(&*re);
    
    _signals.clear(); // clear cache, new radio paramerters starts again
    
    return RS_NoErr;
}


RSCode
RadioShuttle::MaxMessageSize(int *size, int msgFlags)
{
    if (_radios.size() < 1) {
        return RS_NoRadioConfigured;
    }
    
    int overhead = sizeof(RadioHeader);
    if (msgFlags & MF_Encrypted) {
        if (_securityIntf)
        	overhead += _securityIntf->GetEncryptionBlockSize();
    }
	if (size)
        *size = _maxMTUSize - overhead;

	return RS_NoErr;
}


RSCode
RadioShuttle::GetStatistics(struct RadioStats **stats, Radio *radio)
{
    if (_radios.size() < 1) {
        return RS_NoRadioConfigured;
    }
    
    list<RadioEntry>::iterator re;
    for(re = _radios.begin(); re != _radios.end(); re++) {
        if (re->radio == radio || radio == NULL) {
            *stats = &re->rStats;
            return RS_NoErr;
        }
    }
    
    return RS_RadioNotFound;
}


bool
RadioShuttle::Idle(bool forceBusyDuringTransmits)
{
	bool txBusy = false;
	
	if (forceBusyDuringTransmits) {
		list<RadioEntry>::iterator re;
        for(re = _radios.begin(); re != _radios.end(); re++) {
			RadioState_t rState = re->radio->GetStatus();
			if (rState == RF_TX_RUNNING)
				txBusy = true;
		}
	}
	
    if (_recvs.size() == 0 && _sends.size() == 0 && txBusy == false)
        return true;
    return false;
}


const char *
RadioShuttle::StrError(RSErrorCode err)
{
    switch (err) {
        case RS_NoErr:
            return "no error";
        case RS_DuplicateAppID:
            return "DuplicateAppID";
        case RS_AppID_NotFound:
            return "StationNotConnected";
        case RS_StationNotConnected:
            return "StationNotConnected";
        case RS_NoPasswordSet:
            return "NoPasswordSet";
        case RS_PasswordSet:
            return "PasswordSet";
        case RS_NoSecurityInterface:
            return "NoSecurityInterface";
        case RS_MsgID_NotFound:
            return "NoRadioConfigured";
        case RS_NoRadioConfigured:
            return "NoRadioConfigured";
        case RS_NoRadioAvailable:
            return "NoRadioAvailable";
        case RS_RadioNotFound:
            return "RadioNotFound";
        case RS_UnknownModemType:
            return "UnknownModemType";
        case RS_MessageSizeExceeded:
            return "MessageSizeExceeded";
        case RS_InvalidProductCode:
            return "InvalidProductCode";
        case RS_InvalidParam:
            return "InvalidParam";
        case RS_OutOfMemory:
            return "OutOfMemory";
    }
    return "Unkown";
}

const char *
RadioShuttle::GetRadioName(RadioType radioType)
{
    switch(radioType) {
        case RS_Node_Offline:
            return "Node-Offline";
        case RS_Node_Checking:
            return "Node-Checking";
        case RS_Node_Online:
            return "Node-Online";
        case RS_Station_Basic:
            return "Station-Basic";
        case RS_Station_Server:
            return "Station-Server";
        default:
            return "Unknown";
    }
}


int
RadioShuttle::RunShuttle(void)
{
    /*
     * TODO: add new features/problems here
     */
    if (busyInShuttle)
        return 1;
    busyInShuttle = true;
    
    /*
     * Collect received messages from our radios from last received interrupt
     * We add the TX packets here because modifying the _recvs list is not
     * good on interrupt level.
     */
    list<RadioEntry>::iterator re;
    for(re = _radios.begin(); re != _radios.end(); re++) {
        if (re->intrDelayedMsg) {
            if (_wireDumpSettings.sents || _wireDumpSettings.recvs)
            	dprintf("%s", (const char *)re->intrDelayedMsg);
            re->intrDelayedMsg = NULL;
        }
        if (re->txDoneReceived) {
            re->txDoneReceived = false;
            if (_statusIntf)
                _statusIntf->TXComplete();
        }
        if (re->rxMsg.RxData) {
            struct ReceivedMsgEntry r;
    	    memset(&r, 0, sizeof(r));
        	r.RxData = (void *)re->rxMsg.RxData;
        	r.RxSize = re->rxMsg.RxSize;
        	r.rssi = re->rxMsg.rssi;
        	r.snr = re->rxMsg.snr;
            r.re = &*re;
            
            _recvs.push_back(r);

            re->rxMsg.RxData = NULL;
            if (_statusIntf)
                _statusIntf->RxDone(re->rxMsg.RxSize, re->rxMsg.rssi, re->rxMsg.snr);
        }
    }
    
    /*
     * Process all input messages quickly because the
     * data buffer will be overwritten by the next received packet
     */
    if (_recvs.size() > 0) {
        ProcessReceivedMessages();
    	if (_statusIntf && !_recvs.size())
        	_statusIntf->RxCompleted();
    }

    /*
     * Start sending all pending messages on all radio interfaces
     * Note that the timer may overflow with larger numbers
     * at the end of the 32-bit counter and start over again.
     * In this case we skip the request and expect that a retry works.
     */
    uint32_t sendBusyDelay = 0;
    uint32_t tstamp = ticker->read_ms();
    list<SendMsgEntry>::iterator me;
    for(me = _sends.begin(); me != _sends.end(); me++) {
        if (me->lastSentTime > tstamp) { // This checks for timer overflow
            me->lastSentTime = tstamp;
            me->pStatus = PS_SendRequestCompleted; // Abort request on overflow
        }
        
        /*
         * Check if we have something ready for sending, otherwise 
         * continue with other messages.
         */
        switch(me->pStatus) {
            case PS_Queued:
                break;
            case PS_Sent:
            case PS_WaitForConfirm:
                /*
                 * After the last retry we still have to wait for the send request
                 * to be completed before issuing a confirmation timeout.
                 */
                if (me->retryCount >= MAX_SENT_RETRIES && me->lastSentTime &&
                    tstamp > me->lastSentTime + me->lastTimeOnAir + me->confirmTimeout) {
                    me->pStatus = PS_SendTimeout;
                    continue; // Timeout and no confirmation
                } else if (!(tstamp > me->lastSentTime + me->lastTimeOnAir + me->retry_ms)) {
                    continue; // Still waiting for retry time
                }
                break;
            case PS_GotSendSlot:
                if (tstamp < me->responseTime) {
                    continue; // Not ready for sending data
                }
                break;
            case PS_SendRequestCompleted:
                continue;
            case PS_SendRequestConfirmed:
                continue;
            default:
                continue;
        }
        
        /*
         * Send loop over all radios
         */
        list<RadioEntry>::iterator re;
        for(re = _radios.begin(); re != _radios.end(); re++) {
            /*
             * Sending packets without a delay has a problem that we cannot detect
             * answers on previous requests which results into collisions.
             * Lets wait a few ms to give us a change to detect a preamble/signal
             */
            if (tstamp < re->lastTxDone) // overflow
            	re->lastTxDone = 0;
            if (re->lastTxDone && tstamp <= re->lastTxDone + (re->timeOnAir12Bytes/5)) {
                int wait_ms = (re->lastTxDone + (re->timeOnAir12Bytes/5)) - tstamp;
                if ((int)sendBusyDelay > wait_ms)
                    sendBusyDelay = wait_ms;
            	continue;
            }
			
			/*
			 * Check if for this node is already a message in transit
			 * this avoids overloading the server with messages.
			 * this allows than an ongoing Connect will be completed first
			 */
			bool skip = false;
			if (me->pStatus == PS_Queued) {
    			list<SendMsgEntry>::iterator tme;
    			for(tme = _sends.begin(); tme != _sends.end(); tme++) {
					if (tme == me) // skip my
						continue;
					if (tme->AppID == me->AppID && tme->stationID == me->stationID && tme->pStatus > PS_Queued) {
						skip = true;
						break;
					}
				}
			}
			if (skip)
				continue;
		
            /*
        	 * Check that the radio is not busy, no signal on air
        	 */
        	RadioState_t rState = re->radio->GetStatus();
            if (rState == RF_TX_RUNNING) {
            	continue;
            }
            if (rState == RF_RX_RUNNING) {
                if (re->radio->RxSignalPending()) {
                    re->rStats.channelBusyCount++;
                    // dprintf("RX_RUNNING");
                    continue;
                }
            } else {
            	if (CadDetection(&*re))
            		continue;
            }
            /*
             * Finally send the message.
             */
            int msgFlags = 0;
            void *data = NULL;
            int len = me->len;
            
            if (me->pStatus == PS_Queued || me->pStatus == PS_Sent || me->pStatus == PS_WaitForConfirm) {
                if (me->flags & MF_Response) {
                    msgFlags = me->flags; // Response header
                    if (me->flags & MF_Connect)
                        data = me->data;
                } else {
                    msgFlags = me->flags & (MF_LowPriority|MF_HighPriority|MF_Connect); // Request slot state
                    if (_radioType >= RS_Station_Basic && me->pStatus != PS_GotSendSlot)
                        len = 0;
                }
            }
            if (me->pStatus == PS_GotSendSlot) {
                msgFlags = me->flags & (MF_LowPriority|MF_HighPriority|MF_NeedsConfirm|MF_Connect|MF_Encrypted); // send data
                data = me->data;
            }
            SendMessage(&*re, data, len, me->msgID, me->AppID,  me->stationID, msgFlags, me->txPower, me->respWindow, me->channel, me->factor);
            
            me->retryCount++;
			me->lastSentTime = tstamp;
            me->retry_ms = re->retry_ms;
            me->lastTimeOnAir = re->radio->TimeOnAir(re->modem, re->lastTxSize);
            me->confirmTimeout = re->radio->TimeOnAir(re->modem, sizeof(RadioHeader)) + 20;

            if (me->pStatus == PS_GotSendSlot && !(me->flags & MF_NeedsConfirm))
                me->pStatus = PS_SendRequestCompleted;
            if (me->pStatus == PS_Queued || me->pStatus == PS_WaitForConfirm) // try first, try again
				me->pStatus = PS_Sent;
            if (me->pStatus == PS_GotSendSlot) {
                me->pStatus = PS_WaitForConfirm;
            	me->responseTime = 0;
            }
        }
    }
    
    /*
     * For completed or timed-out packets from the queue
     * we call the registered callbacks.
     * Remove completed packets from queue.
     */
    me = _sends.begin();
    while(me != _sends.end()) { // while loop to overcome erase list problem
        bool shouldDelete = false;
        if (me->pStatus == PS_SendRequestCompleted || me->pStatus == PS_SendRequestConfirmed ||
            me->pStatus == PS_SendTimeout) {
            
            map<int, AppEntry>::iterator it = _apps.find(me->AppID);
            if(it != _apps.end()) {
                int status = MS_SentCompleted;
                if (me->pStatus == PS_SendTimeout)
                    status = MS_SentTimeout;
                else if (me->pStatus == PS_SendRequestCompleted)
                    status = MS_SentCompleted;
                else if (me->pStatus == PS_SendRequestConfirmed)
                    status = MS_SentCompletedConfirmed;
                
                if (me->flags != MF_Response) {
                    if (me->pStatus == PS_SendTimeout) {
                        DeleteSignalStrength(me->stationID);
                        if (_statusIntf)
                            _statusIntf->MessageTimeout(me->AppID, me->stationID);
                    }
                    it->second.handler(me->AppID, me->stationID, me->msgID, status, me->data, me->len);
                }
            }
            shouldDelete = true;
        }
        if (shouldDelete) {
            if (me->releaseData)
                delete[] (uint8_t *)me->data;
            me = _sends.erase(me); // erase() returns the next list entry
        } else
        	me++;
    }
    
    /*
     * Restart timer to the nearest timeout time. The timer
     * interrupt ensures that the RunShuffle loop is called to process
     * pending packets.
     * Additional interrupts (e.g. TX Done, other timers,  etc.) will not
     * hurt because we verify that the time is due for a retry or sending.
     */
    uint32_t responseTimeMin = ~0;
    uint32_t lastSentTimeMin = ~0;
    tstamp = ticker->read_ms();

    for(me = _sends.begin(); me != _sends.end(); me++) {
        
        if (me->responseTime && me->responseTime < responseTimeMin) {
            responseTimeMin = me->responseTime;
            // dprintf("responseTimeMin: %d", (int)responseTimeMin);
        }
        
        if (me->lastSentTime && me->lastSentTime + me->lastTimeOnAir + me->retry_ms < lastSentTimeMin) {
            lastSentTimeMin = me->lastSentTime + me->lastTimeOnAir + me->retry_ms;
        }
        if (sendBusyDelay && sendBusyDelay < lastSentTimeMin)
            lastSentTimeMin = sendBusyDelay;
    }
        
    uint32_t time_abs = std::min(responseTimeMin, lastSentTimeMin);
    
    if (time_abs != (uint32_t)~0) {
        uint32_t wakeup;
        if (time_abs > tstamp) // avoid to set negative timeout which waits forever.
            wakeup = time_abs - tstamp;
        else
            wakeup = 5; //now
        if (wakeup < 5)
            wakeup = 5; // otherwise the prints will take longer
        if (wakeup == 5 || prevWakeup != time_abs) { // This blocks setting a timer again on every interrupt
            timer->attach_us(callback(this, &RadioShuttle::TimeoutFunc), wakeup * 1000);
            prevWakeup = time_abs;
            SetTimerCount++;
            // dprintf("Set Timer in %d ms", (int)wakeup); // TODO comment out
        } else {
            // dprintf("Use last timer!"); // TODO comment out
        }
    }
    
    /*
     * Turn off the radios in Sleep mode for offline/checking nodes.
     */
    if (_radioType == RS_Node_Offline || _radioType == RS_Node_Checking) {
        bool turnOff = true;
        
        for(me = _sends.begin(); me != _sends.end(); me++) {
            if (me->lastSentTime && me->pStatus != PS_GotSendSlot && me->responseTime == 0) {
                turnOff = false;
                break;
            }
			if (me->pStatus == PS_Queued) {
                turnOff = false;
                break;
			}
        }
        
        if (turnOff) {
            list<RadioEntry>::iterator re;
            for(re = _radios.begin(); re != _radios.end(); re++) {
                if (re->radio->GetStatus() == RF_RX_RUNNING) {
                    re->radio->Sleep();
                    if (_wireDumpSettings.sents || _wireDumpSettings.recvs) {
                    	dprintf("Putting the radio into Sleep");
                    }
                    if (_radios.size() == 1) // only a single radio, stop timers.
                        timer->detach();
                }
            }
        }
    }

    busyInShuttle = false;
    return 0;
}


void
RadioShuttle::ProcessReceivedMessages()
{
    static int prevLen = 0;
    list<ReceivedMsgEntry>::iterator rme;
    rme = _recvs.begin();
    while(rme != _recvs.end()) { // while loop to overcome erase list problem
        map<int, AppEntry>::iterator it;
        AppEntry *aep = NULL;
        int AppID, respWindow;
        devid_t destination, source;
        void *data;
        int len;
        int msgFlags;
        int msgID;
        uint8_t channel, factor;
        
        
        if (!ReceiveMessage(&*rme, &data, len, msgID, AppID, msgFlags, destination, source, respWindow, channel, factor)) {
            goto ProcessingDone;
        }
 		rme->re->rStats.lastRXdeviceID = source;
		
        if (destination !=  DEV_ID_ANY && destination != _deviceID && msgFlags & MF_Response) {
            int timeOnAir = rme->re->radio->TimeOnAir(rme->re->modem, prevLen);
            
        	SaveTimeOnAirSlot(destination, AppID, msgFlags, respWindow, channel, factor, timeOnAir);
        }
        prevLen = len;
        /*
         * Check if we support the AppID
         */
        it = _apps.find(AppID);
        if(it == _apps.end()) {
            rme->re->rStats.appNotSupported++;
            goto ProcessingDone;
        }
        aep = &it->second;
	
        if (destination !=  DEV_ID_ANY && destination != _deviceID) {
            rme->re->rStats.appNotSupported++;
        	goto ProcessingDone;
        }

        if (aep->password && !(msgFlags & MF_Connect)) {
            map<pair<devid_t,int>, ConnectEntry>::iterator ce;
#if 0
            for(ce = _connections.begin(); ce != _connections.end(); ce++) {
                dprintf("station: %d, app: %d, authorized: %d, random: 0x%x-%x",
                        (int)ce->second.stationID,
                        ce->second.AppID,
                        ce->second.authorized,
                        (int)ce->second.random[0],
                        (int)ce->second.random[1]);
            }
#endif
            ce = _connections.find(pair<devid_t,int>(source, AppID));
            
            if (ce == _connections.end()) {
                rme->re->rStats.noAuthMessageCount++;
                /*
                 * We have no Connection with this node, tell the node about an error
                 * to allow him to connect again.
                 */
                MessageSecurityError(&*rme, aep, msgID, source, channel, factor);
                goto ProcessingDone;
            }
            
            if (!ce->second.authorized) {
                rme->re->rStats.noAuthMessageCount++;
                goto ProcessingDone;
            }
            if (msgFlags & MF_Response && msgFlags & MF_Authentication && !(msgFlags & MF_Connect)) {
                /*
                 * Not authenticated error message from the station.
                 * The station has been restartet, connect again.
                 */
                if (_securityIntf) {
                	ce->second.authorized = false;
                	SendMsg(AppID, NULL, _securityIntf->GetHashBlockSize(), MF_Connect|MF_NeedsConfirm, source);
                }
                goto ProcessingDone;
            }
        }

        /*
         * Check if the response is expected
         */
        if (msgFlags & MF_Response) {
            SendMsgEntry *mep = NULL;
            list<SendMsgEntry>::iterator me;
            for(me = _sends.begin(); me != _sends.end(); me++) {
                if (msgID == (me->msgID & msgIDv1Mask)) {
                    mep = &*me;
                    break;
                }
            }
        	if (!mep) {
                rme->re->rStats.unkownMessageCount++;
                goto ProcessingDone;
        	}
            
            ProcessResponseMessage(&*rme, aep,  mep,msgFlags, data, len, source, respWindow, channel, factor);
        } else {
            ProcessRequestMessage(&*rme, aep, msgFlags, data, len, msgID, source, respWindow, channel, factor);
        }
ProcessingDone:
        rme = _recvs.erase(rme); // erase() returns the next list entry
    }
        
}


bool
RadioShuttle::ProcessResponseMessage(ReceivedMsgEntry *rme, AppEntry *aep, SendMsgEntry *mep, int msgFlags, void *data, int len, devid_t source, uint32_t respWindow, uint8_t channel, uint8_t factor)
{
    if (_wireDumpSettings.recvs)
    	dprintf("ProcessResponseMessage");

    if (!(mep->pStatus == PS_Sent || mep->pStatus == PS_WaitForConfirm))
        return false;

    if (mep->pStatus == PS_WaitForConfirm) { // Ok, this is the confirmation
        mep->pStatus = PS_SendRequestConfirmed;
        if (msgFlags & MF_Connect && !(msgFlags & MF_Authentication)) {
            map<pair<devid_t,int>, ConnectEntry>::iterator cit = _connections.find(pair<devid_t,int>(source, aep->AppID));
            if (cit == _connections.end())
                return false;
            cit->second.authorized = true;
            cit->second.random[0] = mep->tmpRandom[0];
            cit->second.random[1] = mep->tmpRandom[1];
        }
        return true;
    }

    mep->pStatus = PS_GotSendSlot;
    if (mep->stationID == DEV_ID_ANY)
        mep->stationID = source; // Still broadcast, set the address
    uint32_t tstamp = ticker->read_ms();
    mep->responseTime = tstamp + respWindow;
    mep->lastSentTime = 0;
    mep->channel = channel;
    mep->factor = factor;
    if (msgFlags & MF_Connect && _securityIntf) {
        mep->flags |= MF_Connect;
        if (len == sizeof(rme->re->random)+sizeof(rme->re->random2)) {
            int shaLen = _securityIntf->GetHashBlockSize();
            if (shaLen <= (int)sizeof(mep->securityData)) {
                memcpy(mep->tmpRandom, data, sizeof(mep->tmpRandom));
                _securityIntf->HashPassword(data, len, aep->password, aep->pwLen, &mep->securityData);
                // dump("shaBuf", &mep->securityData, shaLen);
                mep->data = &mep->securityData;
                mep->len = shaLen;
            }
        }
    }

    return true;
}


bool
RadioShuttle::ProcessRequestMessage(ReceivedMsgEntry *rme, AppEntry *aep, int msgFlags, void *data, int len, int msgID, devid_t source, uint32_t respWindow, uint8_t channel, uint8_t factor)
{
    UNUSED(respWindow);
	UNUSED(channel);
	UNUSED(factor);
    if (_wireDumpSettings.recvs)
    	dprintf("ProcessRequestMessage: len=%d msgFlags=0x%x", len, msgFlags);
    
    if (!data){ // slot request
        _sends.push_back(SendMsgEntry());
        struct SendMsgEntry &r(_sends.back());
        memset(&r, 0, sizeof(r));

        r.AppID = aep->AppID;
		r.txPower = TX_POWER_AUTO;
        if (msgFlags & MF_Connect && _securityIntf) {

            map<pair<devid_t,int>, ConnectEntry>::iterator cit = _connections.find(pair<devid_t,int>(source, aep->AppID));
            if (cit == _connections.end()) {
                // No connection found, insert and fetch it
                struct ConnectEntry r;
                memset(&r, 0, sizeof(r));
                r.stationID = source;
                r.AppID = aep->AppID;
                r.authorized = false;
                _connections.insert(std::make_pair(pair<devid_t, int>(source, aep->AppID), r));
				cit = _connections.find(pair<devid_t,int>(source, aep->AppID));
            }
            cit->second.random[0] =  rme->re->random + time(NULL); // TODO better than adding time
            cit->second.random[1] =  rme->re->random2 + time(NULL);
            
            r.data = cit->second.random;
            r.len = sizeof(cit->second.random);
            r.flags = MF_Response|MF_Connect;
            r.cep = &cit->second;
            
        } else {
	        r.data = NULL;
	        r.len = 0;
	        r.flags = MF_Response;
            r.cep = NULL;
        }
        r.stationID = source;
        r.msgID = msgID;
        r.aep = aep;
        if (_radioType >= RS_Station_Basic) { // client request
            // TODO find a free channel e.g.: r.respWindow = 5000
            r.respWindow = 0;
        } else {
            r.respWindow = 0; // Server sends only if the channel is free, no respWindow needed
        }
            
        r.channel = 0;		  // TODO use MF_SwitchOptions for other channels
        r.factor = 0;
        r.pStatus = PS_Queued;
        r.retryCount = MAX_SENT_RETRIES-1; // Only one immediate try
    }

    int addFlags = 0;
    if (data && !(msgFlags & MF_Response)) { // Data request
        if (msgFlags & MF_Connect && _securityIntf) {
            
            map<pair<devid_t,int>, ConnectEntry>::iterator cit = _connections.find(pair<devid_t,int>(source, aep->AppID));
            if (cit == _connections.end())
                return false;
            
            int shaLen = _securityIntf->GetHashBlockSize();
            if (len != shaLen)
                return false; // Invalid connect try
            
            uint8_t *shaBuf = new uint8_t[len];
            if (!shaBuf) {
                rme->re->rStats.noMemoryError++;
                return false;
            }
            _securityIntf->HashPassword(cit->second.random, sizeof(cit->second.random), aep->password, aep->pwLen, shaBuf);
            if (memcmp(data, shaBuf, shaLen) == 0) {
                if (_wireDumpSettings.recvs)
                	dprintf("Password: Ok");
                addFlags = MF_Connect;
                cit->second.authorized = true;
            } else {
                if (_wireDumpSettings.recvs)
                    dprintf("Password: Failed");
                addFlags = MF_Connect|MF_Authentication;
            }
            delete[] shaBuf;
            if (!(addFlags & MF_Authentication) && cit->second.authorized) {
                aep->handler(aep->AppID, source, msgID, MS_StationConnected, data, len);
            }
            if (addFlags & MF_Authentication) {
                aep->handler(aep->AppID, source, msgID, MS_AuthenicationRequired, data, len);
            }

        } else {
        	aep->handler(aep->AppID, source, msgID, MS_RecvData, data, len);
        }
	    if (msgFlags & MF_NeedsConfirm) {
    	    struct SendMsgEntry r;
        	memset(&r, 0, sizeof(r));
	        r.AppID = aep->AppID;
		r.data = NULL;
        	r.len = 0;
        	r.flags = MF_Response | addFlags;
        	r.stationID = source;
		r.txPower = TX_POWER_AUTO;
        	r.msgID = msgID;
        	r.cep = NULL;
        	r.aep = aep;
		r.respWindow = 0;
        	r.pStatus = PS_Queued;
        	r.retryCount = MAX_SENT_RETRIES-1; // Only one immediate try
            
	        _sends.push_back(r);
		}
    }
    return true;
}


void
RadioShuttle::MessageSecurityError(ReceivedMsgEntry *rme, AppEntry *aep, int msgID, devid_t source, uint8_t channel, uint8_t factor)
{
	UNUSED(rme);
	UNUSED(channel);
	UNUSED(factor);
    struct SendMsgEntry r;
    memset(&r, 0, sizeof(r));
    r.AppID = aep->AppID;
    r.data = NULL;
    r.len = 0;
    r.flags = MF_Response | MF_Authentication;
    r.stationID = source;
    r.msgID = msgID;
    r.cep = NULL;
    r.aep = aep;
    r.respWindow = 0;
    r.pStatus = PS_Queued;
    r.retryCount = MAX_SENT_RETRIES-1; // Only one immediate try
    r.releaseData = false;
    
    _sends.push_back(r);
}


void
RadioShuttle::SaveTimeOnAirSlot(devid_t destination, int AppID, int msgFlags, int respWindow, uint8_t channel, uint8_t factor, int timeOnAir)
{
	UNUSED(msgFlags);
    uint32_t tstamp = ticker->read_ms();

    struct TimeOnAirSlotEntry r;
    memset(&r, 0, sizeof(r));
    r.stationID = destination;
    r.AppID = AppID;
    r.busy_time = tstamp + respWindow;
    r.busy_ms = timeOnAir;
    
    r.channel = channel;
    r.factor = factor;

    // _airtimes.push_back(r); // TODO we need to test this with a third client
}


bool
RadioShuttle::SendMessage(RadioEntry *re, void *data, int len, int msgID, int AppID, devid_t stationID, int flags, int txPower, int respWindow, uint8_t channel, uint8_t factor)
{
    RadioHeader rh;
    memset(&rh, 0, sizeof(rh));
    int hlen;
    int winScale = 0;
    
    rh.msgFlags = flags;
    rh.s.data.msgID = msgID & msgIDv1Mask;
    
    int maxRespWindow = respWindow;
    if ((rh.msgFlags & MF_Response) && len == 0) {
        if (respWindow >> 4 < Packedv1MaxRespWindow)
            maxRespWindow = respWindow >> 4;
    }
    
    if (AppID <= Packedv1MaxApps && maxRespWindow <= Packedv1MaxRespWindow &&
        _deviceID <= Packedv1MaxDeviceID && stationID <= Packedv1MaxDeviceID) {
        rh.magic = RSMagic;
        rh.version = RSHeaderPacked_v1;
        rh.u.packedv1.appID = AppID;
        while(respWindow > Packedv1MaxRespWindow) {
            respWindow /= 2;
            winScale++;
        }
        rh.u.packedv1.respWindow = respWindow;
        rh.u.packedv1.destination = stationID;
        rh.u.packedv1.source = _deviceID;
        hlen = RSHeaderPackedSize_v1;
    } else {
        rh.magic = RSMagic;
        rh.version = RSHeaderFully_v1;
        rh.u.fullyv1.appID = AppID;
        while(respWindow > Fullyv1MaxRespWindow) {
            respWindow /= 2;
            winScale++;
        }
        rh.u.fullyv1.respWindow = respWindow;
        rh.u.fullyv1.destination = stationID;
        rh.u.fullyv1.source = _deviceID;
        hlen = RSHeaderFullySize_v1;
    }
    
    if (winScale > MaxWinScale) {
        if (_wireDumpSettings.sents || _wireDumpSettings.recvs)
	        dprintf("Window scale too large");
        return false;
    }
    
    bool switchOptions = false;
    if (len == 0) {
        if (_radioType >= RS_Station_Basic && rh.msgFlags & MF_Response)
            switchOptions = true; // Server to client request case
    	if (_radioType <= RS_Node_Online && !(rh.msgFlags & MF_Response))
            switchOptions = true; // Client to server, server response case

    }
            
    if (switchOptions && (channel > 0 || factor > 0 || winScale > 0)) { // any reason to switch
        rh.msgFlags |= MF_SwitchOptions;
        rh.s.option.channel = channel;
        rh.s.option.factor = factor;
        rh.s.option.winScale = winScale;
            
        if (hlen == RSHeaderPackedSize_v1)
            rh.u.packedv1.respWindow = respWindow;
        if (hlen == RSHeaderFullySize_v1)
            rh.u.fullyv1.respWindow = respWindow;
            
    } else {
        rh.s.data.msgSize = len + hlen;
    }
    
    if (txPower == TX_POWER_AUTO)
    	txPower = CalculateTXPower(re, stationID);
    if (txPower != re->lastTxPower) {
        re->radio->SetRfTxPower(txPower);
        re->lastTxPower = txPower;
    }
    
    /*
     * Packet compression and encryption goes here
     */
    uint8_t *crypteddata = NULL;
    int newlen = 0;

    if (_securityIntf && data && flags & MF_Encrypted) {
        map<int, AppEntry>::iterator it = _apps.find(AppID);
        if(it != _apps.end() && it->second.password) {
            map<pair<devid_t,int>, ConnectEntry>::iterator cit = _connections.find(pair<devid_t,int>(stationID, AppID));
            if (cit != _connections.end()) {
                if (!cit->second.authorized)
                    return false;
                EncryptionHeader eh;
                eh.version = _securityIntf->GetSecurityVersion();
                eh.dataSum = GetDataSum(DataSumBits, data, len);
                eh.msgSize = rh.s.data.msgSize;
                eh.msgID = rh.s.data.msgID;
                eh.random = cit->second.random[0];
                
                newlen = len + sizeof(EncryptionHeader);
                int bsize =_securityIntf->GetEncryptionBlockSize();
                int remain = newlen % bsize; // Pad to block size
                if (remain)
                    newlen += bsize - remain;
                uint8_t *newdata = new uint8_t[newlen];
                crypteddata = new uint8_t[newlen];
                if (newdata == NULL || crypteddata == NULL) {
                    if (newdata)
                        delete[] newdata;
                    re->rStats.noMemoryError++;
                    return false;
                }
                memcpy(newdata, &eh, sizeof(eh));
                memcpy(newdata + sizeof(eh), data, len);

                void *context = _securityIntf->CreateEncryptionContext(it->second.password, it->second.pwLen);
                _securityIntf->EncryptMessage(context, newdata, crypteddata, newlen);
                _securityIntf->DestroyEncryptionContext(context);
                delete[] newdata;
                if (_wireDumpSettings.sents)
					dump("EncryptedData", crypteddata, newlen);
            }
        }
    }
    
    /*
     * Encryption completed continue to send it
     */
    if (_statusIntf)
        _statusIntf->TXStart(AppID, stationID, len + hlen, txPower);
    if (data == NULL) {
        re->radio->Send(&rh, hlen); // Request state, header only
	} else {
        if (crypteddata)
            re->radio->Send(crypteddata, newlen, &rh, hlen); // Response state
        else
            re->radio->Send(data, len, &rh, hlen); // Response state
    }
    re->txDoneReceived = false;
    re->lastTxSize = len + hlen;
    PacketTrace(re, "TxSend", &rh, data, data == NULL ? 0 : len, true, NULL);

    if (crypteddata) {
        delete[] crypteddata;
	}
	
	return true;
}


int
RadioShuttle::CalculateTXPower(RadioEntry *re, devid_t stationID)
{
    int maxTXPower = re->profile->TXPower;
    
    
    map<devid_t, SignalStrengthEntry>::iterator it = _signals.find(stationID);
    if(it == _signals.end()) {
        return maxTXPower;
    }
    
    int rx_dBm = it->second.rx_dBm;
    int txpower = maxTXPower;

    if (!rx_dBm)
        return maxTXPower;
    
    if (rx_dBm < -80) 		// Far away
        txpower = maxTXPower;
    else if (rx_dBm < -70) // A few hundred meters away
        txpower = 14;
    else if (rx_dBm < -60)	// Same building, 40 meters
        txpower = 10;
    else if (rx_dBm < -50)	// Very closed nearby 20 meters
        txpower = 6;
    else if (rx_dBm < -40) 	// 40 dBm stations are next to each other
        txpower = 2;
    else
		txpower = 2;

	if (txpower > maxTXPower)
		txpower = maxTXPower;

    return txpower;
}


bool
RadioShuttle::UpdateSignalStrength(devid_t stationID, int dBm)
{
    uint32_t oldestUpdate = ~0;
    
    map<devid_t, SignalStrengthEntry>::iterator it = _signals.find(stationID);
    if(it != _signals.end()) {
        it->second.rx_dBm = dBm;
        it->second.lastUpdate = time(NULL);
        it->second.rcnCnt++;
        return false;
    }

    uint32_t cacheCount = 1;
    switch(_radioType) {
		case RS_RadioType_Invalid:
		case RS_Node_Offline:
		case RS_Node_Checking:
		case RS_Node_Online:
            cacheCount = 10;
            break;
		case RS_Station_Basic:
            cacheCount = 100;
            break;
		case RS_Station_Server:
            cacheCount = 10000;
    		break;
    }
    
    if (_signals.size() >= cacheCount) {
        devid_t saved = 0;
        
        for (it = _signals.begin(); it != _signals.end(); ++it) {
            if ((uint32_t)it->second.lastUpdate < (uint32_t)oldestUpdate) {
                oldestUpdate = it->second.lastUpdate;
                saved = it->first;
            }
        }
        _signals.erase(saved);
    }

    struct SignalStrengthEntry r;
    memset(&r, 0, sizeof(r));
    r.rx_dBm = dBm;
    r.stationID = stationID;
    r.lastUpdate = time(NULL);
    
    _signals.insert(std::pair<devid_t,SignalStrengthEntry> (stationID, r));

    return true;
}


bool
RadioShuttle::DeleteSignalStrength(devid_t stationID)
{
    if (!stationID)
        return false;
    
    map<devid_t, SignalStrengthEntry>::iterator it = _signals.find(stationID);
    if(it == _signals.end()) {
        return false;
    }

    _signals.erase(it);
    
    return true;
}


bool
RadioShuttle::ReceiveMessage(ReceivedMsgEntry *rme,  void **data, int &len, int &msgID, int & AppID, int &flags, devid_t &destination, devid_t &source, int &respWindow, uint8_t &channel, uint8_t &factor)
{
    RadioHeader *rh = (RadioHeader *)rme->RxData;
    
    int hlen = 0;
    if (rh->magic == RSMagic && rh->version == RSHeaderFully_v1)
        hlen = RSHeaderFullySize_v1;
    else if (rh->magic == RSMagic && rh->version == RSHeaderPacked_v1)
        hlen = RSHeaderPackedSize_v1;

    PacketTrace(rme->re, "RxDone", rh, (uint8_t *)rme->RxData+hlen, rme->RxSize-hlen, false, rme);
    if (_wireDumpSettings.recvs)
    	dprintf("RxFrequencyOffset: %d Hz", (int)rme->re->radio->GetFrequencyError(rme->re->modem));

    if (rh->magic != RSMagic) {
        rme->re->rStats.unkownMessageCount++;
        return false;
    }
    if (!(rh->version == RSHeaderFully_v1 || rh->version == RSHeaderPacked_v1)) {
        rme->re->rStats.unkownMessageCount++;
        return false;
    }

    *data = NULL;
    channel = 0;
    factor = 0;
    
    flags = rh->msgFlags;
    msgID = rh->s.data.msgID;
    if (rh->version == RSHeaderFully_v1) {
        AppID = rh->u.fullyv1.appID;
        respWindow = rh->u.fullyv1.respWindow;
        destination = rh->u.fullyv1.destination;
        source = rh->u.fullyv1.source;
        hlen = RSHeaderFullySize_v1;
    } else {
        AppID = rh->u.packedv1.appID;
        respWindow = rh->u.packedv1.respWindow;
        destination = rh->u.packedv1.destination;
        source = rh->u.packedv1.source;
        hlen = RSHeaderPackedSize_v1;
    }
    
    if (flags & MF_SwitchOptions) {
        len = hlen;
        channel = rh->s.option.channel;
        factor = rh->s.option.factor;
        respWindow <<= rh->s.option.winScale;
    } else {
    	len = rh->s.data.msgSize - hlen;
        if (len < 0) {
            rme->re->rStats.protocolError++;
            return false;
        }
    }
 

    
    if (destination != DEV_ID_ANY && destination !=  _deviceID) {
        rme->re->rStats.protocolError++;
        return false;
    }
    
    if (rme->RxSize > hlen)
    	*data = (uint8_t *)rme->RxData + hlen;
    
    UpdateSignalStrength(source, rme->rssi);
    /*
     * Packet de-compression and de-encryption goes here (later)
     */
    if (_securityIntf && *data && flags & MF_Encrypted) {
        map<int, AppEntry>::iterator it = _apps.find(AppID);
        if(it != _apps.end() && it->second.password) {
            map<pair<devid_t,int>, ConnectEntry>::iterator cit = _connections.find(pair<devid_t,int>(source, AppID));
            if (cit != _connections.end()) {
                
                if ((rme->RxSize-hlen) % _securityIntf->GetEncryptionBlockSize() > 0)
                    return false;
                uint8_t *crypteddata = new uint8_t[rme->RxSize-hlen];
                if (!crypteddata) {
                    rme->re->rStats.noMemoryError++;
                    return false;
                }
                memcpy(crypteddata, *data, rme->RxSize-hlen);
                
                void *context = _securityIntf->CreateEncryptionContext(it->second.password, it->second.pwLen);
                _securityIntf->DecryptMessage(context, crypteddata, *data, rme->RxSize-hlen);
                _securityIntf->DestroyEncryptionContext(context);
                delete[] crypteddata;
                EncryptionHeader *eh = (EncryptionHeader *)*data;
                *data = ((uint8_t *)*data) + sizeof(EncryptionHeader);
                bool decryptError = false;
                if (eh->version != _securityIntf->GetSecurityVersion())
                    decryptError = true;
                if (eh->dataSum != GetDataSum(DataSumBits, *data, eh->msgSize-hlen))
                    decryptError = true;
                if (eh->msgID != msgID)
                    decryptError = true;
                if (eh->random != cit->second.random[0])
                    decryptError = true;
                if (eh->msgSize !=  len + hlen)
                    decryptError = true;
                if (decryptError) {
                    rme->re->rStats.decryptError++;
                    return false;
                }
                if (_wireDumpSettings.recvs)
	                dump("Decrypted Ok", *data, len);
            }
        }
    }
	if (_receiveHandler) {
		flags = 0;
		uint8_t *d = (uint8_t *)*data;
		for (int i = 0; i < len; i++) {
			*d ^= *d; d++;
		}
	}

    return true;
}


void
RadioShuttle::EnablePacketTrace(devid_t stationID, bool sents, bool recvs, Radio *radio)
{
    _wireDumpSettings.stationID = stationID;
    _wireDumpSettings.sents = sents;
    _wireDumpSettings.recvs = recvs;
    _wireDumpSettings.radio = radio;
}


void
RadioShuttle::PacketTrace(RadioEntry *re, const char *name, RadioHeader *rh, void *data, int len, bool sent, ReceivedMsgEntry *rme)
{
    char msgFlagsStr[(8*6)+1];
    char *p = msgFlagsStr;
    
    if (_wireDumpSettings.radio && _wireDumpSettings.radio != re->radio)
        return;
    if (sent && !_wireDumpSettings.sents)
        return;
    if (!sent && !_wireDumpSettings.recvs)
        return;
    
    if (!(rh->version == RSHeaderFully_v1 || rh->version == RSHeaderPacked_v1)) {
        dprintf("PacketTrace %s: invalid RadioHeader magic (dBm:%d Snr:%d)", name, rme->rssi, rme->snr);
        if (sent) {
        	if (len > 0) {
            	dump(name, data, len);
        	}
        } else { // Receive
        	dump(name, rme->RxData, rme->RxSize);
        }
        return;
    }
    
    if (_wireDumpSettings.stationID != DEV_ID_ANY) {
        devid_t src = rh->version == RSHeaderFully_v1 ? (int)rh->u.fullyv1.source : (int)rh->u.packedv1.source;
        devid_t dst = rh->version == RSHeaderFully_v1 ? (int)rh->u.fullyv1.destination : (int)rh->u.packedv1.destination;
        if (_wireDumpSettings.stationID != dst || _wireDumpSettings.stationID != src)
            return;
    }
    
#define AddFlagStr(x) { memcpy(p, x, sizeof(x)-1); p += sizeof(x)-1; };
    if (!(rh->msgFlags & MF_Response))
        AddFlagStr("Req|");
    if (rh->msgFlags & MF_Response)
        AddFlagStr("Rsp|");
    if (rh->msgFlags & MF_NeedsConfirm)
        AddFlagStr("rAck|");
    if (rh->msgFlags & MF_LowPriority)
        AddFlagStr("LowP|");
    if (rh->msgFlags & MF_HighPriority)
        AddFlagStr("HighP|");
    if (rh->msgFlags & MF_Direct)
        AddFlagStr("dir|");
    if (rh->msgFlags & MF_Connect)
        AddFlagStr("Con|");
    if (rh->msgFlags & MF_Encrypted)
        AddFlagStr("Encr|");
    if (rh->msgFlags & MF_Authentication)
        AddFlagStr("Auth|");
    if (rh->msgFlags & MF_SwitchOptions)
        AddFlagStr("Opts|");
    if (p > msgFlagsStr)
    	*--p = 0; // remove the last '|'
    ASSERT(p - msgFlagsStr < (int)sizeof(msgFlagsStr));
#undef AddFlagStr

    int AppID = rh->version == RSHeaderFully_v1 ? (int)rh->u.fullyv1.appID : (int)rh->u.packedv1.appID;
    int respWindow = rh->version == RSHeaderFully_v1 ? (int)rh->u.fullyv1.respWindow : (int)rh->u.packedv1.respWindow;
    int source = rh->version == RSHeaderFully_v1 ? (int)rh->u.fullyv1.source : (int)rh->u.packedv1.source;
    int destination = rh->version == RSHeaderFully_v1 ? (int)rh->u.fullyv1.destination : (int)rh->u.packedv1.destination;
    
    if (sent) {
        dprintf("%s: %s(%s) size:%d id:%d app:%d rwin:%d src:%d dst:%d (dBm:%d sz:%d)",
                name,
                rh->version == RSHeaderFully_v1 ? "Fully" : "Packed",
                msgFlagsStr,
                rh->s.data.msgSize,
                rh->s.data.msgID,
                AppID,
                rh->msgFlags & MF_SwitchOptions ? respWindow << rh->s.option.winScale : respWindow,
                source,
                destination,
                re->lastTxPower,
                len);
    } else {
        dprintf("%s: %s(%s) size:%d id:%d app:%d rwin:%d src:%d dst:%d (dBm:%d Snr:%d sz:%d)",
                name,
                rh->version == RSHeaderFully_v1 ? "Fully" : "Packed",
                msgFlagsStr,
                rh->msgFlags & MF_SwitchOptions ? 0 : rh->s.data.msgSize,
                rh->s.data.msgID,
                AppID,
                rh->msgFlags & MF_SwitchOptions ? respWindow << rh->s.option.winScale : respWindow,
                source,
                destination,
                rme->rssi,
                rme->snr,
                len);
    }
    if (len > 0) {
    	dump(name, data, len);
    }
}



bool
RadioShuttle::CadDetection(RadioEntry *re)
{
    re->_CADdetected = -1;
    int msec = 50;
    Timer t;
    t.start();
    
    re->radio->StartCad();
    if (_wireDumpSettings.recvs)
        dprintf("CadStart");

    while(t.read_ms() < msec) {
    	// Wait for the CAD interrupt
        if (re->_CADdetected != -1)
        	break;
    }

    if (re->_CADdetected == 1)
    	return true;
    else
    	return false;
}


void
RadioShuttle::RS_TxDone(Radio *radio, void *userData)
{
	UNUSED(radio);
	RadioEntry *re = (RadioEntry *)userData;
	re->rStats.TxPackets++;
	re->rStats.txBytes += re->lastTxSize;
	re->lastTxDone = ticker->read_ms();
	re->txDoneReceived = true;
	re->radio->Rx(RX_TIMEOUT_30MIN);

    if (_wireDumpSettings.sents) {
        // dprintf("TxDone"); // interrupt printing does not work reliable
        re->intrDelayedMsg = "TxDone";
    }
}


void
RadioShuttle::RS_RxDone(Radio *radio, void *userData, uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr )
{
	UNUSED(radio);
    RadioEntry *re = (RadioEntry *)userData;
    re->rStats.rxBytes += size;
    re->rStats.RxPackets++;
	re->rStats.lastRSSI = rssi;
	re->rStats.lastSNR = snr;
    re->rxMsg.RxData = payload;
    re->rxMsg.RxSize = size;
    re->rxMsg.rssi = rssi;
    re->rxMsg.snr = snr;
    RadioHeader *rh = (RadioHeader *)payload;
    if (rh->magic != RSMagic || !(rh->version == RSHeaderFully_v1 || rh->version == RSHeaderPacked_v1)) {
        /*
         * On errors including invalid packets we need to turn off the radio (Sleep)
         * and on again, otherwise the received packet data contains wrong data with
         * the correct length. This is strange, however I observed this several
         * times when receiving packets or getting crc errors, it does not recover.
         *
         * The Sleep workaround works.
         */
        re->radio->Sleep();
    }
    /*
     * There is a problem that in radio continues receive mode sometimes we receive
     * an old packet instead of the correct one, this may be related to
     * the FIFO, anyways unable to find out what is wrong, a Sleep or Standby
     * function overcomes the problem. Helmut
     */
    re->radio->Standby();
    re->radio->Rx(RX_TIMEOUT_30MIN);
}


void
RadioShuttle::RS_TxTimeout(Radio *radio, void *userData)
{
    RadioEntry *re = (RadioEntry *)userData;
    if (_wireDumpSettings.sents) {
        // dprintf("TxTimeout"); // interrupt printing does not work reliable
        re->intrDelayedMsg = "TxTimeout";
    }
    /*
     * Should not happen
     * Maybe if the SPI transer is stuck, forward to TXDone
     */
    RS_TxDone(radio, userData);
}


void
RadioShuttle::RS_RxTimeout(Radio *radio, void *userData)
{
	UNUSED(radio);
    RadioEntry *re = (RadioEntry *)userData;
    re->radio->Rx(RX_TIMEOUT_30MIN);
    
    if (_wireDumpSettings.recvs) {
        // dprintf("RxTimeout"); // interrupt printing does not work reliable
    	dprintf("RxTimeout");
    	re->intrDelayedMsg = "RxTimeout";
    }
}


void
RadioShuttle::RS_RxError(Radio *radio, void *userData)
{
	UNUSED(radio);
    RadioEntry *re = (RadioEntry *)userData;
    re->rStats.RxErrorCount++;
    /*
     * Sleep() is needed to recover from CRC errors.
     * It looks like this happens when sender/receiver are close to each other.
     */
    re->radio->Sleep();
    re->radio->Rx(RX_TIMEOUT_30MIN);
    
    if (_wireDumpSettings.recvs) {
        // dprintf("RxError"); // interrupt printing does not work reliable
        re->intrDelayedMsg = "RxError";
    }
}


void
RadioShuttle::RS_CadDone(Radio *radio, void *userData, bool channelActivityDetected)
{
	UNUSED(radio);
    RadioEntry *re = (RadioEntry *)userData;
    if (channelActivityDetected) {
        re->_CADdetected = 1;
        re->rStats.channelBusyCount++;
    } else
        re->_CADdetected = 0;
    
    if (_wireDumpSettings.recvs) {
	    // dprintf("CadDone: Activity: %d", channelActivityDetected); // interrupt printing does not work reliable
        if (channelActivityDetected)
            re->intrDelayedMsg = "CadDone: activity detected";
        else
            re->intrDelayedMsg = "CadDone: no activity";
    }
}


void
IRAM_ATTR RadioShuttle::TimeoutFunc()
{
    InterruptMSG(INT_LORA);
    SetTimerCount++;
    if (_wireDumpSettings.sents || _wireDumpSettings.recvs) {
        // dprintf("TimeoutDone: SetTimerCount=%d", SetTimerCount); // interrupt printing does not work reliable        
        list<RadioEntry>::iterator re;
        re = _radios.begin();
        static char buf[40];
        snprintf(buf, sizeof(buf)-1, "TimeoutDone: SetTimerCount=%d", SetTimerCount);
        re->intrDelayedMsg = buf;
    }
    prevWakeup = 0;
    SetTimerCount = 0;
    timer->detach(); // to release deep_sleep_lock
}


uint32_t
RadioShuttle::GetDataSum(int maxbits, void *data, int len)
{
    uint8_t *p = (uint8_t *)data;
    int32_t sum = 0;
    while(len--)
        sum += *p++;
    // Limit to the number of requested bits, carry over the remaining
    sum = (sum & ((1<<maxbits)-1)) + (sum >> maxbits);
    return sum;
}


#endif // FEATURE_LORA

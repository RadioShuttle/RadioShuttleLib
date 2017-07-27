/*
 * This is an unpublished work copyright
 * (c) 2017 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 */
#ifndef __RADIOSHUTTLE_H__
#define __RADIOSHUTTLE_H__

#ifdef ARDUINO
#include "arduino-mbed.h"
#include <time.h>
#include <assert.h>
#undef min
#undef max
#undef map
#define us_ticker_read	us_getTicker
#define STATIC_ASSERT   _Static_assert
#define ASSERT assert
using namespace std;
#define FEATURE_LORA	1
#endif

#include <list>
#include <map>
#include "radio.h"
#include "RadioStatusInterface.h"
#include "RadioSecurityInterface.h"

#ifdef FEATURE_LORA


typedef enum RSErrorCode {
    RS_NoErr    = 0,
    RS_DuplicateAppID,
    RS_AppID_NotFound,
    RS_StationNotConnected,
    RS_NoPasswordSet,
    RS_PasswordSet,
    RS_NoSecurityInterface,
    RS_MsgID_NotFound,
    RS_NoRadioConfigured,			// no Radio added so far
    RS_NoRadioAvailable,            // Radio could not be detected
    RS_RadioNotFound,				// the specified radio is not available
    RS_UnknownModemType,            // no FSK no LORA modem type
    RS_MessageSizeExceeded,			// message size to long
} RSCode;



// Shuttle
class RadioShuttle
{
public:
    typedef uint32_t	devid_t;
    static const int DEV_ID_ANY = 0;
    
    struct RadioProfile {
        int Frequency;      // in Hz
        int Bandwidth;      // in Hz
        int TXPower;        // in dBm
        int SpreadingFaktor;// 7-12
    };
    
    enum RadioType {
        RS_Node_Offline,   	// sleep mode until sending, < 10k RAM
        RS_Node_Checking,   // sleep mode, checks for messages regulary, < 10k RAM
        RS_Node_Online,     // always powered on, < 10k RAM
        RS_Station_Basic,	// always active for one or more apps < 15k RAM
        RS_Station_Server,	// always active, lots of memory, routing options, < 1MB Ram
    };
    
    typedef void (*AppRecvHandler)(int AppID, devid_t stationID, int msgID, int status, void *data, int length);


    enum MsgStatus {
        MS_SentCompleted,			// A previous SendMsg has been sent.
        MS_SentCompletedConfirmed,	// A previous SendMsg has been sent and confirmed
        MS_SentTimeout,				// A timeout occurred, number of retires exceeded
        
        MS_RecvData,				// a simple input message
        MS_RecvDataConfirmed,		// received a confirmed message
        MS_NoStationFound,
        MS_NoStationSupportsApp,
        MS_AuthenicationRequired,
        
        MS_StationConnected,		// a confirmation that the connection was accepted
        MS_StationDisconnected,		// a confirmation that the disconnect was accepted
    };
    
    enum MsgFlags {
        MF_Response			= 0x01, // a response from a previous request, or request
        MF_NeedsConfirm		= 0x02, // station needs to acknloglage receivd
        MF_LowPriority		= 0x04, // transfer within one minute
        MF_HighPriority		= 0x08, // Immidate transfer
        MF_MoreDataToCome	= 0x10, // additional data is wait to be sent
        MF_Connect			= 0x20, // Connect a node to the station with password.
        MF_Encrypted		= 0x40, // message is encrypted
        MF_Authentication	= 0x80,	// message requires prior authentication
        MF_SwitchOptions	= 0x100,// tell the node to switch the channel for this trans ID.
    };
    
    /*
     * Constructor requires an Radio 
     */
    RadioShuttle(const char *deviceName, devid_t deviveID);

    ~RadioShuttle();

    /*
     * Adds a new Radio with a given profile
     * Optional profile, must be called before Startup
     */
    RSCode AddRadio(Radio *radio, RadioModems_t modem, const struct RadioProfile *profile = NULL);
    
    /*
     * The status interface allows custom status callbacks for
     * reporting send/receiving/timeout activity e.g.: led blinks
     */
    RSCode AddRadioStatus(RadioStatusInterface *statusIntf);
    
    /*
     * Support function for password hash generation and encryption
     * the extern API interface has the advantage that the security
     * can be upgraded independent of the RadioShuttle library.
     * AES hardware excelleration can be one reason for it.
     */
    RSCode AddRadioSecurity(RadioSecurityInterface *securityIntf);

    /*
     * Starts the service with the specified RadioType
     */
    RSCode Startup(RadioType radioType);
    
    
    /*
     * get the current radio type
     */
    RadioType GetRadioType(void);
    
    /*
     * registers an application. the AppType is unique world wide.
     * and must be used for sending and receiving app data
     * Multiple appID registrations are suppored.
     * The password can be a string to the password,
     * in case of binary passwd  data the pwLen must be set.
     * If the password is set only clients must be do a connect prior
     * to any SendMsg.
     */
    RSCode RegisterApplication(int AppID, AppRecvHandler, void *password = NULL, int pwLen = 0);
    /*
     * Removes an AppID
     */
    RSCode DeRegisterApplication(int AppID);
    
    /*
     * Check if the password is specified for a app
     */
    RSCode  AppRequiresAuthentication(int AppID);
                       
    /*
     * Start a pairing process to connect a node to a station
     */
    RSCode StartPairing(char *name = NULL);
    /*
     * Connect the node against a station, it can be called multiple times
     * in case mutiple stations IDs are being used.
     */
    RSCode Connect(int AppID, devid_t stationID = DEV_ID_ANY);
    
    /*
     * Inform the station that an App is discontinuing.
     */
    RSCode Disconnect(int AppID, devid_t stationID = ~0);
    
    /*
     * Prepare sending data to a remote application
     * The request is being queued for sending.
     * The data is busy until the AppRecvHandler is being called.
     */
    RSCode SendMsg(int AppID, void *data, int len, int flags = 0, devid_t stationID = DEV_ID_ANY, int *msgID = NULL);
    /*
     * Removes a message from the queue
     */
    RSCode KillMsg(int AppID, int msgID);
    
    /*
     * Sets the size value to the largest messages available
     * with all configured Radios
     * The flags are important because encrypted message need more space
     */
    RSCode MaxMessageSize(int *size, int msgFlags = 0);
    
    /*
     * starts Shuttle jobs, returns 0 when nothing is needs to be done
     * retuns > 0 when it should be called again.
     * RunShuttle is called on user level (non interrupts)
     */
    int RunShuttle(void);
    
    /*
     * We need to process all input messages, the _recv list schould be empty ASAP
     * because the data is only temporary available until the next packet.
     */
    void ProcessReceivedMessages(void);
    
    void SaveTimeOnAirSlot(devid_t destination, int AppID, int msgFlags, int respWindow, uint8_t channel, uint8_t factor, int timeOnAir);

private:
    enum PacketStatus {
        PS_Queued,
        PS_Sent,
        PS_GotSendSlot,
        PS_WaitForConfirm,
        PS_SendRequestCompleted,
        PS_SendRequestConfirmed,
        PS_SendTimeout,
    };

    struct RadioEntry; // forward decl.
    struct ReceivedMsgEntry {
        void *RxData;
        int RxSize;
        int rssi;
        int snr;
        struct RadioEntry *re;
    };
    
    struct RadioStats {
        int RxPackets;
        int TxPackets;
        int RxErrorCount;
        int channelBusyCount;
        long long rxBytes;
        long long txBytes;
        int noAuthMessageCount;
        int unkownMessageCount;
        int appNotSupported;
        int protocolError;
        int noMemoryError;
        time_t startupTime;
    };

    struct RadioEntry {
        Radio *radio;
        RadioEvents_t radioEvents;
        const RadioProfile *profile;
        RadioModems_t modem;
        volatile signed char _CADdetected;
        uint16_t lastTxSize;
        int lastTxPower;
        struct ReceivedMsgEntry rxMsg;
        struct RadioStats rStats;
        int maxTimeOnAir;
        int retry_ms;
        uint32_t random;
        uint32_t random2;
    };
    
    struct AppEntry {
        int AppID;
        AppRecvHandler handler;
        int msgID;
        void *password;
        uint8_t pwLen;
        bool pwdConnected;
    };
    
    struct ConnectEntry {
        devid_t stationID;
        int AppID;
        bool authorized;
        uint32_t random[2];
    };
    
    struct SendMsgEntry {
        int AppID;
        void *data;
        int len;
        int flags;
        devid_t stationID;
        int msgID;
        int retryCount;
        
        AppEntry *aep;
        ConnectEntry *cep;
        PacketStatus pStatus;
        int respWindow;
        uint32_t responseTime;
        uint32_t lastSentTime;
        uint32_t lastTimeOnAir;
        uint32_t confirmTimeout;
        int retry_ms;
        uint8_t channel;
        uint8_t factor;
        uint32_t securityData[8];
        uint32_t tmpRandom[2];
    };
    
    struct SignalStrengthEntry {
        int rx_dBm;
        devid_t stationID;
        time_t lastUpdate;
        int rcnCnt;
    };
    
    struct TimeOnAirSlotEntry {
        devid_t stationID;
        int AppID;
        uint8_t channel;
        uint8_t factor;
        uint32_t busy_time;
        int busy_ms;
    };
    
    struct EncryptionHeader {
        uint32_t version : 3;	// 3-bit encryption version
		uint32_t dataSum : 13;	// check sum of all packet data
	    uint16_t msgSize : 11;	// same as in RadioHeader
        uint16_t msgID   : 5;	// same as in RadioHeader
        uint32_t random;
    };
    
    enum RadioHeaderVersions {
        RSMagic					= 0b1011,
        RSHeaderFully_v1 		= 0b001,
        RSHeaderPacked_v1		= 0b010,
        RSHeaderFullySize_v1 	= 16,
        RSHeaderPackedSize_v1 	= 12,
        msgIDv1Mask 			= 0b11111,
        Packedv1MaxApps			= (1<<11)-1,
        Packedv1MaxRespWindow 	= (1<<11)-1,
        Fullyv1MaxRespWindow 	= (1<<16)-1,
        Packedv1MaxDeviceID 	= (1<<21)-1,
        MaxWinScale				= (1<<4)-1,
        DataSumBits				= 13,
    };
    
    /*
     * The RadioShuttle communication header (similar to UDP),
     * but optimzed for radios.
     * A packet crc checksum gets done on the link layer, no field needed here
     * The source sends always this request to the other side
     * - Check immidiately for a response after sending.
     * - Check again for a response in responseWindow*2 milli seconds
     *
     */
    struct RadioHeader {
        // 4 bytes
        uint16_t magic   : 4;		// 4-bit magic,
        uint16_t version : 3;		// 3-bit version
        uint16_t msgFlags: 9;		// e.g.: Request/Response NeedsConfirm, Priority, Encrypted
        union {
            struct {
                uint16_t msgSize : 11;	// 11 bit message size bits allows max 2048 bytes
                uint16_t msgID   : 5;	// ID for the app communication, truncated to 5 bit.
            } data;
            struct {
                uint16_t channel : 4;	// specify the channel to switch for this transaction
                uint16_t factor  : 3;   // specifies the spreading factor index (6-12)
                uint16_t winScale: 4;	// set the window scaling factor
            } option;
        } s;
        union {
            struct { // 8 bytes
                uint32_t appID      : 11;	// first 2048 Application identifiers for the request
                devid_t destination : 21;	// device ID of the destination 2^11 first 2 million devices
                uint32_t respWindow : 11;	// wait for the respWindow (ms) max 2048 ms before sending data.
                devid_t source      : 21;	// device ID of the destination 2^11 first 2 million devices
            } packedv1;
            struct { // 12 bytes
                uint16_t appID;			// Application identifier for the request
                uint16_t respWindow;	// we listen for a 2nd resonse in responseWindow (ms)
                devid_t destination;	// device ID of the destination
                devid_t source;			// deviceID of the source
            } fullyv1;
        } u;
    };
    
    struct WireDumpSettings {
        devid_t stationID;
        bool sents;
        bool recvs;
        Radio *radio;
    };
    
    /*
     * Radio specific callbacks
     */
public:
    void RS_TxDone(Radio *radio, void *userData);
    void RS_RxDone(Radio *radio, void *userData, uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
    void RS_TxTimeout(Radio *radio, void *userData);
    void RS_RxTimeout(Radio *radio, void *userData);
    void RS_RxError(Radio *radio, void *userData);
    void RS_CadDone(Radio *radio, void *userData, bool channelActivityDetected);

private:
    /*
 	 * The CadDetection will turn the radio into a Cad listen mode
     * to detect if a LoRa modulated signal in the air on our channel.
     * This takes a few ms (at SF7) and the interrupt RS_CadDone will report
     * the status.
     * The radio->_CADdetected contains the result (0 for idle, 1 for busy)
     */
    bool CadDetection(RadioEntry *re);
    
    /*
     * Get statistics of all messages and errors
     * A pointer of the pointer containing the RadioStats must be provided.
     * The statistics contains the information of the first radio, unless
     * a radio is specified.
     */
    RSCode GetStatistics(struct RadioStats **stats, Radio *radio = NULL);
    
    /*
     * The processing code returns a status code if it has been able to process this message
     * true or if skips the message false
     */
    bool ProcessResponseMessage(ReceivedMsgEntry *rme, AppEntry *aep, SendMsgEntry *mep, int  msgFlags, void *data, int len, devid_t source, devid_t respWindow, uint8_t channel, uint8_t factor);
    
    bool ProcessRequestMessage(ReceivedMsgEntry *rme, AppEntry *aep, int  msgFlags, void *data, int len, int msgID, devid_t source, uint32_t respWindow, uint8_t channel, uint8_t factor);
    
    /*
     * Our main send function is responsible for header packing,
     * compression and encryption and finally sends a packet via the radio.
     * It returns true if we have been able to sent the message.
     */
    bool SendMessage(RadioEntry *re, void *data, int len, int msgID, int AppID, devid_t stationID, int flags, int respWindow, uint8_t channel, uint8_t factor);
    
    /*
     * We keep a little cache lists of the power needed for different stations
     * This saves a lot of energy and reduces signal strength to keep the network
     * less busy. e.g. other radio networks do not need to receive our signals.
     */
    int CalculateTXPower(RadioEntry *re, devid_t stationID);
    bool UpdateSignalStrength(devid_t stationID, int dBm);
    bool DeleteSignalStrength(devid_t stationID);
    
    
    /*
     * Our main receive function is responsible for header unpacking,
     * de-compression and decryption and finally proivdes the unpacked data.
     * It returns true if we have been able to detect and unpack the message.
     */
    bool ReceiveMessage(ReceivedMsgEntry *rme, void **data, int &len, int &msgID, int &AppID, int &flags, devid_t &destination, devid_t &source, int &respWindow, uint8_t &channel, uint8_t &factor);

    
    /*
     * Dump all received and sent packets
     */
    void EnablePacketTrace(devid_t stationID = DEV_ID_ANY, bool sents = false, bool recvs = false, Radio *radio = 0);
    
    void PacketTrace(RadioEntry *re, const char *name, RadioHeader *rh, void *data, int len, bool sent, ReceivedMsgEntry *rme);
    
    /*
     * A dummy function which is being called for every timeout, the timer wakes up
     * the sleep and therefore the RunShuttle starts processing.
     */
    void TimeoutFunc(void);
    
    uint32_t GetDataSum(int maxbits, void *data, int len);
    
    
private:
    const char *_name;
    devid_t _deviceID;
    RadioType _radioType;
    int _maxMTUSize;
    list<RadioEntry> _radios;
    map<int, AppEntry> _apps;
    map<std::pair<devid_t,int>, ConnectEntry> _connections;
    list<SendMsgEntry> _sends;
    list<ReceivedMsgEntry> _recvs;
    map<devid_t, SignalStrengthEntry> _signals;
    list<TimeOnAirSlotEntry> _airtimes;
    Timeout *timer;
    volatile uint32_t prevTimeout;
    int SetTimerCount;
    
    static const RadioProfile defaultProfile[];
    volatile bool busyInShuttle;
    WireDumpSettings _wireDumpSettings;
    const static int MAX_SENT_RETRIES = 3;	// defines the number of retries of sents (with confirm)
    const static int RX_TIMEOUT_1HOUR	= 3600000;
    RadioStatusInterface *_statusIntf;
    RadioSecurityInterface *_securityIntf;
};

#endif // __RADIOSHUTTLE_H__
#endif // FEATURE_LORA

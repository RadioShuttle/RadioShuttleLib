/*
 * This is an unpublished work copyright
 * (c) 2017 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 *
 *
 * Use is granted to registered RadioShuttle licensees only.
 * Licensees must own a valid serial number and product code.
 * Details see: www.radioshuttle.de
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
#define my_us_ticker_read	us_getTicker
#define STATIC_ASSERT   _Static_assert
#define ASSERT assert
using namespace std;
#define FEATURE_LORA	1
#endif

#ifdef DEVICE_LOWPOWERTIMER
#define MyTimeout LowPowerTimeout
#define my_us_ticker_read	lp_ticker_read
#else
#define MyTimeout Timeout
#endif

#include <list>
#include <map>
#include "radio.h"
#include "RadioStatusInterface.h"
#include "RadioSecurityInterface.h"

#ifdef ARDUINO
#define map	std::map // map clashes with Arduino map()
#endif
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
    RS_NoRadioConfigured,			// No radio added so far
    RS_NoRadioAvailable,            // Radio could not be detected
    RS_RadioNotFound,				// The specified radio is not available
    RS_UnknownModemType,            // No FSK no LoRa modem type
    RS_MessageSizeExceeded,			// Message size too long
    RS_InvalidProductCode,			// license does not match
    RS_InvalidParam,				// invalid parameter.
    RS_OutOfMemory,					// unable to allocate memory
} RSCode;



// Shuttle
class RadioShuttle
{
public:
    typedef uint32_t	devid_t;
    static const int DEV_ID_ANY = 0;
    static const int TX_POWER_AUTO = 9999;
    
    struct RadioProfile {
        int Frequency;      // in Hz
        int Bandwidth;      // in Hz
        int TXPower;        // in dBm
        int SpreadingFaktor;// 7-12
        int FrequencyOffset;// +/- in Hz
    };
    
    enum RadioType {
        RS_Node_Offline,   	// Sleep mode until sending, < 10k RAM
        RS_Node_Checking,   // Sleep mode, checks for messages regulary, < 10k RAM
        RS_Node_Online,     // Always powered-on, < 10k RAM
        RS_Station_Basic,	// Always active for one or more apps < 15k RAM
        RS_Station_Server,	// Always active, lots of memory, routing options, < 1 MB RAM
    };
    
    typedef void (*AppRecvHandler)(int AppID, devid_t stationID, int msgID, int status, void *data, int length);


    enum MsgStatus {
        MS_SentCompleted,			// A previous SendMsg has been sent
        MS_SentCompletedConfirmed,	// A previous SendMsg has been sent and confirmed
        MS_SentTimeout,				// A timeout occurred, number of retries exceeded
        
        MS_RecvData,				// A simple input message
        MS_RecvDataConfirmed,		// Received a confirmed message
        MS_NoStationFound,
        MS_NoStationSupportsApp,
        MS_AuthenicationRequired,
        
        MS_StationConnected,		// A confirmation that the connection was accepted
        MS_StationDisconnected,		// A confirmation that the disconnect was accepted
    };
    
    enum MsgFlags {
        MF_Response			= 0x01, // A response from a previous request, or request
        MF_NeedsConfirm		= 0x02, // Station needs to acknloglage receivd
        MF_LowPriority		= 0x04, // Transfer within one minute
        MF_HighPriority		= 0x08, // ImmEdate transfer
        MF_MoreDataToCome	= 0x10, // Additional data is wait to be sent
        MF_Connect			= 0x20, // Connect a node to the station with password
        MF_Encrypted		= 0x40, // Message is encrypted
        MF_Authentication	= 0x80,	// Message requires prior authentication
        MF_SwitchOptions	= 0x100,// Tell the node to switch the channel for this trans ID
        MF_FlagsMask		= 0b111111111, // max flags for the RadioShuttle protocol, see msgFlags : 9
        /*
         * optional control flags are here.
         */
        CF_FreeData			= 0x200, // if a data buffer is provided, free it afterwords.
        CF_CopyData			= 0x400, // create a copy of the data.
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
        int decryptError;
		int lastRSSI;
		int lastSNR;
		devid_t lastRXdeviceID;
        time_t startupTime;
    };
    
    /*
     * Constructor requires a radio
     */
    RadioShuttle(const char *deviceName);

    /*
     * Destructor, stop radios, free resources
     */
    ~RadioShuttle();
    
    /*
     * Adds a license to use the RadioShuttle, licenses are
     * may be bundled with the board or are available for 
     * purchase at: www.radioshuttle.com
     * The license is bound to a special board ID and a fixed
     * node deviceID.
     */
    RSCode AddLicense(devid_t deviceID, uint32_t productCode);

    /*
     * Adds a new radio with a given profile
     * Optional profile, must be called before startup
     */
    RSCode AddRadio(Radio *radio, RadioModems_t modem, const struct RadioProfile *profile = NULL);
    
    /*
     * This allows to swtich between RS_Node_Offline and RS_Node_Online
     * after the Startup() is already completed.
     */
    RSCode UpdateNodeStartup(RadioType newRadioType);

    /*
     * The status interface allows custom status callbacks for
     * reporting send/receive/timeout activity e.g.: LED blinks
     */
    RSCode AddRadioStatus(RadioStatusInterface *statusIntf);
    
    /*
     * Support function for password hash generation and encryption
     * The external API interface has the advantage that the security
     * can be upgraded independently of the RadioShuttle library.
     * AES hardware accelleration can be one reason for it.
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
     * Registers an application, the AppType is unique worldwide
     * and must be used for sending and receiving app data.
     * Multiple AppID registrations are supported.
     * The password can be a string to the password,
     * in case of binary passwd data the pwLen must be set.
     * If the password is set, clients must call Connect() prior
     * to any SendMsg.
     */
    RSCode RegisterApplication(int AppID, AppRecvHandler, void *password = NULL, int pwLen = 0);
    /*
     * Removes the AppID
     */
    RSCode DeRegisterApplication(int AppID);
    
    /*
     * Check if the password is specified for an app
     */
    RSCode  AppRequiresAuthentication(int AppID);
                       
    /*
     * Start a pairing process to connect a node to a station
     */
    RSCode StartPairing(char *name = NULL);
    /*
     * Connect the node against a station, it can be called multiple times
     * if communication with multiple station IDs is used.
     * The connect verifies the password against the app of remote station.
     */
    RSCode Connect(int AppID, devid_t stationID = DEV_ID_ANY);
    
    /*
     * Inform the station that an app is discontinuing
     */
    RSCode Disconnect(int AppID, devid_t stationID = ~0);
    
    /*
     * Prepare sending data to a remote application
     * The request is queued for sending.
     * valid flags see enum MsgFlags
     * txPower allows to overwrite the txPower in dBm.
     * The data is busy until the AppRecvHandler is called
     */
    RSCode SendMsg(int AppID, void *data, int len, int flags = 0, devid_t stationID = DEV_ID_ANY, int txPower = TX_POWER_AUTO, int *msgID = NULL);
    /*
     * Removes a message from the queue
     */
    RSCode KillMsg(int AppID, int msgID);
    
    /*
     * Sets a new profile for a given radio with a given profile
     * This can be called anytime after the RadioShuttle startup.
     */
    RSCode UpdateRadioProfile(Radio *radio, RadioType radioType, const struct RadioProfile *profile);
    
    /*
     * Sets the size value to the largest messages available
     * for all configured radios
     * The flags are important because encrypted messages need more space
     */
    RSCode MaxMessageSize(int *size, int msgFlags = 0);
    
    /*
     * Get statistics of all messages and errors
     * A pointer reference containing the RadioStats must be provided.
     * The statistics contain the information of the first radio, unless
     * the RadioEntry parameter is provided for a specified radio.
     */
    RSCode GetStatistics(struct RadioStats **stats, Radio *radio = NULL);
    

    /*
     * Dump all received and sent packets
     */
    void EnablePacketTrace(devid_t stationID = DEV_ID_ANY, bool sents = false, bool recvs = false, Radio *radio = 0);
    
    /*
     * The RadioShuttle is idle when there are no ongoing jobs, etc.
     */
    bool Idle(void);
    
    /*
     * Converts a RadioShuttle error code into a string.
     */
    const char *StrError(RSErrorCode err);
    
    /*
     * Converts the radio type into a clear text name
     */
    const char *GetRadioName(RadioType radioType);
    
    /*
     * Starts the main RadioShuttle loop, returns 0 when nothing needs to be done
     * Retuns > 0 when it should be called again
     * RunShuttle is called on user level (non-interrupt level)
     */
    int RunShuttle(void);
    
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
    

    struct RadioEntry {
        Radio *radio;
        RadioEvents_t radioEvents;
        const RadioProfile *profile;
        RadioModems_t modem;
        volatile signed char _CADdetected;
        uint16_t lastTxSize;
        int lastTxPower;
        int timeOnAir12Bytes;
        struct ReceivedMsgEntry rxMsg;
        struct RadioStats rStats;
        int maxTimeOnAir;
        int retry_ms;
        uint32_t lastTxDone;
        volatile bool txDoneReceived;
        volatile const char *intrDelayedMsg;
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
        int txPower;
        int msgID;
        int retryCount;
        bool releaseData;
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
		uint32_t dataSum : 13;	// Checksum of all packet data
	    uint16_t msgSize : 11;	// Same as in RadioHeader
        uint16_t msgID   : 5;	// Same as in RadioHeader
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
     * but optimized for radios.
     * A packet crc checksum is done on the link layer, no field needed here
     * The source always sends this request to the other side
     * - Check immediately for a response after sending
     * - Check again for a response in responseWindow*2 milliseconds
     *
     */
    struct RadioHeader {
        // 4 bytes
        uint16_t magic   : 4;		// 4-bit magic,
        uint16_t version : 3;		// 3-bit version
        uint16_t msgFlags: 9;		// e.g.: Request/Response NeedsConfirm, Priority, Encrypted
        union {
            struct {
                uint16_t msgSize : 11;	// 11-bit message size allows a maximum of 2048 bytes
                uint16_t msgID   : 5;	// ID for the app communication, truncated to 5-bit
            } data;
            struct {
                uint16_t channel : 4;	// Specify the channel to switch to for this transaction
                uint16_t factor  : 3;   // Specifies the spreading factor index (6-12)
                uint16_t winScale: 4;	// Set the window scaling factor
            } option;
        } s;
        union {
            struct { // 8 bytes
                uint32_t appID      : 11;	// First 2048 application identifiers for the request
                devid_t destination : 21;	// Device ID of the destination 2^11 first 2 million devices
                uint32_t respWindow : 11;	// Wait for the respWindow (ms) max 2048 ms before sending data.
                devid_t source      : 21;	// Device ID of the destination 2^11 first 2 million devices
            } packedv1;
            struct { // 12 bytes
                uint16_t appID;			// Application identifier for the request
                uint16_t respWindow;	// We listen for a 2nd resonse in responseWindow (ms)
                devid_t destination;	// Device ID of the destination
                devid_t source;			// DeviceID of the source
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
     * Radio specific callbacks are public to allow C wrapper function to call us.
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
 	 * The CadDetection will turn the radio into a Cad listening mode
     * to detect radio-modulated signals on the channel.
     * This takes a few ms (at SF7) and the interrupt RS_CadDone will report
     * the status.
     * The radio->_CADdetected contains the result (0 for idle, 1 for busy)
     */
    bool CadDetection(RadioEntry *re);
    
    /*
     * init the RX/TX of the Radio.
     */
    RSCode _initRadio(RadioEntry *re);
    
    /*
     * The processing function returns a status code if it has been able to process
     * this message. true' for messages processed, false for messages skipped.
     */
    bool ProcessResponseMessage(ReceivedMsgEntry *rme, AppEntry *aep, SendMsgEntry *mep, int  msgFlags, void *data, int len, devid_t source, devid_t respWindow, uint8_t channel, uint8_t factor);
    
    bool ProcessRequestMessage(ReceivedMsgEntry *rme, AppEntry *aep, int  msgFlags, void *data, int len, int msgID, devid_t source, uint32_t respWindow, uint8_t channel, uint8_t factor);
    
    void MessageSecurityError(ReceivedMsgEntry *rme, AppEntry *aep, int msgID, devid_t source, uint8_t channel, uint8_t factor);
    
    void SaveTimeOnAirSlot(devid_t destination, int AppID, int msgFlags, int respWindow, uint8_t channel, uint8_t factor, int timeOnAir);
    
    /*
     * Our main send function is responsible for header packing,
     * compression and encryption, and finally sends a packet via the radio.
     * It returns true if we have been able to sent the message.
     */
    bool SendMessage(RadioEntry *re, void *data, int len, int msgID, int AppID, devid_t stationID, int flags, int txPower, int respWindow, uint8_t channel, uint8_t factor);
    
    /*
     * We keep a little cache list of the power needed for different stations
     * This saves a lot of energy and reduces signal strength to keep the network
     * less busy. For example, other radio networks need not receive our signals.
     */
    int CalculateTXPower(RadioEntry *re, devid_t stationID);
    bool UpdateSignalStrength(devid_t stationID, int dBm);
    bool DeleteSignalStrength(devid_t stationID);
    
    
    /*
     * Our main receive function is responsible for header unpacking,
     * de-compression and decryption, and finally provides the unpacked data.
     * It returns true if we have been able to detect and unpack the message.
     */
    bool ReceiveMessage(ReceivedMsgEntry *rme, void **data, int &len, int &msgID, int &AppID, int &flags, devid_t &destination, devid_t &source, int &respWindow, uint8_t &channel, uint8_t &factor);
    
    /*
     * We need to process all input messages, the _recv list should be empty ASAP
     * because the data is only temporarily available until the next packet.
     */
    void ProcessReceivedMessages(void);
    
    
    void PacketTrace(RadioEntry *re, const char *name, RadioHeader *rh, void *data, int len, bool sent, ReceivedMsgEntry *rme);
    
    /*
     * A dummy function which is called for every timeout, the timer wakes up
     * the sleep and therefore the RunShuttle starts processing.
     */
    void TimeoutFunc(void);
    
    uint32_t GetDataSum(int maxbits, void *data, int len);
    
    
private:
    const char *_name;
    devid_t _deviceID;
    devid_t _tmpdeviceID;
    uint8_t _uuid[16];
    RadioType _radioType;
    int _maxMTUSize;
    list<RadioEntry> _radios;
    map<int, AppEntry> _apps;
    map<std::pair<devid_t,int>, ConnectEntry> _connections;
    list<SendMsgEntry> _sends;
    list<ReceivedMsgEntry> _recvs;
    map<devid_t, SignalStrengthEntry> _signals;
    list<TimeOnAirSlotEntry> _airtimes;
    MyTimeout *timer;
    volatile uint32_t prevWakeup;
    int SetTimerCount;
    
    static const RadioProfile defaultProfile[];
    volatile bool busyInShuttle;
    WireDumpSettings _wireDumpSettings;
    const static int MAX_SENT_RETRIES = 3;	// Defines the number of retries of sents (with confirm)
    const static int RX_TIMEOUT_1HOUR	= 3600000;
    RadioStatusInterface *_statusIntf;
    RadioSecurityInterface *_securityIntf;
    uint32_t _;
};

#endif // __RADIOSHUTTLE_H__
#endif // FEATURE_LORA

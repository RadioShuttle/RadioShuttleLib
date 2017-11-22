/*
 * The file is licensed under the Apache License, Version 2.0
 * (c) 2017 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 */



class RadioStatusInterface {
public:
    virtual ~RadioStatusInterface() { }
    /*
     * Signaling that a radio message send has been initiated
     */
    virtual	void TXStart(int AppID, int toStation, int length, int dBm) = 0;
    /*
     * Signaling that a radio message send has been completed
     */
    virtual void TXComplete(void) = 0;
    /*
     * Signaling that a radio message input has been received
     * and queued for later processing
     */
    virtual void RxDone(int size, int rssi, int snr) = 0;
    /*
     * Signaling that a radio message protocol processing has been completed
     */
    virtual void RxCompleted(void) = 0;
    /*
     * Signaling that a higher-level message received a timeout
     * after the specified retry period
     */
    virtual void MessageTimeout(int AppID, int toStation) = 0;
    
    void SetStationID(int stationID) { _stationID = stationID; };
    
    void SetRadioType(const char *radioType) { _radioType = radioType; };

    void SetRadioParams(int frequency, int spreadingFactor) {
        _frequency = frequency; _spreadingFactor = spreadingFactor; };
    
    int _frequency;			/* automaticlally set on RadioShuttle::Startup  */
    int _spreadingFactor;	/* automaticlally set on RadioShuttle::Startup */
    int _stationID;			/* automaticlally set */
    const char *_radioType;	/* automaticlally set */
};

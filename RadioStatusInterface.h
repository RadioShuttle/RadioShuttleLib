/*
 * The file is Licensed under the Apache License, Version 2.0
 * (c) 2017 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 */



class RadioStatusInterface {
public:
    /*
     * Signaling that a radio message send has been startet
     */
    virtual	void TXStart(int AppID, int toStation, int length) = 0;
    virtual void TXComplete(void) = 0;
    virtual void RxDone(int size, int rssi, int snr) = 0;
    virtual void RxCompleted(void) = 0;
    /*
     * Signaling that a higher level message received an timeout
     * after the specified retry period
     */
    virtual void MessageTimeout(int AppID, int toStation) = 0;
};

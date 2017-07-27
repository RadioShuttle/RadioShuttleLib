/*
 * The file is Licensed under the Apache License, Version 2.0
 * (c) 2017 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 */



class STMRadioStatus : public RadioStatusInterface {
public:
   	STMRadioStatus();
    
    virtual	void TXStart(int AppID, int toStation, int length);
    virtual void TXComplete(void);
    virtual void RxDone(int size, int rssi, int snr);
    virtual void RxCompleted(void);
    virtual void MessageTimeout(int AppID, int toStation);
    
private:
    DigitalOut *ledTX;
    DigitalOut *ledRX;
    DigitalOut *ledTimeout;
};


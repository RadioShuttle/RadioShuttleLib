/*
 * The file is licensed under the Apache License, Version 2.0
 * (c) 2017 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 */

#ifdef ARDUINO_Heltec_WIFI_LoRa_32
#include <Wire.h>
#include "SSD1306.h"
#endif


class MyRadioStatus : public RadioStatusInterface {
public:
   	MyRadioStatus();
    virtual ~MyRadioStatus();
    
    virtual	void TXStart(int AppID, int toStation, int length, int dBm);
    virtual void TXComplete(void);
    virtual void RxDone(int size, int rssi, int snr);
    virtual void RxCompleted(void);
    virtual void MessageTimeout(int AppID, int toStation);
    
    void UpdateDisplay(bool invert);
private:
    DigitalOut *ledTX;
    DigitalOut *ledRX;
    DigitalOut *ledTimeout;
    int _totalTX;
    int _totalRX;
    int _totalError;
    int _totalTimeout;
    bool inverted;
    
#ifdef ARDUINO_Heltec_WIFI_LoRa_32
    SSD1306 *display;
    DigitalOut *displayReset;
    char _line1[64];
    char _line2[64];
    char _line3[64];
    char _line4[64];
    char _line5[64];
    bool invertedDisplay;
#endif
};

